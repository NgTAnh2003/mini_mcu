/**
 * @file    Uart_DmaIf.h
 * @brief   [Nội bộ] Giao diện tách biệt giữa lõi Uart.c và backend DMA cụ thể
 * của chip.
 * @details
 * Mục đích thiết kế:
 * - Dự án hiện chưa có driver Mcal/Dma dùng chung. Tách mọi truy cập thanh ghi
 * DMA1 vào sau giao diện này giúp sau này có thể thay Uart_Dma_Stm32f1.c bằng
 * một adapter gọi sang Mcal/Dma mà KHÔNG phải sửa lõi trạng thái trong Uart.c.
 * - Khi UART_CFG_DMA_ENABLE == 0U, backend vẫn biên dịch sạch dưới dạng stub
 * rỗng.
 */

#ifndef UART_DMAIF_H
#define UART_DMAIF_H

#include "Uart_Types.h"

/**
 * @brief Sự kiện trả về từ trình xử lý ngắt kênh DMA của backend.
 */
typedef enum {
  UART_DMA_IRQ_NONE = 0U, /**< Không có cờ ngắt nào thuộc kênh này đang bật */
  UART_DMA_IRQ_COMPLETE =
      1U, /**< Kênh DMA đã hoàn tất chuyển đủ số phần tử (TCIF) */
  UART_DMA_IRQ_ERROR = 2U /**< Kênh DMA gặp lỗi truy cập bus (TEIF) */
} Uart_DmaIrqEventType;

/**
 * @brief Chuẩn bị kênh DMA1 riêng cho một chiều của cổng UART.
 * @details Chỉ bật xung DMA1 và đưa duy nhất kênh thuộc chiều đó về trạng thái
 * tắt/sạch; KHÔNG reset toàn bộ bộ điều khiển DMA1 để tránh phá hỏng các ngoại
 * vi khác.
 * @param Port Mã cổng logic.
 * @param IsTx TRUE cho kênh TX, FALSE cho kênh RX.
 * @return UART_STATUS_OK hoặc UART_STATUS_UNSUPPORTED /
 * UART_STATUS_INVALID_ARG.
 */
Uart_StatusType Uart_DmaIf_InitChannel(Uart_PortIdType Port, boolean IsTx);

/**
 * @brief Dừng và xóa sạch cờ của kênh DMA1 thuộc một chiều khi DeInit cổng
 * UART.
 * @details KHÔNG tắt xung RCC_AHBENR_DMA1EN vì cổng UART khác hoặc ngoại vi
 * khác có thể đang dùng DMA1.
 * @param Port Mã cổng logic.
 * @param IsTx TRUE cho kênh TX, FALSE cho kênh RX.
 */
void Uart_DmaIf_DeInitChannel(Uart_PortIdType Port, boolean IsTx);

/**
 * @brief Cấu hình và kích hoạt kênh DMA1 chiều TX (RAM -> USART_DR).
 * @param Port       Mã cổng logic.
 * @param PeriphAddr Địa chỉ thanh ghi USARTx->DR.
 * @param MemAddr    Địa chỉ vùng nhớ nguồn trong RAM (chỉ đọc).
 * @param Count      Số phần tử cần truyền (1..65535).
 * @param Width16Bit TRUE nếu truyền khung 16-bit (PSIZE=16, MSIZE=16), FALSE
 * cho 8-bit.
 * @param Priority   Mức ưu tiên kênh DMA.
 * @return UART_STATUS_OK hoặc mã lỗi.
 */
Uart_StatusType Uart_DmaIf_StartTx(Uart_PortIdType Port, uint32 PeriphAddr,
                                   const void *MemAddr, uint16 Count,
                                   boolean Width16Bit,
                                   Uart_DmaPriorityType Priority);

/**
 * @brief Cấu hình và kích hoạt kênh DMA1 chiều RX (USART_DR -> RAM).
 * @param Port       Mã cổng logic.
 * @param PeriphAddr Địa chỉ thanh ghi USARTx->DR.
 * @param MemAddr    Địa chỉ vùng nhớ đích trong RAM.
 * @param Count      Số phần tử cần nhận (1..65535).
 * @param Width16Bit TRUE nếu nhận khung 16-bit (PSIZE=16, MSIZE=16), FALSE cho
 * 8-bit.
 * @param Priority   Mức ưu tiên kênh DMA.
 * @return UART_STATUS_OK hoặc mã lỗi.
 */
Uart_StatusType Uart_DmaIf_StartRx(Uart_PortIdType Port, uint32 PeriphAddr,
                                   void *MemAddr, uint16 Count,
                                   boolean Width16Bit,
                                   Uart_DmaPriorityType Priority);

/**
 * @brief Dừng cưỡng bức kênh DMA1 theo đúng trình tự an toàn và trả về CNDTR
 * còn lại.
 * @param Port         Mã cổng logic.
 * @param IsTx         TRUE cho kênh TX, FALSE cho kênh RX.
 * @param RemainingOut Con trỏ nhận giá trị CNDTR sau khi kênh đã dừng hẳn (có
 * thể NULL_PTR).
 * @return TRUE nếu kênh dừng gọn trong giới hạn vòng chờ, FALSE nếu hết giới
 * hạn chờ.
 */
boolean Uart_DmaIf_StopChannel(Uart_PortIdType Port, boolean IsTx,
                               uint16 *RemainingOut);

/**
 * @brief Đọc số phần tử chưa truyền xong hiện tại từ thanh ghi CNDTR của kênh.
 * @param Port Mã cổng logic.
 * @param IsTx TRUE cho kênh TX, FALSE cho kênh RX.
 * @return Giá trị CNDTR hiện tại (0..65535).
 */
uint16 Uart_DmaIf_GetRemaining(Uart_PortIdType Port, boolean IsTx);

/**
 * @brief Xử lý thanh ghi DMA1->ISR / DMA1->IFCR cho kênh tương ứng trong ngữ
 * cảnh ISR.
 * @details Nếu phát hiện TCIF hoặc TEIF, backend tự tắt kênh DMA, tắt ngắt
 * kênh, xóa cờ IFCR của đúng kênh đó, đọc CNDTR còn lại và trả về sự kiện cho
 * Uart.c.
 * @param Port         Mã cổng logic.
 * @param IsTx         TRUE cho kênh TX, FALSE cho kênh RX.
 * @param RemainingOut Con trỏ nhận giá trị CNDTR sau khi tắt kênh.
 * @return Loại sự kiện DMA (NONE, COMPLETE hoặc ERROR).
 */
Uart_DmaIrqEventType Uart_DmaIf_HandleIrq(Uart_PortIdType Port, boolean IsTx,
                                          uint16 *RemainingOut);

#endif /* UART_DMAIF_H */