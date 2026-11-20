/**
 * @file board.h
 * @brief Giao diện của bo mạch để ứng dụng khởi tạo bo và yêu cầu bật/tắt đèn.
 */
#ifndef BOARD_H
/** @brief Ngăn nạp lặp nội dung header trong cùng đơn vị biên dịch. */
#define BOARD_H

#include <stdbool.h>

/**
 * @brief Chuẩn bị chân đèn rồi yêu cầu chuyển sang nguồn xung ngoài.
 * @return true khi chip xác nhận đã chuyển nguồn; false khi hết lượt chờ.
 */
bool Board_Init(void);
/**
 * @brief Điều khiển đèn tích hợp sau khi Board_Init đã cấu hình chân.
 * @param[in] on Bật đèn khi true, tắt khi false.
 */
void Board_SetLed(bool on);

#endif
