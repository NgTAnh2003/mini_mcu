/**
 * @file    test_mcu_host.c
 * @brief   Bộ kiểm thử host C11 có thể chạy ngay cho MCAL MCU Driver (AUTOSAR
 * CP R20-11).
 * @details Bao phủ trọng tâm:
 *          1. Chuỗi timeout SWS -> rollback HSI -> retry PLL -> STOP -> thức
 * dậy 72 MHz.
 *          2. Fallback HSI thất bại -> FAULT -> từ chối retry mù -> phục hồi
 * qua InitClock.
 *          3. Đọc chẩn đoán Mcu_Stm32f1_GetDiagStatus() từ ISR giả lập giữa lúc
 * chuyển xung.
 *          4. Báo DEM FAILED cho lỗi nguồn xung, KHÔNG báo DEM cho lỗi Flash,
 * báo PASSED khi phục hồi.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Mcu.h"
#if (MCU_DEM_ERROR_REPORT == STD_ON)
#include "Dem.h"
#endif
#include "Det.h"
#include "Mcu_Stm32f1_Diag.h"
#include "stm32f1xx.h"

/*============================================================================*/
/*                        BIẾN MÔ PHỎNG PHẦN CỨNG & SPY                       */
/*============================================================================*/

RCC_TypeDef Mock_RccReg;
FLASH_TypeDef Mock_FlashReg;
PWR_TypeDef Mock_PwrReg;
SCB_Type Mock_ScbReg;

static uint32_t Mock_Primask = 0UL;
static boolean Mock_FailSwsToPllOnce = FALSE;
static boolean Mock_FailRollbackToHsi = FALSE;
static boolean Mock_FailHseReady;
static boolean Mock_FailFlashLatency = FALSE;
static boolean Mock_AutoLockPll = FALSE;
static boolean Mock_InjectIsrDuringSw = FALSE;
static boolean Mock_IsrCaptured = FALSE;
static uint32_t Mock_WfiCount = 0UL;
static uint32_t Mock_PrimaskAtWfi = 0UL;
static Mcu_Stm32f1_DiagStatusType Mock_IsrDiagSnapshot;

static uint32_t Spy_DetCount = 0UL;
static uint8 Spy_LastDetApi;
static uint8 Spy_LastDetErr = 0U;

static uint32_t Spy_DemFailCount = 0UL;
static uint32_t Spy_DemPassCount = 0UL;
static uint32_t Spy_CbCount = 0UL;
static uint32_t Spy_CbPrimask = 0xFFUL;
static Mcu_ClockFailureReasonType Spy_CbReason = MCU_CLK_FAIL_NONE;
static Mcu_ClockStateType Spy_CbState = MCU_CLK_STATE_UNSPECIFIED;

static void Spy_ClockFailureCallback(Mcu_ClockFailureReasonType Reason,
                                     Mcu_ClockStateType State) {
  Spy_CbCount++;
  Spy_CbReason = Reason;
  Spy_CbState = State;
  Spy_CbPrimask =
      Mock_Primask; /* Xác nhận callback chạy ngoài vùng khóa ngắt */
}

/* Linker chuyển lời gọi qua bộ quan sát rồi tới Det.c thật. */
Std_ReturnType __real_Det_ReportError(uint16, uint8, uint8, uint8);
Std_ReturnType __wrap_Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId,
                               uint8 ErrorId) {
  assert(ModuleId == MCU_MODULE_ID && InstanceId == MCU_INSTANCE_ID);
  Spy_DetCount++;
  Spy_LastDetApi = ApiId;
  Spy_LastDetErr = ErrorId;
  return __real_Det_ReportError(ModuleId, InstanceId, ApiId, ErrorId);
}

#if (MCU_DEM_ERROR_REPORT == STD_ON)
Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId,
                                  Dem_EventStatusType EventStatus) {
  assert(EventId == DemConf_DemEventParameter_MCU_E_CLOCK_FAILURE);
  /* Bên gọi có thể đã khóa ngắt trước khi vào driver. */
  if (EventStatus == DEM_EVENT_STATUS_FAILED) {
    Spy_DemFailCount++;
  } else if (EventStatus == DEM_EVENT_STATUS_PASSED) {
    Spy_DemPassCount++;
  }
  return E_OK;
}

#endif

