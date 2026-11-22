/**
 * @file    Mcu_Cfg.h
 * @brief   Cấu hình tiền biên dịch (Pre-Compile) và tên ký hiệu ECUC cho MCAL
 * MCU.
 * @details Tuân thủ AUTOSAR Classic Platform R20-11. Tuyệt đối không include
 * CMSIS hoặc Dem.h khi MCU_DEM_ERROR_REPORT == STD_OFF để giữ cấu hình độc lập
 * hoàn toàn với DEM.
 */

#ifndef MCU_CFG_H
#define MCU_CFG_H

#include "Mcu_Types.h"
#include "Std_Types.h"

/*============================================================================*/
/*                    THÔNG TIN PHIÊN BẢN FILE (R20-11)                       */
/*============================================================================*/

/** @brief Phiên bản chính AUTOSAR Classic (4.6.0 = R20-11). */
#define MCU_CFG_AR_RELEASE_MAJOR_VERSION (4U)
/** @brief Phiên bản phụ AUTOSAR Classic (4.6.0 = R20-11). */
#define MCU_CFG_AR_RELEASE_MINOR_VERSION (6U)
/** @brief Bản sửa đổi AUTOSAR Classic (4.6.0 = R20-11). */
#define MCU_CFG_AR_RELEASE_REVISION_VERSION (0U)

/** @brief Phiên bản chính phần mềm cấu hình. */
#define MCU_CFG_SW_MAJOR_VERSION (1U)
/** @brief Phiên bản phụ phần mềm cấu hình. */
#define MCU_CFG_SW_MINOR_VERSION (2U)
/** @brief Bản vá phần mềm cấu hình. */
#define MCU_CFG_SW_PATCH_VERSION (0U)

/*============================================================================*/
/*                    CÔNG TẮC CẤU HÌNH TIỀN BIÊN DỊCH                        */
/*============================================================================*/

/** @brief [McuDevErrorDetect] Bật/tắt kiểm tra và báo lỗi phát triển qua DET.
 */
#ifndef MCU_DEV_ERROR_DETECT
#define MCU_DEV_ERROR_DETECT (STD_ON)
#endif

/** @brief [McuInitClock, SWS_Mcu_00210] Bật/tắt dịch vụ Mcu_InitClock(). */
#ifndef MCU_INIT_CLOCK
#define MCU_INIT_CLOCK (STD_ON)
#endif

/**
 * @brief [McuNoPll, SWS_Mcu_00205] Cho biết phần cứng có PLL hay không.
 *        STD_OFF: Có PLL -> bật Mcu_DistributePllClock().
 *        STD_ON : Không PLL -> tắt Mcu_DistributePllClock(), Mcu_GetPllStatus()
 * trả UNDEFINED.
 */
#ifndef MCU_NO_PLL
#define MCU_NO_PLL (STD_OFF)
#endif

/** @brief [McuPerformResetApi, SWS_Mcu_00146] Bật/tắt dịch vụ
 * Mcu_PerformReset(). */
#ifndef MCU_PERFORM_RESET_API
#define MCU_PERFORM_RESET_API (STD_ON)
#endif

/** @brief [McuVersionInfoApi] Bật/tắt dịch vụ Mcu_GetVersionInfo(). */
#ifndef MCU_VERSION_INFO_API
#define MCU_VERSION_INFO_API (STD_ON)
#endif

/**
 * @brief [McuGetRamStateApi, SWS_Mcu_00209] Bật/tắt dịch vụ Mcu_GetRamState().
 * @note  Mặc định STD_OFF vì STM32F103C6/C8 không có phần cứng SRAM
 * ECC/retention flag.
 */
#ifndef MCU_GET_RAM_STATE_API
#define MCU_GET_RAM_STATE_API (STD_OFF)
#endif

/**
 * @brief [McuDemEventParameterRefs, SWS_Mcu_00053, SWS_Mcu_00257,
 * SWS_Mcu_00258] Bật/tắt báo lỗi sản xuất mở rộng MCU_E_CLOCK_FAILURE sang DEM.
 * @note  Mặc định STD_OFF để driver hoàn toàn độc lập với DEM. Khi đặt STD_ON,
 *        dự án cung cấp Dem.h (chứa Dem_SetEventStatus,
 * DEM_EVENT_STATUS_FAILED/PASSED) và có thể ánh xạ
 * MCU_DEM_EVENT_ID_CLOCK_FAILURE tới
 * DemConf_DemEventParameter_MCU_E_CLOCK_FAILURE.
 */
#ifndef MCU_DEM_ERROR_REPORT
#define MCU_DEM_ERROR_REPORT (STD_OFF)
#endif

#if (MCU_DEM_ERROR_REPORT == STD_ON)
/** @brief Mã sự kiện DEM mặc định ánh xạ cho MCU_E_CLOCK_FAILURE (ghi đè bởi
 * cấu hình ECU). */
#ifndef MCU_DEM_EVENT_ID_CLOCK_FAILURE
#define MCU_DEM_EVENT_ID_CLOCK_FAILURE                                         \
  (DemConf_DemEventParameter_MCU_E_CLOCK_FAILURE)
