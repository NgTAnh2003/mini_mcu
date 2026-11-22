/**
 * @file    Mcu.c
 * @brief   Hiện thực đầy đủ MCAL MCU Driver cho STM32F103C6T6 và STM32F103C8T6.
 * @details Tuân thủ AUTOSAR Classic Platform R20-11
 * (AUTOSAR_SWS_MCUDriver 4.6.0).
 *          - Toàn bộ symbol hàm đặt vào section .text.mcu qua
 * MCU_MEMMAP_CODE_ATTR.
 *          - Công bố trạng thái phần mềm nguyên tử trong vùng khóa ngắt ngắn.
 *          - Giữ cấu hình PLL đang chờ sau timeout + rollback HSI để hỗ trợ
 * retry và STOP.
 *          - Ánh xạ DEM MCU_E_CLOCK_FAILURE (FAILED/PASSED) tách riêng khỏi lỗi
 * Flash.
 */

#include "Mcu.h"
#include "Mcu_Hw_Stm32f1.h"
#include "Mcu_Stm32f1_Diag.h"
#include <stdint.h>

#if (MCU_DEV_ERROR_DETECT == STD_ON)
#include "Det.h"
#endif /* (MCU_DEV_ERROR_DETECT == STD_ON) */

#if (MCU_DEM_ERROR_REPORT == STD_ON)
#include "Dem.h"
#endif /* (MCU_DEM_ERROR_REPORT == STD_ON) */

/*============================================================================*/
/*                    KIỂM TRA NHẤT QUÁN PHIÊN BẢN FILE .C                    */
/*============================================================================*/

#define MCU_C_AR_RELEASE_MAJOR_VERSION (4U)
#define MCU_C_AR_RELEASE_MINOR_VERSION (6U)
#define MCU_C_AR_RELEASE_REVISION_VERSION (0U)

#define MCU_C_SW_MAJOR_VERSION (1U)
#define MCU_C_SW_MINOR_VERSION (2U)
#define MCU_C_SW_PATCH_VERSION (0U)

#if ((MCU_C_AR_RELEASE_MAJOR_VERSION != MCU_AR_RELEASE_MAJOR_VERSION) ||       \
     (MCU_C_AR_RELEASE_MINOR_VERSION != MCU_AR_RELEASE_MINOR_VERSION) ||       \
     (MCU_C_AR_RELEASE_REVISION_VERSION != MCU_AR_RELEASE_REVISION_VERSION))
#error "Lệch phiên bản AUTOSAR Release giữa Mcu.c và Mcu.h"
#endif

#if ((MCU_C_SW_MAJOR_VERSION != MCU_SW_MAJOR_VERSION) ||                       \
     (MCU_C_SW_MINOR_VERSION != MCU_SW_MINOR_VERSION))
#error "Lệch phiên bản phần mềm giữa Mcu.c và Mcu.h"
#endif

_Static_assert(FALSE == 0U,
               "FALSE phải bằng 0U để phù hợp khởi tạo 0 trong .bss.mcu");

/*============================================================================*/
/*                        TRẠNG THÁI NỘI BỘ CỦA DRIVER                        */
/*============================================================================*/

/**
 * @brief Cấu trúc trạng thái vận hành nội bộ đã công bố của module MCU
 * (.bss.mcu).
 */
typedef struct {
  const Mcu_ConfigType *ConfigPtr; /**< Bộ cấu hình Post-Build đang áp dụng. */
  const Mcu_ClockConfigType
      *ActiveClockCfgPtr; /**< Cấu hình xung đã công bố ổn định (dùng khôi phục
                             sau STOP). */
  const Mcu_ClockConfigType
      *PendingPllClockCfgPtr; /**< Cấu hình PLL đang chờ phân phối hoặc retry
                                 sau rollback HSI. */
  Mcu_ClockStateType ClockState; /**< Trạng thái máy xung đã công bố. */
  Mcu_ClockFailureReasonType
      LastFailureReason; /**< Mã lỗi phần cứng gần nhất. */
  Mcu_FailureClassType
      LastFailureClass; /**< Nhóm lỗi (CLOCK_SOURCE hay FLASH_INTERFACE). */
  Mcu_SysClkSourceType PublishedSwSource; /**< Giá trị SW ghi nhận tại lần công
                                             bố trạng thái gần nhất. */
  Mcu_SysClkSourceType PublishedSysClkSource; /**< Giá trị SWS ghi nhận tại lần
                                                 công bố trạng thái gần nhất. */
  boolean PllDistributed; /**< TRUE khi PLL đã công bố phân phối ổn định. */
  boolean StopWakeupRestoreFailed; /**< TRUE nếu lần thoát STOP gần nhất khôi
                                      phục lỗi. */
  Mcu_RawResetType LatchedRawReset; /**< Giá trị RCC->CSR chốt một lần duy nhất
                                       trong Mcu_Init. */
  boolean ResetFlagsLatched; /**< Cờ bảo vệ không đọc đè RCC->CSR khi gọi lại
                                Mcu_Init. */
} Mcu_RuntimeStateType;

