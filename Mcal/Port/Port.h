/**
 * @file Port.h
 * @brief Giao diện cấu hình chân vi điều khiển. Mỗi mục trong bảng mô tả hướng, chế độ điện và các thay đổi cho phép khi chạy.
 */
#ifndef PORT_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define PORT_H

#include "Std_Types.h"
#include "Port_Cfg.h"

/*==============================================================================
 * MODULE IDENTIFICATION & VERSION INFO
 *============================================================================*/
/** @brief Mã nhà cung cấp của Port. */
#define PORT_VENDOR_ID                      (0x0001u)
/** @brief Mã nhận diện module của Port. */
#define PORT_MODULE_ID                      (124u)
/** @brief Mã thực thể của module của Port. */
#define PORT_INSTANCE_ID                    (0u)

/** @brief Phiên bản phần mềm, thành phần chính. */
#define PORT_SW_MAJOR_VERSION               (1u)
/** @brief Phiên bản phần mềm, thành phần phụ. */
#define PORT_SW_MINOR_VERSION               (0u)
/** @brief Phiên bản phần mềm, thành phần bản vá. */
#define PORT_SW_PATCH_VERSION               (0u)

/*==============================================================================
 * DEVELOPMENT ERROR CODES (SWS_Port_00051)
 *============================================================================*/
/** @brief Chân không hợp lệ hoặc không có trong bảng cấu hình. */
#define PORT_E_PARAM_PIN                    (0x0Au)
/** @brief Cấu hình khóa hướng của chân. */
#define PORT_E_DIRECTION_UNCHANGEABLE       (0x0Bu)
/** @brief Bảng khởi tạo thiếu hoặc số chân ngoài giới hạn. */
#define PORT_E_INIT_FAILED                  (0x0Cu)
/** @brief Chế độ chân ngoài phạm vi hỗ trợ. */
#define PORT_E_PARAM_INVALID_MODE           (0x0Du)
/** @brief Cấu hình khóa chế độ của chân. */
#define PORT_E_MODE_UNCHANGEABLE            (0x0Eu)
/** @brief Gọi hàm trước khi khởi tạo driver. */
#define PORT_E_UNINIT                       (0x0Fu)
/** @brief Truyền con trỏ rỗng. */
#define PORT_E_PARAM_POINTER                (0x10u)

/*==============================================================================
 * SERVICE / API IDS (SWS_PortDriver R4.4.0)
 *============================================================================*/
/** @brief Mã hàm dùng khi báo lỗi từ Port_Init. */
#define PORT_INIT_SID                       (0x00u)
/** @brief Mã hàm dùng khi báo lỗi từ Port_SetPinDirection. */
#define PORT_SET_PIN_DIRECTION_SID          (0x01u)
/** @brief Mã hàm dùng khi báo lỗi từ Port_RefreshPortDirection. */
#define PORT_REFRESH_PORT_DIRECTION_SID     (0x02u)
/** @brief Mã hàm dùng khi báo lỗi từ Port_GetVersionInfo. */
#define PORT_GET_VERSION_INFO_SID           (0x03u)
/** @brief Mã hàm dùng khi báo lỗi từ Port_SetPinMode. */
#define PORT_SET_PIN_MODE_SID               (0x04u)

/*==============================================================================
 * PIN MODE DEFINITIONS
 *============================================================================*/
/** @brief Điều khiển chân như đầu vào hoặc đầu ra số. */
#define PORT_PIN_MODE_DIO                   ((Port_PinModeType)0x00u)
/** @brief Đặt chân ở chế độ đầu vào tương tự. */
#define PORT_PIN_MODE_ADC                   ((Port_PinModeType)0x01u)
/** @brief Dùng chân cho chức năng của ngoại vi. */
#define PORT_PIN_MODE_ALT                   ((Port_PinModeType)0x02u)

/*==============================================================================
 * STANDARD AUTOSAR DATA TYPES (SWS_PortDriver Section 8.2)
 *============================================================================*/
typedef uint16 Port_PinType; /**< Mã cổng và chân dùng trong bảng cấu hình. */

/** @brief Hướng truyền của chân. */
typedef enum
{
    PORT_PIN_IN  = 0x00u, /**< Cấu hình chân làm đầu vào. */
    PORT_PIN_OUT = 0x01u /**< Cấu hình chân làm đầu ra. */
} Port_PinDirectionType;

typedef uint8 Port_PinModeType; /**< Chức năng được chọn cho chân. */

/*==============================================================================
 * IMPLEMENTATION-SPECIFIC CONFIGURATION TYPES FOR STM32F103
 *============================================================================*/
/** @brief Mức ban đầu của chốt đầu ra. */
typedef enum
{
    PORT_PIN_LEVEL_LOW  = STD_LOW, /**< Khởi tạo đầu ra ở mức thấp. */
    PORT_PIN_LEVEL_HIGH = STD_HIGH /**< Khởi tạo đầu ra ở mức cao. */
} Port_PinLevelType;