#endif
#endif /* (MCU_DEM_ERROR_REPORT == STD_ON) */

/** @brief [Mở rộng tích hợp] Bật/tắt gọi hàm ClockFailureCallback khi xảy ra
 * lỗi phần cứng. */
#ifndef MCU_CLOCK_FAILURE_NOTIFICATION
#define MCU_CLOCK_FAILURE_NOTIFICATION (STD_ON)
#endif

/*============================================================================*/
/*                    THAM SỐ NỘI BỘ DO NGƯỜI TRIỂN KHAI CHỌN                 */
/*============================================================================*/

/** @brief Số vòng lặp tối đa chờ cờ phần cứng (đơn vị: số lần đọc thanh ghi).
 */
#ifndef MCU_HW_TIMEOUT_COUNT
#define MCU_HW_TIMEOUT_COUNT (100000UL)
#endif

/**
 * @brief [SWS_Mcu_00135] Giá trị thô khác 0 trả về khi gọi
 * Mcu_GetResetRawValue() trước Mcu_Init().
 * @details Bit 25 của STM32F103 RCC->CSR là Reserved luôn đọc ra 0 (RM0008
 * §7.3.10).
 */
#define MCU_RAW_RESET_UNINIT_VALUE ((Mcu_RawResetType)0x02000000UL)

/*============================================================================*/
/*               TÊN KÝ HIỆU SINH TỪ CẤU HÌNH ECU (SYMBOLIC NAMES)            */
/*============================================================================*/

/** @brief Mã cấu hình xung nội HSI 8 MHz. */
#define McuConf_McuClockSettingConfig_HSI_8MHz ((Mcu_ClockType)0U)
/** @brief Mã cấu hình xung HSE 8 MHz -> PLL x9 -> SYSCLK 72 MHz. */
#define McuConf_McuClockSettingConfig_HSE_PLL_72MHz ((Mcu_ClockType)1U)
/** @brief Tổng số cấu hình xung mẫu. */
#define MCU_NUM_CLOCK_SETTINGS (2U)

/** @brief Mã chế độ nguồn RUN. */
#define McuConf_McuModeSettingConf_RUN ((Mcu_ModeType)0U)
/** @brief Mã chế độ nguồn SLEEP. */
#define McuConf_McuModeSettingConf_SLEEP ((Mcu_ModeType)1U)
/** @brief Mã chế độ nguồn STOP (LPDS = 1). */
#define McuConf_McuModeSettingConf_STOP ((Mcu_ModeType)2U)
/** @brief Tổng số chế độ nguồn mẫu. */
#define MCU_NUM_MODE_SETTINGS (3U)

/** @brief Mã phân vùng RAM mẫu 1-byte (16 byte, .noinit.mcu). */
#define McuConf_McuRamSectorSettingConf_SampleRam1B ((Mcu_RamSectionType)0U)
/** @brief Mã phân vùng RAM mẫu 2-byte (32 byte, .noinit.mcu). */
#define McuConf_McuRamSectorSettingConf_SampleRam2B ((Mcu_RamSectionType)1U)
/** @brief Mã phân vùng RAM mẫu 4-byte (64 byte, .noinit.mcu). */
#define McuConf_McuRamSectorSettingConf_SampleRam4B ((Mcu_RamSectionType)2U)
/** @brief Tổng số phân vùng RAM mẫu. */
#define MCU_NUM_RAM_SECTIONS (3U)

/** @brief Kích thước phân vùng RAM mẫu 1-byte (đơn vị: byte). */
#define MCU_SAMPLE_RAM1B_SIZE_BYTES (16UL)
/** @brief Giá trị byte điền mặc định cho phân vùng RAM mẫu 1-byte. */
#define MCU_SAMPLE_RAM1B_DEFAULT_VALUE ((uint8)0xA5U)

/** @brief Kích thước phân vùng RAM mẫu 2-byte (đơn vị: byte). */
#define MCU_SAMPLE_RAM2B_SIZE_BYTES (32UL)
/** @brief Giá trị byte điền mặc định cho phân vùng RAM mẫu 2-byte. */
#define MCU_SAMPLE_RAM2B_DEFAULT_VALUE ((uint8)0x3CU)

/** @brief Kích thước phân vùng RAM mẫu 4-byte (đơn vị: byte). */
#define MCU_SAMPLE_RAM4B_SIZE_BYTES (64UL)
/** @brief Giá trị byte điền mặc định cho phân vùng RAM mẫu 4-byte. */
#define MCU_SAMPLE_RAM4B_DEFAULT_VALUE ((uint8)0x5AU)

/*============================================================================*/
/*                    KHAI BÁO BỘ CẤU HÌNH POST-BUILD                         */
/*============================================================================*/

#define MCU_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "Mcu_MemMap.h"

/** @brief Biến cấu hình Post-Build mẫu truyền vào Mcu_Init(&Mcu_Config). */
extern const Mcu_ConfigType Mcu_Config;

#define MCU_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "Mcu_MemMap.h"

#endif /* MCU_CFG_H */