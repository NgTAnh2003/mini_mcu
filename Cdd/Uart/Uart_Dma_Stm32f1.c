/**
 * @file    Uart_Dma_Stm32f1.c
 * @brief   [Nội bộ] Backend điều khiển kênh DMA1 của STM32F103 cho driver UART.
 * @details
 * - Quy ước ngăn xung đột kênh DMA1 với các ngoại vi khác (RM0008 Bảng 78):
 *   + USART1_TX = DMA1_Ch4 (dùng chung tuyến request với SPI2_RX, I2C2_TX,
 * TIM1_CH4, TIM4_CH2)
 *   + USART1_RX = DMA1_Ch5 (dùng chung với SPI2_TX, I2C2_RX, TIM1_UP, TIM2_CH1,
 * TIM4_CH3)
 *   + USART2_RX = DMA1_Ch6 (dùng chung với I2C1_TX, TIM1_CH3, TIM3_CH1/TRIG)
 *   + USART2_TX = DMA1_Ch7 (dùng chung với I2C1_RX, TIM2_CH2/CH4, TIM4_UP)
 *   + USART3_TX = DMA1_Ch2 [chỉ C8T6] (dùng chung với ADC1 không dùng, SPI1_RX,
 * TIM1_CH1, TIM2_UP, TIM3_CH3)
 *   + USART3_RX = DMA1_Ch3 [chỉ C8T6] (dùng chung với SPI1_TX, TIM3_CH4/UP)
 *   STM32F1 dùng dây OR cứng cho các request trên cùng kênh: người tích hợp
 * KHÔNG được đồng thời bật yêu cầu DMA của hai ngoại vi cùng chung một kênh
 * DMA1. Backend này chỉ tác động vào đúng thanh ghi CCR/CNDTR/CPAR/CMAR và 4
 * bit IFCR của kênh sở hữu, tuyệt đối không reset toàn bộ DMA1.
 */

#include "Uart_DmaIf.h"
#include "Uart_Hw_Stm32f1.h"
#include <stdint.h>

#if (UART_CFG_DMA_ENABLE == 1U)

/*============================================================================
 * MẶT NẠ CỜ DMA1->ISR / DMA1->IFCR THEO CHỈ SỐ KÊNH (0..6 -> CHANNEL 1..7)
 *============================================================================*/

#define UART_DMA_FLAG_GIF(idx) (0x1UL << ((uint32)(idx) * 4UL))
#define UART_DMA_FLAG_TCIF(idx) (0x2UL << ((uint32)(idx) * 4UL))
#define UART_DMA_FLAG_HTIF(idx) (0x4UL << ((uint32)(idx) * 4UL))
#define UART_DMA_FLAG_TEIF(idx) (0x8UL << ((uint32)(idx) * 4UL))
#define UART_DMA_FLAG_ALL(idx) (0xFUL << ((uint32)(idx) * 4UL))

/**
 * @brief Lấy con trỏ kênh DMA1 và chỉ số kênh (0..6) cho một chiều của cổng
 * UART.
 */
static boolean Uart_Dma_ResolveChannel(Uart_PortIdType Port, boolean IsTx,
                                       DMA_Channel_TypeDef **ChannelOut,
                                       uint8 *IdxOut) {
  const Uart_HwPortDescType *hw = Uart_Hw_GetPortDesc(Port);

  if ((hw == NULL_PTR) || (hw->Available == FALSE)) {
    return FALSE;
  }

  if (IsTx != FALSE) {
    if (hw->DmaTxChannel == NULL_PTR) {
      return FALSE;
    }
    *ChannelOut = hw->DmaTxChannel;
    *IdxOut = hw->DmaTxChannelIdx;
  } else {
    if (hw->DmaRxChannel == NULL_PTR) {
      return FALSE;
    }
    *ChannelOut = hw->DmaRxChannel;
    *IdxOut = hw->DmaRxChannelIdx;
  }

  return TRUE;
}

/**
 * @brief Chuyển mức ưu tiên API sang bit trường DMA_CCR.PL[13:12].
 */
static uint32_t Uart_Dma_EncodePriority(Uart_DmaPriorityType Priority) {
  switch (Priority) {
  case UART_DMA_PRIO_MEDIUM:
    return DMA_CCR_PL_0;

  case UART_DMA_PRIO_HIGH:
    return DMA_CCR_PL_1;

  case UART_DMA_PRIO_VERY_HIGH:
    return (DMA_CCR_PL_1 | DMA_CCR_PL_0);

  case UART_DMA_PRIO_LOW:
  default:
    return 0UL;
  }
}

