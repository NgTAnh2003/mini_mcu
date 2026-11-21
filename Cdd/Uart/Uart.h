/**
 * @file    Uart.h
 * @brief   Giao diện công khai (Public API) của thư viện UART bất đồng bộ
 * STM32F103.
 * @details
 * - Thiết kế đa instance theo mã cổng logic (Uart_PortIdType).
 * - Không phụ thuộc HAL/SPL/RTOS, không dùng cấp phát động (malloc/free).
 * - Không làm lộ thanh ghi STM32 ra header công khai.
 * - Tách rạch ròi quyền sở hữu: Port cấu hình chân điện, Mcu cấu hình xung hệ
 * thống, Uart quản lý cấp xung ngoại vi USART, cấu hình USART và backend DMA1
 * của riêng UART.
 */

#ifndef UART_H
#define UART_H

#include "Uart_Types.h"

/*============================================================================
 * NHÓM A: NGUỒN THỜI GIAN VÀ VÒNG ĐỜI CỔNG
 *============================================================================*/

/**
 * @brief Đăng ký hàm đọc bộ đếm tick thời gian thực dùng cho
 * UART_TIMEOUT_TICKS().
 * @param Provider Con trỏ hàm trả về tick tăng đơn điệu 32-bit (ví dụ ms từ
 * SysTick), hoặc NULL_PTR để hủy nguồn thời gian.
 * @note  Ngữ cảnh gọi: Vòng lặp chính trước khi dùng UART_TIMEOUT_TICKS().
 *        Phép đo thời gian xử lý an toàn khi bộ đếm 32-bit quay vòng.
 */
void Uart_RegisterTickProvider(Uart_GetTickFn Provider);

/**
 * @brief Khởi tạo một cổng UART ở trạng thái UNINIT.
 * @param Port   Mã cổng logic (UART_PORT_1..UART_PORT_3 tùy chip).
 * @param Config Con trỏ tới struct cấu hình (driver sao chép nội dung; có thể
 * là biến cục bộ).
 * @return
 * - UART_STATUS_OK: Khởi tạo hoàn tất, ngoại vi đã bật và sẵn sàng.
 * - UART_STATUS_INVALID_ARG: Port/Config sai, thiếu BusClock, ExtBuffer sai
 * kích thước.
 * - UART_STATUS_UNSUPPORTED: Chip không có cổng này, tổ hợp bit/parity không
 * hợp lệ, 9-bit hoặc DMA chưa bật trong Uart_Cfg.h.
 * - UART_STATUS_ALREADY_INIT: Cổng đang ở trạng thái READY.
 * - UART_STATUS_BAUD_OUT_OF_RANGE: Tần số bus và baud nằm ngoài dải BRR.
 * @note  Trạng thái sau lỗi: Mọi kiểm tra hợp lệ chạy TRƯỚC khi bật xung hay
 * ghi thanh ghi; nếu trả lỗi, cổng giữ nguyên trạng thái UNINIT, không bị khởi
 * tạo dở.
 * @note  Ngữ cảnh gọi: Chỉ gọi từ vòng lặp chính (KHÔNG gọi trong ISR).
 */
Uart_StatusType Uart_Init(Uart_PortIdType Port,
                          const Uart_PortConfigType *Config);

/**
 * @brief Dừng hoạt động và giải phóng trạng thái một cổng UART về UNINIT.
 * @param Port Mã cổng logic.
 * @return
 * - UART_STATUS_OK: Đã tắt ngắt, tắt UE, tắt xung ngoại vi USART và đặt về
 * UNINIT.
 * - UART_STATUS_INVALID_ARG / UART_STATUS_UNSUPPORTED / UART_STATUS_NOT_INIT.
 * - UART_STATUS_BUSY: Còn dữ liệu trong hàng đợi TX ngắt hoặc giao dịch DMA
 * TX/RX đang chạy. Người gọi phải chờ truyền xong (Uart_WaitTxComplete) hoặc
 * hủy DMA (Uart_DmaAbort*) trước.
 * @note  Ngữ cảnh gọi: Chỉ gọi từ vòng lặp chính (KHÔNG gọi trong ISR).
 */