#define MCU_START_SEC_VAR_CLEARED_BOOLEAN
#include "Mcu_MemMap.h"

/** @brief Cờ trạng thái khởi tạo (không gán = FALSE để giữ section .bss.mcu
 * dạng NOBITS đồng nhất). */
static boolean Mcu_Initialized MCU_MEMMAP_VAR_CLEARED_ATTR;

#define MCU_STOP_SEC_VAR_CLEARED_BOOLEAN
#include "Mcu_MemMap.h"

#define MCU_START_SEC_VAR_CLEARED_UNSPECIFIED
#include "Mcu_MemMap.h"

/** @brief Biến trạng thái vận hành nội bộ của MCU Driver (.bss.mcu, startup xóa
 * về 0). */
static Mcu_RuntimeStateType Mcu_State MCU_MEMMAP_VAR_CLEARED_ATTR;

#define MCU_STOP_SEC_VAR_CLEARED_UNSPECIFIED
#include "Mcu_MemMap.h"

/*============================================================================*/
/*                          CÁC HÀM TRỢ GIÚP NỘI BỘ                           */
/*============================================================================*/

#define MCU_START_SEC_CODE
#include "Mcu_MemMap.h"

/**
 * @brief   Gửi lỗi phát triển (API misuse) sang DET nếu MCU_DEV_ERROR_DETECT ==
 * STD_ON.
 */
static MCU_MEMMAP_CODE_ATTR void Mcu_ReportDetError(uint8 ApiId,
                                                    uint8 ErrorId) {
#if (MCU_DEV_ERROR_DETECT == STD_ON)
  (void)Det_ReportError(MCU_MODULE_ID, MCU_INSTANCE_ID, ApiId, ErrorId);
#else
  (void)ApiId;
  (void)ErrorId;
#endif /* (MCU_DEV_ERROR_DETECT == STD_ON) */
}

/**
 * @brief   Báo DEM trạng thái PASSED cho MCU_E_CLOCK_FAILURE khi nguồn xung
 * đích đã xác nhận ổn định.
 * @details [SWS_Mcu_00053, SWS_Mcu_00258] Luôn gọi NGOÀI vùng khóa ngắt.
 */
static MCU_MEMMAP_CODE_ATTR void Mcu_ReportDemClockPassed(void) {
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  (void)Dem_SetEventStatus(MCU_DEM_EVENT_ID_CLOCK_FAILURE,
                           DEM_EVENT_STATUS_PASSED);
#endif /* (MCU_DEM_ERROR_REPORT == STD_ON) */
}

/**
 * @brief   Thông báo lỗi phần cứng ra ngoài module (luôn gọi NGOÀI vùng khóa
 * ngắt).
 * @details - Nếu FailureClass == MCU_FAIL_CLASS_CLOCK_SOURCE: báo DEM FAILED
 * [SWS_Mcu_00053, SWS_Mcu_00257].
 *          - Nếu FailureClass == MCU_FAIL_CLASS_FLASH_INTERFACE: KHÔNG báo DEM
 * MCU_E_CLOCK_FAILURE.
 *          - Luôn gọi Callback tích hợp nếu được bật và khác NULL_PTR.
 */
static MCU_MEMMAP_CODE_ATTR void
Mcu_NotifyHardwareFailure(Mcu_ClockFailureCallbackType Callback,
                          Mcu_ClockFailureReasonType FailureReason,
                          Mcu_FailureClassType FailureClass,
                          Mcu_ClockStateType CurrentState) {
#if (MCU_DEM_ERROR_REPORT == STD_ON)
  if (FailureClass == MCU_FAIL_CLASS_CLOCK_SOURCE) {
    (void)Dem_SetEventStatus(MCU_DEM_EVENT_ID_CLOCK_FAILURE,
                             DEM_EVENT_STATUS_FAILED);
  }
#else
  (void)FailureClass;
#endif /* (MCU_DEM_ERROR_REPORT == STD_ON) */

#if (MCU_CLOCK_FAILURE_NOTIFICATION == STD_ON)
  if (Callback != NULL_PTR) {
    Callback(FailureReason, CurrentState);
  }
#else
  (void)Callback;
  (void)FailureReason;
  (void)CurrentState;
#endif /* (MCU_CLOCK_FAILURE_NOTIFICATION == STD_ON) */
}

