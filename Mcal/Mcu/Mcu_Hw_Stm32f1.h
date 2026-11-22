/**
 * @file    Mcu_Hw_Stm32f1.h
 * @brief   Tầng truy cập thanh ghi CMSIS nội bộ cho STM32F103C6T6 và
 * STM32F103C8T6.
 * @details HEADER RIÊNG TƯ: Chỉ được phép include từ Mcal/Mcu/Mcu.c.
 *          Thao tác trực tiếp RCC, FLASH, PWR, SCB theo ST RM0008, PM0075 và
 * ARMv7-M.
 */

#ifndef MCU_HW_STM32F1_H
#define MCU_HW_STM32F1_H

#include "Mcu_Cfg.h"
#include "Mcu_Types.h"
#include "stm32f1xx.h"
#include <stdint.h>

/*============================================================================*/
/*                    KIỂU KẾT QUẢ THAO TÁC PHẦN CỨNG NỘI BỘ                  */
/*============================================================================*/

/** @brief Kết quả chuyển nguồn xung SW -> SWS kèm cơ chế thu hồi (rollback) về
 * HSI. */
typedef enum {
  MCU_HW_SWITCH_OK = 0x00U, /**< Đích đã xác nhận qua SW == SWS. */
  MCU_HW_SWITCH_TIMEOUT_FALLBACK_OK =
      0x01U, /**< Đích timeout; đã hủy SW và xác nhận về HSI. */
  MCU_HW_SWITCH_TIMEOUT_FALLBACK_FAIL =
      0x02U /**< Đích timeout và cả HSI dự phòng không xác nhận được. */
} Mcu_Hw_SwitchResultType;

/** @brief Kết quả chi tiết trả về từ thao tác khởi tạo xung hoặc khôi phục sau
 * STOP. */
typedef struct {
  Std_ReturnType
      Status; /**< E_OK nếu thành công; E_NOT_OK nếu lỗi phần cứng. */
  Mcu_ClockFailureReasonType
      FailureReason; /**< Mã nguyên nhân chi tiết (NONE nếu E_OK). */
  Mcu_FailureClassType
      FailureClass;   /**< Phân nhóm lỗi (CLOCK_SOURCE hay FLASH_INTERFACE). */
  boolean FallbackOk; /**< TRUE nếu sau lỗi phần cứng đang ổn định trên HSI. */
} Mcu_Hw_ClockOpResultType;

/*============================================================================*/
/*                    HÀM BẢO VỆ VÙNG GĂNG (CORTEX-M3 PRIMASK)                */
/*============================================================================*/

