/**
 * @file    Mcu.h
 * @brief   Header giao diện công khai (Public API) của MCAL MCU Driver -
 * AUTOSAR CP R20-11.
 * @details Giữ nguyên đúng 11 dịch vụ tiêu chuẩn mục 8.3 (Service ID
 * 0x00..0x0A). Tuyệt đối không include header thanh ghi phần cứng CMSIS
 * (stm32f1xx.h).
 */

#ifndef MCU_H
#define MCU_H

#include "Mcu_Cfg.h"
#include "Mcu_Types.h"
#include "Std_Types.h"

/*============================================================================*/
/*                    ĐỊNH DANH MODULE & PHIÊN BẢN (R20-11)                   */
/*============================================================================*/

/** @brief Mã Vendor ID mẫu (bên tích hợp phải thay bằng Vendor ID chính thức).
 */
#define MCU_VENDOR_ID (0xFFFFU)
/** @brief Mã module chuẩn AUTOSAR của MCU Driver (101). */
#define MCU_MODULE_ID (101U)
/** @brief Mã thực thể của module MCU. */
#define MCU_INSTANCE_ID (0U)

/** @brief Phiên bản chính AUTOSAR Classic (R20-11 = 4.6.0). */
#define MCU_AR_RELEASE_MAJOR_VERSION (4U)
/** @brief Phiên bản phụ AUTOSAR Classic (R20-11 = 4.6.0). */
#define MCU_AR_RELEASE_MINOR_VERSION (6U)
/** @brief Bản sửa đổi AUTOSAR Classic (R20-11 = 4.6.0). */
#define MCU_AR_RELEASE_REVISION_VERSION (0U)

/** @brief Phiên bản chính phần mềm driver MCU. */
#define MCU_SW_MAJOR_VERSION (1U)
/** @brief Phiên bản phụ phần mềm driver MCU. */
#define MCU_SW_MINOR_VERSION (2U)
/** @brief Bản vá lỗi phần mềm driver MCU. */
#define MCU_SW_PATCH_VERSION (0U)

#if ((MCU_AR_RELEASE_MAJOR_VERSION != MCU_TYPES_AR_RELEASE_MAJOR_VERSION) ||   \
     (MCU_AR_RELEASE_MINOR_VERSION != MCU_TYPES_AR_RELEASE_MINOR_VERSION) ||   \
     (MCU_AR_RELEASE_REVISION_VERSION !=                                       \
      MCU_TYPES_AR_RELEASE_REVISION_VERSION))
#error "Lệch phiên bản AUTOSAR Release giữa Mcu.h và Mcu_Types.h"
#endif

#if ((MCU_AR_RELEASE_MAJOR_VERSION != MCU_CFG_AR_RELEASE_MAJOR_VERSION) ||     \
     (MCU_AR_RELEASE_MINOR_VERSION != MCU_CFG_AR_RELEASE_MINOR_VERSION) ||     \
     (MCU_AR_RELEASE_REVISION_VERSION != MCU_CFG_AR_RELEASE_REVISION_VERSION))
#error "Lệch phiên bản AUTOSAR Release giữa Mcu.h và Mcu_Cfg.h"
#endif

#if ((MCU_SW_MAJOR_VERSION != MCU_TYPES_SW_MAJOR_VERSION) ||                   \
     (MCU_SW_MINOR_VERSION != MCU_TYPES_SW_MINOR_VERSION) ||                   \
     (MCU_SW_MAJOR_VERSION != MCU_CFG_SW_MAJOR_VERSION) ||                     \
     (MCU_SW_MINOR_VERSION != MCU_CFG_SW_MINOR_VERSION))
#error "Lệch phiên bản phần mềm giữa Mcu.h, Mcu_Types.h và Mcu_Cfg.h"
#endif

/*============================================================================*/
/*                         MÃ DỊCH VỤ API (SERVICE IDs)                       */
/*============================================================================*/

