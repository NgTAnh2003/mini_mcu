/**
 * @file    Mcu_Types.h
 * @brief   Các kiểu dữ liệu công khai của MCAL MCU Driver theo AUTOSAR CP
 * R20-11.
 * @details Tuyệt đối không include header thanh ghi CMSIS (stm32f1xx.h) trong
 * file này.
 */

#ifndef MCU_TYPES_H
#define MCU_TYPES_H

#include "Std_Types.h"

/*============================================================================*/
/*                    THÔNG TIN PHIÊN BẢN FILE (R20-11)                       */
/*============================================================================*/

/** @brief Phiên bản chính AUTOSAR Classic (R20-11 tương ứng 4.6.0). */
#define MCU_TYPES_AR_RELEASE_MAJOR_VERSION (4U)
/** @brief Phiên bản phụ AUTOSAR Classic (R20-11 tương ứng 4.6.0). */
#define MCU_TYPES_AR_RELEASE_MINOR_VERSION (6U)
/** @brief Bản sửa đổi AUTOSAR Classic (R20-11 tương ứng 4.6.0). */
#define MCU_TYPES_AR_RELEASE_REVISION_VERSION (0U)

/** @brief Phiên bản chính mã nguồn driver. */
#define MCU_TYPES_SW_MAJOR_VERSION (1U)
/** @brief Phiên bản phụ mã nguồn driver. */
#define MCU_TYPES_SW_MINOR_VERSION (2U)
/** @brief Bản vá lỗi mã nguồn driver. */
#define MCU_TYPES_SW_PATCH_VERSION (0U)

/*============================================================================*/
/*                     KIỂU DỮ LIỆU CHUẨN AUTOSAR R20-11                      */
/*============================================================================*/

/** @brief [SWS_Mcu_00251] Mã định danh một cấu hình xung hệ thống. */
typedef uint8 Mcu_ClockType;

/** @brief [SWS_Mcu_00253] Giá trị thanh ghi nguyên nhân reset ở dạng thô
 * (32-bit). */
typedef uint32 Mcu_RawResetType;

/** @brief [SWS_Mcu_00254] Mã định danh một chế độ nguồn MCU. */
typedef uint8 Mcu_ModeType;

/** @brief [SWS_Mcu_00255] Mã định danh một phân vùng RAM cần khởi tạo. */
typedef uint8 Mcu_RamSectionType;

/**
 * @brief [SWS_Mcu_00250] Trạng thái khóa pha PLL trả về bởi Mcu_GetPllStatus().
 */
typedef enum {
  MCU_PLL_LOCKED = 0x00U, /**< PLL đã khóa pha ổn định (RCC_CR_PLLRDY = 1). */
  MCU_PLL_UNLOCKED =
      0x01U, /**< PLL chưa khóa pha hoặc đang tắt (RCC_CR_PLLRDY = 0). */
  MCU_PLL_STATUS_UNDEFINED =
      0x02U /**< Không xác định (chưa Mcu_Init hoặc MCU_NO_PLL = STD_ON). */
} Mcu_PllStatusType;

/**
 * @brief [SWS_Mcu_00252] Nguyên nhân reset giải mã trả về bởi
 * Mcu_GetResetReason().
 * @details 4 giá trị đầu (0x00..0x03) là chuẩn AUTOSAR; tập tối thiểu bắt buộc
 * theo SWS_Mcu_00134 gồm MCU_POWER_ON_RESET và MCU_RESET_UNDEFINED. 3 giá trị
 * cuối là mở rộng phần cứng cho STM32F103 (RM0008).
 */
typedef enum {
  MCU_POWER_ON_RESET =
      0x00U, /**< [Chuẩn/Tối thiểu SWS_Mcu_00134] Reset nguồn (PORRSTF). */
  MCU_WATCHDOG_RESET =
      0x01U, /**< [Chuẩn SWS_Mcu_00252] Independent Watchdog (IWDGRSTF). */
  MCU_SW_RESET = 0x02U, /**< [Chuẩn SWS_Mcu_00252] Reset phần mềm (SFTRSTF). */
  MCU_RESET_UNDEFINED = 0x03U, /**< [Chuẩn/Tối thiểu SWS_Mcu_00134] Chưa khởi
                                  tạo hoặc không rõ. */
  MCU_PIN_RESET = 0x04U, /**< [Mở rộng STM32F1] Reset chân NRST (PINRSTF). */
  MCU_WINDOW_WATCHDOG_RESET =
      0x05U, /**< [Mở rộng STM32F1] Window Watchdog (WWDGRSTF). */
  MCU_LOW_POWER_RESET =
      0x06U /**< [Mở rộng STM32F1] Reset quản lý nguồn thấp (LPWRRSTF). */
} Mcu_ResetType;

