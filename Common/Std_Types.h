/**
 * @file Std_Types.h
 * @brief Quy ước dùng chung cho mức điện (STD_LOW/HIGH), tùy chọn biên dịch (STD_OFF/ON), kết quả (E_OK/E_NOT_OK) và thông tin phiên bản.
 */
#ifndef STD_TYPES_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define STD_TYPES_H

#include "Platform_Types.h"
#include "Compiler.h"

/*==============================================================================
 * STANDARD SYMBOLS (SWS_StandardTypes R4.4.0)
 *============================================================================*/
/* Cho phép dùng chung với header hệ điều hành đã khai báo StatusType và E_OK. */
#ifndef STATUSTYPEDEFINED
    /** @brief Đánh dấu kiểu trạng thái đã được khai báo. */
    #define STATUSTYPEDEFINED
    /** @brief Thao tác thành công. */
    #define E_OK            (0x00u)
    typedef unsigned char   StatusType; /**< Kiểu mã trạng thái tương thích OSEK. */
#endif

/** @brief Thao tác thất bại. */
#define E_NOT_OK            (0x01u)

/** @brief Mức logic cao trên chân số. */
#define STD_HIGH            (0x01u)     /* Physical state 3.3V / 5V */
/** @brief Mức logic thấp trên chân số. */
#define STD_LOW             (0x00u)     /* Physical state 0V */

/** @brief Trạng thái logic đang hoạt động. */
#define STD_ACTIVE          (0x01u)     /* Logical state active */
/** @brief Trạng thái logic nghỉ. */
#define STD_IDLE            (0x00u)     /* Logical state idle */

/** @brief Bật tùy chọn biên dịch. */
#define STD_ON              (0x01u)
/** @brief Tắt tùy chọn biên dịch. */
#define STD_OFF             (0x00u)

/*==============================================================================
 * STANDARD DATA TYPES
 *============================================================================*/
typedef uint8 Std_ReturnType; /**< Kiểu trả kết quả E_OK hoặc E_NOT_OK. */

/** @brief Thông tin nhận diện và phiên bản phần mềm. */
typedef struct
{
    uint16 vendorID; /**< Mã nhà cung cấp driver. */
    uint16 moduleID; /**< Mã nhận diện module. */
    uint8  sw_major_version; /**< Thành phần phiên bản chính. */
    uint8  sw_minor_version; /**< Thành phần phiên bản phụ. */
    uint8  sw_patch_version; /**< Số bản vá. */
} Std_VersionInfoType;

#endif /* STD_TYPES_H */