Uart_StatusType Uart_DeInit(Uart_PortIdType Port);

/**
 * @brief Đổi cấu hình (baud, parity, chế độ...) cho một cổng đang ở trạng thái
 * READY.
 * @param Port   Mã cổng logic.
 * @param Config Cấu hình mới cần áp dụng.
 * @return
 * - UART_STATUS_OK: Đã cập nhật cấu hình mới sạch sẽ.
 * - UART_STATUS_BUSY: Cổng đang bận truyền/nhận (IRQ TX hoặc DMA TX/RX đang
 * chạy).
 * - Các mã lỗi kiểm tra cấu hình giống Uart_Init().
 * @note  Trạng thái sau lỗi: Cấu hình mới được kiểm tra hợp lệ toàn bộ TRƯỚC
 * khi DeInit cấu hình cũ. Nếu cấu hình mới sai hoặc cổng đang bận, cấu hình cũ
 * được giữ nguyên.
 * @note  Ngữ cảnh gọi: Chỉ gọi từ vòng lặp chính (KHÔNG gọi trong ISR).
 */
Uart_StatusType Uart_ReInit(Uart_PortIdType Port,
                            const Uart_PortConfigType *Config);

/**
 * @brief Truy vấn trạng thái vòng đời hiện tại của một cổng UART.
 * @param Port     Mã cổng logic.
 * @param StateOut Con trỏ nhận trạng thái (UART_STATE_UNINIT hoặc
 * UART_STATE_READY).
 * @return UART_STATUS_OK hoặc mã lỗi tham số/không hỗ trợ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_GetState(Uart_PortIdType Port,
                              Uart_PortStateType *StateOut);

/*============================================================================
 * NHÓM B: TRUYỀN VÀ NHẬN BẰNG POLLING
 *============================================================================*/