uint32_t Mock_GetPrimask(void) { return Mock_Primask; }
void Mock_DisableIrq(void) { Mock_Primask = 1UL; }
void Mock_SetPrimask(uint32_t val) { Mock_Primask = val; }

void Mock_DataBarrier(void) {
  /* Mô phỏng cờ sẵn sàng HSI / HSE / PLL */
  if ((Mock_RccReg.CR & RCC_CR_HSION) != 0UL) {
    Mock_RccReg.CR |= RCC_CR_HSIRDY;
  } else {
    Mock_RccReg.CR &= ~RCC_CR_HSIRDY;
  }

  if ((Mock_RccReg.CR & RCC_CR_HSEON) != 0UL && !Mock_FailHseReady) {
    Mock_RccReg.CR |= RCC_CR_HSERDY;
  } else {
    Mock_RccReg.CR &= ~RCC_CR_HSERDY;
  }

  if ((Mock_RccReg.CR & RCC_CR_PLLON) == 0UL) {
    Mock_RccReg.CR &= ~RCC_CR_PLLRDY;
  } else if (Mock_AutoLockPll == TRUE) {
    Mock_RccReg.CR |= RCC_CR_PLLRDY;
  }

  /* Mô phỏng lỗi xác nhận FLASH->ACR.LATENCY */
  if (Mock_FailFlashLatency == TRUE) {
    Mock_FlashReg.ACR &= ~FLASH_ACR_LATENCY;
  }

  /* Mô phỏng chuyển SWS theo SW kèm chèn lỗi và chèn ISR giữa quá trình chuyển
   */
  {
    uint32_t sw = (Mock_RccReg.CFGR & RCC_CFGR_SW);

    if ((Mock_InjectIsrDuringSw == TRUE) && (sw == RCC_CFGR_SW_PLL) &&
        (Mock_Primask == 0UL)) {
      /* Giả lập ISR cắt ngang đúng lúc SW đã ghi PLL nhưng SWS chưa xác nhận và
       * chưa công bố trạng thái */
      Mock_InjectIsrDuringSw = FALSE;
      assert(Mcu_Stm32f1_GetDiagStatus(&Mock_IsrDiagSnapshot) == E_OK);
      Mock_IsrCaptured = TRUE;
    }

    if (sw == RCC_CFGR_SW_PLL) {
      if (Mock_FailSwsToPllOnce == TRUE) {
        /* Giữ SWS ở HSI để gây timeout SWS -> kích hoạt rollback */
        /* Giữ nguyên nguồn thực tế cả khi ban đầu đang dùng HSE. */
      } else if ((Mock_RccReg.CR & RCC_CR_PLLRDY) != 0UL) {
        Mock_RccReg.CFGR =
            (Mock_RccReg.CFGR & ~RCC_CFGR_SWS) | RCC_CFGR_SWS_PLL;
      }
    } else if (sw == RCC_CFGR_SW_HSE) {
      if ((Mock_RccReg.CR & RCC_CR_HSERDY) != 0UL) {
        Mock_RccReg.CFGR =
            (Mock_RccReg.CFGR & ~RCC_CFGR_SWS) | RCC_CFGR_SWS_HSE;
      }
    } else {
      if (Mock_FailRollbackToHsi == FALSE) {
        Mock_RccReg.CFGR =
            (Mock_RccReg.CFGR & ~RCC_CFGR_SWS) | RCC_CFGR_SWS_HSI;
      }
    }
  }
}

void Mock_WaitForInterrupt(void) {
  Mock_WfiCount++;
  Mock_PrimaskAtWfi = Mock_Primask;

  /* Mô phỏng hành vi phần cứng STM32F103 khi thoát STOP: tắt HSE/PLL, đưa SW &
   * SWS về HSI */
  if (((Mock_ScbReg.SCR & SCB_SCR_SLEEPDEEP_Msk) != 0UL) &&
      ((Mock_PwrReg.CR & PWR_CR_PDDS) == 0UL)) {
    Mock_RccReg.CR &=
        ~(RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY);
    Mock_RccReg.CFGR = (Mock_RccReg.CFGR & ~(RCC_CFGR_SW | RCC_CFGR_SWS)) |
                       RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI;
  }
}

