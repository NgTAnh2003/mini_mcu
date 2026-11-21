/**
 * @file    Uart_Calc.h
 * @brief   [Nội bộ] Tính toán BRR chống tràn số và kiểm tra hợp lệ cấu hình
 * UART.
 * @details Module này hoàn toàn độc lập với thanh ghi STM32 để có thể biên dịch
 * và kiểm thử đơn vị trực tiếp trên máy tính (host unit test).
 */

#ifndef UART_CALC_H
#define UART_CALC_H

#include "Uart_Types.h"

/**
 * @brief Năng lực phần cứng trừu tượng của một cổng UART dùng cho kiểm tra cấu
 * hình.
 */
typedef struct {
  boolean Available; /**< TRUE nếu biến thể chip hiện tại thực sự có cổng này */
  boolean HasRtsCts; /**< TRUE nếu cổng và gói chân hỗ trợ điều khiển luồng
                        RTS/CTS */
  boolean HasDmaTx;      /**< TRUE nếu cổng có ánh xạ kênh DMA chiều TX */
  boolean HasDmaRx;      /**< TRUE nếu cổng có ánh xạ kênh DMA chiều RX */
  boolean DmaCompiledIn; /**< TRUE nếu UART_CFG_DMA_ENABLE == 1U */
  boolean Support9Bit;   /**< TRUE nếu UART_CFG_SUPPORT_9BIT == 1U */
  uint16 StaticTxSlots; /**< Tổng số ô mảng tĩnh TX mặc định (0 nếu không cấp
                           phát) */
  uint16 StaticRxSlots; /**< Tổng số ô mảng tĩnh RX mặc định (0 nếu không cấp
                           phát) */
} Uart_HwCapsType;

/**
 * @brief Kết quả sau khi kiểm tra hợp lệ cấu hình, sẵn sàng ghi vào thanh ghi
 * USART.
 */
typedef struct {
  uint16 BrrValue; /**< Giá trị thanh ghi USART_BRR đã làm tròn và kiểm biên */
  uint16
      DataMask; /**< Mặt nạ tách dữ liệu hữu ích (0x007F, 0x00FF hoặc 0x01FF) */
  boolean Use9BitFrame; /**< TRUE nếu cần đặt CR1.M = 1 (khung tổng 9 bit),
                           FALSE nếu CR1.M = 0 */
  boolean Use16BitDma; /**< TRUE nếu DMA cần độ rộng 16-bit (khi DataBits ==
                          UART_DATA_BITS_9) */
} Uart_ValidatedParamsType;

/**
 * @brief Tính giá trị thanh ghi USART_BRR cho STM32F1 (oversampling cố định
 * 16x).
 * @details Tránh hoàn toàn tràn số uint32 bằng phép chia và so sánh phần dư
 * thay vì cộng (BusClockHz + BaudRate / 2U). Kiểm tra biên phần cứng [0x0010,
 * 0xFFFF].
 * @param BusClockHz    Tần số xung bus thực tế của ngoại vi (Hz), phải > 0.
 * @param BaudRate      Tốc độ baud mong muốn (bps), phải > 0.
 * @param BrrOut        Con trỏ nhận giá trị BRR 16-bit.
 * @param ActualBaudOut Con trỏ tùy chọn nhận tốc độ baud thực tế đạt được
 * (bps).
 * @return UART_STATUS_OK, UART_STATUS_INVALID_ARG hoặc
 * UART_STATUS_BAUD_OUT_OF_RANGE.
 */
Uart_StatusType Uart_Calc_BaudBrr(uint32 BusClockHz, uint32 BaudRate,
                                  uint16 *BrrOut, uint32 *ActualBaudOut);

/**
 * @brief Kiểm tra tổ hợp (DataBits, Parity, StopBits) theo bảng khung phần cứng
 * STM32F1.
 * @param DataBits     Số bit dữ liệu hữu ích mong muốn.
 * @param Parity       Chế độ parity.
 * @param StopBits     Số stop bit.
 * @param Support9Bit  TRUE nếu bản build hỗ trợ 9 bit dữ liệu hữu ích.
 * @param DataMaskOut  Con trỏ nhận mặt nạ dữ liệu hữu ích (0x7F / 0xFF /
 * 0x1FF).
 * @param UseM9BitOut  Con trỏ nhận giá trị cần ghi vào bit CR1.M (TRUE = 1,
 * FALSE = 0).
 * @return UART_STATUS_OK, UART_STATUS_INVALID_ARG hoặc UART_STATUS_UNSUPPORTED.
 */
Uart_StatusType
Uart_Calc_ValidateFrame(Uart_DataBitsType DataBits, Uart_ParityType Parity,
                        Uart_StopBitsType StopBits, boolean Support9Bit,
                        uint16 *DataMaskOut, boolean *UseM9BitOut);

/**
 * @brief Kiểm tra toàn diện cấu hình một cổng TRƯỚC khi chạm vào bất kỳ thanh
 * ghi nào.
 * @param Config             Cấu hình người dùng truyền vào.
 * @param Caps               Năng lực phần cứng và cấu hình biên dịch của cổng
 * đó.
 * @param ResolvedBusClockHz Tần số bus thực tế đã phân giải (Hz).
 * @param ParamsOut          Con trỏ nhận các tham số phần cứng đã tính toán
 * xong.
 * @return UART_STATUS_OK hoặc mã lỗi chi tiết đầu tiên phát hiện được.
 */
Uart_StatusType Uart_Calc_ValidateConfig(const Uart_PortConfigType *Config,
                                         const Uart_HwCapsType *Caps,
                                         uint32 ResolvedBusClockHz,
                                         Uart_ValidatedParamsType *ParamsOut);

/**
 * @brief Kiểm tra bộ đếm tick 32-bit đã vượt giới hạn chờ chưa (an toàn khi
 * quay vòng).
 * @param StartTick Giá trị tick tại thời điểm bắt đầu chờ.
 * @param NowTick   Giá trị tick hiện tại.
 * @param Timeout   Số tick giới hạn.
 * @return TRUE nếu (uint32)(NowTick - StartTick) >= Timeout, ngược lại FALSE.
 */
boolean Uart_Calc_IsTickExpired(uint32 StartTick, uint32 NowTick,
                                uint32 Timeout);

#endif /* UART_CALC_H */