/**
 * @brief [SWS_Mcu_00256] Trạng thái toàn vẹn RAM trả về bởi Mcu_GetRamState().
 */
typedef enum {
  MCU_RAMSTATE_INVALID =
      0x00U, /**< Nội dung RAM không hợp lệ hoặc không thể xác minh. */
  MCU_RAMSTATE_VALID =
      0x01U /**< Nội dung RAM được xác nhận còn hợp lệ sau reset. */
} Mcu_RamStateType;

/*============================================================================*/
/*               KIỂU CẤU HÌNH ĐẶC THÙ STM32F103 (TRỪU TƯỢNG HÓA)             */
/*============================================================================*/

/** @brief Nguồn xung hệ thống SYSCLK trừu tượng (ánh xạ tới RCC_CFGR_SW / SWS).
 */
typedef enum {
  MCU_SYSCLK_SRC_HSI = 0x00U, /**< Dao động RC nội HSI 8 MHz. */
  MCU_SYSCLK_SRC_HSE = 0x01U, /**< Thạch anh ngoài HSE trực tiếp. */
  MCU_SYSCLK_SRC_PLL = 0x02U  /**< Đầu ra bộ nhân pha PLL. */
} Mcu_SysClkSourceType;

/** @brief Nguồn xung đầu vào cho bộ nhân PLL trên STM32F103. */
typedef enum {
  MCU_PLL_SRC_HSI_DIV2 = 0x00U, /**< HSI / 2 (4 MHz). */
  MCU_PLL_SRC_HSE_DIV1 =
      0x01U,                   /**< HSE trực tiếp (PLLSRC = 1, PLLXTPRE = 0). */
  MCU_PLL_SRC_HSE_DIV2 = 0x02U /**< HSE / 2 (PLLSRC = 1, PLLXTPRE = 1). */
} Mcu_PllSourceType;

/** @brief Hệ số nhân PLL trên STM32F103 (từ x2 đến x16; giới hạn SYSCLK <= 72
 * MHz). */
typedef enum {
  MCU_PLL_MUL_2 = 0x00U,  /**< Nhân x2. */
  MCU_PLL_MUL_3 = 0x01U,  /**< Nhân x3. */
  MCU_PLL_MUL_4 = 0x02U,  /**< Nhân x4. */
  MCU_PLL_MUL_5 = 0x03U,  /**< Nhân x5. */
  MCU_PLL_MUL_6 = 0x04U,  /**< Nhân x6. */
  MCU_PLL_MUL_7 = 0x05U,  /**< Nhân x7. */
  MCU_PLL_MUL_8 = 0x06U,  /**< Nhân x8. */
  MCU_PLL_MUL_9 = 0x07U,  /**< Nhân x9 (HSE 8 MHz -> 72 MHz). */
  MCU_PLL_MUL_10 = 0x08U, /**< Nhân x10. */
  MCU_PLL_MUL_11 = 0x09U, /**< Nhân x11. */
  MCU_PLL_MUL_12 = 0x0AU, /**< Nhân x12. */
  MCU_PLL_MUL_13 = 0x0BU, /**< Nhân x13. */
  MCU_PLL_MUL_14 = 0x0CU, /**< Nhân x14. */
  MCU_PLL_MUL_15 = 0x0DU, /**< Nhân x15. */
  MCU_PLL_MUL_16 = 0x0EU  /**< Nhân x16. */
} Mcu_PllMulType;

