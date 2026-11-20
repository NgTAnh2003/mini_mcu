/**
 * @file Dio_Cfg.h
 * @brief Tên cổng, tên chân và tùy chọn biên dịch cho Dio. Kênh đèn phải chỉ đúng chân đã được cấu hình trong Port.
 */
#ifndef DIO_CFG_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define DIO_CFG_H

#include "Std_Types.h"

/*==============================================================================
 * PRE-COMPILE CONFIGURATION SWITCHES
 *============================================================================*/
/** @brief Bật báo lỗi phát triển qua Det. */
#define DIO_DEV_ERROR_DETECT            STD_ON
/** @brief Bật hoặc tắt việc biên dịch hàm Dio_FlipChannel. */
#define DIO_FLIP_CHANNEL_API            STD_ON
/** @brief Bật hoặc tắt việc biên dịch hàm Dio_VersionInfo. */
#define DIO_VERSION_INFO_API            STD_ON

/*==============================================================================
 * SYMBOLIC NAMES FOR DIO PORTS
 *============================================================================*/
/** @brief Chỉ số của cổng A trong bảng phần cứng. */
#define DioConf_DioPort_PORTA           (0u)
/** @brief Chỉ số của cổng B trong bảng phần cứng. */
#define DioConf_DioPort_PORTB           (1u)
/** @brief Chỉ số của cổng C trong bảng phần cứng. */
#define DioConf_DioPort_PORTC           (2u)
/** @brief Chỉ số của cổng D trong bảng phần cứng. */
#define DioConf_DioPort_PORTD           (3u)

/*==============================================================================
 * SYMBOLIC NAMES FOR DIO CHANNELS
 * PC13 = (2 * 16) + 13 = 45
 *============================================================================*/
/** @brief Chân đèn tích hợp: cổng C (2), chân 13, mã 2 × 16 + 13. */
#define DioConf_DioChannel_LED_PC13     (45u)

#endif /* DIO_CFG_H */