#define MCU_SID_INIT ((uint8)0x00U)
#define MCU_SID_INIT_RAM_SECTION ((uint8)0x01U)
#define MCU_SID_INIT_CLOCK ((uint8)0x02U)
#define MCU_SID_DISTRIBUTE_PLL_CLOCK ((uint8)0x03U)
#define MCU_SID_GET_PLL_STATUS ((uint8)0x04U)
#define MCU_SID_GET_RESET_REASON ((uint8)0x05U)
#define MCU_SID_GET_RESET_RAW_VALUE ((uint8)0x06U)
#define MCU_SID_PERFORM_RESET ((uint8)0x07U)
#define MCU_SID_SET_MODE ((uint8)0x08U)
#define MCU_SID_GET_VERSION_INFO ((uint8)0x09U)
#define MCU_SID_GET_RAM_STATE ((uint8)0x0AU)

/*============================================================================*/
/*                  MÃ LỖI PHÁT TRIỂN DET [SWS_Mcu_00012]                     */
/*============================================================================*/

#define MCU_E_PARAM_CONFIG ((uint8)0x0AU)
#define MCU_E_PARAM_CLOCK ((uint8)0x0BU)
#define MCU_E_PARAM_MODE ((uint8)0x0CU)
#define MCU_E_PARAM_RAMSECTION ((uint8)0x0DU)
#define MCU_E_PLL_NOT_LOCKED ((uint8)0x0EU)
#define MCU_E_UNINIT ((uint8)0x0FU)
#define MCU_E_PARAM_POINTER ((uint8)0x10U)
#define MCU_E_INIT_FAILED ((uint8)0x11U)

/*============================================================================*/
/*                   KHAI BÁO 11 DỊCH VỤ CÔNG KHAI (MỤC 8.3)                  */
/*============================================================================*/

#define MCU_START_SEC_CODE
#include "Mcu_MemMap.h"

/**
 * @brief   [SWS_Mcu_00153] Khởi tạo module MCAL MCU (Service ID 0x00, Sync, Non
 * Reentrant).
 * @param   ConfigPtr Con trỏ bộ cấu hình Post-Build (không được NULL_PTR).
 */
void Mcu_Init(const Mcu_ConfigType *ConfigPtr);

/**
 * @brief   [SWS_Mcu_00154, SWS_Mcu_00011] Điền dữ liệu mặc định vào phân vùng
 * RAM (Service ID 0x01, Sync, Non Reentrant).
 * @param   RamSection Mã phân vùng RAM từ cấu hình.
 * @return  E_OK nếu thành công; E_NOT_OK nếu chưa khởi tạo hoặc cấu hình RAM
 * không hợp lệ.
 */
Std_ReturnType Mcu_InitRamSection(Mcu_RamSectionType RamSection);

#if (MCU_INIT_CLOCK == STD_ON)
/**
 * @brief   [SWS_Mcu_00155, SWS_Mcu_00138] Cấu hình cây xung và khởi động PLL
 * nếu có (Service ID 0x02, Sync, Non Reentrant).
 * @details Với cấu hình không dùng PLL và xác nhận SW == SWS == đích, báo DEM
 * PASSED (nếu bật DEM). Với cấu hình PLL, chỉ bật PLLON và chuyển trạng thái
 * PLL_PENDING (chưa báo DEM PASSED).
 * @param   ClockSetting Mã cấu hình xung từ bảng Post-Build.
 * @return  E_OK nếu thành công; E_NOT_OK nếu lỗi tham số hoặc timeout phần
 * cứng.
 */
Std_ReturnType Mcu_InitClock(Mcu_ClockType ClockSetting);
#endif /* (MCU_INIT_CLOCK == STD_ON) */