/** @brief Lưu PRIMASK hiện tại và khóa ngắt toàn cục. */
static inline uint32_t Mcu_Hw_EnterCritical(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

/** @brief Khôi phục đúng giá trị PRIMASK trước đó của bên gọi. */
static inline void Mcu_Hw_ExitCritical(uint32_t Primask) {
  __set_PRIMASK(Primask);
  __ISB();
}

/*============================================================================*/
/*                      VÒNG ĐỢI CỜ PHẦN CỨNG CÓ TIMEOUT                      */
/*============================================================================*/

/**
 * @brief   Đợi hữu hạn cho tới khi ((*RegPtr) & Mask) == Expected hoặc hết
 * MCU_HW_TIMEOUT_COUNT.
 * @param   RegPtr   Con trỏ thanh ghi phần cứng volatile.
 * @param   Mask     Mặt nạ bit cần kiểm tra.
 * @param   Expected Giá trị mong đợi sau khi áp dụng Mask.
 * @return  E_OK nếu thỏa điều kiện trước khi hết timeout; E_NOT_OK nếu hết
 * timeout.
 */
static inline Std_ReturnType Mcu_Hw_WaitFlag(volatile const uint32_t *RegPtr,
                                             uint32_t Mask, uint32_t Expected) {
  uint32_t remaining = (uint32_t)MCU_HW_TIMEOUT_COUNT;
  Std_ReturnType status = E_OK;

  while (((*RegPtr) & Mask) != Expected) {
    if (remaining == 0UL) {
      status = E_NOT_OK;
      break;
    }
    remaining--;
  }

  return status;
}

/*============================================================================*/
/*                CHUYỂN ĐỔI ENUM TRỪU TƯỢNG SANG BITFIELD CMSIS              */
/*============================================================================*/

static inline uint32_t
Mcu_Hw_EncodeAhbPrescaler(Mcu_AhbPrescalerType Prescaler) {
  uint32_t val = RCC_CFGR_HPRE_DIV1;

  switch (Prescaler) {
  case MCU_AHB_DIV_1:
    val = RCC_CFGR_HPRE_DIV1;
    break;
  case MCU_AHB_DIV_2:
    val = RCC_CFGR_HPRE_DIV2;
    break;
  case MCU_AHB_DIV_4:
    val = RCC_CFGR_HPRE_DIV4;
    break;
  case MCU_AHB_DIV_8:
    val = RCC_CFGR_HPRE_DIV8;
    break;
  case MCU_AHB_DIV_16:
    val = RCC_CFGR_HPRE_DIV16;
    break;
  case MCU_AHB_DIV_64:
    val = RCC_CFGR_HPRE_DIV64;
    break;
  case MCU_AHB_DIV_128:
    val = RCC_CFGR_HPRE_DIV128;
    break;
  case MCU_AHB_DIV_256:
    val = RCC_CFGR_HPRE_DIV256;
    break;
  case MCU_AHB_DIV_512:
    val = RCC_CFGR_HPRE_DIV512;
    break;
  default:
    val = RCC_CFGR_HPRE_DIV1;
    break;
  }

  return val;
}

static inline uint32_t
Mcu_Hw_EncodeApb1Prescaler(Mcu_ApbPrescalerType Prescaler) {
  uint32_t val = RCC_CFGR_PPRE1_DIV2;

  switch (Prescaler) {
  case MCU_APB_DIV_1:
    val = RCC_CFGR_PPRE1_DIV1;
    break;
  case MCU_APB_DIV_2:
    val = RCC_CFGR_PPRE1_DIV2;
    break;
  case MCU_APB_DIV_4:
    val = RCC_CFGR_PPRE1_DIV4;
    break;
  case MCU_APB_DIV_8:
    val = RCC_CFGR_PPRE1_DIV8;
    break;
  case MCU_APB_DIV_16:
    val = RCC_CFGR_PPRE1_DIV16;
    break;
  default:
    val = RCC_CFGR_PPRE1_DIV2;
    break;
  }

  return val;
}

static inline uint32_t
Mcu_Hw_EncodeApb2Prescaler(Mcu_ApbPrescalerType Prescaler) {
  uint32_t val = RCC_CFGR_PPRE2_DIV1;

  switch (Prescaler) {
  case MCU_APB_DIV_1:
    val = RCC_CFGR_PPRE2_DIV1;
    break;
  case MCU_APB_DIV_2:
    val = RCC_CFGR_PPRE2_DIV2;
    break;
  case MCU_APB_DIV_4:
    val = RCC_CFGR_PPRE2_DIV4;
    break;
  case MCU_APB_DIV_8:
    val = RCC_CFGR_PPRE2_DIV8;
    break;
  case MCU_APB_DIV_16:
    val = RCC_CFGR_PPRE2_DIV16;
    break;
  default:
    val = RCC_CFGR_PPRE2_DIV1;
    break;
  }

  return val;
}

static inline uint32_t
Mcu_Hw_EncodeAdcPrescaler(Mcu_AdcPrescalerType Prescaler) {
  uint32_t val = RCC_CFGR_ADCPRE_DIV6;

  switch (Prescaler) {
  case MCU_ADC_DIV_2:
    val = RCC_CFGR_ADCPRE_DIV2;
    break;
  case MCU_ADC_DIV_4:
    val = RCC_CFGR_ADCPRE_DIV4;
    break;
  case MCU_ADC_DIV_6:
    val = RCC_CFGR_ADCPRE_DIV6;
    break;
  case MCU_ADC_DIV_8:
    val = RCC_CFGR_ADCPRE_DIV8;
    break;
  default:
    val = RCC_CFGR_ADCPRE_DIV6;
    break;
  }

  return val;
}

static inline uint32_t Mcu_Hw_EncodePllMul(Mcu_PllMulType PllMul) {
  uint32_t val = RCC_CFGR_PLLMULL2;

  switch (PllMul) {
  case MCU_PLL_MUL_2:
    val = RCC_CFGR_PLLMULL2;
    break;
  case MCU_PLL_MUL_3:
    val = RCC_CFGR_PLLMULL3;
    break;
  case MCU_PLL_MUL_4:
    val = RCC_CFGR_PLLMULL4;
    break;
  case MCU_PLL_MUL_5:
    val = RCC_CFGR_PLLMULL5;
    break;
  case MCU_PLL_MUL_6:
    val = RCC_CFGR_PLLMULL6;
    break;
  case MCU_PLL_MUL_7:
    val = RCC_CFGR_PLLMULL7;
    break;
  case MCU_PLL_MUL_8:
    val = RCC_CFGR_PLLMULL8;
    break;
  case MCU_PLL_MUL_9:
    val = RCC_CFGR_PLLMULL9;
    break;
  case MCU_PLL_MUL_10:
    val = RCC_CFGR_PLLMULL10;
    break;
  case MCU_PLL_MUL_11:
    val = RCC_CFGR_PLLMULL11;
    break;
  case MCU_PLL_MUL_12:
    val = RCC_CFGR_PLLMULL12;
    break;
  case MCU_PLL_MUL_13:
    val = RCC_CFGR_PLLMULL13;
    break;
  case MCU_PLL_MUL_14:
    val = RCC_CFGR_PLLMULL14;
    break;
  case MCU_PLL_MUL_15:
    val = RCC_CFGR_PLLMULL15;
    break;
  case MCU_PLL_MUL_16:
    val = RCC_CFGR_PLLMULL16;
    break;
  default:
    val = RCC_CFGR_PLLMULL2;
    break;
  }

  return val;
}

static inline uint32_t Mcu_Hw_EncodeFlashLatency(Mcu_FlashLatencyType Latency) {
  uint32_t val = 2UL;

  switch (Latency) {
  case MCU_FLASH_LATENCY_0WS:
    val = 0UL;
    break;
  case MCU_FLASH_LATENCY_1WS:
    val = 1UL;
    break;
  case MCU_FLASH_LATENCY_2WS:
    val = 2UL;
    break;
  default:
    val = 2UL;
    break;
  }

  return (val & FLASH_ACR_LATENCY);
}

/*============================================================================*/
/*                       THAO TÁC THANH GHI RESET                             */
/*============================================================================*/

static inline Mcu_RawResetType Mcu_Hw_LatchAndClearResetFlags(void) {
  Mcu_RawResetType rawReset = (Mcu_RawResetType)(RCC->CSR);

  RCC->CSR |= RCC_CSR_RMVF;
  __DSB();
  RCC->CSR &= ~RCC_CSR_RMVF;
  __DSB();

  return rawReset;
}

static inline Mcu_ResetType
Mcu_Hw_DecodeResetReason(Mcu_RawResetType RawReset) {
  Mcu_ResetType reason = MCU_RESET_UNDEFINED;

  if ((RawReset & RCC_CSR_PORRSTF) != 0UL) {
    reason = MCU_POWER_ON_RESET;
  } else if ((RawReset & RCC_CSR_SFTRSTF) != 0UL) {
    reason = MCU_SW_RESET;
  } else if ((RawReset & RCC_CSR_IWDGRSTF) != 0UL) {
    reason = MCU_WATCHDOG_RESET;
  } else if ((RawReset & RCC_CSR_WWDGRSTF) != 0UL) {
    reason = MCU_WINDOW_WATCHDOG_RESET;
  } else if ((RawReset & RCC_CSR_LPWRRSTF) != 0UL) {
    reason = MCU_LOW_POWER_RESET;
  } else if ((RawReset & RCC_CSR_PINRSTF) != 0UL) {
    reason = MCU_PIN_RESET;
  } else {
    reason = MCU_RESET_UNDEFINED;
  }

  return reason;
}

static inline void Mcu_Hw_PerformReset(Mcu_ResetHwType ResetHwType) {
  if (ResetHwType == MCU_RESET_HW_SYSRESETREQ) {
    __DSB();
    NVIC_SystemReset();

    for (;;) {
      __NOP();
    }
  }
}

/*============================================================================*/
/*                    THAO TÁC FLASH & XUNG HỆ THỐNG                          */
/*============================================================================*/

static inline Std_ReturnType Mcu_Hw_ConfigureFlash(Mcu_FlashLatencyType Latency,
                                                   boolean PrefetchEnable) {
  uint32_t targetLatency = Mcu_Hw_EncodeFlashLatency(Latency);
  uint32_t acrVal = FLASH->ACR;

  if (PrefetchEnable == TRUE) {
    acrVal |= FLASH_ACR_PRFTBE;
  } else {
    acrVal &= ~FLASH_ACR_PRFTBE;
  }

  acrVal &= ~FLASH_ACR_LATENCY;
  acrVal |= targetLatency;

  FLASH->ACR = acrVal;
  __DSB();

  return Mcu_Hw_WaitFlag(&FLASH->ACR, FLASH_ACR_LATENCY, targetLatency);
}

static inline Mcu_SysClkSourceType Mcu_Hw_GetRequestedSwSource(void) {
  uint32_t sw = (RCC->CFGR & RCC_CFGR_SW);
  Mcu_SysClkSourceType src = MCU_SYSCLK_SRC_HSI;

  if (sw == RCC_CFGR_SW_HSE) {
    src = MCU_SYSCLK_SRC_HSE;
  } else if (sw == RCC_CFGR_SW_PLL) {
    src = MCU_SYSCLK_SRC_PLL;
  } else {
    src = MCU_SYSCLK_SRC_HSI;
  }

  return src;
}

static inline Mcu_SysClkSourceType Mcu_Hw_GetActiveSysClkSource(void) {
  uint32_t sws = (RCC->CFGR & RCC_CFGR_SWS);
  Mcu_SysClkSourceType src = MCU_SYSCLK_SRC_HSI;

  if (sws == RCC_CFGR_SWS_HSE) {
    src = MCU_SYSCLK_SRC_HSE;
  } else if (sws == RCC_CFGR_SWS_PLL) {
    src = MCU_SYSCLK_SRC_PLL;
  } else {
    src = MCU_SYSCLK_SRC_HSI;
  }

  return src;
}

static inline Mcu_PllStatusType Mcu_Hw_GetPllStatus(void) {
  Mcu_PllStatusType status = MCU_PLL_UNLOCKED;

  if ((RCC->CR & RCC_CR_PLLRDY) != 0UL) {
    status = MCU_PLL_LOCKED;
  }

  return status;
}

static inline Std_ReturnType
Mcu_Hw_SwitchSysClkToHsi(Mcu_ClockFailureReasonType *OutFailureReason) {
  Std_ReturnType status = E_OK;

  if ((RCC->CR & RCC_CR_HSION) == 0UL) {
    RCC->CR |= RCC_CR_HSION;
    __DSB();
  }

  status = Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY);
  if (status != E_OK) {
    *OutFailureReason = MCU_CLK_FAIL_HSI_READY_TIMEOUT;
  } else {
    if (((RCC->CFGR & RCC_CFGR_SW) != RCC_CFGR_SW_HSI) ||
        ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)) {
      RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
      __DSB();
      status = Mcu_Hw_WaitFlag(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI);
      if (status != E_OK) {
        *OutFailureReason = MCU_CLK_FAIL_HSI_FALLBACK_TIMEOUT;
      }
    }
  }

  return status;
}

