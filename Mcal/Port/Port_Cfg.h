/**
 * @file Port_Cfg.h
 * @brief Chọn chức năng Port khi biên dịch và đặt tên cho các chân. Mã chân bằng chỉ số cổng nhân 16 cộng số chân.
 */
#ifndef PORT_CFG_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define PORT_CFG_H

#include "Std_Types.h"

/*==============================================================================
 * PRE-COMPILE CONFIGURATION SWITCHES
 *============================================================================*/
/** @brief Bật báo lỗi phát triển qua Det. */
#define PORT_DEV_ERROR_DETECT           STD_ON
/** @brief Bật hoặc tắt việc biên dịch hàm Port_SetPinDirection. */
#define PORT_SET_PIN_DIRECTION_API      STD_ON
/** @brief Bật hoặc tắt việc biên dịch hàm Port_SetPinMode. */
#define PORT_SET_PIN_MODE_API           STD_ON
/** @brief Bật hoặc tắt việc biên dịch hàm Port_VersionInfo. */
#define PORT_VERSION_INFO_API           STD_ON

/*==============================================================================
 * NUMBER OF CONFIGURED PINS IN PORT_CONFIG
 *============================================================================*/
/** @brief Số mục cấu hình chân và dung lượng bảng trạng thái tương ứng. */
#define PORT_NUM_OF_CONFIGURED_PINS     (1u)

/*==============================================================================
 * SYMBOLIC NAMES FOR PORT PINS
 * Formula: (PortIndex * 16) + PinIndex
 * PortA=0, PortB=1, PortC=2, PortD=3 -> PC13 = (2 * 16) + 13 = 45
 *============================================================================*/
/** @brief Chân đèn tích hợp: cổng C (2), chân 13, mã 2 × 16 + 13. */
#define PortConf_PortPin_LED_PC13       (45u)

#endif /* PORT_CFG_H */