/** @brief Hệ số chia bus AHB (HCLK = SYSCLK / HPRE, tối đa 72 MHz). */
typedef enum {
  MCU_AHB_DIV_1 = 0x00U,   /**< Chia 1. */
  MCU_AHB_DIV_2 = 0x01U,   /**< Chia 2. */
  MCU_AHB_DIV_4 = 0x02U,   /**< Chia 4. */
  MCU_AHB_DIV_8 = 0x03U,   /**< Chia 8. */
  MCU_AHB_DIV_16 = 0x04U,  /**< Chia 16. */
  MCU_AHB_DIV_64 = 0x05U,  /**< Chia 64. */
  MCU_AHB_DIV_128 = 0x06U, /**< Chia 128. */
  MCU_AHB_DIV_256 = 0x07U, /**< Chia 256. */
  MCU_AHB_DIV_512 = 0x08U  /**< Chia 512. */
} Mcu_AhbPrescalerType;

/** @brief Hệ số chia bus APB1 (tối đa 36 MHz) và APB2 (tối đa 72 MHz). */
typedef enum {
  MCU_APB_DIV_1 = 0x00U, /**< Chia 1. */
  MCU_APB_DIV_2 = 0x01U, /**< Chia 2. */
  MCU_APB_DIV_4 = 0x02U, /**< Chia 4. */
  MCU_APB_DIV_8 = 0x03U, /**< Chia 8. */
  MCU_APB_DIV_16 = 0x04U /**< Chia 16. */
} Mcu_ApbPrescalerType;

/** @brief Hệ số chia xung ADC từ PCLK2 (ADCCLK tối đa 14 MHz theo RM0008). */
typedef enum {
  MCU_ADC_DIV_2 = 0x00U, /**< PCLK2 / 2. */
  MCU_ADC_DIV_4 = 0x01U, /**< PCLK2 / 4. */
  MCU_ADC_DIV_6 = 0x02U, /**< PCLK2 / 6 (72 MHz / 6 = 12 MHz). */
  MCU_ADC_DIV_8 = 0x03U  /**< PCLK2 / 8. */
} Mcu_AdcPrescalerType;

/** @brief Số chu kỳ trễ đọc Flash (RM0008/PM0075: 0WS <=24MHz, 1WS <=48MHz, 2WS
 * <=72MHz). */
typedef enum {
  MCU_FLASH_LATENCY_0WS = 0x00U, /**< 0 chu kỳ chờ (0 < SYSCLK <= 24 MHz). */
  MCU_FLASH_LATENCY_1WS = 0x01U, /**< 1 chu kỳ chờ (24 < SYSCLK <= 48 MHz). */
  MCU_FLASH_LATENCY_2WS = 0x02U  /**< 2 chu kỳ chờ (48 < SYSCLK <= 72 MHz). */
} Mcu_FlashLatencyType;

/** @brief Chế độ công suất hỗ trợ trên STM32F103. */
typedef enum {
  MCU_POWER_MODE_RUN = 0x00U, /**< Chế độ hoạt động bình thường. */
  MCU_POWER_MODE_SLEEP =
      0x01U, /**< Chế độ Sleep (dừng lõi CPU, giữ xung ngoại vi). */
  MCU_POWER_MODE_STOP =
      0x02U /**< Chế độ Stop (dừng HSE/PLL/HSI, giữ SRAM và thanh ghi). */
} Mcu_PowerModeType;

/** @brief Cơ chế reset phần cứng sử dụng trong Mcu_PerformReset()
 * [SWS_Mcu_00144]. */
typedef enum {
  MCU_RESET_HW_SYSRESETREQ =
      0x00U /**< Reset hệ thống qua SCB->AIRCR.SYSRESETREQ. */
} Mcu_ResetHwType;

/** @brief Độ rộng mỗi lần ghi vào phân vùng RAM trong Mcu_InitRamSection() (đơn
 * vị: byte). */
typedef enum {
  MCU_RAM_WRITE_SIZE_1BYTE =
      1U, /**< Ghi từng byte bằng truy cập volatile uint8. */
  MCU_RAM_WRITE_SIZE_2BYTE =
      2U, /**< Ghi từng half-word bằng truy cập volatile uint16. */
  MCU_RAM_WRITE_SIZE_4BYTE =
      4U /**< Ghi từng word bằng truy cập volatile uint32. */
} Mcu_RamWriteSizeType;

/**
 * @brief [Mở rộng tích hợp] Trạng thái máy trạng thái xung đã công bố của
 * driver.
 */