static inline Std_ReturnType Mcu_Hw_DisablePll(void) {
  Std_ReturnType status = E_OK;

  if ((RCC->CR & RCC_CR_PLLON) != 0UL) {
    RCC->CR &= ~RCC_CR_PLLON;
    __DSB();
    status = Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_PLLRDY, 0UL);
  }

  return status;
}

static inline void
Mcu_Hw_WriteBusPrescalers(const Mcu_ClockConfigType *ClockCfg) {
  uint32_t cfgr = RCC->CFGR;

  cfgr &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE);
  cfgr |= Mcu_Hw_EncodeAhbPrescaler(ClockCfg->AhbPrescaler);
  cfgr |= Mcu_Hw_EncodeApb1Prescaler(ClockCfg->Apb1Prescaler);
  cfgr |= Mcu_Hw_EncodeApb2Prescaler(ClockCfg->Apb2Prescaler);
  cfgr |= Mcu_Hw_EncodeAdcPrescaler(ClockCfg->AdcPrescaler);

  RCC->CFGR = cfgr;
  __DSB();
}

static inline void Mcu_Hw_WritePllConfig(const Mcu_ClockConfigType *ClockCfg) {
  uint32_t cfgr = RCC->CFGR;

  cfgr &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);

  if (ClockCfg->PllSource == MCU_PLL_SRC_HSE_DIV1) {
    cfgr |= RCC_CFGR_PLLSRC;
  } else if (ClockCfg->PllSource == MCU_PLL_SRC_HSE_DIV2) {
    cfgr |= (RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE);
  } else {
    /* MCU_PLL_SRC_HSI_DIV2: PLLSRC = 0, PLLXTPRE = 0 */
  }

  cfgr |= Mcu_Hw_EncodePllMul(ClockCfg->PllMul);
  RCC->CFGR = cfgr;
  __DSB();
}

