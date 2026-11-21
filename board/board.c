/**
 * @file board.c
 * @brief Ánh xạ trạng thái đèn sang mức điện trên chân 13 cổng C và chọn nguồn xung nhịp cho chip.
 */
#include "board.h"
#include "Dio.h"
#include "Mcu.h"
#include "Port.h"
#include "Uart.h"
#include <string.h>

bool Board_Init(void)
{
    Port_Init(&Port_Config);
    Mcu_Init(&Mcu_Config);

    /* E_OK bằng 0; main cần true khi chip đã chuyển sang thạch anh ngoài. */
    return Mcu_InitClock(McuConf_McuClockSettingConfig_HSE_8MHz) == E_OK;
}

bool Board_SendStartupMessage(void)
{
    /* Hai nguồn xung của bo đều có tần số danh định 8 MHz.
     * Cấu hình khởi động giữ bộ chia AHB và APB2 bằng 1. */
    static const Uart_PortConfigType config = {
        .BusClockHz = 8000000u,
        .BaudRate = 115200u,
        .DataBits = UART_DATA_BITS_8,
        .Parity = UART_PARITY_NONE,
        .StopBits = UART_STOP_BITS_1,
        .FlowControl = UART_FLOW_NONE,
        .TxMode = UART_XFER_POLLING,
        .RxMode = UART_XFER_DISABLED
    };
    static const char *const status_lines[] = {
        "SYSCLK: HSI (SWS=00)\r\n", "SYSCLK: HSE (SWS=01)\r\n",
        "SYSCLK: PLL (SWS=10)\r\n", "SYSCLK: reserved (SWS=11)\r\n"
    };
    static const uint8 message[] = "mini_mcu ready\r\n";
    const char *status = status_lines[Mcu_GetClockSourceStatus()];

    if (Uart_Init(UART_PORT_1, &config) != UART_STATUS_OK) {
        return false;
    }

    /* Bỏ byte kết thúc chuỗi C. Giới hạn chờ tính bằng lượt kiểm tra mỗi byte. */
    if (Uart_TransmitPolling(UART_PORT_1, message, sizeof(message) - 1u,
                             NULL_PTR, UART_TIMEOUT_LOOPS(100000u)) != UART_STATUS_OK) {
        return false;
    }
    return Uart_TransmitPolling(UART_PORT_1, (const uint8 *)status, strlen(status),
                                NULL_PTR, UART_TIMEOUT_LOOPS(100000u)) == UART_STATUS_OK;
}

void Board_SetLed(bool on)
{
    Dio_WriteChannel(DioConf_DioChannel_LED_PC13, on ? STD_LOW : STD_HIGH);
}
