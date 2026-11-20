/**
 * @file    Mcu_Cfg.h
 * @brief   Cấu hình biên dịch (Pre-Compile) và tên tượng trưng cho module
 * Mcal/Mcu.
 * @details Đây là một tập con gọn nhẹ của MCU Driver xây dựng cho mục đích học
 * tập AUTOSAR Classic trên STM32F103 (Blue Pill 8 MHz), không phải triển khai
 *          tuân thủ đầy đủ toàn bộ đặc tả AUTOSAR.
 *          Các macro có tiền tố MCU_CFG_ hoặc chính sách giới hạn vòng chờ dưới
 * đây là quy ước riêng của dự án học tập này.
 */

#ifndef MCU_CFG_H
#define MCU_CFG_H

#include "Std_Types.h"

/**
 * @brief   Bật hoặc tắt tính năng báo lỗi phát triển qua dịch vụ Det.
 * @details Đặt thành STD_ON để gọi Det_ReportError() khi phát hiện lỗi lập
 * trình (con trỏ rỗng, gọi API trước khi khởi tạo, sai mã cấu hình). Khi đặt
 * thành STD_OFF, driver không gọi Det nhưng vẫn kiểm tra đầu vào và thoát an
 * toàn. Tên công tắc tham chiếu khái niệm McuDevErrorDetect trong AUTOSAR SWS
 * MCU Driver.
 */
#define MCU_DEV_ERROR_DETECT (STD_ON)

/**
 * @brief   Số lượt kiểm tra thanh ghi tối đa khi chờ cờ ổn định nguồn xung
 * (HSERDY/HSIRDY).
 * @details [Quy ước riêng của dự án] Giá trị này là SỐ LƯỢT ĐỌC THANH GHI tối
 * đa trong vòng lặp chờ phần cứng, KHÔNG phải thời gian tính bằng micro giây
 * (us). Thời gian thực tế phụ thuộc vào tần số CPU hiện tại và mức tối ưu trình
 * biên dịch.
 */
#define MCU_CLOCK_READY_MAX_CHECKS (100000UL)

/**
 * @brief   Số lượt kiểm tra thanh ghi tối đa khi chờ phần cứng xác nhận chuyển
 * xung (SWS).
 * @details [Quy ước riêng của dự án] Giá trị này là SỐ LƯỢT ĐỌC THANH GHI tối
 * đa khi kiểm tra trường SWS trong RCC->CFGR, KHÔNG phải thời gian tính bằng
 * micro giây (us).
 */
#define MCU_CLOCK_SWITCH_MAX_CHECKS (100000UL)

/**
 * @brief   Tổng số mục cấu hình xung có trong bảng cấu hình Mcu_Config.
 * @details [Quy ước riêng của dự án] Hiện tại bảng cấu hình có 1 mục duy nhất
 *          dành cho thạch anh ngoài 8 MHz nối trực tiếp.
 */
#define MCU_CLOCK_SETTING_COUNT ((uint8)1U)

/**
 * @brief   Mã chọn cấu hình xung hệ thống: dùng trực tiếp thạch anh ngoài 8 MHz
 * (không PLL).
 * @details Quy ước đặt tên theo mẫu McuConf_McuClockSettingConfig_<Ten> của
 * AUTOSAR. LƯU Ý QUAN TRỌNG: Giá trị 0U là MÃ CHỌN (chỉ số mục trong bảng cấu
 * hình), tuyệt đối KHÔNG phải giá trị tần số 8000000 Hz.
 */
#define McuConf_McuClockSettingConfig_HSE_8MHz ((uint8)0U)

/**
 * @brief   Giá trị tần số mục tiêu (tính bằng Hz) của cấu hình thạch anh ngoài
 * 8 MHz.
 * @details [Quy ước riêng của dự án] Hằng số mô tả tần số vật lý 8.000.000 Hz
 * của thạch anh trên bo Blue Pill, tách biệt rõ với mã chọn cấu hình (0U).
 */
#define MCU_CFG_HSE_TARGET_FREQ_HZ (8000000UL)

#endif /* MCU_CFG_H */