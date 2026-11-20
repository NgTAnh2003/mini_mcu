/**
 * @file Platform_Types.h
 * @brief Kiểu số dùng chung trên chip ARM Cortex-M3. uint16 chứa đúng 16 bit, còn uint16_least chỉ yêu cầu ít nhất 16 bit. Chip lưu byte thấp ở địa chỉ thấp.
 */
#ifndef PLATFORM_TYPES_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define PLATFORM_TYPES_H

#include <stdint.h>

/*==============================================================================
 * CPU & ENDIANNESS DEFINITIONS (ARM Cortex-M3 - STM32F103)
 *============================================================================*/
/** @brief Định danh nền tảng có từ máy 8 bit. */
#define CPU_TYPE_8          (8u)
/** @brief Định danh nền tảng có từ máy 16 bit. */
#define CPU_TYPE_16         (16u)
/** @brief Định danh nền tảng có từ máy 32 bit. */
#define CPU_TYPE_32         (32u)
/** @brief Định danh nền tảng có từ máy 64 bit. */
#define CPU_TYPE_64         (64u)

/** @brief Quy ước bit có trọng số cao nhất ở vị trí đầu. */
#define MSB_FIRST           (0u)
/** @brief Quy ước bit có trọng số thấp nhất ở vị trí đầu. */
#define LSB_FIRST           (1u)

/** @brief Byte cao nằm ở địa chỉ thấp. */
#define HIGH_BYTE_FIRST     (0u)
/** @brief Byte thấp nằm ở địa chỉ thấp. */
#define LOW_BYTE_FIRST      (1u)

/** @brief Độ rộng từ máy của nền tảng. */
#define CPU_TYPE            CPU_TYPE_32
/** @brief Quy ước đánh số bit của nền tảng. */
#define CPU_BIT_ORDER       LSB_FIRST
/** @brief Thứ tự byte trong bộ nhớ của nền tảng. */
#define CPU_BYTE_ORDER      LOW_BYTE_FIRST

/*==============================================================================
 * BOOLEAN SYMBOLS
 *============================================================================*/
#ifndef TRUE
    /** @brief Giá trị logic đúng. */
    #define TRUE            (1u)
#endif

#ifndef FALSE
    /** @brief Giá trị logic sai. */
    #define FALSE           (0u)
#endif

/*==============================================================================
 * STANDARD PLATFORM DATA TYPES (SWS_PlatformTypes R4.4.0)
 *============================================================================*/
typedef uint8_t             boolean; /**< Kiểu lưu TRUE hoặc FALSE trong một byte. */

typedef int8_t              sint8; /**< Số nguyên có dấu, rộng đúng 8 bit. */
typedef uint8_t             uint8; /**< Số nguyên không dấu, rộng đúng 8 bit. */
typedef int16_t             sint16; /**< Số nguyên có dấu, rộng đúng 16 bit. */
typedef uint16_t            uint16; /**< Số nguyên không dấu, rộng đúng 16 bit. */
typedef int32_t             sint32; /**< Số nguyên có dấu, rộng đúng 32 bit. */
typedef uint32_t            uint32; /**< Số nguyên không dấu, rộng đúng 32 bit. */
typedef int64_t             sint64; /**< Số nguyên có dấu, rộng đúng 64 bit. */
typedef uint64_t            uint64; /**< Số nguyên không dấu, rộng đúng 64 bit. */

/* Các kiểu hậu tố least bảo đảm độ rộng tối thiểu khi chuyển mã sang nền tảng khác. */
typedef uint_least8_t       uint8_least; /**< Số nguyên không dấu, rộng ít nhất 8 bit. */
typedef uint_least16_t      uint16_least; /**< Số nguyên không dấu, rộng ít nhất 16 bit. */
typedef uint_least32_t      uint32_least; /**< Số nguyên không dấu, rộng ít nhất 32 bit. */
typedef int_least8_t        sint8_least; /**< Số nguyên có dấu, rộng ít nhất 8 bit. */
typedef int_least16_t       sint16_least; /**< Số nguyên có dấu, rộng ít nhất 16 bit. */
typedef int_least32_t       sint32_least; /**< Số nguyên có dấu, rộng ít nhất 32 bit. */

typedef float               float32; /**< Số thực dấu phẩy động 32 bit trên nền tảng này. */
typedef double              float64; /**< Số thực dấu phẩy động 64 bit trên nền tảng này. */

#endif /* PLATFORM_TYPES_H */
