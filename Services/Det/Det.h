/**
 * @file Det.h
 * @brief Giao diện báo lỗi trong quá trình phát triển. Mã module và mã hàm cho biết lỗi phát sinh từ đâu.
 */
#ifndef DET_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define DET_H

#include "Std_Types.h"

/*==============================================================================
 * MODULE IDENTIFICATION
 *============================================================================*/
/** @brief Mã nhà cung cấp của Det. */
#define DET_VENDOR_ID               (0x0001u)
/** @brief Mã nhận diện module của Det. */
#define DET_MODULE_ID               (15u)
/** @brief Mã thực thể của module của Det. */
#define DET_INSTANCE_ID             (0u)

/*==============================================================================
 * DEBUG DATA STRUCTURE (Read-only inspection via Debugger)
 *============================================================================*/
/** @brief Lỗi gần nhất cùng bộ đếm cộng dồn trong RAM. */
typedef struct
{
    uint16 ModuleId; /**< Mã module phát hiện lỗi gần nhất. */
    uint8  InstanceId; /**< Số nhận diện thực thể báo lỗi. */
    uint8  ApiId; /**< Mã hàm phát hiện lỗi trong module. */
    uint8  ErrorId; /**< Mã nguyên nhân do module báo về. */
    uint32 ErrorCount; /**< Tổng số lần báo lỗi kể từ khi khởi động. */
} Det_ErrorRecordType;

/*==============================================================================
 * PUBLIC API PROTOTYPES (SWS_DefaultErrorTracer R4.4.0)
 *============================================================================*/
/**
 * @brief Ghi thông tin lỗi gần nhất và tăng bộ đếm để kiểm tra bằng trình gỡ lỗi.
 * @param[in] ModuleId Mã module phát hiện lỗi.
 * @param[in] InstanceId Số nhận diện thực thể của module.
 * @param[in] ApiId Mã hàm phát hiện lỗi.
 * @param[in] ErrorId Mã nguyên nhân do module quy định.
 * @return E_OK sau khi ghi bản báo lỗi.
 */
Std_ReturnType Det_ReportError(
    uint16 ModuleId,
    uint8  InstanceId,
    uint8  ApiId,
    uint8  ErrorId
);

#endif /* DET_H */
