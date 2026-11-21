/**
 * @file    Uart_Calc.c
 * @brief   [Nội bộ] Hiện thực tính BRR chống tràn và kiểm tra cấu hình UART
 * STM32F1.
 */

#include "Uart_Calc.h"

/*============================================================================
 * HẰNG SỐ GIỚI HẠN THANH GHI USART_BRR TRÊN STM32F1 (RM0008 MỤC 27.3.4)
 *============================================================================*/

/**
 * @brief Giá trị BRR nhỏ nhất hợp lệ (DIV_Mantissa = 1, DIV_Fraction = 0 ->
 * USARTDIV = 1.0).
 * @details RM0008 quy định USARTDIV >= 1.0 (tức BaudRate tối đa = f_CK / 16).
 */
#define UART_BRR_MIN_VALUE (0x0010UL)

/**
 * @brief Giá trị BRR lớn nhất hợp lệ (DIV_Mantissa = 4095, DIV_Fraction = 15).
 */
#define UART_BRR_MAX_VALUE (0xFFFFUL)

/*============================================================================
 * HIỆN THỰC HÀM NỘI BỘ
 *============================================================================*/

Uart_StatusType Uart_Calc_BaudBrr(uint32 BusClockHz, uint32 BaudRate,
                                  uint16 *BrrOut, uint32 *ActualBaudOut) {
  uint32 quot;
  uint32 rem;
  uint32 halfThreshold;

  if ((BrrOut == NULL_PTR) || (BusClockHz == 0UL) || (BaudRate == 0UL)) {
    return UART_STATUS_INVALID_ARG;
  }

  /*
   * Giải thích công thức tính BRR trên STM32F1 (oversampling cố định 16x):
   *   USARTDIV = BusClockHz / (16 * BaudRate)
   *   BRR      = (DIV_Mantissa << 4) | DIV_Fraction = round(16 * USARTDIV)
   *            = round(BusClockHz / BaudRate)
   *
   * Tại sao KHÔNG viết (BusClockHz + BaudRate / 2U) / BaudRate?
   * Nếu BusClockHz lớn gần UINT32_MAX (ví dụ trong unit test biên), phép cộng
   * BusClockHz + BaudRate/2 có thể tràn số 32-bit mà không báo lỗi.
   *
   * Thay vào đó dùng thương và số dư:
   *   halfThreshold = BaudRate - (BaudRate / 2U)
   *   - Với BaudRate chẵn (vd 10): halfThreshold = 5 -> rem >= 5 làm tròn lên.
   *   - Với BaudRate lẻ   (vd 11): halfThreshold = 6 -> rem >= 6 làm tròn lên
   *     (tương đương floor((BusClockHz + 5) / 11)).
   * Phép tính này KHÔNG BAO GIỜ tràn uint32 với mọi BusClockHz, BaudRate >= 1,
   * đồng thời tự cộng nhớ (carry) từ DIV_Fraction sang DIV_Mantissa khi phần
   * lẻ làm tròn lên 16.
   */
  quot = BusClockHz / BaudRate;
  rem = BusClockHz % BaudRate;
  halfThreshold = BaudRate - (BaudRate / 2UL);

  if (rem >= halfThreshold) {
    if (quot == 0xFFFFFFFFUL) {
      return UART_STATUS_BAUD_OUT_OF_RANGE;
    }
    quot++;
  }

  if ((quot < UART_BRR_MIN_VALUE) || (quot > UART_BRR_MAX_VALUE)) {
    return UART_STATUS_BAUD_OUT_OF_RANGE;
  }

  *BrrOut = (uint16)quot;

  if (ActualBaudOut != NULL_PTR) {
    uint32 actualQuot = BusClockHz / quot;
    uint32 actualRem = BusClockHz % quot;
    uint32 actualHalf = quot - (quot / 2UL);

    if ((actualRem >= actualHalf) && (actualQuot < 0xFFFFFFFFUL)) {
      actualQuot++;
    }
    *ActualBaudOut = actualQuot;
  }

  return UART_STATUS_OK;
}