/** @brief Lựa chọn điện trở kéo nội. */
typedef enum
{
    PORT_PIN_PULL_NONE = 0x00u, /**< Đầu vào thả nổi, tắt điện trở kéo nội. */
    PORT_PIN_PULL_UP   = 0x01u, /**< Kéo yếu đầu vào lên nguồn. */
    PORT_PIN_PULL_DOWN = 0x02u /**< Kéo yếu đầu vào xuống đất. */
} Port_PinPullType;

/** @brief Kiểu mạch lái đầu ra. */
typedef enum
{
    PORT_PIN_OUT_PUSHPULL  = 0x00u, /**< Chủ động lái cả mức cao và thấp. */
    PORT_PIN_OUT_OPENDRAIN = 0x01u /**< Chỉ chủ động kéo thấp, cần đường kéo lên để đạt mức cao. */
} Port_PinOutputType;

/** @brief Mã tốc độ ghi vào hai bit MODE của STM32F1. */
typedef enum
{
    PORT_PIN_SPEED_2MHZ  = 0x02u, /**< Chọn chế độ tốc độ đầu ra 2 MHz. */
    PORT_PIN_SPEED_10MHZ = 0x01u, /**< Chọn chế độ tốc độ đầu ra 10 MHz. */
    PORT_PIN_SPEED_50MHZ = 0x03u /**< Chọn chế độ tốc độ đầu ra 50 MHz. */
} Port_PinSpeedType;

/** @brief Cấu hình điện và quyền thay đổi của một chân. */
typedef struct
{
    Port_PinType          PinId; /**< Mã chân bằng chỉ số cổng nhân 16 cộng số chân. */
    Port_PinDirectionType Direction; /**< Hướng ban đầu của chân. */
    Port_PinModeType      Mode; /**< Chức năng ban đầu của chân. */
    Port_PinLevelType     InitialLevel; /**< Mức nạp vào chốt trước khi bật đầu ra số. */
    Port_PinPullType      Pull; /**< Điện trở kéo nội dùng ở chế độ đầu vào số. */
    Port_PinOutputType    OutputType; /**< Cách mạch đầu ra lái chân. */
    Port_PinSpeedType     Speed; /**< Chế độ tốc độ điện của đầu ra. */
    boolean               DirectionChangeable; /**< TRUE cho phép đổi hướng sau khi khởi tạo. */
    boolean               ModeChangeable; /**< TRUE cho phép đổi chức năng sau khi khởi tạo. */
} Port_PinConfigType;

/** @brief Bảng chân truyền cho Port_Init. */
typedef struct
{
    const Port_PinConfigType *PinConfigs; /**< Địa chỉ phần tử đầu tiên của bảng cấu hình chân. */
    uint16                    NumOfPins; /**< Số phần tử hợp lệ trong bảng PinConfigs. */
} Port_ConfigType;

/*==============================================================================
 * EXTERNAL POST-BUILD CONFIGURATION DECLARATION
 *============================================================================*/
extern const Port_ConfigType Port_Config;

/*==============================================================================
 * PUBLIC API PROTOTYPES (SWS_PortDriver Section 8.3)
 *============================================================================*/
/**
 * @brief Áp dụng cấu hình lần lượt cho từng chân.
 * @param[in] ConfigPtr Địa chỉ bảng cấu hình và danh sách chân. Cả hai phải còn hiệu lực khi driver đang dùng.
 */
void Port_Init(const Port_ConfigType *ConfigPtr);

#if (PORT_SET_PIN_DIRECTION_API == STD_ON)
/**
 * @brief Đổi hướng chân đã khởi tạo và được phép đổi hướng.
 * @param[in] Pin Mã chân trong bảng cấu hình.
 * @param[in] Direction Hướng đầu vào hoặc đầu ra mới.
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);
#endif

/**
 * @brief Áp dụng lại hướng ban đầu cho các chân có hướng cố định sau khi khởi tạo.
 */
void Port_RefreshPortDirection(void);

#if (PORT_VERSION_INFO_API == STD_ON)
/**
 * @brief Đọc mã nhận diện và phiên bản của driver Port.
 * @param[out] versioninfo Vùng nhớ nhận thông tin phiên bản.
 */
void Port_GetVersionInfo(Std_VersionInfoType *versioninfo);
#endif

#if (PORT_SET_PIN_MODE_API == STD_ON)
/**
 * @brief Đổi chức năng chân đã khởi tạo và được phép đổi chế độ.
 * @param[in] Pin Mã chân trong bảng cấu hình.
 * @param[in] Mode Chế độ chân số, đầu vào tương tự hoặc chức năng ngoại vi.
 */
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode);
#endif

#endif /* PORT_H */