/**
 * @brief   Công bố trạng thái lỗi vào Mcu_State (PHẢI gọi bên trong vùng khóa
 * ngắt PRIMASK = 1).
 * @param   OpResult         Kết quả thao tác phần cứng chứa FailureReason,
 * FailureClass và FallbackOk.
 * @param   PreservePending  Cấu hình PLL cần giữ lại cho retry (NULL_PTR nếu
 * phải xóa sạch).
 */
static MCU_MEMMAP_CODE_ATTR void
Mcu_PublishFailureLocked(const Mcu_Hw_ClockOpResultType *OpResult,
                         const Mcu_ClockConfigType *PreservePending) {
  Mcu_SysClkSourceType sw = Mcu_Hw_GetRequestedSwSource();
  Mcu_SysClkSourceType sws = Mcu_Hw_GetActiveSysClkSource();

  Mcu_State.PublishedSwSource = sw;
  Mcu_State.PublishedSysClkSource = sws;
  Mcu_State.LastFailureReason = OpResult->FailureReason;
  Mcu_State.LastFailureClass = OpResult->FailureClass;
  Mcu_State.ActiveClockCfgPtr = NULL_PTR;
  Mcu_State.PllDistributed = FALSE;

  if ((OpResult->FallbackOk == TRUE) && (sw == MCU_SYSCLK_SRC_HSI) &&
      (sws == MCU_SYSCLK_SRC_HSI)) {
    Mcu_State.PendingPllClockCfgPtr = PreservePending;
    Mcu_State.ClockState = MCU_CLK_STATE_FALLBACK_HSI;
  } else {
    /* Fallback thất bại: xóa sạch cả Pending lẫn Active để ngăn retry mù */
    Mcu_State.PendingPllClockCfgPtr = NULL_PTR;
    Mcu_State.ClockState = MCU_CLK_STATE_FAULT;
  }
}

/**
 * @brief   Kiểm tra tính hợp lệ cấu trúc con trỏ mảng bên trong Mcu_ConfigType.
 */
static MCU_MEMMAP_CODE_ATTR boolean
Mcu_IsConfigStructValid(const Mcu_ConfigType *ConfigPtr) {
  boolean isValid = TRUE;

  if ((ConfigPtr->NumClockConfigs > 0U) &&
      (ConfigPtr->ClockConfigPtr == NULL_PTR)) {
    isValid = FALSE;
  } else if ((ConfigPtr->NumModeConfigs > 0U) &&
             (ConfigPtr->ModeConfigPtr == NULL_PTR)) {
    isValid = FALSE;
  } else if ((ConfigPtr->NumRamSections > 0U) &&
             (ConfigPtr->RamSectionConfigPtr == NULL_PTR)) {
    isValid = FALSE;
  } else {
    isValid = TRUE;
  }

  return isValid;
}

/*============================================================================*/
/*                    ĐỊNH NGHĨA 11 DỊCH VỤ CHUẨN AUTOSAR                     */
/*============================================================================*/

MCU_MEMMAP_CODE_ATTR void Mcu_Init(const Mcu_ConfigType *ConfigPtr) {
  if (ConfigPtr == NULL_PTR) {
    Mcu_ReportDetError(MCU_SID_INIT, MCU_E_INIT_FAILED);
  } else if (Mcu_IsConfigStructValid(ConfigPtr) == FALSE) {
    Mcu_ReportDetError(MCU_SID_INIT, MCU_E_PARAM_CONFIG);
  } else {
    uint32_t primask = Mcu_Hw_EnterCritical();
    Mcu_SysClkSourceType sw;
    Mcu_SysClkSourceType sws;

    if (Mcu_State.ResetFlagsLatched == FALSE) {
      Mcu_State.LatchedRawReset = Mcu_Hw_LatchAndClearResetFlags();
      Mcu_State.ResetFlagsLatched = TRUE;
    }

    sw = Mcu_Hw_GetRequestedSwSource();
    sws = Mcu_Hw_GetActiveSysClkSource();

    Mcu_State.ConfigPtr = ConfigPtr;
    Mcu_State.ActiveClockCfgPtr = NULL_PTR;
    Mcu_State.PendingPllClockCfgPtr = NULL_PTR;
    Mcu_State.LastFailureReason = MCU_CLK_FAIL_NONE;
    Mcu_State.LastFailureClass = MCU_FAIL_CLASS_NONE;
    Mcu_State.PublishedSwSource = sw;
    Mcu_State.PublishedSysClkSource = sws;
    Mcu_State.StopWakeupRestoreFailed = FALSE;
    Mcu_State.PllDistributed = FALSE;
    Mcu_State.ClockState =
        (sw != sws) ? MCU_CLK_STATE_FAULT : MCU_CLK_STATE_UNSPECIFIED;

    Mcu_Initialized = TRUE;
    Mcu_Hw_ExitCritical(primask);
  }
}