static void ResetMockAndSpies(void) {
  memset(&Mock_RccReg, 0, sizeof(Mock_RccReg));
  memset(&Mock_FlashReg, 0, sizeof(Mock_FlashReg));
  memset(&Mock_PwrReg, 0, sizeof(Mock_PwrReg));
  memset(&Mock_ScbReg, 0, sizeof(Mock_ScbReg));

  Mock_RccReg.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
  Mock_RccReg.CFGR = RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI;
  Mock_RccReg.CSR = RCC_CSR_PORRSTF | RCC_CSR_PINRSTF;

  Mock_Primask = 0UL;
  Mock_FailSwsToPllOnce = FALSE;
  Mock_FailRollbackToHsi = FALSE;
  Mock_FailHseReady = FALSE;
  Mock_FailFlashLatency = FALSE;
  Mock_AutoLockPll = FALSE;
  Mock_InjectIsrDuringSw = FALSE;
  Mock_IsrCaptured = FALSE;
  Mock_WfiCount = 0UL;
  Mock_PrimaskAtWfi = 0UL;

  Spy_DetCount = 0UL;
  Spy_LastDetErr = 0U;
  Spy_DemFailCount = 0UL;
  Spy_DemPassCount = 0UL;
  Spy_CbCount = 0UL;
  Spy_CbPrimask = 0xFFUL;
  Spy_CbReason = MCU_CLK_FAIL_NONE;
  Spy_CbState = MCU_CLK_STATE_UNSPECIFIED;
}

/*============================================================================*/
/*                              CÁC CA KIỂM THỬ                               */
/*============================================================================*/

static void Test_Timeout_Rollback_Retry_Stop_Wakeup_And_Dem(void) {
  Mcu_ConfigType cfg = Mcu_Config;
  Mcu_Stm32f1_DiagStatusType diag;

  ResetMockAndSpies();
  cfg.ClockFailureCallback = Spy_ClockFailureCallback;
  Mcu_Init(&cfg);

  /* 1. Khởi động PLL -> trạng thái PLL_PENDING, chưa báo DEM PASSED */
  assert(Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_PLL_72MHz) == E_OK);
  assert(Mcu_GetPllStatus() == MCU_PLL_UNLOCKED);
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_PLL_PENDING);
  assert(diag.PllPending == TRUE);
  assert(diag.ActiveConfigValid == FALSE);
  assert(Spy_DemPassCount == 0UL);

  /* 2. Giả lập PLL khóa; lần Distribute thứ nhất bị timeout SWS -> rollback HSI
   * thành công */
  Mock_RccReg.CR |= RCC_CR_PLLRDY;
  Mock_FailSwsToPllOnce = TRUE;

  assert(Mcu_DistributePllClock() == E_NOT_OK);
  assert(Spy_DetCount == 0UL); /* Lỗi phần cứng KHÔNG báo DET */
  assert(Spy_CbCount == 1UL);
  assert(Spy_CbPrimask == 0UL); /* Callback gọi ngoài khóa ngắt */
  assert(Spy_CbReason == MCU_CLK_FAIL_SWS_SWITCH_TIMEOUT);
  assert(Spy_CbState == MCU_CLK_STATE_FALLBACK_HSI);
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  assert(Spy_DemFailCount == 1UL);
  assert(Spy_DemPassCount == 0UL);
