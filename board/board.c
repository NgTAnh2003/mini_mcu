/**
 * @file board.c
 * @brief Ánh xạ trạng thái đèn sang mức điện trên chân 13 cổng C và chọn nguồn xung nhịp cho chip.
 */
#include "board.h"
#include "Dio.h"
#include "Mcu.h"
#include "Port.h"

bool Board_Init(void)
{
    Port_Init(&Port_Config);
    Mcu_Init(&Mcu_Config);

    /* E_OK bằng 0; main cần true khi chip đã chuyển sang thạch anh ngoài. */
    return Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_8MHz) == E_OK;
}

void Board_SetLed(bool on)
{
    Dio_WriteChannel(DioConf_DioChannel_LED_PC13, on ? STD_LOW : STD_HIGH);
}
