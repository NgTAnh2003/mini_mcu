/**
 * @file Det.c
 * @brief Lưu lỗi gần nhất và bộ đếm trong RAM để xem bằng trình gỡ lỗi. Mỗi lần báo lỗi ghi đè chi tiết của lần trước.
 */
#include "Det.h"

/*==============================================================================
 * INTERNAL VARIABLES
 *============================================================================*/
/* volatile giữ các lần ghi để quan sát bằng debugger; cập nhật cả bản ghi gồm nhiều thao tác. */
static volatile Det_ErrorRecordType Det_LastError = {
    0u, /* ModuleId */
    0u, /* InstanceId */
    0u, /* ApiId */
    0u, /* ErrorId */
    0u  /* ErrorCount */
};

/*==============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/
Std_ReturnType Det_ReportError(
    uint16 ModuleId,
    uint8  InstanceId,
    uint8  ApiId,
    uint8  ErrorId
)
{
    /* Bốn trường đầu mô tả cùng một lần lỗi; bộ đếm tiếp tục tăng qua các lần ghi đè. */
    Det_LastError.ModuleId   = ModuleId;
    Det_LastError.InstanceId = InstanceId;
    Det_LastError.ApiId      = ApiId;
    Det_LastError.ErrorId    = ErrorId;
    Det_LastError.ErrorCount++;

    /*
     * Dat breakpoint tai day khi debug (GDB / STM32CubeIDE)
     * de bat loi ngay lap tuc khi MCAL phat hien tham so khong hop le.
     */
    return E_OK;
}