#endif

  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_FALLBACK_HSI);
  assert(diag.PllDistributed == FALSE);
  assert(diag.PllPending ==
         TRUE); /* QUAN TRỌNG: vẫn giữ cấu hình chờ để retry */
  assert(diag.ActiveConfigValid == FALSE);
  assert(diag.LiveSwSource == MCU_SYSCLK_SRC_HSI);
  assert(diag.LiveSysClkSource == MCU_SYSCLK_SRC_HSI);
  assert(diag.LivePllLocked == TRUE);

  /* 3. Retry DistributePllClock lần 2 (không còn lỗi SWS) + chèn ISR đọc chẩn
   * đoán giữa lúc chuyển */
  Mock_FailSwsToPllOnce = FALSE;
  Mock_InjectIsrDuringSw = TRUE;

  assert(Mcu_DistributePllClock() == E_OK);
  assert(Mock_IsrCaptured == TRUE);

  /* Kiểm tra ảnh chụp ISR giữa chuyển trạng thái: Published vẫn nhất quán
   * (FALLBACK_HSI), Live SW = PLL */
  assert(Mock_IsrDiagSnapshot.ClockState == MCU_CLK_STATE_FALLBACK_HSI);
  assert(Mock_IsrDiagSnapshot.PllDistributed == FALSE);
  assert(Mock_IsrDiagSnapshot.PllPending == TRUE);
  assert(Mock_IsrDiagSnapshot.PublishedSwSource == MCU_SYSCLK_SRC_HSI);
  assert(Mock_IsrDiagSnapshot.LiveSwSource == MCU_SYSCLK_SRC_PLL);
  assert(Mock_IsrDiagSnapshot.LiveSysClkSource == MCU_SYSCLK_SRC_HSI);

  /* Sau khi retry hoàn tất: STABLE + ActiveConfigValid == TRUE + DEM PASSED */
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_STABLE);
  assert(diag.PllDistributed == TRUE);
  assert(diag.PllPending == FALSE);
  assert(diag.ActiveConfigValid == TRUE);
  assert(diag.LiveSwSource == MCU_SYSCLK_SRC_PLL);
  assert(diag.LiveSysClkSource == MCU_SYSCLK_SRC_PLL);
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  assert(Spy_DemPassCount == 1UL);
#endif

  /* 4. Vào STOP và thức dậy: phải khôi phục đủ HSE -> PLL -> 72 MHz từ
   * ActiveClockCfgPtr */
  Mock_AutoLockPll = TRUE;
  Mcu_SetMode(McuConf_McuModeSettingConf_STOP);

  assert(Mock_WfiCount == 1UL);
  assert(Mock_PrimaskAtWfi == 1UL); /* PRIMASK luôn = 1 khi gọi WFI */
  assert(Mock_Primask == 0UL); /* Trả lại đúng PRIMASK ban đầu của bên gọi */
  assert((Mock_ScbReg.SCR & SCB_SCR_SLEEPDEEP_Msk) == 0UL);

  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_STABLE);
  assert(diag.PllDistributed == TRUE);
  assert(diag.ActiveConfigValid == TRUE);
  assert(diag.StopWakeupRestoreFailed == FALSE);
  assert(diag.LiveSysClkSource == MCU_SYSCLK_SRC_PLL);
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  assert(Spy_DemPassCount == 2UL);
#endif

  printf("[PASS] Test_Timeout_Rollback_Retry_Stop_Wakeup_And_Dem\n");
}

static void Test_FallbackFail_Blocks_BlindRetry(void) {
  Mcu_ConfigType cfg = Mcu_Config;
  Mcu_Stm32f1_DiagStatusType diag;

  ResetMockAndSpies();
  cfg.ClockFailureCallback = Spy_ClockFailureCallback;
  Mcu_Init(&cfg);

  assert(Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_PLL_72MHz) == E_OK);
  Mock_RccReg.CR |= RCC_CR_PLLRDY;

  /* Cả SWS = PLL lẫn rollback SWS = HSI đều thất bại */
  Mock_FailSwsToPllOnce = TRUE;
  Mock_FailRollbackToHsi = TRUE;
  Mock_RccReg.CFGR = (Mock_RccReg.CFGR & ~RCC_CFGR_SWS) | RCC_CFGR_SWS_HSE;

  assert(Mcu_DistributePllClock() == E_NOT_OK);
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_FAULT);
  assert(diag.PllPending == FALSE);
  assert(diag.ActiveConfigValid == FALSE);

  /* Thử gọi lại DistributePllClock khi đang FAULT dù PLLRDY vẫn 1: PHẢI bị từ
   * chối */
  Mock_FailSwsToPllOnce = FALSE;
  Mock_FailRollbackToHsi = FALSE;
  assert(Mcu_DistributePllClock() == E_NOT_OK);
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_FAULT);
  assert(diag.PllDistributed == FALSE);

  /* Phục hồi hợp lệ bằng cách gọi lại Mcu_InitClock(HSI_8MHz) */
  assert(Mcu_InitClock(McuConf_McuClockSettingConfig_HSI_8MHz) == E_OK);
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_STABLE);
  assert(diag.ActiveConfigValid == TRUE);
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  assert(Spy_DemPassCount == 1UL);
#endif

  printf("[PASS] Test_FallbackFail_Blocks_BlindRetry\n");
}