MCU_MEMMAP_CODE_ATTR Std_ReturnType
Mcu_InitRamSection(Mcu_RamSectionType RamSection) {
  Std_ReturnType retVal = E_NOT_OK;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_INIT_RAM_SECTION, MCU_E_UNINIT);
  } else if (RamSection >= Mcu_State.ConfigPtr->NumRamSections) {
    Mcu_ReportDetError(MCU_SID_INIT_RAM_SECTION, MCU_E_PARAM_RAMSECTION);
  } else {
    const Mcu_RamSectionConfigType *ramCfg =
        &Mcu_State.ConfigPtr->RamSectionConfigPtr[RamSection];

    if ((ramCfg->BaseAddress == NULL_PTR) || (ramCfg->SectionSize == 0UL)) {
      Mcu_ReportDetError(MCU_SID_INIT_RAM_SECTION, MCU_E_PARAM_RAMSECTION);
    } else {
      uintptr_t baseAddr = (uintptr_t)ramCfg->BaseAddress;
      uint32 index;

      if (ramCfg->WriteSize == MCU_RAM_WRITE_SIZE_1BYTE) {
        volatile uint8 *dst8 = (volatile uint8 *)ramCfg->BaseAddress;

        for (index = 0UL; index < ramCfg->SectionSize; index++) {
          dst8[index] = ramCfg->DefaultValue;
        }
        retVal = E_OK;
      } else if (ramCfg->WriteSize == MCU_RAM_WRITE_SIZE_2BYTE) {
        if (((baseAddr % 2U) == 0U) && ((ramCfg->SectionSize % 2UL) == 0UL)) {
          volatile uint16 *dst16 = (volatile uint16 *)ramCfg->BaseAddress;
          uint16 pattern16 = (uint16)(((uint16)ramCfg->DefaultValue << 8U) |
                                      (uint16)ramCfg->DefaultValue);
          uint32 count16 = ramCfg->SectionSize / 2UL;

          for (index = 0UL; index < count16; index++) {
            dst16[index] = pattern16;
          }
          retVal = E_OK;
        } else {
          Mcu_ReportDetError(MCU_SID_INIT_RAM_SECTION, MCU_E_PARAM_RAMSECTION);
        }
      } else if (ramCfg->WriteSize == MCU_RAM_WRITE_SIZE_4BYTE) {
        if (((baseAddr % 4U) == 0U) && ((ramCfg->SectionSize % 4UL) == 0UL)) {
          volatile uint32 *dst32 = (volatile uint32 *)ramCfg->BaseAddress;
          uint32 b = (uint32)ramCfg->DefaultValue;
          uint32 pattern32 = (b << 24U) | (b << 16U) | (b << 8U) | b;
          uint32 count32 = ramCfg->SectionSize / 4UL;

          for (index = 0UL; index < count32; index++) {
            dst32[index] = pattern32;
          }
          retVal = E_OK;
        } else {
          Mcu_ReportDetError(MCU_SID_INIT_RAM_SECTION, MCU_E_PARAM_RAMSECTION);
        }
      } else {
        Mcu_ReportDetError(MCU_SID_INIT_RAM_SECTION, MCU_E_PARAM_RAMSECTION);
      }
    }
  }

  return retVal;
}

