/**
 * @file    Std_Types.h
 * @brief   Stub kiểu dữ liệu chuẩn CHỈ dùng khi biên dịch test trên máy tính
 * (PC).
 * @note    KHÔNG dùng file này cho bản build STM32; firmware thật dùng
 * Common/Std_Types.h.
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint8_t boolean;
typedef uint8_t Std_ReturnType;

#ifndef TRUE
#define TRUE ((boolean)1U)
#endif

#ifndef FALSE
#define FALSE ((boolean)0U)
#endif

#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

#ifndef E_OK
#define E_OK ((Std_ReturnType)0x00U)
#endif

#ifndef E_NOT_OK
#define E_NOT_OK ((Std_ReturnType)0x01U)
#endif

#endif /* STD_TYPES_H */