/**
 * @brief Dừng một kênh DMA1 cụ thể theo trình tự an toàn của RM0008.
 * @details
 * Vì sao phải theo đúng thứ tự này?
 * 1. Tắt ngắt (TCIE/TEIE/HTIE) và xóa bit EN trong CCR: DMA1 ngừng nhận request
 * mới.
 * 2. Chờ bit EN thực sự đọc về 0 (có giới hạn vòng lặp): đảm bảo bus AHB kết
 * thúc chu kỳ truyền cuối đang dở trước khi chạm vào CNDTR hay giải phóng
 * buffer RAM.
 * 3. Đọc CNDTR lúc kênh đã dừng để lấy số phần tử còn lại chính xác, không bị
 * lệch.
 * 4. Xóa sạch toàn bộ 4 cờ ngắt của RIÊNG kênh này qua thanh ghi DMA1->IFCR.
 */
static boolean Uart_Dma_DisableChannelSafe(DMA_Channel_TypeDef *Channel,
                                           uint8 ChannelIdx,
                                           uint16 *RemainingOut) {
  uint32 loops = UART_CFG_DMA_DISABLE_TIMEOUT_LOOPS;
  boolean stopped;

  /* Bước 1: Tắt các bit ngắt và tắt EN của riêng kênh này */
  Channel->CCR &= ~(DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE);

  /* Bước 2: Chờ phần cứng xác nhận EN == 0 với giới hạn chống treo */
  while (((Channel->CCR & DMA_CCR_EN) != 0UL) && (loops > 0UL)) {
    loops--;
  }
  stopped = ((Channel->CCR & DMA_CCR_EN) == 0UL) ? TRUE : FALSE;

  /* Bước 3: Chốt số phần tử còn lại sau khi kênh đã ngừng ghi/đọc */
  if (RemainingOut != NULL_PTR) {
    *RemainingOut = (uint16)(Channel->CNDTR & 0xFFFFUL);
  }

  /* Bước 4: Xóa sạch cờ ngắt tồn đọng của riêng kênh này (ghi 1 vào IFCR để
   * xóa) */
  DMA1->IFCR = UART_DMA_FLAG_ALL(ChannelIdx);

  return stopped;
}

Uart_StatusType Uart_DmaIf_InitChannel(Uart_PortIdType Port, boolean IsTx) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;
  volatile uint32_t dummyRead;

  if (Uart_Dma_ResolveChannel(Port, IsTx, &ch, &idx) == FALSE) {
    return UART_STATUS_UNSUPPORTED;
  }

  /*
   * Bật xung cho bộ điều khiển DMA1 trên bus AHB.
   * Lưu ý: KHÔNG reset DMA1 vì các ngoại vi khác (hoặc cổng UART khác) có thể
   * đang chạy trên các kênh DMA1 còn lại.
   */
  RCC->AHBENR |= RCC_AHBENR_DMA1EN;
  dummyRead = RCC->AHBENR;
  (void)dummyRead;

  (void)Uart_Dma_DisableChannelSafe(ch, idx, NULL_PTR);
  ch->CCR = 0UL;
  ch->CNDTR = 0UL;
  ch->CPAR = 0UL;
  ch->CMAR = 0UL;

  return UART_STATUS_OK;
}

void Uart_DmaIf_DeInitChannel(Uart_PortIdType Port, boolean IsTx) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;

  if (Uart_Dma_ResolveChannel(Port, IsTx, &ch, &idx) == FALSE) {
    return;
  }

  (void)Uart_Dma_DisableChannelSafe(ch, idx, NULL_PTR);
  ch->CCR = 0UL;
  ch->CNDTR = 0UL;

  /* Không tắt RCC_AHBENR_DMA1EN để không làm gián đoạn các kênh DMA1 khác */
}