/**
 * @brief   Chuyển SW sang nguồn đích và xác nhận SWS; rollback về HSI nếu
 * timeout để chống xác nhận muộn.
 */
static inline Mcu_Hw_SwitchResultType
Mcu_Hw_SwitchSysClkWithRollback(uint32_t TargetSw, uint32_t TargetSws) {
  Mcu_Hw_SwitchResultType result = MCU_HW_SWITCH_OK;
  Std_ReturnType status;

  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | TargetSw;
  __DSB();

  status = Mcu_Hw_WaitFlag(&RCC->CFGR, RCC_CFGR_SWS, TargetSws);
  if (status != E_OK) {
    Mcu_ClockFailureReasonType fbReason = MCU_CLK_FAIL_NONE;

    if (Mcu_Hw_SwitchSysClkToHsi(&fbReason) == E_OK) {
      result = MCU_HW_SWITCH_TIMEOUT_FALLBACK_OK;
    } else {
      result = MCU_HW_SWITCH_TIMEOUT_FALLBACK_FAIL;
    }
  }

  return result;
}

/**
 * @brief   Thiết lập cây xung an toàn cho cả hai chiều 8 <-> 72 MHz, phân loại
 * riêng lỗi nguồn xung và lỗi Flash.
 */
static inline Mcu_Hw_ClockOpResultType
Mcu_Hw_InitClock(const Mcu_ClockConfigType *ClockCfg) {
  Mcu_Hw_ClockOpResultType op = {E_OK, MCU_CLK_FAIL_NONE, MCU_FAIL_CLASS_NONE,
                                 TRUE};

  op.Status = Mcu_Hw_SwitchSysClkToHsi(&op.FailureReason);
  if (op.Status != E_OK) {
    op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
    op.FallbackOk = FALSE;
  }

  if (op.Status == E_OK) {
    if (Mcu_Hw_DisablePll() != E_OK) {
      op.Status = E_NOT_OK;
      op.FailureReason = MCU_CLK_FAIL_PLL_DISABLE_TIMEOUT;
      op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
      op.FallbackOk =
          ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI) ? TRUE : FALSE;
    }
  }

  if (op.Status == E_OK) {
    if (ClockCfg->HseEnable == TRUE) {
      RCC->CR |= RCC_CR_HSEON;
      __DSB();
      if (Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY) != E_OK) {
        RCC->CR &= ~RCC_CR_HSEON;
        __DSB();
        op.Status = E_NOT_OK;
        op.FailureReason = MCU_CLK_FAIL_HSE_READY_TIMEOUT;
        op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
        op.FallbackOk = TRUE;
      }
    } else {
      if ((RCC->CR & RCC_CR_HSEON) != 0UL) {
        RCC->CR &= ~RCC_CR_HSEON;
        __DSB();
        if (Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_HSERDY, 0UL) != E_OK) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_HSE_READY_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = TRUE;
        }
      }
    }
  }

  if (op.Status == E_OK) {
    if (ClockCfg->PllEnable == TRUE) {
      if (Mcu_Hw_ConfigureFlash(ClockCfg->FlashLatency,
                                ClockCfg->FlashPrefetchEnable) != E_OK) {
        /* Phân loại riêng lỗi xác nhận độ trễ Flash (không báo DEM
         * MCU_E_CLOCK_FAILURE) */
        op.Status = E_NOT_OK;
        op.FailureReason = MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT;
        op.FailureClass = MCU_FAIL_CLASS_FLASH_INTERFACE;
        op.FallbackOk = TRUE;
      } else {
        Mcu_Hw_WriteBusPrescalers(ClockCfg);
        Mcu_Hw_WritePllConfig(ClockCfg);

        /* [SWS_Mcu_00138]: Khởi động PLL và trả về ngay, không chuyển SW = PLL
         */
        RCC->CR |= RCC_CR_PLLON;
        __DSB();
      }
    } else if (ClockCfg->SysClkSource == MCU_SYSCLK_SRC_HSE) {
      Mcu_Hw_WriteBusPrescalers(ClockCfg);
      if (Mcu_Hw_ConfigureFlash(ClockCfg->FlashLatency,
                                ClockCfg->FlashPrefetchEnable) != E_OK) {
        op.Status = E_NOT_OK;
        op.FailureReason = MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT;
        op.FailureClass = MCU_FAIL_CLASS_FLASH_INTERFACE;
        op.FallbackOk = TRUE;
      } else {
        Mcu_Hw_SwitchResultType swRes =
            Mcu_Hw_SwitchSysClkWithRollback(RCC_CFGR_SW_HSE, RCC_CFGR_SWS_HSE);

        if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_OK) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_SWS_SWITCH_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = TRUE;
        } else if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_FAIL) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_HSI_FALLBACK_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = FALSE;
        } else {
          op.Status = E_OK;
        }
      }
    } else {
      Mcu_Hw_WriteBusPrescalers(ClockCfg);
      if (Mcu_Hw_ConfigureFlash(ClockCfg->FlashLatency,
                                ClockCfg->FlashPrefetchEnable) != E_OK) {
        op.Status = E_NOT_OK;
        op.FailureReason = MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT;
        op.FailureClass = MCU_FAIL_CLASS_FLASH_INTERFACE;
        op.FallbackOk = TRUE;
      }
    }
  }

  return op;
}