/**
 * @brief Gửi 1 byte dữ liệu (cổng 7-bit hoặc 8-bit) bằng polling.
 * @param Port    Mã cổng logic (phải cấu hình TxMode == UART_XFER_POLLING và
 * DataBits != 9).
 * @param Byte    Dữ liệu cần gửi (đơn vị: 1 byte; ở chế độ 7-bit chỉ dùng 7 bit
 * thấp).
 * @param Timeout Giới hạn chờ cờ SR.TXE == 1 (LOOPS hoặc TICKS).
 * @return UART_STATUS_OK (đã ghi vào DR), UART_STATUS_TIMEOUT, hoặc lỗi tham
 * số/chế độ.
 * @note  Trả UART_STATUS_OK nghĩa là byte đã được ghi vào thanh ghi DR, KHÔNG
 * có nghĩa bit stop cuối đã ra khỏi chân TX. Gọi Uart_WaitTxComplete() nếu cần
 * đợi phát xong hẳn.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_TransmitByte(Uart_PortIdType Port, uint8 Byte,
                                  Uart_TimeoutType Timeout);

/**
 * @brief Nhận 1 byte dữ liệu (cổng 7-bit hoặc 8-bit) bằng polling.
 * @param Port    Mã cổng logic (phải cấu hình RxMode == UART_XFER_POLLING và
 * DataBits != 9).
 * @param ByteOut Con trỏ nhận byte dữ liệu hữu ích (đã loại bỏ bit parity nếu
 * bật parity).
 * @param Timeout Giới hạn chờ cờ SR.RXNE == 1.
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, UART_STATUS_LINE_ERROR (nếu
 * khung lỗi), hoặc lỗi tham số/chế độ.
 * @note  Chính sách lỗi: Khi phát hiện PE/FE/NE/ORE, thanh ghi DR vẫn được đọc
 * để xóa cờ lỗi theo đúng trình tự RM0008, bộ đếm lỗi được cập nhật, khung lỗi
 * bị bỏ và trả LINE_ERROR.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_ReceiveByte(Uart_PortIdType Port, uint8 *ByteOut,
                                 Uart_TimeoutType Timeout);

/**
 * @brief Gửi một dãy byte bằng polling (cổng 7-bit hoặc 8-bit).
 * @param Port    Mã cổng logic (TxMode == UART_XFER_POLLING, DataBits != 9).
 * @param Data    Con trỏ mảng byte nguồn (đơn vị: byte).
 * @param Length  Số byte cần gửi (phải > 0).
 * @param SentOut Con trỏ tùy chọn (có thể NULL_PTR) nhận số byte đã ghi thành
 * công vào DR, kể cả khi dừng giữa chừng do timeout.
 * @param Timeout Giới hạn chờ cho toàn bộ dãy (TICKS) hoặc cho mỗi byte
 * (LOOPS).
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_TransmitPolling(Uart_PortIdType Port, const uint8 *Data,
                                     uint16 Length, uint16 *SentOut,
                                     Uart_TimeoutType Timeout);

/**
 * @brief Nhận một dãy byte bằng polling (cổng 7-bit hoặc 8-bit).
 * @param Port        Mã cổng logic (RxMode == UART_XFER_POLLING, DataBits !=
 * 9).
 * @param Buffer      Con trỏ mảng đích nhận byte hữu ích (đơn vị: byte).
 * @param Length      Số byte cần nhận (phải > 0).
 * @param ReceivedOut Con trỏ tùy chọn (có thể NULL_PTR) nhận số byte hợp lệ đã
 * thu được trước khi hoàn tất, timeout hoặc gặp lỗi đường truyền.
 * @param Timeout     Giới hạn chờ cho toàn bộ dãy (TICKS) hoặc cho mỗi byte
 * (LOOPS).
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, UART_STATUS_LINE_ERROR, hoặc lỗi
 * tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_ReceivePolling(Uart_PortIdType Port, uint8 *Buffer,
                                    uint16 Length, uint16 *ReceivedOut,
                                    Uart_TimeoutType Timeout);

/**
 * @brief Gửi 1 khung dữ liệu (hỗ trợ giữ nguyên bit thứ 9 cho chế độ 9N1/9N2)
 * bằng polling.
 * @param Port    Mã cổng logic (TxMode == UART_XFER_POLLING).
 * @param Frame   Giá trị khung dữ liệu (đơn vị: 1 khung; tự cắt mặt nạ theo
 * 7/8/9 bit).
 * @param Timeout Giới hạn chờ cờ SR.TXE == 1.
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_TransmitFrame(Uart_PortIdType Port, uint16 Frame,
                                   Uart_TimeoutType Timeout);

/**
 * @brief Nhận 1 khung dữ liệu (giữ nguyên bit thứ 9 ở bit 8 của uint16) bằng
 * polling.
 * @param Port     Mã cổng logic (RxMode == UART_XFER_POLLING).
 * @param FrameOut Con trỏ nhận khung dữ liệu hữu ích (đã che mặt nạ 0x7F / 0xFF
 * / 0x1FF).
 * @param Timeout  Giới hạn chờ cờ SR.RXNE == 1.
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, UART_STATUS_LINE_ERROR, hoặc lỗi
 * tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_ReceiveFrame(Uart_PortIdType Port, uint16 *FrameOut,
                                  Uart_TimeoutType Timeout);

/**
 * @brief Gửi một dãy khung dữ liệu 16-bit bằng polling (giữ trọn 9 bit dữ liệu
 * hữu ích).
 * @param Port    Mã cổng logic (TxMode == UART_XFER_POLLING).
 * @param Frames  Mảng khung nguồn (đơn vị: khung dữ liệu).
 * @param Count   Số khung cần gửi (phải > 0).
 * @param SentOut Con trỏ tùy chọn nhận số khung đã ghi thành công vào DR.
 * @param Timeout Giới hạn chờ.
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_TransmitFramesPolling(Uart_PortIdType Port,
                                           const uint16 *Frames, uint16 Count,
                                           uint16 *SentOut,
                                           Uart_TimeoutType Timeout);

/**
 * @brief Nhận một dãy khung dữ liệu 16-bit bằng polling (giữ trọn 9 bit dữ liệu
 * hữu ích).
 * @param Port        Mã cổng logic (RxMode == UART_XFER_POLLING).
 * @param Buffer      Mảng đích nhận khung dữ liệu (đơn vị: khung dữ liệu).
 * @param Count       Số khung cần nhận (phải > 0).
 * @param ReceivedOut Con trỏ tùy chọn nhận số khung hợp lệ đã nhận được.
 * @param Timeout     Giới hạn chờ.
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, UART_STATUS_LINE_ERROR, hoặc lỗi
 * tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_ReceiveFramesPolling(Uart_PortIdType Port, uint16 *Buffer,
                                          uint16 Count, uint16 *ReceivedOut,
                                          Uart_TimeoutType Timeout);

/**
 * @brief Chờ cho tới khi đường truyền TX thực sự hoàn tất (bit stop cuối đã rời
 * chân TX).
 * @details Kiểm tra điều kiện:
 *          - Nếu TxMode == UART_XFER_IRQ: đợi bộ đệm vòng TX cạn VÀ cờ phần
 * cứng SR.TC == 1.
 *          - Nếu TxMode == UART_XFER_DMA: đợi kênh DMA TX xong VÀ cờ phần cứng
 * SR.TC == 1.
 *          - Nếu TxMode == UART_XFER_POLLING: đợi cờ phần cứng SR.TC == 1.
 *          Hữu ích ngay trước khi gọi Uart_DeInit(), Uart_ReInit() (đổi baud)
 * hoặc vào chế độ ngủ.
 * @param Port    Mã cổng logic (chiều TX phải đang bật).
 * @param Timeout Giới hạn chờ.
 * @return UART_STATUS_OK, UART_STATUS_TIMEOUT, hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính (KHÔNG gọi trong ISR).
 */