#if (MCU_INIT_CLOCK == STD_ON)
MCU_MEMMAP_CODE_ATTR Std_ReturnType Mcu_InitClock(Mcu_ClockType ClockSetting) {
  Std_ReturnType retVal = E_NOT_OK;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_INIT_CLOCK, MCU_E_UNINIT);
  } else if (ClockSetting >= Mcu_State.ConfigPtr->NumClockConfigs) {
    Mcu_ReportDetError(MCU_SID_INIT_CLOCK, MCU_E_PARAM_CLOCK);
  } else {
    const Mcu_ClockConfigType *clockCfg =
        &Mcu_State.ConfigPtr->ClockConfigPtr[ClockSetting];
    Mcu_ClockFailureCallbackType callback =
        Mcu_State.ConfigPtr->ClockFailureCallback;
    Mcu_Hw_ClockOpResultType op;
    boolean reportDemPassed = FALSE;
    boolean notifyFailure = FALSE;
    Mcu_ClockStateType pubState = MCU_CLK_STATE_UNSPECIFIED;
    Mcu_ClockFailureReasonType pubReason = MCU_CLK_FAIL_NONE;
    Mcu_FailureClassType pubClass = MCU_FAIL_CLASS_NONE;
    uint32_t primask;

    /* Thao tác phần cứng & vòng đợi thực hiện NGOÀI vùng khóa ngắt */
    op = Mcu_Hw_InitClock(clockCfg);

    /* Công bố toàn bộ trạng thái phần mềm NGUYÊN TỬ trong vùng khóa ngắn */
    primask = Mcu_Hw_EnterCritical();
    if (op.Status == E_OK) {
      Mcu_SysClkSourceType sw = Mcu_Hw_GetRequestedSwSource();
      Mcu_SysClkSourceType sws = Mcu_Hw_GetActiveSysClkSource();

      Mcu_State.PublishedSwSource = sw;
      Mcu_State.PublishedSysClkSource = sws;
      Mcu_State.LastFailureReason = MCU_CLK_FAIL_NONE;
      Mcu_State.LastFailureClass = MCU_FAIL_CLASS_NONE;
      Mcu_State.StopWakeupRestoreFailed = FALSE;
      Mcu_State.PllDistributed = FALSE;

      if (clockCfg->PllEnable == TRUE) {
        /* PLL mới khởi động -> chưa đủ căn cứ báo DEM PASSED */
        Mcu_State.PendingPllClockCfgPtr = clockCfg;
        Mcu_State.ActiveClockCfgPtr = NULL_PTR;
        Mcu_State.ClockState = MCU_CLK_STATE_PLL_PENDING;
        retVal = E_OK;
      } else if ((sw == clockCfg->SysClkSource) &&
                 (sws == clockCfg->SysClkSource)) {
        /* Xung không PLL đã xác nhận SW == SWS == đích -> đủ căn cứ báo DEM
         * PASSED */
        Mcu_State.PendingPllClockCfgPtr = NULL_PTR;
        Mcu_State.ActiveClockCfgPtr = clockCfg;
        Mcu_State.ClockState = MCU_CLK_STATE_STABLE;
        reportDemPassed = TRUE;
        retVal = E_OK;
      } else {
        op.Status = E_NOT_OK;
        op.FailureReason = MCU_CLK_FAIL_SWS_SWITCH_TIMEOUT;
        op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
        op.FallbackOk = FALSE;
        Mcu_PublishFailureLocked(&op, NULL_PTR);
        notifyFailure = TRUE;
        pubState = Mcu_State.ClockState;
        pubReason = Mcu_State.LastFailureReason;
        pubClass = Mcu_State.LastFailureClass;
        retVal = E_NOT_OK;
      }
    } else {
      /* Khởi tạo cấu hình mới thất bại -> xóa sạch cấu hình chờ cũ */
      Mcu_PublishFailureLocked(&op, NULL_PTR);
      notifyFailure = TRUE;
      pubState = Mcu_State.ClockState;
      pubReason = Mcu_State.LastFailureReason;
      pubClass = Mcu_State.LastFailureClass;
      retVal = E_NOT_OK;
    }
    Mcu_Hw_ExitCritical(primask);

    /* Báo DEM và callback NGOÀI vùng khóa ngắt */
    if (reportDemPassed == TRUE) {
      Mcu_ReportDemClockPassed();
    } else if (notifyFailure == TRUE) {
      Mcu_NotifyHardwareFailure(callback, pubReason, pubClass, pubState);
    } else {
      /* Trạng thái PLL_PENDING: chưa báo DEM */
    }
  }

  return retVal;
}
#endif /* (MCU_INIT_CLOCK == STD_ON) */

