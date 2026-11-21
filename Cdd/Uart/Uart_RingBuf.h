/**
 * @file    Uart_RingBuf.h
 * @brief   [Nội bộ] Bộ đệm vòng SPSC (Single-Producer, Single-Consumer) độc lập
 * phần cứng.
 * @details
 * Quy tắc sở hữu chỉ số giữa vòng lặp chính và trình phục vụ ngắt (ISR):
 * - Chỉ số Head (vị trí ghi kế tiếp): CHỈ bên sản xuất (Producer) được cập
 * nhật.
 * - Chỉ số Tail (vị trí đọc kế tiếp): CHỈ bên tiêu thụ (Consumer) được cập
 * nhật.
 *   + Với hàng đợi RX: ISR là Producer (ghi Head), vòng lặp chính là Consumer
 * (ghi Tail).
 *   + Với hàng đợi TX: vòng lặp chính là Producer (ghi Head), ISR là Consumer
 * (ghi Tail).
 *
 * Quy ước 1 ô trống phân biệt đầy và rỗng:
 * - Rỗng khi Head == Tail.
 * - Đầy khi Next(Head) == Tail.
 * - Sức chứa hữu ích tối đa = Capacity - 1 phần tử (yêu cầu Capacity >= 2).
 * - Đánh đổi: tốn thêm 1 phần tử (1-2 byte) nhưng cho phép chọn mọi kích thước
 * tùy ý (không bắt buộc lũy thừa của 2) và không cần khóa ngắt toàn cục khi
 * đọc/ghi từng ô.
 */

#ifndef UART_RINGBUF_H
#define UART_RINGBUF_H

#include "Uart_Types.h"

/**
 * @brief Cấu trúc điều khiển một bộ đệm vòng SPSC.
 */
typedef struct {
  Uart_RingSlotType
      *Storage; /**< Con trỏ tới vùng nhớ tĩnh hoặc vùng nhớ do ứng dụng cấp */
  uint16 Capacity; /**< Tổng số ô trong Storage (sức chứa hữu ích = Capacity -
                      1) */
  volatile uint16 Head; /**< Chỉ số ghi tiếp theo — CHỈ Producer cập nhật */
  volatile uint16 Tail; /**< Chỉ số đọc tiếp theo — CHỈ Consumer cập nhật */
} Uart_RingBufType;

/**
 * @brief Khởi tạo bộ đệm vòng gắn với một vùng nhớ sẵn có.
 * @param Rb       Con trỏ cấu trúc điều khiển bộ đệm vòng.
 * @param Storage  Mảng phần tử nền (phải tồn tại trong suốt thời gian sử dụng).
 * @param Capacity Tổng số ô trong mảng Storage (phải >= 2U).
 * @return TRUE nếu khởi tạo thành công, FALSE nếu tham số không hợp lệ.
 */
boolean Uart_RingBuf_Init(Uart_RingBufType *Rb, Uart_RingSlotType *Storage,
                          uint16 Capacity);

/**
 * @brief Đặt lại chỉ số đọc/ghi về rỗng.
 * @warning Chỉ gọi khi cả Producer và Consumer đều đã dừng (hoặc trong vùng cấm
 * ngắt).
 * @param Rb Con trỏ bộ đệm vòng.
 */
void Uart_RingBuf_Reset(Uart_RingBufType *Rb);

/**
 * @brief Đẩy 1 phần tử vào cuối bộ đệm vòng (chỉ gọi từ Producer).
 * @param Rb   Con trỏ bộ đệm vòng.
 * @param Item Phần tử cần ghi (uint8 hoặc uint16 tùy UART_CFG_SUPPORT_9BIT).
 * @return TRUE nếu ghi thành công, FALSE nếu bộ đệm đã đầy (không ghi đè dữ
 * liệu cũ).
 */
boolean Uart_RingBuf_Push(Uart_RingBufType *Rb, Uart_RingSlotType Item);

/**
 * @brief Lấy 1 phần tử từ đầu bộ đệm vòng (chỉ gọi từ Consumer).
 * @param Rb      Con trỏ bộ đệm vòng.
 * @param ItemOut Con trỏ nhận phần tử đọc ra.
 * @return TRUE nếu đọc thành công, FALSE nếu bộ đệm đang rỗng.
 */
boolean Uart_RingBuf_Pop(Uart_RingBufType *Rb, Uart_RingSlotType *ItemOut);

/**
 * @brief Đếm số phần tử hiện đang nằm trong bộ đệm vòng.
 * @param Rb Con trỏ bộ đệm vòng.
 * @return Số phần tử sẵn có (0 .. Capacity - 1).
 */
uint16 Uart_RingBuf_GetCount(const Uart_RingBufType *Rb);

/**
 * @brief Đếm số ô trống còn có thể ghi thêm vào bộ đệm vòng.
 * @param Rb Con trỏ bộ đệm vòng.
 * @return Số ô trống (0 .. Capacity - 1).
 */
uint16 Uart_RingBuf_GetFree(const Uart_RingBufType *Rb);

/**
 * @brief Kiểm tra bộ đệm vòng có đang rỗng không.
 * @param Rb Con trỏ bộ đệm vòng.
 * @return TRUE nếu rỗng, FALSE nếu có ít nhất 1 phần tử.
 */
boolean Uart_RingBuf_IsEmpty(const Uart_RingBufType *Rb);

#endif /* UART_RINGBUF_H */