Uart_StatusType Uart_WaitTxComplete(Uart_PortIdType Port,
                                    Uart_TimeoutType Timeout);

/*============================================================================
 * NHÓM C: TRUYỀN VÀ NHẬN NỀN BẰNG NGẮT VÀ BỘ ĐỆM VÒNG (TRẢ NGAY)
 *============================================================================*/

/**
 * @brief Đưa dãy byte vào hàng đợi TX ngắt và tự kích hoạt ngắt TXEIE (trả
 * ngay, không chờ).
 * @param Port       Mã cổng logic (TxMode == UART_XFER_IRQ, DataBits != 9).
 * @param Data       Mảng byte cần gửi (đơn vị: byte; được sao chép vào bộ đệm
 * vòng).
 * @param Length     Số byte mong muốn đưa vào hàng đợi (phải > 0).
 * @param WrittenOut Con trỏ tùy chọn nhận số byte thực tế đã chép vào hàng đợi
 * (0..Length).
 * @return
 * - UART_STATUS_OK: Đã chép đủ toàn bộ Length byte (hoặc >= 1 byte; kiểm tra
 * *WrittenOut).
 * - UART_STATUS_BUFFER_FULL: Hàng đợi đầy hoàn toàn (0 byte được ghi) hoặc chỉ
 * ghi được một phần
 *   (*WrittenOut < Length).
 * @note  Ngữ cảnh gọi: Vòng lặp chính (mô hình SPSC: chỉ một ngữ cảnh ghi TX
 * cho mỗi cổng).
 */
Uart_StatusType Uart_IrqWrite(Uart_PortIdType Port, const uint8 *Data,
                              uint16 Length, uint16 *WrittenOut);

