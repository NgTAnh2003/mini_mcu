/**
 * @file    Mcu_PBcfg.c
 * @brief   Định nghĩa dữ liệu cấu hình hằng (Post-Build style) cho module
 * Mcal/Mcu.
 * @details Chứa bảng cấu hình nguồn xung thạch anh ngoài 8 MHz cho hai bo Blue
 * Pill (STM32F103C6T6 và STM32F103C8T6). Toàn bộ dữ liệu được khai báo const
 * nên có thời gian sống suốt chương trình (nằm trong vùng Flash/.rodata). File
 * này KHÔNG include "stm32f1xx.h".
 */

#include "Mcu.h"

/**
 * @brief   Mảng hằng chứa danh sách các mục cấu hình xung hệ thống của dự án.
 * @details Chỉ số phần tử trong mảng phải khớp với trường ClockId và macro
 * tượng trưng trong Mcu_Cfg.h (chỉ số 0 ứng với
 * McuConf_McuClockSettingConfig_HSE_8MHz).
 */
static const Mcu_ClockSettingConfigType
    Mcu_ClockSettings[MCU_CLOCK_SETTING_COUNT] = {{
        .ClockId =
            McuConf_McuClockSettingConfig_HSE_8MHz, /* Mã chọn cấu hình = 0U */
        .ClockSource = MCU_CLOCK_SOURCE_HSE, /* Chọn thạch anh ngoài trực tiếp,
                                                không PLL */
        .EnableHseBypass =
            FALSE, /* Blue Pill gắn thạch anh 8 MHz thật, không dùng bypass */
        .TargetFrequencyHz =
            MCU_CFG_HSE_TARGET_FREQ_HZ /* Tần số mục tiêu = 8000000UL Hz */
    }};

/**
 * @brief   Bảng cấu hình tổng của module Mcu được truyền vào
 * Mcu_Init(&Mcu_Config).
 */
const Mcu_ConfigType Mcu_Config = {.ClockSettings = Mcu_ClockSettings,
                                   .ClockSettingCount =
                                       MCU_CLOCK_SETTING_COUNT};