Uart_StatusType
Uart_Calc_ValidateFrame(Uart_DataBitsType DataBits, Uart_ParityType Parity,
                        Uart_StopBitsType StopBits, boolean Support9Bit,
                        uint16 *DataMaskOut, boolean *UseM9BitOut) {
  if ((DataMaskOut == NULL_PTR) || (UseM9BitOut == NULL_PTR)) {
    return UART_STATUS_INVALID_ARG;
  }

  if ((StopBits != UART_STOP_BITS_1) && (StopBits != UART_STOP_BITS_2)) {
    return UART_STATUS_INVALID_ARG;
  }

  if ((Parity != UART_PARITY_NONE) && (Parity != UART_PARITY_EVEN) &&
      (Parity != UART_PARITY_ODD)) {
    return UART_STATUS_INVALID_ARG;
  }

  /*
   * Giải thích cách Parity ảnh hưởng độ rộng khung trên STM32F1 (RM0008 Bảng
   * 191): Bit CR1.M chọn TỔNG số bit giữa Start và Stop (gồm cả bit Parity nếu
   * PCE=1):
   *   - M = 0 (8 bit tổng):
   *       + PCE = 0 -> 8 bit dữ liệu hữu ích (mặt nạ 0x00FF)
   *       + PCE = 1 -> 7 bit dữ liệu hữu ích + 1 bit parity ở bit 7 (mặt nạ
   * 0x007F)
   *   - M = 1 (9 bit tổng):
   *       + PCE = 0 -> 9 bit dữ liệu hữu ích (mặt nạ 0x01FF)
   *       + PCE = 1 -> 8 bit dữ liệu hữu ích + 1 bit parity ở bit 8 (mặt nạ
   * 0x00FF) Do đó 7N (cần 7 bit tổng) và 9E/9O (cần 10 bit tổng) KHÔNG biểu
   * diễn được trên phần cứng.
   */
  switch (DataBits) {
  case UART_DATA_BITS_7:
    if (Parity == UART_PARITY_NONE) {
      return UART_STATUS_UNSUPPORTED;
    }
    *UseM9BitOut = FALSE;
    *DataMaskOut = 0x007FU;
    break;

  case UART_DATA_BITS_8:
    *UseM9BitOut = (Parity == UART_PARITY_NONE) ? FALSE : TRUE;
    *DataMaskOut = 0x00FFU;
    break;

  case UART_DATA_BITS_9:
    if (Parity != UART_PARITY_NONE) {
      return UART_STATUS_UNSUPPORTED;
    }
    if (Support9Bit == FALSE) {
      return UART_STATUS_UNSUPPORTED;
    }
    *UseM9BitOut = TRUE;
    *DataMaskOut = 0x01FFU;
    break;

  default:
    return UART_STATUS_INVALID_ARG;
  }

  return UART_STATUS_OK;
}