/**
 * @brief Đọc dữ liệu đã nhận từ bộ đệm vòng RX (trả ngay, không chờ).
 * @param Port      Mã cổng logic (RxMode == UART_XFER_IRQ, DataBits != 9).
 * @param Buffer    Mảng đích nhận byte hữu ích (đơn vị: byte).
 * @param MaxLength Số byte tối đa cần đọc (phải > 0).
 * @param ReadOut   Con trỏ BẮT BUỘC nhận số byte thực tế đã đọc được
 * (0..MaxLength).
 * @return
 * - UART_STATUS_OK: Đọc được ít nhất 1 byte (*ReadOut >= 1).
 * - UART_STATUS_BUFFER_EMPTY: Bộ đệm vòng RX đang rỗng (*ReadOut == 0).
 * @note  Ngữ cảnh gọi: Vòng lặp chính (mô hình SPSC: chỉ một ngữ cảnh đọc RX
 * cho mỗi cổng).
 */
Uart_StatusType Uart_IrqRead(Uart_PortIdType Port, uint8 *Buffer,
                             uint16 MaxLength, uint16 *ReadOut);

/**
 * @brief Đưa dãy khung dữ liệu 16-bit vào hàng đợi TX ngắt (giữ trọn bit thứ 9,
 * trả ngay).
 * @param Port       Mã cổng logic (TxMode == UART_XFER_IRQ).
 * @param Frames     Mảng khung nguồn (đơn vị: khung dữ liệu).
 * @param Count      Số khung cần ghi (phải > 0).
 * @param WrittenOut Con trỏ tùy chọn nhận số khung thực tế đã ghi.
 * @return UART_STATUS_OK, UART_STATUS_BUFFER_FULL, hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_IrqWriteFrames(Uart_PortIdType Port, const uint16 *Frames,
                                    uint16 Count, uint16 *WrittenOut);

/**
 * @brief Đọc dãy khung dữ liệu 16-bit từ bộ đệm vòng RX (giữ trọn bit thứ 9,
 * trả ngay).
 * @param Port     Mã cổng logic (RxMode == UART_XFER_IRQ).
 * @param Buffer   Mảng đích nhận khung dữ liệu (đơn vị: khung dữ liệu).
 * @param MaxCount Số khung tối đa cần đọc (phải > 0).
 * @param ReadOut  Con trỏ BẮT BUỘC nhận số khung thực tế đã đọc được.
 * @return UART_STATUS_OK, UART_STATUS_BUFFER_EMPTY, hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_IrqReadFrames(Uart_PortIdType Port, uint16 *Buffer,
                                   uint16 MaxCount, uint16 *ReadOut);

/**
 * @brief Hỏi số phần tử (byte hoặc khung) còn trống trong bộ đệm vòng TX (trả
 * ngay).
 * @param Port    Mã cổng logic (TxMode == UART_XFER_IRQ).
 * @param FreeOut Con trỏ nhận số ô trống hiện có.
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_IrqGetTxFree(Uart_PortIdType Port, uint16 *FreeOut);

/**
 * @brief Hỏi số phần tử (byte hoặc khung) đang sẵn có trong bộ đệm vòng RX (trả
 * ngay).
 * @param Port         Mã cổng logic (RxMode == UART_XFER_IRQ).
 * @param AvailableOut Con trỏ nhận số phần tử chờ đọc.
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_IrqGetRxAvailable(Uart_PortIdType Port,
                                       uint16 *AvailableOut);

/**
 * @brief Xóa sạch toàn bộ dữ liệu chưa đọc trong bộ đệm vòng RX (trả ngay).
 * @param Port Mã cổng logic (RxMode == UART_XFER_IRQ).
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_IrqFlushRx(Uart_PortIdType Port);

/**
 * @brief Kiểm tra chiều TX có đang bận truyền không (trả ngay).
 * @details TRUE khi hàng đợi TX ngắt còn dữ liệu, DMA TX đang chạy, hoặc cờ
 * SR.TC == 0.
 * @param Port    Mã cổng logic.
 * @param BusyOut Con trỏ nhận kết quả (TRUE/FALSE).
 * @return UART_STATUS_OK hoặc lỗi tham số.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_IsTxBusy(Uart_PortIdType Port, boolean *BusyOut);

/**
 * @brief Kiểm tra chiều RX có đang có giao dịch nhận nền hoặc dữ liệu chờ không
 * (trả ngay).
 * @details Ở chế độ DMA: TRUE khi kênh DMA RX đang bận.
 *          Ở chế độ IRQ: TRUE khi bộ đệm vòng RX đang có dữ liệu chưa đọc.
 *          Ở chế độ Polling: luôn FALSE.
 * @param Port    Mã cổng logic.
 * @param BusyOut Con trỏ nhận kết quả (TRUE/FALSE).
 * @return UART_STATUS_OK hoặc lỗi tham số.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_IsRxBusy(Uart_PortIdType Port, boolean *BusyOut);

/*============================================================================
 * NHÓM D: TRUYỀN VÀ NHẬN KHỐI DỮ LIỆU BẰNG DMA (TRẢ NGAY)
 *============================================================================*/