/**
 * @brief   Khôi phục cây xung sau khi thức dậy từ STOP mode.
 */
static inline Mcu_Hw_ClockOpResultType
Mcu_Hw_RestoreClockAfterStop(const Mcu_ClockConfigType *ActiveClockCfg,
                             const Mcu_ClockConfigType *PendingPllCfg,
                             boolean PllDistributed) {
  Mcu_Hw_ClockOpResultType op = {E_OK, MCU_CLK_FAIL_NONE, MCU_FAIL_CLASS_NONE,
                                 TRUE};

  if (PendingPllCfg != NULL_PTR) {
    /* Trường hợp 1: PLL đang chờ (chưa phân phối hoặc đang chờ retry sau
     * rollback) */
    if (PendingPllCfg->HseEnable == TRUE) {
      if ((RCC->CR & RCC_CR_HSERDY) == 0UL) {
        RCC->CR |= RCC_CR_HSEON;
        __DSB();
        if (Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY) != E_OK) {
          RCC->CR &= ~RCC_CR_HSEON;
          __DSB();
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_STOP_HSE_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk =
              ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI) ? TRUE : FALSE;
        }
      }
    }

    if ((op.Status == E_OK) && (PendingPllCfg->PllEnable == TRUE)) {
      if ((RCC->CR & RCC_CR_PLLON) == 0UL) {
        if (Mcu_Hw_ConfigureFlash(PendingPllCfg->FlashLatency,
                                  PendingPllCfg->FlashPrefetchEnable) != E_OK) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_FLASH_INTERFACE;
          op.FallbackOk = TRUE;
        } else {
          Mcu_Hw_WriteBusPrescalers(PendingPllCfg);
          Mcu_Hw_WritePllConfig(PendingPllCfg);
          /* Khởi động lại quá trình khóa PLL; KHÔNG chuyển SW = PLL */
          RCC->CR |= RCC_CR_PLLON;
          __DSB();
        }
      }
    }
  } else if ((ActiveClockCfg != NULL_PTR) && (PllDistributed == TRUE)) {
    /* Trường hợp 2: PLL đã phân phối ổn định trước STOP */
    boolean stopBypassed = (((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL) &&
                            ((RCC->CR & RCC_CR_PLLRDY) != 0UL) &&
                            ((ActiveClockCfg->HseEnable == FALSE) ||
                             ((RCC->CR & RCC_CR_HSERDY) != 0UL)))
                               ? TRUE
                               : FALSE;

    if (stopBypassed == FALSE) {
      if (ActiveClockCfg->HseEnable == TRUE) {
        RCC->CR |= RCC_CR_HSEON;
        __DSB();
        if (Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY) != E_OK) {
          RCC->CR &= ~RCC_CR_HSEON;
          __DSB();
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_STOP_HSE_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk =
              ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI) ? TRUE : FALSE;
        }
      }

      if (op.Status == E_OK) {
        if (Mcu_Hw_ConfigureFlash(ActiveClockCfg->FlashLatency,
                                  ActiveClockCfg->FlashPrefetchEnable) !=
            E_OK) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_FLASH_INTERFACE;
          op.FallbackOk = TRUE;
        }
      }

      if (op.Status == E_OK) {
        Mcu_Hw_WriteBusPrescalers(ActiveClockCfg);
        Mcu_Hw_WritePllConfig(ActiveClockCfg);
        RCC->CR |= RCC_CR_PLLON;
        __DSB();

        if (Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY) != E_OK) {
          (void)Mcu_Hw_DisablePll();
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_STOP_PLL_LOCK_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk =
              ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI) ? TRUE : FALSE;
        }
      }

      if (op.Status == E_OK) {
        Mcu_Hw_SwitchResultType swRes =
            Mcu_Hw_SwitchSysClkWithRollback(RCC_CFGR_SW_PLL, RCC_CFGR_SWS_PLL);

        if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_OK) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_STOP_SWS_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = TRUE;
        } else if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_FAIL) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_HSI_FALLBACK_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = FALSE;
        } else {
          op.Status = E_OK;
        }
      }
    }
  } else if ((ActiveClockCfg != NULL_PTR) &&
             (ActiveClockCfg->SysClkSource == MCU_SYSCLK_SRC_HSE)) {
    if ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE) {
      RCC->CR |= RCC_CR_HSEON;
      __DSB();
      if (Mcu_Hw_WaitFlag(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY) != E_OK) {
        RCC->CR &= ~RCC_CR_HSEON;
        __DSB();
        op.Status = E_NOT_OK;
        op.FailureReason = MCU_CLK_FAIL_STOP_HSE_TIMEOUT;
        op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
        op.FallbackOk =
            ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI) ? TRUE : FALSE;
      } else {
        Mcu_Hw_SwitchResultType swRes =
            Mcu_Hw_SwitchSysClkWithRollback(RCC_CFGR_SW_HSE, RCC_CFGR_SWS_HSE);

        if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_OK) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_STOP_SWS_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = TRUE;
        } else if (swRes == MCU_HW_SWITCH_TIMEOUT_FALLBACK_FAIL) {
          op.Status = E_NOT_OK;
          op.FailureReason = MCU_CLK_FAIL_HSI_FALLBACK_TIMEOUT;
          op.FailureClass = MCU_FAIL_CLASS_CLOCK_SOURCE;
          op.FallbackOk = FALSE;
        } else {
          op.Status = E_OK;
        }
      }
    }
  } else {
    /* HSI hoặc chưa cấu hình xung: phần cứng đã sẵn ở HSI 8 MHz */
  }

  return op;
}