Uart_StatusType Uart_DmaIf_StartTx(Uart_PortIdType Port, uint32 PeriphAddr,
                                   const void *MemAddr, uint16 Count,
                                   boolean Width16Bit,
                                   Uart_DmaPriorityType Priority) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;
  uint32_t ccr;

  if ((MemAddr == NULL_PTR) || (Count == 0U)) {
    return UART_STATUS_INVALID_ARG;
  }

  if (Uart_Dma_ResolveChannel(Port, TRUE, &ch, &idx) == FALSE) {
    return UART_STATUS_UNSUPPORTED;
  }

  /* Đảm bảo kênh đang tắt và sạch cờ trước khi nạp cấu hình mới */
  if (Uart_Dma_DisableChannelSafe(ch, idx, NULL_PTR) == FALSE) {
    return UART_STATUS_DMA_ERROR;
  }

  /*
   * Cấu hình DMA1 chiều TX (Memory -> Peripheral):
   * - DIR  = 1: đọc từ RAM ghi vào USART->DR
   * - MINC = 1: tăng địa chỉ RAM sau mỗi phần tử
   * - PINC = 0: giữ cố định địa chỉ thanh ghi USART->DR
   * - CIRC = 0: chế độ truyền độ dài xác định (Normal)
   * - PSIZE/MSIZE = 16-bit khi truyền khung 16-bit (giữ bit thứ 9), 8-bit cho
   * byte
   * - TCIE = 1, TEIE = 1: báo ngắt khi hoàn tất hoặc lỗi bus DMA
   */
  ccr = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE |
        Uart_Dma_EncodePriority(Priority);

  if (Width16Bit != FALSE) {
    ccr |= (DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0);
  }

  ch->CPAR = (uint32_t)PeriphAddr;
  ch->CMAR = (uint32_t)(uintptr_t)MemAddr;
  ch->CNDTR = (uint32_t)Count;
  ch->CCR = ccr;

  /* Rào chắn bộ nhớ đảm bảo dữ liệu RAM của ứng dụng đã ghi xuống trước khi DMA
   * đọc */
  __DSB();

  /* Bật kênh DMA trước khi Uart.c bật USART->CR3.DMAT */
  ch->CCR |= DMA_CCR_EN;

  return UART_STATUS_OK;
}

Uart_StatusType Uart_DmaIf_StartRx(Uart_PortIdType Port, uint32 PeriphAddr,
                                   void *MemAddr, uint16 Count,
                                   boolean Width16Bit,
                                   Uart_DmaPriorityType Priority) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;
  uint32_t ccr;

  if ((MemAddr == NULL_PTR) || (Count == 0U)) {
    return UART_STATUS_INVALID_ARG;
  }

  if (Uart_Dma_ResolveChannel(Port, FALSE, &ch, &idx) == FALSE) {
    return UART_STATUS_UNSUPPORTED;
  }

  if (Uart_Dma_DisableChannelSafe(ch, idx, NULL_PTR) == FALSE) {
    return UART_STATUS_DMA_ERROR;
  }

  /*
   * Cấu hình DMA1 chiều RX (Peripheral -> Memory):
   * - DIR  = 0: đọc từ USART->DR ghi vào RAM
   * - MINC = 1: tăng địa chỉ RAM sau mỗi phần tử
   * - PINC = 0: giữ cố định địa chỉ thanh ghi USART->DR
   * - CIRC = 0: nhận đủ Count phần tử rồi dừng
   * - TCIE = 1, TEIE = 1: báo ngắt khi nhận đủ hoặc lỗi bus DMA
   */
  ccr = DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE |
        Uart_Dma_EncodePriority(Priority);

  if (Width16Bit != FALSE) {
    ccr |= (DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0);
  }

  ch->CPAR = (uint32_t)PeriphAddr;
  ch->CMAR = (uint32_t)(uintptr_t)MemAddr;
  ch->CNDTR = (uint32_t)Count;
  ch->CCR = ccr;

  __DSB();

  /* Bật kênh DMA trước khi Uart.c bật USART->CR3.DMAR */
  ch->CCR |= DMA_CCR_EN;

  return UART_STATUS_OK;
}

boolean Uart_DmaIf_StopChannel(Uart_PortIdType Port, boolean IsTx,
                               uint16 *RemainingOut) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;

  if (Uart_Dma_ResolveChannel(Port, IsTx, &ch, &idx) == FALSE) {
    if (RemainingOut != NULL_PTR) {
      *RemainingOut = 0U;
    }
    return TRUE;
  }

  return Uart_Dma_DisableChannelSafe(ch, idx, RemainingOut);
}

uint16 Uart_DmaIf_GetRemaining(Uart_PortIdType Port, boolean IsTx) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;

  if (Uart_Dma_ResolveChannel(Port, IsTx, &ch, &idx) == FALSE) {
    return 0U;
  }

  return (uint16)(ch->CNDTR & 0xFFFFUL);
}