/**
 * @brief Khởi động truyền khối byte bằng DMA1 (cổng 7-bit hoặc 8-bit, trả
 * ngay).
 * @param Port   Mã cổng logic (TxMode == UART_XFER_DMA, DataBits != 9).
 * @param Data   Con trỏ tới vùng nhớ nguồn (đơn vị: byte).
 *               QUAN TRỌNG: Vùng nhớ Data KHÔNG được thay đổi hay giải phóng
 * cho đến khi Uart_DmaGetTxStatus() báo khác BUSY hoặc nhận callback
 * UART_EVENT_DMA_TX_DONE.
 * @param Length Số byte cần truyền (1..65535).
 * @return UART_STATUS_OK, UART_STATUS_BUSY (đang có DMA TX khác),
 * UART_STATUS_UNSUPPORTED (nếu UART_CFG_DMA_ENABLE == 0U), hoặc lỗi tham số/chế
 * độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc callback sau khi giao dịch trước đã
 * hoàn tất.
 */
Uart_StatusType Uart_DmaTransmit(Uart_PortIdType Port, const uint8 *Data,
                                 uint16 Length);

/**
 * @brief Khởi động nhận khối byte bằng DMA1 (cổng 7-bit hoặc 8-bit, trả ngay).
 * @param Port   Mã cổng logic (RxMode == UART_XFER_DMA, DataBits != 9).
 * @param Buffer Con trỏ tới vùng nhớ đích (đơn vị: byte).
 *               QUAN TRỌNG: Ứng dụng không được đọc/tái sử dụng Buffer cho đến
 * khi giao dịch hoàn tất, lỗi hoặc bị hủy. Lưu ý 7-bit + Parity: phần cứng DMA
 * chép thẳng 8 bit của DR nên bit 7 chứa bit parity; chế độ 8-bit + Parity
 * không bị ảnh hưởng (bit 8 tự bỏ).
 * @param Length Số byte cần nhận (1..65535).
 * @return UART_STATUS_OK, UART_STATUS_BUSY, UART_STATUS_UNSUPPORTED, hoặc lỗi
 * tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc callback sau khi giao dịch trước đã
 * hoàn tất.
 */
Uart_StatusType Uart_DmaReceive(Uart_PortIdType Port, uint8 *Buffer,
                                uint16 Length);

/**
 * @brief Khởi động truyền khối khung 16-bit bằng DMA1 (giữ nguyên bit thứ 9,
 * trả ngay).
 * @details Cấu hình kênh DMA1 ở độ rộng PSIZE=16-bit, MSIZE=16-bit.
 * @param Port   Mã cổng logic (TxMode == UART_XFER_DMA, DataBits == 9 hoặc
 * khung 16-bit).
 * @param Frames Con trỏ mảng uint16 nguồn (phải căn lề 2 byte và giữ sống tới
 * khi DMA xong).
 * @param Count  Số khung cần truyền (1..65535).
 * @return UART_STATUS_OK, UART_STATUS_BUSY, UART_STATUS_UNSUPPORTED, hoặc lỗi
 * tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc callback sau khi giao dịch trước đã
 * hoàn tất.
 */
Uart_StatusType Uart_DmaTransmitFrames(Uart_PortIdType Port,
                                       const uint16 *Frames, uint16 Count);