/**
 * @brief   Thực thi chuyển chế độ nguồn trong ngữ cảnh đã khóa PRIMASK = 1.
 */
static inline Mcu_Hw_ClockOpResultType
Mcu_Hw_EnterModeLocked(const Mcu_ModeConfigType *ModeCfg,
                       const Mcu_ClockConfigType *ActiveClockCfg,
                       const Mcu_ClockConfigType *PendingPllCfg,
                       boolean PllDistributed) {
  Mcu_Hw_ClockOpResultType op = {E_OK, MCU_CLK_FAIL_NONE, MCU_FAIL_CLASS_NONE,
                                 TRUE};

  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  __DSB();

  if (ModeCfg->PowerMode == MCU_POWER_MODE_RUN) {
    SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk);
    PWR->CR &= ~(PWR_CR_PDDS | PWR_CR_LPDS);
    __DSB();
  } else if (ModeCfg->PowerMode == MCU_POWER_MODE_SLEEP) {
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    if (ModeCfg->SleepOnExit == TRUE) {
      SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk;
    } else {
      SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;
    }

    PWR->CR &= ~PWR_CR_PDDS;
    __DSB();
    __WFI();
    __ISB();
  } else if (ModeCfg->PowerMode == MCU_POWER_MODE_STOP) {
    PWR->CR &= ~PWR_CR_PDDS;
    SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;

    if (ModeCfg->LowPowerRegulator == TRUE) {
      PWR->CR |= PWR_CR_LPDS;
    } else {
      PWR->CR &= ~PWR_CR_LPDS;
    }

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __WFI();
    __ISB();

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    __DSB();

    op = Mcu_Hw_RestoreClockAfterStop(ActiveClockCfg, PendingPllCfg,
                                      PllDistributed);
  } else {
    /* Nhánh phòng vệ */
  }

  return op;
}

#endif /* MCU_HW_STM32F1_H */