Uart_StatusType Uart_Calc_ValidateConfig(const Uart_PortConfigType *Config,
                                         const Uart_HwCapsType *Caps,
                                         uint32 ResolvedBusClockHz,
                                         Uart_ValidatedParamsType *ParamsOut) {
  Uart_StatusType status;

  if ((Config == NULL_PTR) || (Caps == NULL_PTR) || (ParamsOut == NULL_PTR)) {
    return UART_STATUS_INVALID_ARG;
  }

  /* 1. Kiểm tra phần cứng thực sự có ngoại vi USART này không (chặn USART3 trên
   * C6T6) */
  if (Caps->Available == FALSE) {
    return UART_STATUS_UNSUPPORTED;
  }

  /* 2. Kiểm tra chế độ TX và RX */
  if ((Config->TxMode > UART_XFER_DMA) || (Config->RxMode > UART_XFER_DMA)) {
    return UART_STATUS_INVALID_ARG;
  }

  if ((Config->TxMode == UART_XFER_DISABLED) &&
      (Config->RxMode == UART_XFER_DISABLED)) {
    return UART_STATUS_INVALID_ARG;
  }

  /* 3. Kiểm tra hỗ trợ DMA nếu bất kỳ chiều nào chọn UART_XFER_DMA */
  if (Config->TxMode == UART_XFER_DMA) {
    if ((Caps->DmaCompiledIn == FALSE) || (Caps->HasDmaTx == FALSE)) {
      return UART_STATUS_UNSUPPORTED;
    }
    if (Config->DmaTxPriority > UART_DMA_PRIO_VERY_HIGH) {
      return UART_STATUS_INVALID_ARG;
    }
  }

  if (Config->RxMode == UART_XFER_DMA) {
    if ((Caps->DmaCompiledIn == FALSE) || (Caps->HasDmaRx == FALSE)) {
      return UART_STATUS_UNSUPPORTED;
    }
    if (Config->DmaRxPriority > UART_DMA_PRIO_VERY_HIGH) {
      return UART_STATUS_INVALID_ARG;
    }
  }

  /* 4. Kiểm tra điều khiển luồng phần cứng RTS/CTS */
  if (Config->FlowControl > UART_FLOW_RTS_CTS) {
    return UART_STATUS_INVALID_ARG;
  }

  if (Config->FlowControl != UART_FLOW_NONE) {
    if (Caps->HasRtsCts == FALSE) {
      return UART_STATUS_UNSUPPORTED;
    }

    /* CTS điều tiết chiều phát (TX); RTS điều tiết chiều thu (RX) */
    if (((Config->FlowControl == UART_FLOW_CTS) ||
         (Config->FlowControl == UART_FLOW_RTS_CTS)) &&
        (Config->TxMode == UART_XFER_DISABLED)) {
      return UART_STATUS_INVALID_ARG;
    }

    if (((Config->FlowControl == UART_FLOW_RTS) ||
         (Config->FlowControl == UART_FLOW_RTS_CTS)) &&
        (Config->RxMode == UART_XFER_DISABLED)) {
      return UART_STATUS_INVALID_ARG;
    }
  }

  /* 5. Kiểm tra tính khả dụng của bộ đệm vòng khi dùng chế độ IRQ */
  if (Config->TxMode == UART_XFER_IRQ) {
    if (Config->ExtTxBuffer != NULL_PTR) {
      if (Config->ExtTxBufferSlots < 2U) {
        return UART_STATUS_INVALID_ARG;
      }
    } else if (Caps->StaticTxSlots < 2U) {
      return UART_STATUS_INVALID_ARG;
    } else {
      /* Dùng bộ đệm tĩnh mặc định hợp lệ */
    }
  }

  if (Config->RxMode == UART_XFER_IRQ) {
    if (Config->ExtRxBuffer != NULL_PTR) {
      if (Config->ExtRxBufferSlots < 2U) {
        return UART_STATUS_INVALID_ARG;
      }
    } else if (Caps->StaticRxSlots < 2U) {
      return UART_STATUS_INVALID_ARG;
    } else {
      /* Dùng bộ đệm tĩnh mặc định hợp lệ */
    }
  }

  /* 6. Kiểm tra khung dữ liệu (7/8/9 bit + parity + stop bit) */
  status = Uart_Calc_ValidateFrame(
      Config->DataBits, Config->Parity, Config->StopBits, Caps->Support9Bit,
      &ParamsOut->DataMask, &ParamsOut->Use9BitFrame);
  if (status != UART_STATUS_OK) {
    return status;
  }

  ParamsOut->Use16BitDma =
      (Config->DataBits == UART_DATA_BITS_9) ? TRUE : FALSE;

  /* 7. Tính và kiểm biên thanh ghi BRR từ tần số bus thực tế */
  status = Uart_Calc_BaudBrr(ResolvedBusClockHz, Config->BaudRate,
                             &ParamsOut->BrrValue, NULL_PTR);
  return status;
}

boolean Uart_Calc_IsTickExpired(uint32 StartTick, uint32 NowTick,
                                uint32 Timeout) {
  /*
   * Phép trừ không dấu (uint32)(NowTick - StartTick) luôn cho khoảng thời gian
   * đã trôi qua chính xác kể cả khi NowTick vừa quay vòng từ 0xFFFFFFFF về 0,
   * miễn là Timeout < 2^31 (tương đương ~24.8 ngày nếu 1 tick = 1 ms).
   */
  uint32 elapsed = (uint32)(NowTick - StartTick);
  return (elapsed >= Timeout) ? TRUE : FALSE;
}