/**
 * @brief Khởi động nhận khối khung 16-bit bằng DMA1 (giữ nguyên bit thứ 9, trả
 * ngay).
 * @details Cấu hình kênh DMA1 ở độ rộng PSIZE=16-bit, MSIZE=16-bit.
 * @param Port   Mã cổng logic (RxMode == UART_XFER_DMA).
 * @param Buffer Con trỏ mảng uint16 đích (phải căn lề 2 byte và giữ sống tới
 * khi DMA xong).
 * @param Count  Số khung cần nhận (1..65535).
 * @return UART_STATUS_OK, UART_STATUS_BUSY, UART_STATUS_UNSUPPORTED, hoặc lỗi
 * tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc callback sau khi giao dịch trước đã
 * hoàn tất.
 */
Uart_StatusType Uart_DmaReceiveFrames(Uart_PortIdType Port, uint16 *Buffer,
                                      uint16 Count);

/**
 * @brief Truy vấn trạng thái và tiến độ giao dịch DMA chiều TX (trả ngay).
 * @param Port    Mã cổng logic (TxMode == UART_XFER_DMA).
 * @param InfoOut Con trỏ nhận thông tin trạng thái, số phần tử yêu cầu và đã
 * chuyển.
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_DmaGetTxStatus(Uart_PortIdType Port,
                                    Uart_DmaTransferInfoType *InfoOut);

/**
 * @brief Truy vấn trạng thái và tiến độ giao dịch DMA chiều RX (trả ngay).
 * @param Port    Mã cổng logic (RxMode == UART_XFER_DMA).
 * @param InfoOut Con trỏ nhận thông tin trạng thái, số phần tử yêu cầu và đã
 * chuyển.
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_DmaGetRxStatus(Uart_PortIdType Port,
                                    Uart_DmaTransferInfoType *InfoOut);

/**
 * @brief Hủy an toàn giao dịch DMA chiều TX đang chạy và trả số phần tử đã kịp
 * chuyển.
 * @details Ngắt yêu cầu CR3.DMAT, tắt kênh DMA theo đúng trình tự RM0008, đọc
 * CNDTR đã đóng băng để tính chính xác số phần tử đã chuyển sang DR. Sau hàm
 * này buffer TX an toàn để tái sử dụng.
 * @param Port           Mã cổng logic (TxMode == UART_XFER_DMA).
 * @param TransferredOut Con trỏ tùy chọn nhận số phần tử đã kịp chuyển trước
 * khi hủy.
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_DmaAbortTx(Uart_PortIdType Port, uint16 *TransferredOut);

/**
 * @brief Hủy an toàn giao dịch DMA chiều RX đang chạy và trả số phần tử đã nhận
 * vào buffer.
 * @details Tắt CR3.DMAR, EIE, PEIE và kênh DMA trước khi đọc CNDTR và xóa cờ
 * lỗi tồn đọng. Sau hàm này ứng dụng được phép đọc từ phần tử 0 tới
 * (*TransferredOut - 1) của buffer.
 * @param Port           Mã cổng logic (RxMode == UART_XFER_DMA).
 * @param TransferredOut Con trỏ tùy chọn nhận số phần tử đã ghi vào buffer
 * trước khi hủy.
 * @return UART_STATUS_OK hoặc lỗi tham số/chế độ.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_DmaAbortRx(Uart_PortIdType Port, uint16 *TransferredOut);

/*============================================================================
 * NHÓM E: CHẨN ĐOÁN LỖI VÀ HÀM TIỆN ÍCH
 *============================================================================*/

/**
 * @brief Đọc ảnh chụp nhất quán (snapshot) toàn bộ bộ đếm và cờ lỗi của một
 * cổng.
 * @param Port     Mã cổng logic.
 * @param StatsOut Con trỏ nhận struct thống kê lỗi.
 * @return UART_STATUS_OK hoặc lỗi tham số/chưa khởi tạo.
 * @note  Ngữ cảnh gọi: Vòng lặp chính hoặc ISR.
 */