Uart_DmaIrqEventType Uart_DmaIf_HandleIrq(Uart_PortIdType Port, boolean IsTx,
                                          uint16 *RemainingOut) {
  DMA_Channel_TypeDef *ch = NULL_PTR;
  uint8 idx = 0U;
  uint32_t isr;
  uint32_t ccr;

  if (RemainingOut != NULL_PTR) {
    *RemainingOut = 0U;
  }

  if (Uart_Dma_ResolveChannel(Port, IsTx, &ch, &idx) == FALSE) {
    return UART_DMA_IRQ_NONE;
  }

  isr = DMA1->ISR;
  ccr = ch->CCR;

  /* Kiểm tra lỗi bus DMA (TEIF) trước */
  if (((isr & UART_DMA_FLAG_TEIF(idx)) != 0UL) &&
      ((ccr & DMA_CCR_TEIE) != 0UL)) {
    (void)Uart_Dma_DisableChannelSafe(ch, idx, RemainingOut);
    return UART_DMA_IRQ_ERROR;
  }

  /* Kiểm tra hoàn tất toàn bộ khối (TCIF) */
  if (((isr & UART_DMA_FLAG_TCIF(idx)) != 0UL) &&
      ((ccr & DMA_CCR_TCIE) != 0UL)) {
    (void)Uart_Dma_DisableChannelSafe(ch, idx, RemainingOut);
    return UART_DMA_IRQ_COMPLETE;
  }

  /* Xóa cờ phụ (như HTIF) nếu chẳng may dựng lên để tránh treo ngắt */
  if ((isr & UART_DMA_FLAG_GIF(idx)) != 0UL) {
    DMA1->IFCR = UART_DMA_FLAG_ALL(idx);
  }

  return UART_DMA_IRQ_NONE;
}

#else /* (UART_CFG_DMA_ENABLE == 0U) */

/*============================================================================
 * BẢN STUB KHI TẮT DMA (UART_CFG_DMA_ENABLE == 0U) — BIÊN DỊCH SẠCH KHÔNG
 * WARNING
 *============================================================================*/

Uart_StatusType Uart_DmaIf_InitChannel(Uart_PortIdType Port, boolean IsTx) {
  (void)Port;
  (void)IsTx;
  return UART_STATUS_UNSUPPORTED;
}

void Uart_DmaIf_DeInitChannel(Uart_PortIdType Port, boolean IsTx) {
  (void)Port;
  (void)IsTx;
}

Uart_StatusType Uart_DmaIf_StartTx(Uart_PortIdType Port, uint32 PeriphAddr,
                                   const void *MemAddr, uint16 Count,
                                   boolean Width16Bit,
                                   Uart_DmaPriorityType Priority) {
  (void)Port;
  (void)PeriphAddr;
  (void)MemAddr;
  (void)Count;
  (void)Width16Bit;
  (void)Priority;
  return UART_STATUS_UNSUPPORTED;
}

Uart_StatusType Uart_DmaIf_StartRx(Uart_PortIdType Port, uint32 PeriphAddr,
                                   void *MemAddr, uint16 Count,
                                   boolean Width16Bit,
                                   Uart_DmaPriorityType Priority) {
  (void)Port;
  (void)PeriphAddr;
  (void)MemAddr;
  (void)Count;
  (void)Width16Bit;
  (void)Priority;
  return UART_STATUS_UNSUPPORTED;
}

boolean Uart_DmaIf_StopChannel(Uart_PortIdType Port, boolean IsTx,
                               uint16 *RemainingOut) {
  (void)Port;
  (void)IsTx;
  if (RemainingOut != NULL_PTR) {
    *RemainingOut = 0U;
  }
  return TRUE;
}

uint16 Uart_DmaIf_GetRemaining(Uart_PortIdType Port, boolean IsTx) {
  (void)Port;
  (void)IsTx;
  return 0U;
}

Uart_DmaIrqEventType Uart_DmaIf_HandleIrq(Uart_PortIdType Port, boolean IsTx,
                                          uint16 *RemainingOut) {
  (void)Port;
  (void)IsTx;
  if (RemainingOut != NULL_PTR) {
    *RemainingOut = 0U;
  }
  return UART_DMA_IRQ_NONE;
}

#endif /* (UART_CFG_DMA_ENABLE == 1U) */