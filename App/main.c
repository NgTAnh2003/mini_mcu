/**
 * @file main.c
 * @brief Chương trình nháy đèn. Ứng dụng quyết định lúc bật/tắt, còn module board biết cách điều khiển đèn trên bo.
 */
#include "board.h"

/** @brief Khởi tạo bo rồi lặp bật/tắt đèn cho tới khi chip bị reset. */
int main(void)
{
    bool hse_active = Board_Init();
    /* Chọn số lượt đếm theo kết quả khởi tạo nguồn xung; cả hai nhánh vẫn chạy vòng nháy đèn. */
    unsigned delay_count = hse_active ? 400000u : 100000u;
    bool led_on = true;

    for (;;) {
        Board_SetLed(led_on);
        for (volatile unsigned i = 0; i < delay_count; ++i) {
            /* Biến volatile buộc trình biên dịch giữ các lượt đếm. Đây là khoảng chờ ước lượng. */
        }
        led_on = !led_on;
    }
}