#if (MCU_NO_PLL == STD_OFF)
MCU_MEMMAP_CODE_ATTR Std_ReturnType Mcu_DistributePllClock(void) {
  Std_ReturnType retVal = E_NOT_OK;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_DISTRIBUTE_PLL_CLOCK, MCU_E_UNINIT);
  } else if (Mcu_Hw_GetPllStatus() != MCU_PLL_LOCKED) {
    /* [SWS_Mcu_00122, SWS_Mcu_00142]: Trả E_NOT_OK ngay không chạm thanh ghi */
    Mcu_ReportDetError(MCU_SID_DISTRIBUTE_PLL_CLOCK, MCU_E_PLL_NOT_LOCKED);
  } else {
    const Mcu_ClockConfigType *targetPllCfg = NULL_PTR;
    Mcu_ClockFailureCallbackType callback = NULL_PTR;
    Mcu_ClockStateType currentState;
    uint32_t primask;

    /* Đọc cấu hình PLL hợp lệ dưới khóa ngắn */
    primask = Mcu_Hw_EnterCritical();
    currentState = Mcu_State.ClockState;
    callback = Mcu_State.ConfigPtr->ClockFailureCallback;

    if (currentState != MCU_CLK_STATE_FAULT) {
      if (Mcu_State.PendingPllClockCfgPtr != NULL_PTR) {
        targetPllCfg = Mcu_State.PendingPllClockCfgPtr;
      } else if ((Mcu_State.PllDistributed == TRUE) &&
                 (Mcu_State.ActiveClockCfgPtr != NULL_PTR)) {
        targetPllCfg = Mcu_State.ActiveClockCfgPtr;
      } else {
        targetPllCfg = NULL_PTR;
      }
    }
    Mcu_Hw_ExitCritical(primask);

    /* Không cho phép công bố STABLE nếu không có cấu hình PLL hợp lệ để khôi
     * phục sau STOP */
    if (targetPllCfg != NULL_PTR) {
      Mcu_Hw_SwitchResultType swRes;
      boolean reportDemPassed = FALSE;
      boolean notifyFailure = FALSE;
      Mcu_ClockStateType pubState = MCU_CLK_STATE_UNSPECIFIED;
      Mcu_ClockFailureReasonType pubReason = MCU_CLK_FAIL_NONE;
      Mcu_FailureClassType pubClass = MCU_FAIL_CLASS_NONE;

      /* Chuyển nguồn SW = PLL + xác nhận SWS (kèm rollback HSI) NGOÀI vùng khóa
       */
      swRes =
          Mcu_Hw_SwitchSysClkWithRollback(RCC_CFGR_SW_PLL, RCC_CFGR_SWS_PLL);

      /* Công bố nguyên tử kết quả vào Mcu_State trong vùng khóa ngắn */
      primask = Mcu_Hw_EnterCritical();
      {
        Mcu_SysClkSourceType sw = Mcu_Hw_GetRequestedSwSource();
        Mcu_SysClkSourceType sws = Mcu_Hw_GetActiveSysClkSource();

        if ((swRes == MCU_HW_SWITCH_OK) && (sw == MCU_SYSCLK_SRC_PLL) &&
            (sws == MCU_SYSCLK_SRC_PLL)) {
          Mcu_State.ActiveClockCfgPtr = targetPllCfg;
          Mcu_State.PendingPllClockCfgPtr = NULL_PTR;
          Mcu_State.PllDistributed = TRUE;
          Mcu_State.ClockState = MCU_CLK_STATE_STABLE;
          Mcu_State.LastFailureReason = MCU_CLK_FAIL_NONE;
          Mcu_State.LastFailureClass = MCU_FAIL_CLASS_NONE;
          Mcu_State.StopWakeupRestoreFailed = FALSE;
          Mcu_State.PublishedSwSource = sw;
          Mcu_State.PublishedSysClkSource = sws;
          reportDemPassed = TRUE;
          retVal = E_OK;
        } else if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_OK) {
          /* Timeout SWS nhưng rollback HSI thành công: GIỮ
           * PendingPllClockCfgPtr để retry/STOP */
          Mcu_Hw_ClockOpResultType failOp = {E_NOT_OK,
                                             MCU_CLK_FAIL_SWS_SWITCH_TIMEOUT,
                                             MCU_FAIL_CLASS_CLOCK_SOURCE, TRUE};
          Mcu_PublishFailureLocked(&failOp, targetPllCfg);
          notifyFailure = TRUE;
          pubState = Mcu_State.ClockState;
          pubReason = Mcu_State.LastFailureReason;
          pubClass = Mcu_State.LastFailureClass;
          retVal = E_NOT_OK;
        } else {
          /* Rollback HSI thất bại hoặc SW/SWS không nhất quán: vào FAULT và xóa
           * sạch cấu hình */
          Mcu_Hw_ClockOpResultType failOp = {
              E_NOT_OK, MCU_CLK_FAIL_HSI_FALLBACK_TIMEOUT,
              MCU_FAIL_CLASS_CLOCK_SOURCE, FALSE};
          Mcu_PublishFailureLocked(&failOp, NULL_PTR);
          notifyFailure = TRUE;
          pubState = Mcu_State.ClockState;
          pubReason = Mcu_State.LastFailureReason;
          pubClass = Mcu_State.LastFailureClass;
          retVal = E_NOT_OK;
        }
      }
      Mcu_Hw_ExitCritical(primask);

      if (reportDemPassed == TRUE) {
        Mcu_ReportDemClockPassed();
      } else if (notifyFailure == TRUE) {
        Mcu_NotifyHardwareFailure(callback, pubReason, pubClass, pubState);
      } else {
        /* Không có thông báo */
      }
    }
  }

  return retVal;
}
#endif /* (MCU_NO_PLL == STD_OFF) */