Uart_StatusType Uart_GetErrorStats(Uart_PortIdType Port,
                                   Uart_ErrorStatsType *StatsOut);

/**
 * @brief Đặt lại toàn bộ bộ đếm lỗi và cờ lỗi tích lũy của một cổng về 0.
 * @param Port Mã cổng logic.
 * @return UART_STATUS_OK hoặc lỗi tham số/chưa khởi tạo.
 * @note  Ngữ cảnh gọi: Vòng lặp chính.
 */
Uart_StatusType Uart_ClearErrorStats(Uart_PortIdType Port);

/**
 * @brief Tính giá trị thanh ghi USART_BRR và baud thực tế (không phụ thuộc
 * trạng thái cổng).
 * @param BusClockHz    Tần số bus ngoại vi tính bằng Hz (PCLK2 cho USART1,
 * PCLK1 cho USART2/3).
 * @param BaudRate      Tốc độ baud mong muốn tính bằng bps.
 * @param BrrOut        Con trỏ BẮT BUỘC nhận giá trị BRR 16-bit hợp lệ
 * ([0x0010, 0xFFFF]).
 * @param ActualBaudOut Con trỏ tùy chọn (có thể NULL_PTR) nhận tốc độ baud thực
 * tế đạt được (bps).
 * @return UART_STATUS_OK, UART_STATUS_INVALID_ARG, hoặc
 * UART_STATUS_BAUD_OUT_OF_RANGE.
 * @note  Ngữ cảnh gọi: Bất kỳ đâu (hàm thuần toán học, không chạm thanh ghi).
 */
Uart_StatusType Uart_ComputeBaudRate(uint32 BusClockHz, uint32 BaudRate,
                                     uint16 *BrrOut, uint32 *ActualBaudOut);

/*============================================================================
 * NHÓM F: ĐIỂM VÀO TRÌNH PHỤC VỤ NGẮT (PROJECT SỞ HỮU BẢNG VECTOR)
 *============================================================================*/

/**
 * @brief Điểm vào xử lý ngắt ngoại vi USART cho một cổng logic.
 * @details Driver KHÔNG tự định nghĩa USART1_IRQHandler / USART2_IRQHandler /
 * USART3_IRQHandler để tránh trùng biểu tượng với bảng vector sẵn có của dự án.
 * File vector của dự án (ví dụ stm32f1xx_it.c) chỉ cần gọi hàm này với mã cổng
 * tương ứng.
 * @param Port Mã cổng logic tương ứng với vector vừa kích hoạt.
 * @note  Ngữ cảnh gọi: CHỈ gọi từ trình phục vụ ngắt USARTx_IRQHandler.
 */
void Uart_IrqHandler(Uart_PortIdType Port);

/**
 * @brief Điểm vào xử lý ngắt kênh DMA TX của một cổng logic.
 * @details Gọi từ DMA1_Channel4_IRQHandler (USART1), DMA1_Channel7_IRQHandler
 * (USART2), hoặc DMA1_Channel2_IRQHandler (USART3 trên C8T6).
 * @param Port Mã cổng logic tương ứng.
 * @note  Ngữ cảnh gọi: CHỈ gọi từ trình phục vụ ngắt DMA1_Channelx_IRQHandler.
 */
void Uart_DmaTxIrqHandler(Uart_PortIdType Port);

/**
 * @brief Điểm vào xử lý ngắt kênh DMA RX của một cổng logic.
 * @details Gọi từ DMA1_Channel5_IRQHandler (USART1), DMA1_Channel6_IRQHandler
 * (USART2), hoặc DMA1_Channel3_IRQHandler (USART3 trên C8T6).
 * @param Port Mã cổng logic tương ứng.
 * @note  Ngữ cảnh gọi: CHỈ gọi từ trình phục vụ ngắt DMA1_Channelx_IRQHandler.
 */
void Uart_DmaRxIrqHandler(Uart_PortIdType Port);

#endif /* UART_H */