static void Test_FlashLatencyTimeout_DoesNotReportDemClockFailure(void) {
  Mcu_ConfigType cfg = Mcu_Config;
  Mcu_Stm32f1_DiagStatusType diag;

  ResetMockAndSpies();
  cfg.ClockFailureCallback = Spy_ClockFailureCallback;
  Mcu_Init(&cfg);

  /* Tiêm lỗi xác nhận FLASH->ACR.LATENCY khi chuyển lên 72 MHz (2 WS) */
  Mock_FailFlashLatency = TRUE;
  assert(Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_PLL_72MHz) ==
         E_NOT_OK);

  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.LastFailureReason == MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT);
  assert(diag.LastFailureClass == MCU_FAIL_CLASS_FLASH_INTERFACE);
  assert(Spy_CbCount == 1UL);
  assert(Spy_DemFailCount ==
         0UL); /* QUAN TRỌNG: Lỗi Flash KHÔNG báo DEM MCU_E_CLOCK_FAILURE */

  printf("[PASS] Test_FlashLatencyTimeout_DoesNotReportDemClockFailure\n");
}

/** @brief Kiểm tra dữ liệu sau khi driver ghi theo độ rộng cấu hình của từng vùng. */
static void Test_RamSections(void) {
  Mcu_Init(&Mcu_Config);
  for (Mcu_RamSectionType i = 0U; i < Mcu_Config.NumRamSections; ++i) {
    const Mcu_RamSectionConfigType *region = &Mcu_Config.RamSectionConfigPtr[i];
    const unsigned char *bytes = region->BaseAddress;
    memset(region->BaseAddress, region->DefaultValue ^ 0xFFU, region->SectionSize);
    assert(Mcu_InitRamSection(i) == E_OK);
    for (uint32 j = 0U; j < region->SectionSize; ++j) {
      assert(bytes[j] == region->DefaultValue);
    }
  }
}

/** @brief Thức dậy khi PLL đang chờ, giữ nguyên quyền phân phối và mặt nạ ngắt của bên gọi. */
static void Test_StopWithPendingPll(void) {
  Mcu_Stm32f1_DiagStatusType diag;
  ResetMockAndSpies();
  Mcu_Init(&Mcu_Config);
  assert(Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_PLL_72MHz) == E_OK);
  Mock_Primask = 1U;
  Mcu_SetMode(McuConf_McuModeSettingConf_STOP);
  assert(Mock_PrimaskAtWfi == 1U && Mock_Primask == 1U);
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.ClockState == MCU_CLK_STATE_PLL_PENDING && diag.PllPending);
  assert(!diag.PllDistributed && diag.LiveSysClkSource == MCU_SYSCLK_SRC_HSI);
  assert((Mock_RccReg.CR & RCC_CR_PLLON) != 0U);
}

/** @brief Sau STOP, thạch anh không sẵn sàng phải để lại trạng thái dự phòng và báo lỗi. */
static void Test_StopHseTimeout(void) {
  Mcu_Stm32f1_DiagStatusType diag;
  ResetMockAndSpies();
  Mcu_Init(&Mcu_Config);
  Mock_AutoLockPll = TRUE;
  assert(Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_PLL_72MHz) == E_OK);
  assert(Mcu_DistributePllClock() == E_OK);
  Mock_FailHseReady = TRUE;
  Mock_Primask = 1U;
  Mcu_SetMode(McuConf_McuModeSettingConf_STOP);
  assert(Mock_Primask == 1U);
  assert(Mcu_Stm32f1_GetDiagStatus(&diag) == E_OK);
  assert(diag.StopWakeupRestoreFailed && !diag.PllDistributed);
  assert(diag.ClockState == MCU_CLK_STATE_FALLBACK_HSI);
  assert(diag.LiveSysClkSource == MCU_SYSCLK_SRC_HSI);
  assert(diag.LastFailureReason == MCU_CLK_FAIL_STOP_HSE_TIMEOUT);
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  assert(Spy_DemFailCount == 1U);
#endif
}

int main(void) {
  assert(Mcu_GetResetRawValue() == MCU_RAW_RESET_UNINIT_VALUE);
  assert(Spy_DetCount == 1U && Spy_LastDetErr == MCU_E_UNINIT);
  assert(Spy_LastDetApi == MCU_SID_GET_RESET_RAW_VALUE);
  Test_Timeout_Rollback_Retry_Stop_Wakeup_And_Dem();
  Test_FallbackFail_Blocks_BlindRetry();
  Test_FlashLatencyTimeout_DoesNotReportDemClockFailure();
  Test_RamSections();
  Test_StopWithPendingPll();
  Test_StopHseTimeout();
  printf("ALL HOST TESTS PASSED!\n");
  return 0;
}