MCU_MEMMAP_CODE_ATTR Mcu_PllStatusType Mcu_GetPllStatus(void) {
  Mcu_PllStatusType pllStatus = MCU_PLL_STATUS_UNDEFINED;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_GET_PLL_STATUS, MCU_E_UNINIT);
    pllStatus = MCU_PLL_STATUS_UNDEFINED;
  } else {
#if (MCU_NO_PLL == STD_ON)
    pllStatus = MCU_PLL_STATUS_UNDEFINED;
#else
    pllStatus = Mcu_Hw_GetPllStatus();
#endif /* (MCU_NO_PLL == STD_ON) */
  }

  return pllStatus;
}

MCU_MEMMAP_CODE_ATTR Mcu_ResetType Mcu_GetResetReason(void) {
  Mcu_ResetType resetReason = MCU_RESET_UNDEFINED;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_GET_RESET_REASON, MCU_E_UNINIT);
    resetReason = MCU_RESET_UNDEFINED;
  } else {
    resetReason = Mcu_Hw_DecodeResetReason(Mcu_State.LatchedRawReset);
  }

  return resetReason;
}

MCU_MEMMAP_CODE_ATTR Mcu_RawResetType Mcu_GetResetRawValue(void) {
  Mcu_RawResetType rawReset = MCU_RAW_RESET_UNINIT_VALUE;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_GET_RESET_RAW_VALUE, MCU_E_UNINIT);
    rawReset = MCU_RAW_RESET_UNINIT_VALUE;
  } else {
    rawReset = Mcu_State.LatchedRawReset;
  }

  return rawReset;
}

#if (MCU_PERFORM_RESET_API == STD_ON)
MCU_MEMMAP_CODE_ATTR void Mcu_PerformReset(void) {
  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_PERFORM_RESET, MCU_E_UNINIT);
  } else {
    Mcu_Hw_PerformReset(Mcu_State.ConfigPtr->ResetHwType);
  }
}
#endif /* (MCU_PERFORM_RESET_API == STD_ON) */

MCU_MEMMAP_CODE_ATTR void Mcu_SetMode(Mcu_ModeType McuMode) {
  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_SET_MODE, MCU_E_UNINIT);
  } else if (McuMode >= Mcu_State.ConfigPtr->NumModeConfigs) {
    Mcu_ReportDetError(MCU_SID_SET_MODE, MCU_E_PARAM_MODE);
  } else {
    const Mcu_ModeConfigType *modeCfg =
        &Mcu_State.ConfigPtr->ModeConfigPtr[McuMode];
    Mcu_ClockFailureCallbackType callback =
        Mcu_State.ConfigPtr->ClockFailureCallback;
    Mcu_Hw_ClockOpResultType op;
    boolean reportDemPassed = FALSE;
    boolean notifyFailure = FALSE;
    Mcu_ClockStateType pubState = MCU_CLK_STATE_UNSPECIFIED;
    Mcu_ClockFailureReasonType pubReason = MCU_CLK_FAIL_NONE;
    Mcu_FailureClassType pubClass = MCU_FAIL_CLASS_NONE;
    uint32_t primask;

    /* Giữ PRIMASK = 1 xuyên suốt cấu hình ngủ, WFI, xóa SLEEPDEEP, khôi phục
     * xung và công bố trạng thái */
    primask = Mcu_Hw_EnterCritical();

    op = Mcu_Hw_EnterModeLocked(modeCfg, Mcu_State.ActiveClockCfgPtr,
                                Mcu_State.PendingPllClockCfgPtr,
                                Mcu_State.PllDistributed);

    if (modeCfg->PowerMode == MCU_POWER_MODE_STOP) {
      if (op.Status == E_OK) {
        Mcu_SysClkSourceType sw = Mcu_Hw_GetRequestedSwSource();
        Mcu_SysClkSourceType sws = Mcu_Hw_GetActiveSysClkSource();

        Mcu_State.PublishedSwSource = sw;
        Mcu_State.PublishedSysClkSource = sws;
        Mcu_State.StopWakeupRestoreFailed = FALSE;

        if (Mcu_State.PendingPllClockCfgPtr != NULL_PTR) {
          /* Khôi phục lại yêu cầu PLL đang chờ; KHÔNG chuyển SW, KHÔNG báo DEM
           * PASSED */
          Mcu_State.PllDistributed = FALSE;
          Mcu_State.ClockState = MCU_CLK_STATE_PLL_PENDING;
        } else if (Mcu_State.ActiveClockCfgPtr != NULL_PTR) {
          Mcu_State.ClockState = MCU_CLK_STATE_STABLE;
          Mcu_State.LastFailureReason = MCU_CLK_FAIL_NONE;
          Mcu_State.LastFailureClass = MCU_FAIL_CLASS_NONE;
          reportDemPassed = TRUE;
        } else {
          /* Chưa cấu hình xung trước STOP: giữ trạng thái hiện có */
        }
      } else {
        Mcu_State.StopWakeupRestoreFailed = TRUE;
        Mcu_PublishFailureLocked(&op, NULL_PTR);
        notifyFailure = TRUE;
        pubState = Mcu_State.ClockState;
        pubReason = Mcu_State.LastFailureReason;
        pubClass = Mcu_State.LastFailureClass;
      }
    }

    /* Trả đúng PRIMASK của bên gọi TRƯỚC khi gọi DEM hoặc callback tích hợp */
    Mcu_Hw_ExitCritical(primask);

    if (reportDemPassed == TRUE) {
      Mcu_ReportDemClockPassed();
    } else if (notifyFailure == TRUE) {
      Mcu_NotifyHardwareFailure(callback, pubReason, pubClass, pubState);
    } else {
      /* Không có thông báo */
    }
  }
}

