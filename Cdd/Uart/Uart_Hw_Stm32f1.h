/**
 * @file    Uart_Hw_Stm32f1.h
 * @brief   [Nội bộ] Mô tả phần cứng USART và ánh xạ kênh DMA1 cho STM32F103x6 /
 * STM32F103xB.
 * @details
 * - Header NỘI BỘ: chỉ được include bởi Uart.c và Uart_Dma_Stm32f1.c.
 * - Tuyệt đối KHÔNG include file này từ Uart.h hoặc Uart_Types.h để tầng trên
 * không bị phụ thuộc vào thanh ghi STM32.
 * - Khi chuyển sang họ chip khác (STM32F4, STM32G4, S32K...), đây là một trong
 * những file cần viết lại theo bảng địa chỉ thanh ghi và ánh xạ DMA của họ chip
 * mới.
 */

#ifndef UART_HW_STM32F1_H
#define UART_HW_STM32F1_H

#include "Uart_Types.h"
#include "stm32f1xx.h"

/**
 * @brief Bảng mô tả phần cứng bất biến cho một cổng UART logic trên STM32F103.
 */
typedef struct {
  boolean Available;    /**< TRUE nếu biến thể chip hiện tại có ngoại vi này */
  Uart_BusIdType BusId; /**< Bus cấp xung: UART_BUS_APB2 (USART1) hoặc
                           UART_BUS_APB1 (USART2/3) */
  USART_TypeDef *Instance; /**< Địa chỉ nền thanh ghi USARTx (NULL_PTR nếu không
                              tồn tại) */
  volatile uint32_t
      *RccEnReg; /**< Thanh ghi RCC APB1ENR / APB2ENR để bật/tắt xung USART */
  uint32_t RccEnMask; /**< Mặt nạ bit bật xung trong RccEnReg */
  volatile uint32_t
      *RccRstReg; /**< Thanh ghi RCC APB1RSTR / APB2RSTR để reset sạch USART */
  uint32_t RccRstMask; /**< Mặt nạ bit reset trong RccRstReg */
  boolean HasRtsCts;   /**< TRUE nếu cổng hỗ trợ RTS/CTS trên gói LQFP48 */
  DMA_Channel_TypeDef
      *DmaTxChannel; /**< Kênh DMA1 ánh xạ chiều TX (RM0008 Bảng 78) */
  DMA_Channel_TypeDef
      *DmaRxChannel;     /**< Kênh DMA1 ánh xạ chiều RX (RM0008 Bảng 78) */
  uint8 DmaTxChannelIdx; /**< Chỉ số kênh TX nền 0 (0..6 tương ứng Channel 1..7)
                          */
  uint8 DmaRxChannelIdx; /**< Chỉ số kênh RX nền 0 (0..6 tương ứng Channel 1..7)
                          */
} Uart_HwPortDescType;

/**
 * @brief Tra cứu mô tả phần cứng bất biến theo mã cổng logic.
 * @param Port Mã cổng logic (UART_PORT_1 .. UART_PORT_3).
 * @return Con trỏ tới mô tả phần cứng, hoặc NULL_PTR nếu Port >=
 * UART_PORT_COUNT.
 */
const Uart_HwPortDescType *Uart_Hw_GetPortDesc(Uart_PortIdType Port);

#endif /* UART_HW_STM32F1_H */