#if (MCU_NO_PLL == STD_OFF)
/**
 * @brief   [SWS_Mcu_00156, SWS_Mcu_00140, SWS_Mcu_00142] Phân phối xung PLL ra
 * SYSCLK (Service ID 0x03, Sync, Non Reentrant).
 * @details Kiểm tra DET theo thứ tự MCU_E_UNINIT -> MCU_E_PLL_NOT_LOCKED.
 *          Chỉ công bố STABLE khi xác nhận SW == SWS == PLL và có cấu hình PLL
 * hợp lệ để khôi phục sau STOP. Nếu timeout SWS nhưng rollback HSI thành công,
 * vẫn giữ cấu hình đang chờ để cho phép retry. Nếu rollback HSI thất bại,
 * chuyển FAULT và xóa cấu hình chờ để ngăn retry mù.
 * @return  E_OK nếu đã phân phối PLL ổn định; E_NOT_OK nếu lỗi hoặc timeout.
 */
Std_ReturnType Mcu_DistributePllClock(void);
#endif /* (MCU_NO_PLL == STD_OFF) */

/**
 * @brief   [SWS_Mcu_00157, SWS_Mcu_00008] Đọc trạng thái khóa pha PLL (Service
 * ID 0x04, Sync, Reentrant).
 * @return  MCU_PLL_LOCKED, MCU_PLL_UNLOCKED hoặc MCU_PLL_STATUS_UNDEFINED.
 */
Mcu_PllStatusType Mcu_GetPllStatus(void);

/**
 * @brief   [SWS_Mcu_00158, SWS_Mcu_00005] Lấy nguyên nhân reset đã giải mã
 * (Service ID 0x05, Sync, Reentrant).
 * @return  Giá trị Mcu_ResetType nhất quán từ RCC->CSR đã chốt trong
 * Mcu_Init().
 */
Mcu_ResetType Mcu_GetResetReason(void);

/**
 * @brief   [SWS_Mcu_00159, SWS_Mcu_00006, SWS_Mcu_00135] Lấy giá trị thô thanh
 * ghi reset (Service ID 0x06, Sync, Reentrant).
 * @return  Giá trị RCC->CSR đã chốt hoặc MCU_RAW_RESET_UNINIT_VALUE
 * (0x02000000UL) trước Mcu_Init().
 */
Mcu_RawResetType Mcu_GetResetRawValue(void);

#if (MCU_PERFORM_RESET_API == STD_ON)
/**
 * @brief   [SWS_Mcu_00160, SWS_Mcu_00143] Thực hiện reset phần cứng vi điều
 * khiển (Service ID 0x07, Sync, Non Reentrant).
 */
void Mcu_PerformReset(void);
#endif /* (MCU_PERFORM_RESET_API == STD_ON) */

/**
 * @brief   [SWS_Mcu_00161, SWS_Mcu_00147] Chuyển chế độ nguồn RUN/SLEEP/STOP
 * (Service ID 0x08, Sync, Reentrant).
 * @param   McuMode Mã chế độ nguồn cấu hình.
 */
void Mcu_SetMode(Mcu_ModeType McuMode);

#if (MCU_VERSION_INFO_API == STD_ON)
/**
 * @brief   [SWS_Mcu_00162] Đọc thông tin phiên bản module (Service ID 0x09,
 * Sync, Reentrant).
 * @param   versioninfo Con trỏ đích (không được NULL_PTR).
 */
void Mcu_GetVersionInfo(Std_VersionInfoType *versioninfo);
#endif /* (MCU_VERSION_INFO_API == STD_ON) */

#if (MCU_GET_RAM_STATE_API == STD_ON)
/**
 * @brief   [SWS_Mcu_00207, SWS_Mcu_00208] Đọc trạng thái toàn vẹn RAM (Service
 * ID 0x0A, Sync, Reentrant).
 * @return  MCU_RAMSTATE_VALID hoặc MCU_RAMSTATE_INVALID.
 */
Mcu_RamStateType Mcu_GetRamState(void);
#endif /* (MCU_GET_RAM_STATE_API == STD_ON) */

#define MCU_STOP_SEC_CODE
#include "Mcu_MemMap.h"

#endif /* MCU_H */