#if (MCU_VERSION_INFO_API == STD_ON)
MCU_MEMMAP_CODE_ATTR void Mcu_GetVersionInfo(Std_VersionInfoType *versioninfo) {
  if (versioninfo == NULL_PTR) {
    Mcu_ReportDetError(MCU_SID_GET_VERSION_INFO, MCU_E_PARAM_POINTER);
  } else {
    versioninfo->vendorID = MCU_VENDOR_ID;
    versioninfo->moduleID = MCU_MODULE_ID;
    versioninfo->sw_major_version = MCU_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = MCU_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = MCU_SW_PATCH_VERSION;
  }
}
#endif /* (MCU_VERSION_INFO_API == STD_ON) */

#if (MCU_GET_RAM_STATE_API == STD_ON)
MCU_MEMMAP_CODE_ATTR Mcu_RamStateType Mcu_GetRamState(void) {
  Mcu_RamStateType ramState = MCU_RAMSTATE_INVALID;

  if (Mcu_Initialized == FALSE) {
    Mcu_ReportDetError(MCU_SID_GET_RAM_STATE, MCU_E_UNINIT);
  } else {
    ramState = MCU_RAMSTATE_INVALID;
  }

  return ramState;
}
#endif /* (MCU_GET_RAM_STATE_API == STD_ON) */

/*============================================================================*/
/*                HÀM CHẨN ĐOÁN MỞ RỘNG CHO TẦNG TÍCH HỢP                     */
/*============================================================================*/

MCU_MEMMAP_CODE_ATTR Std_ReturnType
Mcu_Stm32f1_GetDiagStatus(Mcu_Stm32f1_DiagStatusType *StatusPtr) {
  Std_ReturnType retVal = E_NOT_OK;

  if (StatusPtr != NULL_PTR) {
    uint32_t primask = Mcu_Hw_EnterCritical();

    /* Nhóm 1: Ảnh chụp nguyên tử trạng thái phần mềm đã công bố */
    StatusPtr->ClockState = Mcu_State.ClockState;
    StatusPtr->LastFailureReason = Mcu_State.LastFailureReason;
    StatusPtr->LastFailureClass = Mcu_State.LastFailureClass;
    StatusPtr->PublishedSwSource = Mcu_State.PublishedSwSource;
    StatusPtr->PublishedSysClkSource = Mcu_State.PublishedSysClkSource;
    StatusPtr->PllDistributed = Mcu_State.PllDistributed;
    StatusPtr->PllPending =
        (Mcu_State.PendingPllClockCfgPtr != NULL_PTR) ? TRUE : FALSE;
    StatusPtr->ActiveConfigValid =
        (Mcu_State.ActiveClockCfgPtr != NULL_PTR) ? TRUE : FALSE;
    StatusPtr->StopWakeupRestoreFailed = Mcu_State.StopWakeupRestoreFailed;

    /* Nhóm 2: Quan sát trực tiếp thanh ghi phần cứng tại thời điểm đọc */
    StatusPtr->LiveSwSource = Mcu_Hw_GetRequestedSwSource();
    StatusPtr->LiveSysClkSource = Mcu_Hw_GetActiveSysClkSource();
    StatusPtr->LivePllLocked =
        (Mcu_Hw_GetPllStatus() == MCU_PLL_LOCKED) ? TRUE : FALSE;

    Mcu_Hw_ExitCritical(primask);
    retVal = E_OK;
  }

  return retVal;
}

#define MCU_STOP_SEC_CODE
#include "Mcu_MemMap.h"