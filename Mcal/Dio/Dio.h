/**
 * @file Dio.h
 * @brief Giao diện đọc và ghi mức logic của một chân, một nhóm chân hoặc cả cổng.
 */
#ifndef DIO_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define DIO_H

#include "Std_Types.h"
#include "Dio_Cfg.h"

/*==============================================================================
 * MODULE IDENTIFICATION & VERSION INFO
 *============================================================================*/
/** @brief Mã nhà cung cấp của Dio. */
#define DIO_VENDOR_ID                       (0x0001u)
/** @brief Mã nhận diện module của Dio. */
#define DIO_MODULE_ID                       (120u)
/** @brief Mã thực thể của module của Dio. */
#define DIO_INSTANCE_ID                     (0u)

/** @brief Phiên bản phần mềm, thành phần chính. */
#define DIO_SW_MAJOR_VERSION                (1u)
/** @brief Phiên bản phần mềm, thành phần phụ. */
#define DIO_SW_MINOR_VERSION                (0u)
/** @brief Phiên bản phần mềm, thành phần bản vá. */
#define DIO_SW_PATCH_VERSION                (0u)

/*==============================================================================
 * DEVELOPMENT ERROR CODES (SWS_Dio_00175)
 *============================================================================*/
/** @brief Mã chân không hợp lệ. */
#define DIO_E_PARAM_INVALID_CHANNEL_ID      (0x0Au)
/** @brief Chỉ số cổng ngoài phạm vi. */
#define DIO_E_PARAM_INVALID_PORT_ID         (0x14u)
/** @brief Tham số nhóm chân không hợp lệ. */
#define DIO_E_PARAM_INVALID_GROUP           (0x1Fu)
/** @brief Truyền con trỏ rỗng. */
#define DIO_E_PARAM_POINTER                 (0x20u)

/*==============================================================================
 * SERVICE / API IDS (SWS_DioDriver R4.4.0)
 *============================================================================*/
/** @brief Mã hàm dùng khi báo lỗi từ Dio_ReadChannel. */
#define DIO_READ_CHANNEL_SID                (0x00u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_WriteChannel. */
#define DIO_WRITE_CHANNEL_SID               (0x01u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_ReadPort. */
#define DIO_READ_PORT_SID                   (0x02u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_WritePort. */
#define DIO_WRITE_PORT_SID                  (0x03u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_ReadChannelGroup. */
#define DIO_READ_CHANNEL_GROUP_SID          (0x04u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_WriteChannelGroup. */
#define DIO_WRITE_CHANNEL_GROUP_SID         (0x05u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_FlipChannel. */
#define DIO_FLIP_CHANNEL_SID                (0x11u)
/** @brief Mã hàm dùng khi báo lỗi từ Dio_GetVersionInfo. */
#define DIO_GET_VERSION_INFO_SID            (0x12u)

/*==============================================================================
 * STANDARD AUTOSAR DATA TYPES (SWS_DioDriver Section 8.2)
 *============================================================================*/
typedef uint16 Dio_ChannelType; /**< Mã kênh số bằng chỉ số cổng nhân 16 cộng số chân. */
typedef uint8  Dio_PortType; /**< Chỉ số cổng GPIO. */
typedef uint8  Dio_LevelType; /**< Mức logic của một chân. */
typedef uint16 Dio_PortLevelType; /**< Mẫu 16 bit ứng với 16 chân của một cổng. */

/** @brief Vị trí của một nhóm bit trong cùng cổng. */
typedef struct
{
    uint16       mask; /**< Mỗi bit 1 chọn một chân thuộc nhóm. */
    uint8        offset; /**< Số vị trí dịch để đưa bit thấp nhất của nhóm về bit 0. */
    Dio_PortType port; /**< Chỉ số cổng chứa tất cả các chân trong nhóm. */
} Dio_ChannelGroupType;

/*==============================================================================
 * EXTERNAL LINK-TIME CHANNEL GROUP DECLARATIONS (Neu co)
 *============================================================================*/
extern const Dio_ChannelGroupType DioConf_DioChannelGroup_PORTC_LED;

/*==============================================================================
 * PUBLIC API PROTOTYPES (SWS_DioDriver Section 8.3)
 *============================================================================*/
/**
 * @brief Đọc mức điện hiện tại tại một chân.
 * @param[in] ChannelId Mã chân cần đọc.
 * @return STD_HIGH hoặc STD_LOW. Mã chân không hợp lệ cũng trả về STD_LOW.
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId);
/**
 * @brief Ghi mức logic nếu chân đã được cấu hình làm đầu ra.
 * @param[in] ChannelId Mã chân cần điều khiển.
 * @param[in] Level Mức điện STD_HIGH hoặc STD_LOW.
 */
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);

/**
 * @brief Đọc các bit đầu vào của cổng trong một lần.
 * @param[in] PortId Chỉ số cổng cần đọc.
 * @return Mẫu 16 bit, hoặc 0 nếu cổng không hợp lệ.
 */
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);
/**
 * @brief Ghi cả chốt đầu ra của cổng; trên STM32F1 thao tác này cũng có thể đổi hướng kéo của chân đầu vào có điện trở kéo.
 * @param[in] PortId Chỉ số cổng.
 * @param[in] Level Mẫu 16 bit, mỗi bit ứng với một chân.
 */
void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);

/**
 * @brief Đọc các chân thuộc mặt nạ rồi dịch kết quả về vị trí bit của nhóm.
 * @param[in] ChannelGroupIdPtr Cấu hình cổng, mặt nạ và độ dịch của nhóm.
 * @return Giá trị nhóm sau khi dịch, hoặc 0 khi tham số bị từ chối.
 */
Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr);
/**
 * @brief Cập nhật các bit trong mặt nạ bằng một lần ghi thanh ghi.
 * @param[in] ChannelGroupIdPtr Cấu hình nhóm cần ghi.
 * @param[in] Level Giá trị nhóm trước khi dịch vào vị trí của các chân.
 */
void Dio_WriteChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr, Dio_PortLevelType Level);

#if (DIO_VERSION_INFO_API == STD_ON)
/**
 * @brief Đọc mã nhận diện và phiên bản của driver Dio.
 * @param[out] VersionInfo Vùng nhớ nhận thông tin phiên bản.
 */
void Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo);
#endif

#if (DIO_FLIP_CHANNEL_API == STD_ON)
/**
 * @brief Đảo chốt đầu ra nếu chân là đầu ra; với đầu vào chỉ đọc mức hiện tại.
 * @param[in] ChannelId Mã chân cần xử lý.
 * @return Mức mới của đầu ra, mức đọc ở đầu vào hoặc STD_LOW khi mã chân sai.
 */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId);
#endif

#endif /* DIO_H */