typedef enum {
  MCU_CLK_STATE_UNSPECIFIED =
      0x00U, /**< Chưa gọi Mcu_InitClock kể từ sau reset. */
  MCU_CLK_STATE_PLL_PENDING =
      0x01U, /**< Đã bật PLL (Mcu_InitClock), SYSCLK vẫn ở HSI chờ phân phối. */
  MCU_CLK_STATE_STABLE = 0x02U, /**< Nguồn xung đích đã xác nhận ổn định (SW ==
                                   SWS == đích) và đủ cấu hình khôi phục. */
  MCU_CLK_STATE_FALLBACK_HSI = 0x03U, /**< Xung đích lỗi/timeout, đã thu hồi an
                                         toàn về HSI (SW == SWS == HSI). */
  MCU_CLK_STATE_FAULT = 0x04U /**< Lỗi nghiêm trọng: cả đích và HSI dự phòng đều
                                 không xác nhận được. */
} Mcu_ClockStateType;

/**
 * @brief [Mở rộng tích hợp] Phân nhóm nguyên nhân lỗi để tách lỗi nguồn xung
 * (DEM) khỏi lỗi Flash.
 */
typedef enum {
  MCU_FAIL_CLASS_NONE = 0x00U, /**< Không có lỗi phần cứng. */
  MCU_FAIL_CLASS_CLOCK_SOURCE = 0x01U, /**< Lỗi nguồn dao động/PLL/chuyển nguồn
                                          -> ánh xạ DEM MCU_E_CLOCK_FAILURE. */
  MCU_FAIL_CLASS_FLASH_INTERFACE =
      0x02U /**< Lỗi xác nhận độ trễ FLASH->ACR -> KHÔNG báo DEM
               MCU_E_CLOCK_FAILURE. */
} Mcu_FailureClassType;

/**
 * @brief [Mở rộng tích hợp] Mã nguyên nhân chi tiết cho sự cố phần cứng trong
 * thao tác xung.
 */
typedef enum {
  MCU_CLK_FAIL_NONE = 0x00U, /**< Không có lỗi. */
  MCU_CLK_FAIL_HSI_READY_TIMEOUT =
      0x01U, /**< [CLOCK_SOURCE] Timeout chờ cờ RCC_CR_HSIRDY. */
  MCU_CLK_FAIL_HSI_FALLBACK_TIMEOUT =
      0x02U, /**< [CLOCK_SOURCE] Timeout khi thu hồi SWS về HSI dự phòng. */
  MCU_CLK_FAIL_HSE_READY_TIMEOUT =
      0x03U, /**< [CLOCK_SOURCE] Timeout chờ cờ RCC_CR_HSERDY (bật hoặc tắt
                HSE). */
  MCU_CLK_FAIL_PLL_DISABLE_TIMEOUT =
      0x04U, /**< [CLOCK_SOURCE] Timeout chờ RCC_CR_PLLRDY về 0 khi tắt PLL. */
  MCU_CLK_FAIL_FLASH_LATENCY_TIMEOUT =
      0x05U, /**< [FLASH_INTERFACE] Timeout xác nhận trường LATENCY trong
                FLASH->ACR. */
  MCU_CLK_FAIL_SWS_SWITCH_TIMEOUT = 0x06U, /**< [CLOCK_SOURCE] Timeout chờ SWS
                                              khớp nguồn đích (đã rollback). */
  MCU_CLK_FAIL_STOP_HSE_TIMEOUT =
      0x07U, /**< [CLOCK_SOURCE] Timeout bật lại HSE khi khôi phục sau STOP. */
  MCU_CLK_FAIL_STOP_PLL_LOCK_TIMEOUT =
      0x08U, /**< [CLOCK_SOURCE] Timeout chờ PLLRDY khóa lại sau STOP. */
  MCU_CLK_FAIL_STOP_SWS_TIMEOUT =
      0x09U /**< [CLOCK_SOURCE] Timeout chờ SWS về PLL/HSE sau STOP. */
} Mcu_ClockFailureReasonType;

/**
 * @brief [Mở rộng tích hợp] Con trỏ hàm callback thông báo lỗi phần cứng (luôn
 * gọi ngoài vùng khóa ngắt).
 * @param FailureReason Mã nguyên nhân lỗi chi tiết.
 * @param CurrentState  Trạng thái máy xung đã công bố sau khi xử lý thu hồi.
 */
typedef void (*Mcu_ClockFailureCallbackType)(
    Mcu_ClockFailureReasonType FailureReason, Mcu_ClockStateType CurrentState);

/**
 * @brief Cấu hình chi tiết một cây xung (McuClockSettingConfig).
 */
typedef struct {
  Mcu_SysClkSourceType
      SysClkSource;  /**< Nguồn SYSCLK mục tiêu (HSI, HSE hoặc PLL). */
  boolean HseEnable; /**< TRUE: bật HSE; FALSE: tắt HSE nếu không dùng. */
  boolean
      PllEnable; /**< TRUE: bật PLL trong Mcu_InitClock (không tự chuyển SW). */
  Mcu_PllSourceType
      PllSource;         /**< Nguồn đầu vào PLL (khi PllEnable == TRUE). */
  Mcu_PllMulType PllMul; /**< Hệ số nhân PLL x2..x16 (khi PllEnable == TRUE). */
  Mcu_AhbPrescalerType AhbPrescaler; /**< Hệ số chia bus AHB (HCLK). */
  Mcu_ApbPrescalerType
      Apb1Prescaler; /**< Hệ số chia bus APB1 (PCLK1 <= 36 MHz). */
  Mcu_ApbPrescalerType
      Apb2Prescaler; /**< Hệ số chia bus APB2 (PCLK2 <= 72 MHz). */
  Mcu_AdcPrescalerType
      AdcPrescaler; /**< Hệ số chia xung ADC từ PCLK2 (ADCCLK <= 14 MHz). */
  Mcu_FlashLatencyType FlashLatency; /**< Số wait state cho FLASH->ACR. */
  boolean
      FlashPrefetchEnable; /**< TRUE: bật bộ đệm nạp trước FLASH_ACR_PRFTBE. */
} Mcu_ClockConfigType;

/**
 * @brief Cấu hình chi tiết một chế độ nguồn (McuModeSettingConf).
 */
typedef struct {
  Mcu_PowerModeType PowerMode; /**< Chế độ nguồn đích: RUN, SLEEP hoặc STOP. */
  boolean
      LowPowerRegulator; /**< Chỉ dùng cho STOP: TRUE đặt PWR_CR_LPDS = 1. */
  boolean
      SleepOnExit; /**< Chỉ dùng cho SLEEP: TRUE đặt SCB_SCR_SLEEPONEXIT = 1. */
} Mcu_ModeConfigType;

/**
 * @brief Cấu hình một phân vùng RAM cho Mcu_InitRamSection()
 * (McuRamSectorSettingConf).
 */
typedef struct {
  void *BaseAddress; /**< Địa chỉ bắt đầu của vùng RAM đích (căn chỉnh bội
                        WriteSize). */
  uint32 SectionSize; /**< Kích thước vùng RAM tính bằng byte (> 0, bội
                         WriteSize). */
  uint8
      DefaultValue; /**< Giá trị byte điền mặc định (nhân bản cho ghi 2B/4B). */
  Mcu_RamWriteSizeType
      WriteSize; /**< Độ rộng mỗi lần ghi phần cứng: 1, 2 hoặc 4 byte. */
} Mcu_RamSectionConfigType;

/**
 * @brief [SWS_Mcu_00249] Cấu trúc cấu hình Post-Build tổng truyền vào
 * Mcu_Init().
 */
typedef struct {
  const Mcu_ClockConfigType *ClockConfigPtr; /**< Con trỏ mảng cấu hình xung. */
  uint8 NumClockConfigs;                     /**< Số lượng cấu hình xung. */
  const Mcu_ModeConfigType
      *ModeConfigPtr;   /**< Con trỏ mảng cấu hình chế độ nguồn. */
  uint8 NumModeConfigs; /**< Số lượng chế độ nguồn. */
  const Mcu_RamSectionConfigType
      *RamSectionConfigPtr; /**< Con trỏ mảng cấu hình phân vùng RAM. */
  uint8 NumRamSections;     /**< Số lượng phân vùng RAM. */
  Mcu_ResetHwType
      ResetHwType; /**< Cơ chế reset phần cứng cho Mcu_PerformReset(). */
  Mcu_ClockFailureCallbackType
      ClockFailureCallback; /**< [Tùy chọn tích hợp] Callback lỗi (có thể
                               NULL_PTR). */
} Mcu_ConfigType;

#endif /* MCU_TYPES_H */