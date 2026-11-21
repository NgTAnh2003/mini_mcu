/**
 * @file    Uart.c
 * @brief   Lõi driver UART bất đồng bộ đa instance cho STM32F103 (C6T6 và
 * C8T6).
 * @details
 * Hiện thực trọn vẹn:
 * - Nhóm A: Vòng đời Init / DeInit / ReInit / GetState (kiểm tra trước khi sửa
 * thanh ghi).
 * - Nhóm B: Polling 1 byte, 1 khung (9-bit), dãy byte, dãy khung, chờ hoàn tất
 * SR.TC.
 * - Nhóm C: Truyền/nhận nền bằng ngắt USART + bộ đệm vòng SPSC (tĩnh hoặc
 * ngoài).
 * - Nhóm D: Truyền/nhận khối bằng DMA1 thông qua backend Uart_DmaIf.
 * - Nhóm E: Xử lý lỗi PE/FE/NE/ORE, tràn bộ đệm mềm và lỗi DMA theo đúng trình
 * tự RM0008.
 * - Nhóm F: Điểm vào IRQ tách khỏi bảng vector của project.
 */

#include "Uart.h"
#include "Uart_Calc.h"
#include "Uart_DmaIf.h"
#include "Uart_Hw_Stm32f1.h"
#include "Uart_RingBuf.h"
#include <stdint.h>

#if (UART_CFG_DEV_ERROR_DETECT == 1U)
#include "Det.h"
#endif

/*============================================================================
 * 1. BẢNG ÁNH XẠ PHẦN CỨNG STM32F103x6 / STM32F103xB (RM0008 & DS5792/DS5319)
 *============================================================================*/

static const Uart_HwPortDescType s_UartHwTable[UART_PORT_COUNT] = {
    /* UART_PORT_1 -> USART1 (APB2, PA9/PA10/PA11/PA12, DMA1 Ch4 TX / Ch5 RX) */
    {
        .Available = TRUE,
        .BusId = UART_BUS_APB2,
        .Instance = USART1,
        .RccEnReg = &RCC->APB2ENR,
        .RccEnMask = RCC_APB2ENR_USART1EN,
        .RccRstReg = &RCC->APB2RSTR,
        .RccRstMask = RCC_APB2RSTR_USART1RST,
        .HasRtsCts = TRUE,
        .DmaTxChannel = DMA1_Channel4,
        .DmaRxChannel = DMA1_Channel5,
        .DmaTxChannelIdx = 3U, /* Channel 4 -> chỉ số nền 0 là 3 */
        .DmaRxChannelIdx = 4U  /* Channel 5 -> chỉ số nền 0 là 4 */
    },
    /* UART_PORT_2 -> USART2 (APB1, PA2/PA3/PA0/PA1, DMA1 Ch7 TX / Ch6 RX) */
    {
        .Available = TRUE,
        .BusId = UART_BUS_APB1,
        .Instance = USART2,
        .RccEnReg = &RCC->APB1ENR,
        .RccEnMask = RCC_APB1ENR_USART2EN,
        .RccRstReg = &RCC->APB1RSTR,
        .RccRstMask = RCC_APB1RSTR_USART2RST,
        .HasRtsCts = TRUE,
        .DmaTxChannel = DMA1_Channel7,
        .DmaRxChannel = DMA1_Channel6,
        .DmaTxChannelIdx = 6U, /* Channel 7 -> chỉ số nền 0 là 6 */
        .DmaRxChannelIdx = 5U  /* Channel 6 -> chỉ số nền 0 là 5 */
    },
/* UART_PORT_3 -> USART3 (APB1, PB10/PB11/PB13/PB14, DMA1 Ch2 TX / Ch3 RX) — chỉ
 * C8T6 */
#if (UART_HAS_USART3 == 1U)
    {
        .Available = TRUE,
        .BusId = UART_BUS_APB1,
        .Instance = USART3,
        .RccEnReg = &RCC->APB1ENR,
        .RccEnMask = RCC_APB1ENR_USART3EN,
        .RccRstReg = &RCC->APB1RSTR,
        .RccRstMask = RCC_APB1RSTR_USART3RST,
        .HasRtsCts = TRUE,
        .DmaTxChannel = DMA1_Channel2,
        .DmaRxChannel = DMA1_Channel3,
        .DmaTxChannelIdx = 1U, /* Channel 2 -> chỉ số nền 0 là 1 */
        .DmaRxChannelIdx = 2U  /* Channel 3 -> chỉ số nền 0 là 2 */
    }
#else
    /* STM32F103C6T6 (STM32F103x6) KHÔNG có USART3 */
    {.Available = FALSE,
     .BusId = UART_BUS_APB1,
     .Instance = NULL_PTR,
     .RccEnReg = NULL_PTR,
     .RccEnMask = 0UL,
     .RccRstReg = NULL_PTR,
     .RccRstMask = 0UL,
     .HasRtsCts = FALSE,
     .DmaTxChannel = NULL_PTR,
     .DmaRxChannel = NULL_PTR,
     .DmaTxChannelIdx = 0U,
     .DmaRxChannelIdx = 0U}
#endif
};

const Uart_HwPortDescType *Uart_Hw_GetPortDesc(Uart_PortIdType Port) {
  if ((uint32)Port >= (uint32)UART_PORT_COUNT) {
    return NULL_PTR;
  }
  return &s_UartHwTable[Port];
}

/*============================================================================
 * 2. BỘ ĐỆM VÒNG TĨNH MẶC ĐỊNH THEO TỪNG CỔNG (KHÔNG CẤP PHÁT ĐỘNG)
 *============================================================================*/

/** @brief Mô tả vùng nhớ bộ đệm tĩnh mặc định cho mỗi cổng logic. */
typedef struct {
  Uart_RingSlotType
      *TxStorage; /**< Con trỏ mảng tĩnh TX (NULL_PTR nếu cấu hình size = 0) */
  uint16 TxSlots; /**< Tổng số ô mảng tĩnh TX (SIZE + 1) */
  Uart_RingSlotType
      *RxStorage; /**< Con trỏ mảng tĩnh RX (NULL_PTR nếu cấu hình size = 0) */
  uint16 RxSlots; /**< Tổng số ô mảng tĩnh RX (SIZE + 1) */
} Uart_StaticBufDescType;

#if (UART_CFG_PORT1_TX_BUF_SIZE > 0U)
static Uart_RingSlotType s_Port1TxStorage[UART_CFG_PORT1_TX_BUF_SIZE + 1U];
#endif
#if (UART_CFG_PORT1_RX_BUF_SIZE > 0U)
static Uart_RingSlotType s_Port1RxStorage[UART_CFG_PORT1_RX_BUF_SIZE + 1U];
#endif

#if (UART_CFG_PORT2_TX_BUF_SIZE > 0U)
static Uart_RingSlotType s_Port2TxStorage[UART_CFG_PORT2_TX_BUF_SIZE + 1U];
#endif
#if (UART_CFG_PORT2_RX_BUF_SIZE > 0U)
static Uart_RingSlotType s_Port2RxStorage[UART_CFG_PORT2_RX_BUF_SIZE + 1U];
#endif

#if (UART_HAS_USART3 == 1U)
#if (UART_CFG_PORT3_TX_BUF_SIZE > 0U)
static Uart_RingSlotType s_Port3TxStorage[UART_CFG_PORT3_TX_BUF_SIZE + 1U];
#endif
#if (UART_CFG_PORT3_RX_BUF_SIZE > 0U)
static Uart_RingSlotType s_Port3RxStorage[UART_CFG_PORT3_RX_BUF_SIZE + 1U];
#endif
#endif /* (UART_HAS_USART3 == 1U) */

static const Uart_StaticBufDescType s_StaticBufTable[UART_PORT_COUNT] = {
    /* UART_PORT_1 */
    {
#if (UART_CFG_PORT1_TX_BUF_SIZE > 0U)
        .TxStorage = s_Port1TxStorage,
        .TxSlots = (uint16)(UART_CFG_PORT1_TX_BUF_SIZE + 1U),
#else
        .TxStorage = NULL_PTR,
        .TxSlots = 0U,
#endif
#if (UART_CFG_PORT1_RX_BUF_SIZE > 0U)
        .RxStorage = s_Port1RxStorage,
        .RxSlots = (uint16)(UART_CFG_PORT1_RX_BUF_SIZE + 1U),
#else
        .RxStorage = NULL_PTR,
        .RxSlots = 0U,
#endif
    },
    /* UART_PORT_2 */
    {
#if (UART_CFG_PORT2_TX_BUF_SIZE > 0U)
        .TxStorage = s_Port2TxStorage,
        .TxSlots = (uint16)(UART_CFG_PORT2_TX_BUF_SIZE + 1U),
#else
        .TxStorage = NULL_PTR,
        .TxSlots = 0U,
#endif
#if (UART_CFG_PORT2_RX_BUF_SIZE > 0U)
        .RxStorage = s_Port2RxStorage,
        .RxSlots = (uint16)(UART_CFG_PORT2_RX_BUF_SIZE + 1U),
#else
        .RxStorage = NULL_PTR,
        .RxSlots = 0U,
#endif
    },
    /* UART_PORT_3 */
    {
#if (UART_HAS_USART3 == 1U) && (UART_CFG_PORT3_TX_BUF_SIZE > 0U)
        .TxStorage = s_Port3TxStorage,
        .TxSlots = (uint16)(UART_CFG_PORT3_TX_BUF_SIZE + 1U),
#else
        .TxStorage = NULL_PTR,
        .TxSlots = 0U,
#endif
#if (UART_HAS_USART3 == 1U) && (UART_CFG_PORT3_RX_BUF_SIZE > 0U)
        .RxStorage = s_Port3RxStorage,
        .RxSlots = (uint16)(UART_CFG_PORT3_RX_BUF_SIZE + 1U),
#else
        .RxStorage = NULL_PTR,
        .RxSlots = 0U,
#endif
    }};

/*============================================================================
 * 3. NGỮ CẢNH RUNTIME CỦA TỪNG CỔNG UART
 *============================================================================*/

/** @brief Trạng thái runtime của một chiều giao dịch DMA (TX hoặc RX). */
typedef struct {
  volatile Uart_DmaStateType
      State; /**< Trạng thái hiện tại (IDLE/BUSY/DONE/ERROR/ABORTED) */
  volatile Uart_StatusType
      LastStatus;            /**< Mã kết quả cuối cùng của giao dịch */
  volatile uint16 Requested; /**< Tổng số phần tử yêu cầu */
  volatile uint16
      Transferred; /**< Số phần tử đã chuyển (chốt khi xong/lỗi/hủy) */
} Uart_DmaDirCtxType;

/** @brief Toàn bộ trạng thái động của một cổng UART logic. */
typedef struct {
  Uart_PortStateType State;   /**< UNINIT hoặc READY */
  Uart_PortConfigType Config; /**< Bản sao cấu hình đang áp dụng */
  uint16 DataMask;          /**< Mặt nạ dữ liệu hữu ích (0x7F / 0xFF / 0x1FF) */
  Uart_RingBufType TxRing;  /**< Bộ đệm vòng TX cho chế độ IRQ */
  Uart_RingBufType RxRing;  /**< Bộ đệm vòng RX cho chế độ IRQ */
  Uart_DmaDirCtxType DmaTx; /**< Trạng thái giao dịch DMA chiều TX */
  Uart_DmaDirCtxType DmaRx; /**< Trạng thái giao dịch DMA chiều RX */
  Uart_ErrorStatsType ErrorStats; /**< Thống kê lỗi tích lũy của cổng */
} Uart_PortCtxType;

/** @brief Mảng ngữ cảnh runtime tĩnh cho tất cả các cổng. */
static Uart_PortCtxType s_UartCtx[UART_PORT_COUNT];

/** @brief Nguồn đọc tick thời gian thực do bên tích hợp đăng ký (tùy chọn). */
static Uart_GetTickFn s_UartTickProvider = NULL_PTR;

/*============================================================================
 * 4. CÁC HÀM HELPER NỘI BỘ (VÙNG GĂNG, DET, TIMEOUT, LỖI VÀ THANH GHI)
 *============================================================================*/

/**
 * @brief Vào vùng găng ngắn bằng cách lưu và khóa PRIMASK (an toàn khi lồng
 * nhau).
 * @return Giá trị PRIMASK trước khi khóa để khôi phục đúng nguyên trạng.
 */
static uint32_t Uart_EnterCritical(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

/**
 * @brief Thoát vùng găng và khôi phục nguyên trạng PRIMASK trước đó.
 */
static void Uart_ExitCritical(uint32_t SavedPrimask) {
  __set_PRIMASK(SavedPrimask);
}

/**
 * @brief Báo lỗi lập trình lên Services/Det nếu UART_CFG_DEV_ERROR_DETECT ==
 * 1U.
 * @note  CHỈ gọi cho lỗi lập trình; KHÔNG gọi cho hiện tượng vận hành (timeout,
 * busy, line error...).
 */
static void Uart_ReportDet(Uart_PortIdType Port, uint8 ApiId, uint8 ErrorId) {
#if (UART_CFG_DEV_ERROR_DETECT == 1U)
  (void)Det_ReportError((uint16)UART_CFG_MODULE_ID, (uint8)Port, ApiId,
                        ErrorId);
#else
  (void)Port;
  (void)ApiId;
  (void)ErrorId;
#endif
}

/**
 * @brief Ánh xạ mã lỗi lập trình từ Uart_Calc sang ErrorId của Det và báo lỗi
 * nếu phù hợp.
 */
static void Uart_ReportStatusIfProgError(Uart_PortIdType Port, uint8 ApiId,
                                         Uart_StatusType Status) {
  switch (Status) {
  case UART_STATUS_INVALID_ARG:
    Uart_ReportDet(Port, ApiId, UART_DET_E_PARAM_CONFIG);
    break;

  case UART_STATUS_UNSUPPORTED:
    Uart_ReportDet(Port, ApiId, UART_DET_E_UNSUPPORTED);
    break;

  case UART_STATUS_BAUD_OUT_OF_RANGE:
    Uart_ReportDet(Port, ApiId, UART_DET_E_BAUD_RANGE);
    break;

  default:
    break;
  }
}

/**
 * @brief Xóa trắng struct thống kê lỗi và đặt lại trạng thái DMA về IDLE.
 */
static void Uart_ResetRuntimeStats(Uart_PortCtxType *Ctx) {
  Ctx->ErrorStats.ParityErrorCount = 0UL;
  Ctx->ErrorStats.FramingErrorCount = 0UL;
  Ctx->ErrorStats.NoiseErrorCount = 0UL;
  Ctx->ErrorStats.HwOverrunCount = 0UL;
  Ctx->ErrorStats.SwRxOverflowCount = 0UL;
  Ctx->ErrorStats.DmaTxErrorCount = 0UL;
  Ctx->ErrorStats.DmaRxErrorCount = 0UL;
  Ctx->ErrorStats.LastErrorFlags = UART_ERR_NONE;
  Ctx->ErrorStats.StickyErrorFlags = UART_ERR_NONE;

  Ctx->DmaTx.State = UART_DMA_STATE_IDLE;
  Ctx->DmaTx.LastStatus = UART_STATUS_OK;
  Ctx->DmaTx.Requested = 0U;
  Ctx->DmaTx.Transferred = 0U;

  Ctx->DmaRx.State = UART_DMA_STATE_IDLE;
  Ctx->DmaRx.LastStatus = UART_STATUS_OK;
  Ctx->DmaRx.Requested = 0U;
  Ctx->DmaRx.Transferred = 0U;
}

/**
 * @brief Kiểm tra mã cổng hợp lệ, có tồn tại phần cứng và (tùy chọn) đã khởi
 * tạo chưa.
 */
static Uart_StatusType
Uart_ValidatePortAccess(Uart_PortIdType Port, uint8 ApiId, boolean RequireReady,
                        const Uart_HwPortDescType **HwOut,
                        Uart_PortCtxType **CtxOut) {
  const Uart_HwPortDescType *hw = Uart_Hw_GetPortDesc(Port);

  if (hw == NULL_PTR) {
    Uart_ReportDet(Port, ApiId, UART_DET_E_PARAM_PORT);
    return UART_STATUS_INVALID_ARG;
  }

  if (hw->Available == FALSE) {
    Uart_ReportDet(Port, ApiId, UART_DET_E_UNSUPPORTED);
    return UART_STATUS_UNSUPPORTED;
  }

  if ((RequireReady != FALSE) && (s_UartCtx[Port].State != UART_STATE_READY)) {
    Uart_ReportDet(Port, ApiId, UART_DET_E_UNINIT);
    return UART_STATUS_NOT_INIT;
  }

  if (HwOut != NULL_PTR) {
    *HwOut = hw;
  }
  if (CtxOut != NULL_PTR) {
    *CtxOut = &s_UartCtx[Port];
  }

  return UART_STATUS_OK;
}

/**
 * @brief Tạo struct năng lực phần cứng của cổng để truyền vào
 * Uart_Calc_ValidateConfig().
 */
static void Uart_BuildHwCaps(Uart_PortIdType Port,
                             const Uart_HwPortDescType *Hw,
                             Uart_HwCapsType *CapsOut) {
  CapsOut->Available = Hw->Available;
  CapsOut->HasRtsCts = Hw->HasRtsCts;
  CapsOut->HasDmaTx = (Hw->DmaTxChannel != NULL_PTR) ? TRUE : FALSE;
  CapsOut->HasDmaRx = (Hw->DmaRxChannel != NULL_PTR) ? TRUE : FALSE;
#if (UART_CFG_DMA_ENABLE == 1U)
  CapsOut->DmaCompiledIn = TRUE;
#else
  CapsOut->DmaCompiledIn = FALSE;
#endif
#if (UART_CFG_SUPPORT_9BIT == 1U)
  CapsOut->Support9Bit = TRUE;
#else
  CapsOut->Support9Bit = FALSE;
#endif
  CapsOut->StaticTxSlots = s_StaticBufTable[Port].TxSlots;
  CapsOut->StaticRxSlots = s_StaticBufTable[Port].RxSlots;
}

/**
 * @brief Phân giải tần số xung bus thực tế (PCLK2 cho USART1, PCLK1 cho
 * USART2/3).
 */
static uint32 Uart_ResolveBusClockHz(const Uart_HwPortDescType *Hw,
                                     const Uart_PortConfigType *Config) {
  if (Config->BusClockHz > 0UL) {
    return Config->BusClockHz;
  }

  if (Config->GetBusClock != NULL_PTR) {
    return Config->GetBusClock(Hw->BusId);
  }

  return 0UL;
}

/**
 * @brief Kiểm tra tham số timeout hợp lệ (đặc biệt khi dùng TICKS phải đã đăng
 * ký provider).
 */
static Uart_StatusType Uart_ValidateTimeout(Uart_PortIdType Port, uint8 ApiId,
                                            Uart_TimeoutType Timeout) {
  if ((Timeout.Unit != UART_TIMEOUT_UNIT_LOOPS) &&
      (Timeout.Unit != UART_TIMEOUT_UNIT_TICKS)) {
    Uart_ReportDet(Port, ApiId, UART_DET_E_PARAM_CONFIG);
    return UART_STATUS_INVALID_ARG;
  }

  if ((Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) &&
      (s_UartTickProvider == NULL_PTR)) {
    Uart_ReportDet(Port, ApiId, UART_DET_E_PARAM_CONFIG);
    return UART_STATUS_INVALID_ARG;
  }

  return UART_STATUS_OK;
}

/**
 * @brief Đánh giá bộ đếm chờ đã hết hạn hay chưa theo đơn vị LOOPS hoặc TICKS.
 */
static boolean Uart_IsWaitExpired(Uart_TimeoutType Timeout, uint32 StartTick,
                                  uint32 *LoopsLeft) {
  if (Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) {
    return Uart_Calc_IsTickExpired(StartTick, s_UartTickProvider(),
                                   Timeout.Value);
  }

  if (*LoopsLeft == 0UL) {
    return TRUE;
  }

  (*LoopsLeft)--;
  return FALSE;
}

/**
 * @brief Kiểm tra cổng có đang có giao dịch truyền/nhận dở trước khi
 * DeInit/ReInit không.
 */
static boolean Uart_IsPortBusyForLifecycle(const Uart_HwPortDescType *Hw,
                                           const Uart_PortCtxType *Ctx) {
  uint32_t primask = Uart_EnterCritical();
  boolean busy = FALSE;

  if (Ctx->Config.TxMode == UART_XFER_IRQ) {
    if ((Uart_RingBuf_IsEmpty(&Ctx->TxRing) == FALSE) ||
        ((Hw->Instance->CR1 & USART_CR1_TXEIE) != 0UL)) {
      busy = TRUE;
    }
  } else if (Ctx->Config.TxMode == UART_XFER_DMA) {
    if (Ctx->DmaTx.State == UART_DMA_STATE_BUSY) {
      busy = TRUE;
    }
  } else {
    /* Polling: kiểm tra cờ SR.TC bên dưới */
  }

  if ((Ctx->Config.RxMode == UART_XFER_DMA) &&
      (Ctx->DmaRx.State == UART_DMA_STATE_BUSY)) {
    busy = TRUE;
  }

  /* Chặn tắt/đổi baud giữa chừng khi thanh ghi dịch TX vẫn đang đẩy bit cuối ra
   * chân */
  if ((Ctx->Config.TxMode != UART_XFER_DISABLED) &&
      ((Hw->Instance->SR & USART_SR_TC) == 0UL)) {
    busy = TRUE;
  }

  Uart_ExitCritical(primask);
  return busy;
}

/**
 * @brief Giải mã mặt nạ cờ lỗi từ thanh ghi USART_SR và cộng dồn bộ đếm lỗi của
 * cổng.
 * @return Mặt nạ cờ UART_ERR_* tương ứng với lần kiểm tra này (0 nếu không có
 * lỗi).
 */
static uint16 Uart_RecordLineErrors(Uart_PortCtxType *Ctx, uint32_t SrVal) {
  uint16 flags = UART_ERR_NONE;

  if ((SrVal & USART_SR_PE) != 0UL) {
    flags |= UART_ERR_PARITY;
    Ctx->ErrorStats.ParityErrorCount++;
  }
  if ((SrVal & USART_SR_FE) != 0UL) {
    flags |= UART_ERR_FRAMING;
    Ctx->ErrorStats.FramingErrorCount++;
  }
  if ((SrVal & USART_SR_NE) != 0UL) {
    flags |= UART_ERR_NOISE;
    Ctx->ErrorStats.NoiseErrorCount++;
  }
  if ((SrVal & USART_SR_ORE) != 0UL) {
    flags |= UART_ERR_HW_OVERRUN;
    Ctx->ErrorStats.HwOverrunCount++;
  }

  if (flags != UART_ERR_NONE) {
    Ctx->ErrorStats.LastErrorFlags = flags;
    Ctx->ErrorStats.StickyErrorFlags |= flags;
  }

  return flags;
}

/**
 * @brief Ghi nhận cờ lỗi phần mềm / DMA vào thống kê của cổng.
 */
static void Uart_RecordMiscError(Uart_PortCtxType *Ctx, uint16 Flag) {
  if (Flag == UART_ERR_SW_RX_OVERFLOW) {
    Ctx->ErrorStats.SwRxOverflowCount++;
  } else if (Flag == UART_ERR_DMA_TX_TRANSFER) {
    Ctx->ErrorStats.DmaTxErrorCount++;
  } else if (Flag == UART_ERR_DMA_RX_TRANSFER) {
    Ctx->ErrorStats.DmaRxErrorCount++;
  } else {
    /* Không có bộ đếm riêng */
  }

  Ctx->ErrorStats.LastErrorFlags = Flag;
  Ctx->ErrorStats.StickyErrorFlags |= Flag;
}

/**
 * @brief Thực thi trình tự ghi thanh ghi phần cứng để khởi động một cổng UART
 * đã kiểm tra hợp lệ.
 */
static Uart_StatusType
Uart_ApplyHardwareInit(Uart_PortIdType Port, const Uart_HwPortDescType *Hw,
                       Uart_PortCtxType *Ctx, const Uart_PortConfigType *Config,
                       const Uart_ValidatedParamsType *Params) {
  USART_TypeDef *usart = Hw->Instance;
  volatile uint32_t dummyRead;
  uint32_t cr1 = 0UL;
  uint32_t cr2 = 0UL;
  uint32_t cr3 = 0UL;
  Uart_StatusType dmaStatus;

  /* 1. Bật xung ngoại vi USART trên đúng bus APB1 hoặc APB2 */
  *(Hw->RccEnReg) |= Hw->RccEnMask;
  dummyRead =
      *(Hw->RccEnReg); /* Đảm bảo xung ổn định trước khi truy cập thanh ghi */
  (void)dummyRead;

  /* 2. Xung reset mềm ngoại vi USART để xóa sạch cấu hình tồn đọng (nếu có từ
   * bootloader) */
  *(Hw->RccRstReg) |= Hw->RccRstMask;
  *(Hw->RccRstReg) &= ~(Hw->RccRstMask);

  /* 3. Khởi tạo bộ đệm vòng nếu TX hoặc RX dùng chế độ IRQ */
  if (Config->TxMode == UART_XFER_IRQ) {
    Uart_RingSlotType *storage = (Config->ExtTxBuffer != NULL_PTR)
                                     ? Config->ExtTxBuffer
                                     : s_StaticBufTable[Port].TxStorage;
    uint16 slots = (Config->ExtTxBuffer != NULL_PTR)
                       ? Config->ExtTxBufferSlots
                       : s_StaticBufTable[Port].TxSlots;

    (void)Uart_RingBuf_Init(&Ctx->TxRing, storage, slots);
  }

  if (Config->RxMode == UART_XFER_IRQ) {
    Uart_RingSlotType *storage = (Config->ExtRxBuffer != NULL_PTR)
                                     ? Config->ExtRxBuffer
                                     : s_StaticBufTable[Port].RxStorage;
    uint16 slots = (Config->ExtRxBuffer != NULL_PTR)
                       ? Config->ExtRxBufferSlots
                       : s_StaticBufTable[Port].RxSlots;

    (void)Uart_RingBuf_Init(&Ctx->RxRing, storage, slots);
  }

  /* 4. Chuẩn bị kênh DMA1 riêng nếu có chiều dùng DMA */
  if (Config->TxMode == UART_XFER_DMA) {
    dmaStatus = Uart_DmaIf_InitChannel(Port, TRUE);
    if (dmaStatus != UART_STATUS_OK) {
      *(Hw->RccEnReg) &= ~(Hw->RccEnMask);
      return dmaStatus;
    }
  }

  if (Config->RxMode == UART_XFER_DMA) {
    dmaStatus = Uart_DmaIf_InitChannel(Port, FALSE);
    if (dmaStatus != UART_STATUS_OK) {
      if (Config->TxMode == UART_XFER_DMA) {
        Uart_DmaIf_DeInitChannel(Port, TRUE);
      }
      *(Hw->RccEnReg) &= ~(Hw->RccEnMask);
      return dmaStatus;
    }
  }

  /* 5. Xây dựng thanh ghi cấu hình CR1, CR2, CR3 (chưa bật UE) */
  if (Params->Use9BitFrame != FALSE) {
    cr1 |= USART_CR1_M;
  }

  if (Config->Parity == UART_PARITY_EVEN) {
    cr1 |= USART_CR1_PCE; /* PS = 0 -> Even parity */
  } else if (Config->Parity == UART_PARITY_ODD) {
    cr1 |= (USART_CR1_PCE | USART_CR1_PS); /* PS = 1 -> Odd parity */
  } else {
    /* Không parity */
  }

  if (Config->StopBits == UART_STOP_BITS_2) {
    cr2 |= USART_CR2_STOP_1; /* STOP[13:12] = 10b -> 2 stop bit */
  }

  if ((Config->FlowControl == UART_FLOW_RTS) ||
      (Config->FlowControl == UART_FLOW_RTS_CTS)) {
    cr3 |= USART_CR3_RTSE;
  }
  if ((Config->FlowControl == UART_FLOW_CTS) ||
      (Config->FlowControl == UART_FLOW_RTS_CTS)) {
    cr3 |= USART_CR3_CTSE;
  }

  if (Config->TxMode != UART_XFER_DISABLED) {
    cr1 |= USART_CR1_TE;
  }
  if (Config->RxMode != UART_XFER_DISABLED) {
    cr1 |= USART_CR1_RE;
  }

  /*
   * Với chế độ nhận bằng ngắt (UART_XFER_IRQ):
   * - Bật RXNEIE để nhận khung mới và cả sự kiện ORE (RM0008 Bảng 196).
   * - Bật thêm PEIE nếu cấu hình có parity để bắt PE ngay cả khi cần.
   */
  if (Config->RxMode == UART_XFER_IRQ) {
    cr1 |= USART_CR1_RXNEIE;
    if (Config->Parity != UART_PARITY_NONE) {
      cr1 |= USART_CR1_PEIE;
    }
  }

  /* 6. Lưu bản sao cấu hình và đặt lại thống kê TRƯỚC khi bật ngoại vi */
  Ctx->Config = *Config;
  Ctx->DataMask = Params->DataMask;
  Uart_ResetRuntimeStats(Ctx);
  Ctx->State = UART_STATE_READY;

  /* 7. Ghi thanh ghi USART: tắt UE -> ghi BRR, CR2, CR3 -> bật UE + TE/RE/IE */
  usart->CR1 = 0UL;
  usart->BRR = (uint32_t)Params->BrrValue;
  usart->CR2 = cr2;
  usart->CR3 = cr3;
  usart->CR1 = cr1 | USART_CR1_UE;

  /*
   * Đọc xả cặp SR -> DR phòng trường hợp nhiễu chân RX khi mới bật RE tạo cờ
   * giả (chỉ xả nếu RXNE đang bật ngay lúc khởi động).
   */
  if ((usart->SR & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE |
                    USART_SR_PE)) != 0UL) {
    dummyRead = usart->DR;
    (void)dummyRead;
  }

  return UART_STATUS_OK;
}

/**
 * @brief Tắt sạch phần cứng USART và đưa ngữ cảnh cổng về UNINIT.
 */
static void Uart_ApplyHardwareDeInit(Uart_PortIdType Port,
                                     const Uart_HwPortDescType *Hw,
                                     Uart_PortCtxType *Ctx) {
  USART_TypeDef *usart = Hw->Instance;
  uint32_t primask = Uart_EnterCritical();

  /* Tắt toàn bộ nguồn ngắt, tắt DMAT/DMAR và tắt khối USART (UE=0) */
  usart->CR1 &=
      ~(USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_RXNEIE | USART_CR1_PEIE |
        USART_CR1_IDLEIE | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
  usart->CR3 &= ~(USART_CR3_DMAT | USART_CR3_DMAR | USART_CR3_EIE);

  if (Ctx->Config.TxMode == UART_XFER_DMA) {
    Uart_DmaIf_DeInitChannel(Port, TRUE);
  }
  if (Ctx->Config.RxMode == UART_XFER_DMA) {
    Uart_DmaIf_DeInitChannel(Port, FALSE);
  }

  Uart_RingBuf_Reset(&Ctx->TxRing);
  Uart_RingBuf_Reset(&Ctx->RxRing);
  Uart_ResetRuntimeStats(Ctx);
  Ctx->State = UART_STATE_UNINIT;

  /* Tắt xung ngoại vi USART để tiết kiệm năng lượng */
  *(Hw->RccEnReg) &= ~(Hw->RccEnMask);

  Uart_ExitCritical(primask);
}

/**
 * @brief Đánh thức trình phục vụ ngắt TX sau khi ứng dụng vừa đẩy dữ liệu vào
 * TxRing.
 * @details
 * Xử lý race condition khi hàng đợi TX vừa rỗng đúng lúc ứng dụng thêm dữ liệu:
 * - Bọc đoạn kiểm tra !IsEmpty và bật TXEIE trong vùng găng rất ngắn (vài lệnh
 * CPU).
 * - Nếu ISR đã chen vào ngay trước vùng găng và kịp gửi hết dữ liệu mới ->
 * IsEmpty == TRUE
 *   -> không bật lại TXEIE thừa, giữ nguyên TCIE của ISR.
 * - Nếu ISR vừa gửi xong phần tử cũ cuối cùng và tắt TXEIE trước khi vào vùng
 * găng -> IsEmpty == FALSE -> tắt TCIE (vì chưa hết dữ liệu) và bật TXEIE để
 * phát ngay.
 */
static void Uart_KickIrqTx(const Uart_HwPortDescType *Hw,
                           const Uart_PortCtxType *Ctx) {
  uint32_t primask = Uart_EnterCritical();

  if (Uart_RingBuf_IsEmpty(&Ctx->TxRing) == FALSE) {
    Hw->Instance->CR1 &= ~USART_CR1_TCIE;
    Hw->Instance->CR1 |= USART_CR1_TXEIE;
  }

  Uart_ExitCritical(primask);
}

/**
 * @brief Lõi polling gửi 1 khung (dùng chung cho byte và khung 16-bit).
 */
static Uart_StatusType Uart_PollSendSingleFrame(const Uart_HwPortDescType *Hw,
                                                const Uart_PortCtxType *Ctx,
                                                uint16 Frame,
                                                Uart_TimeoutType Timeout,
                                                uint32 StartTick) {
  USART_TypeDef *usart = Hw->Instance;
  uint32 loopsLeft = Timeout.Value;

  while ((usart->SR & USART_SR_TXE) == 0UL) {
    if (Uart_IsWaitExpired(Timeout, StartTick, &loopsLeft) != FALSE) {
      return UART_STATUS_TIMEOUT;
    }
  }

  /*
   * Ghi vào DR: chỉ có nghĩa là phần cứng đã nhận dữ liệu vào thanh ghi đệm
   * phát (Transmit Data Register), chưa có nghĩa bit stop cuối đã ra khỏi chân
   * TX. Thao tác đọc SR trước đó + ghi DR cũng tự động xóa cờ SR.TC về 0
   * (RM0008).
   */
  usart->DR = (uint32_t)(Frame & Ctx->DataMask);
  return UART_STATUS_OK;
}

/**
 * @brief Lõi polling nhận 1 khung (dùng chung cho byte và khung 16-bit).
 */
static Uart_StatusType Uart_PollRecvSingleFrame(const Uart_HwPortDescType *Hw,
                                                Uart_PortCtxType *Ctx,
                                                uint16 *FrameOut,
                                                Uart_TimeoutType Timeout,
                                                uint32 StartTick) {
  USART_TypeDef *usart = Hw->Instance;
  uint32 loopsLeft = Timeout.Value;
  const uint32_t errMask =
      (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE);

  for (;;) {
    uint32_t sr = usart->SR;

    /*
     * RM0008 mục 27.6.1:
     * Cờ PE, FE, NE, ORE được xóa bằng chuỗi: đọc USART_SR rồi đọc USART_DR.
     * Ta đọc SR ở trên; nếu có lỗi (hoặc RXNE), ta đọc DR ngay sau đây để hoàn
     * tất chuỗi xóa cờ. Nếu khung bị lỗi: ghi nhận bộ đếm lỗi, bỏ khung và trả
     * UART_STATUS_LINE_ERROR.
     */
    if ((sr & errMask) != 0UL) {
      volatile uint32_t discard = usart->DR;
      (void)discard;
      (void)Uart_RecordLineErrors(Ctx, sr);
      return UART_STATUS_LINE_ERROR;
    }

    if ((sr & USART_SR_RXNE) != 0UL) {
      *FrameOut = (uint16)(usart->DR & (uint32_t)Ctx->DataMask);
      return UART_STATUS_OK;
    }

    if (Uart_IsWaitExpired(Timeout, StartTick, &loopsLeft) != FALSE) {
      return UART_STATUS_TIMEOUT;
    }
  }
}

/**
 * @brief Lõi khởi động DMA TX dùng chung cho bản 8-bit và bản khung 16-bit.
 */
static Uart_StatusType Uart_DmaStartTxInternal(Uart_PortIdType Port,
                                               const void *Data, uint16 Count,
                                               boolean Width16Bit) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  status = Uart_ValidatePortAccess(Port, UART_API_DMA_TX, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Data == NULL_PTR) || (Count == 0U)) {
    Uart_ReportDet(Port, UART_API_DMA_TX, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

#if (UART_CFG_DMA_ENABLE == 0U)
  (void)Width16Bit;
  Uart_ReportDet(Port, UART_API_DMA_TX, UART_DET_E_UNSUPPORTED);
  return UART_STATUS_UNSUPPORTED;
#else
  uint32_t primask;
  if (ctx->Config.TxMode != UART_XFER_DMA) {
    Uart_ReportDet(Port, UART_API_DMA_TX, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  /* Chặn dùng API 8-bit cho cổng 9-bit để không làm mất bit thứ 9 */
  if ((Width16Bit == FALSE) && (ctx->Config.DataBits == UART_DATA_BITS_9)) {
    Uart_ReportDet(Port, UART_API_DMA_TX, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  primask = Uart_EnterCritical();
  if (ctx->DmaTx.State == UART_DMA_STATE_BUSY) {
    Uart_ExitCritical(primask);
    return UART_STATUS_BUSY;
  }

  ctx->DmaTx.State = UART_DMA_STATE_BUSY;
  ctx->DmaTx.LastStatus = UART_STATUS_BUSY;
  ctx->DmaTx.Requested = Count;
  ctx->DmaTx.Transferred = 0U;

  /*
   * RM0008 mục 27.3.13 (Transmission using DMA):
   * Xóa cờ SR.TC trước khi khởi động DMA để Uart_WaitTxComplete() chờ đúng bit
   * stop cuối của gói mới.
   * QUAN TRỌNG: Ghi thẳng (~USART_SR_TC) thay vì (SR &= ~USART_SR_TC) vì các
   * bit rc_w0 (RXNE, TC, CTS, LBD) chỉ bị xóa khi ghi 0 và giữ nguyên khi ghi
   * 1; dùng read-modify-write có thể vô tình xóa nhầm RXNE đang lên 1 giữa lệnh
   * đọc và ghi.
   */
  hw->Instance->SR = ~USART_SR_TC;

  status =
      Uart_DmaIf_StartTx(Port, (uint32)(uintptr_t)(&hw->Instance->DR), Data,
                         Count, Width16Bit, ctx->Config.DmaTxPriority);
  if (status != UART_STATUS_OK) {
    ctx->DmaTx.State = UART_DMA_STATE_ERROR;
    ctx->DmaTx.LastStatus = status;
    Uart_ExitCritical(primask);
    return status;
  }

  /* Bật yêu cầu DMA phát từ ngoại vi USART */
  hw->Instance->CR3 |= USART_CR3_DMAT;

  Uart_ExitCritical(primask);
  return UART_STATUS_OK;
#endif
}

/**
 * @brief Lõi khởi động DMA RX dùng chung cho bản 8-bit và bản khung 16-bit.
 */
static Uart_StatusType Uart_DmaStartRxInternal(Uart_PortIdType Port,
                                               void *Buffer, uint16 Count,
                                               boolean Width16Bit) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  status = Uart_ValidatePortAccess(Port, UART_API_DMA_RX, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Buffer == NULL_PTR) || (Count == 0U)) {
    Uart_ReportDet(Port, UART_API_DMA_RX, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

#if (UART_CFG_DMA_ENABLE == 0U)
  (void)Width16Bit;
  Uart_ReportDet(Port, UART_API_DMA_RX, UART_DET_E_UNSUPPORTED);
  return UART_STATUS_UNSUPPORTED;
#else
  uint32_t primask;
  if (ctx->Config.RxMode != UART_XFER_DMA) {
    Uart_ReportDet(Port, UART_API_DMA_RX, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  if ((Width16Bit == FALSE) && (ctx->Config.DataBits == UART_DATA_BITS_9)) {
    Uart_ReportDet(Port, UART_API_DMA_RX, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  primask = Uart_EnterCritical();
  if (ctx->DmaRx.State == UART_DMA_STATE_BUSY) {
    Uart_ExitCritical(primask);
    return UART_STATUS_BUSY;
  }

  /*
   * Xóa sạch cờ lỗi tồn đọng TRƯỚC khi bật DMA RX (tránh ngắt lỗi cũ nổ ngay
   * khi vừa bật EIE/PEIE). Lúc này DMA chưa chạy nên đọc SR->DR hoàn toàn an
   * toàn.
   */
  if ((hw->Instance->SR &
       (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) != 0UL) {
    volatile uint32_t discard = hw->Instance->DR;
    (void)discard;
  }

  ctx->DmaRx.State = UART_DMA_STATE_BUSY;
  ctx->DmaRx.LastStatus = UART_STATUS_BUSY;
  ctx->DmaRx.Requested = Count;
  ctx->DmaRx.Transferred = 0U;

  status =
      Uart_DmaIf_StartRx(Port, (uint32)(uintptr_t)(&hw->Instance->DR), Buffer,
                         Count, Width16Bit, ctx->Config.DmaRxPriority);
  if (status != UART_STATUS_OK) {
    ctx->DmaRx.State = UART_DMA_STATE_ERROR;
    ctx->DmaRx.LastStatus = status;
    Uart_ExitCritical(primask);
    return status;
  }

  /*
   * Bật DMAR cùng với EIE (bắt FE/NE/ORE trong chế độ DMA) và PEIE (nếu có
   * parity). KHÔNG bật RXNEIE khi DMA đang nhận để CPU không tranh đọc DR với
   * DMA.
   */
  hw->Instance->CR3 |= (USART_CR3_DMAR | USART_CR3_EIE);
  if (ctx->Config.Parity != UART_PARITY_NONE) {
    hw->Instance->CR1 |= USART_CR1_PEIE;
  }

  Uart_ExitCritical(primask);
  return UART_STATUS_OK;
#endif
}

/*============================================================================
 * NHÓM A: HIỆN THỰC VÒNG ĐỜI VÀ NGUỒN THỜI GIAN
 *============================================================================*/

void Uart_RegisterTickProvider(Uart_GetTickFn Provider) {
  uint32_t primask = Uart_EnterCritical();
  s_UartTickProvider = Provider;
  Uart_ExitCritical(primask);
}

Uart_StatusType Uart_Init(Uart_PortIdType Port,
                          const Uart_PortConfigType *Config) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_HwCapsType caps;
  Uart_ValidatedParamsType params;
  Uart_StatusType status;
  uint32 busClockHz;

  status = Uart_ValidatePortAccess(Port, UART_API_INIT, FALSE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (Config == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_INIT, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  if (ctx->State != UART_STATE_UNINIT) {
    Uart_ReportDet(Port, UART_API_INIT, UART_DET_E_ALREADY_INIT);
    return UART_STATUS_ALREADY_INIT;
  }

  /* Kiểm tra toàn bộ cấu hình TRƯỚC khi bật xung hay sửa thanh ghi */
  Uart_BuildHwCaps(Port, hw, &caps);
  busClockHz = Uart_ResolveBusClockHz(hw, Config);
  status = Uart_Calc_ValidateConfig(Config, &caps, busClockHz, &params);
  if (status != UART_STATUS_OK) {
    Uart_ReportStatusIfProgError(Port, UART_API_INIT, status);
    return status;
  }

  return Uart_ApplyHardwareInit(Port, hw, ctx, Config, &params);
}

Uart_StatusType Uart_DeInit(Uart_PortIdType Port) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  status = Uart_ValidatePortAccess(Port, UART_API_DEINIT, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (Uart_IsPortBusyForLifecycle(hw, ctx) != FALSE) {
    return UART_STATUS_BUSY;
  }

  Uart_ApplyHardwareDeInit(Port, hw, ctx);
  return UART_STATUS_OK;
}

Uart_StatusType Uart_ReInit(Uart_PortIdType Port,
                            const Uart_PortConfigType *Config) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_HwCapsType caps;
  Uart_ValidatedParamsType params;
  Uart_StatusType status;
  uint32 busClockHz;

  status = Uart_ValidatePortAccess(Port, UART_API_REINIT, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (Config == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_REINIT, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  /* Kiểm tra bận trước: không cắt ngang truyền/nhận đang chạy */
  if (Uart_IsPortBusyForLifecycle(hw, ctx) != FALSE) {
    return UART_STATUS_BUSY;
  }

  /* Kiểm tra cấu hình MỚI trước khi dừng cấu hình CŨ để không làm hỏng cổng khi
   * tham số sai */
  Uart_BuildHwCaps(Port, hw, &caps);
  busClockHz = Uart_ResolveBusClockHz(hw, Config);
  status = Uart_Calc_ValidateConfig(Config, &caps, busClockHz, &params);
  if (status != UART_STATUS_OK) {
    Uart_ReportStatusIfProgError(Port, UART_API_REINIT, status);
    return status;
  }

  Uart_ApplyHardwareDeInit(Port, hw, ctx);
  return Uart_ApplyHardwareInit(Port, hw, ctx, Config, &params);
}

Uart_StatusType Uart_GetState(Uart_PortIdType Port,
                              Uart_PortStateType *StateOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  if (StateOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_GET_STATE, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_GET_STATE, FALSE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  *StateOut = ctx->State;
  return UART_STATUS_OK;
}

/*============================================================================
 * NHÓM B: HIỆN THỰC TRUYỀN/NHẬN POLLING VÀ CHỜ HOÀN TẤT ĐƯỜNG TRUYỀN
 *============================================================================*/

Uart_StatusType Uart_TransmitByte(Uart_PortIdType Port, uint8 Byte,
                                  Uart_TimeoutType Timeout) {
  return Uart_TransmitPolling(Port, &Byte, 1U, NULL_PTR, Timeout);
}

Uart_StatusType Uart_ReceiveByte(Uart_PortIdType Port, uint8 *ByteOut,
                                 Uart_TimeoutType Timeout) {
  if (ByteOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_RX_POLLING, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  return Uart_ReceivePolling(Port, ByteOut, 1U, NULL_PTR, Timeout);
}

Uart_StatusType Uart_TransmitPolling(Uart_PortIdType Port, const uint8 *Data,
                                     uint16 Length, uint16 *SentOut,
                                     Uart_TimeoutType Timeout) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32 startTick = 0UL;
  uint16 i;

  if (SentOut != NULL_PTR) {
    *SentOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_TX_POLLING, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Data == NULL_PTR) || (Length == 0U)) {
    Uart_ReportDet(Port, UART_API_TX_POLLING, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidateTimeout(Port, UART_API_TX_POLLING, Timeout);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((ctx->Config.TxMode != UART_XFER_POLLING) ||
      (ctx->Config.DataBits == UART_DATA_BITS_9)) {
    Uart_ReportDet(Port, UART_API_TX_POLLING, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  if (Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) {
    startTick = s_UartTickProvider();
  }

  for (i = 0U; i < Length; i++) {
    status =
        Uart_PollSendSingleFrame(hw, ctx, (uint16)Data[i], Timeout, startTick);
    if (status != UART_STATUS_OK) {
      break;
    }
  }

  if (SentOut != NULL_PTR) {
    *SentOut = i;
  }

  return status;
}

Uart_StatusType Uart_ReceivePolling(Uart_PortIdType Port, uint8 *Buffer,
                                    uint16 Length, uint16 *ReceivedOut,
                                    Uart_TimeoutType Timeout) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32 startTick = 0UL;
  uint16 i;

  if (ReceivedOut != NULL_PTR) {
    *ReceivedOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_RX_POLLING, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Buffer == NULL_PTR) || (Length == 0U)) {
    Uart_ReportDet(Port, UART_API_RX_POLLING, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidateTimeout(Port, UART_API_RX_POLLING, Timeout);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((ctx->Config.RxMode != UART_XFER_POLLING) ||
      (ctx->Config.DataBits == UART_DATA_BITS_9)) {
    Uart_ReportDet(Port, UART_API_RX_POLLING, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  if (Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) {
    startTick = s_UartTickProvider();
  }

  for (i = 0U; i < Length; i++) {
    uint16 frame = 0U;
    status = Uart_PollRecvSingleFrame(hw, ctx, &frame, Timeout, startTick);
    if (status != UART_STATUS_OK) {
      break;
    }
    Buffer[i] = (uint8)(frame & 0x00FFU);
  }

  if (ReceivedOut != NULL_PTR) {
    *ReceivedOut = i;
  }

  return status;
}

Uart_StatusType Uart_TransmitFrame(Uart_PortIdType Port, uint16 Frame,
                                   Uart_TimeoutType Timeout) {
  return Uart_TransmitFramesPolling(Port, &Frame, 1U, NULL_PTR, Timeout);
}

Uart_StatusType Uart_ReceiveFrame(Uart_PortIdType Port, uint16 *FrameOut,
                                  Uart_TimeoutType Timeout) {
  if (FrameOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_RX_POLLING, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  return Uart_ReceiveFramesPolling(Port, FrameOut, 1U, NULL_PTR, Timeout);
}

Uart_StatusType Uart_TransmitFramesPolling(Uart_PortIdType Port,
                                           const uint16 *Frames, uint16 Count,
                                           uint16 *SentOut,
                                           Uart_TimeoutType Timeout) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32 startTick = 0UL;
  uint16 i;

  if (SentOut != NULL_PTR) {
    *SentOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_TX_POLLING, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Frames == NULL_PTR) || (Count == 0U)) {
    Uart_ReportDet(Port, UART_API_TX_POLLING, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidateTimeout(Port, UART_API_TX_POLLING, Timeout);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.TxMode != UART_XFER_POLLING) {
    Uart_ReportDet(Port, UART_API_TX_POLLING, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  if (Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) {
    startTick = s_UartTickProvider();
  }

  for (i = 0U; i < Count; i++) {
    status = Uart_PollSendSingleFrame(hw, ctx, Frames[i], Timeout, startTick);
    if (status != UART_STATUS_OK) {
      break;
    }
  }

  if (SentOut != NULL_PTR) {
    *SentOut = i;
  }

  return status;
}

Uart_StatusType Uart_ReceiveFramesPolling(Uart_PortIdType Port, uint16 *Buffer,
                                          uint16 Count, uint16 *ReceivedOut,
                                          Uart_TimeoutType Timeout) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32 startTick = 0UL;
  uint16 i;

  if (ReceivedOut != NULL_PTR) {
    *ReceivedOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_RX_POLLING, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Buffer == NULL_PTR) || (Count == 0U)) {
    Uart_ReportDet(Port, UART_API_RX_POLLING, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidateTimeout(Port, UART_API_RX_POLLING, Timeout);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.RxMode != UART_XFER_POLLING) {
    Uart_ReportDet(Port, UART_API_RX_POLLING, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  if (Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) {
    startTick = s_UartTickProvider();
  }

  for (i = 0U; i < Count; i++) {
    status = Uart_PollRecvSingleFrame(hw, ctx, &Buffer[i], Timeout, startTick);
    if (status != UART_STATUS_OK) {
      break;
    }
  }

  if (ReceivedOut != NULL_PTR) {
    *ReceivedOut = i;
  }

  return status;
}

Uart_StatusType Uart_WaitTxComplete(Uart_PortIdType Port,
                                    Uart_TimeoutType Timeout) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32 startTick = 0UL;
  uint32 loopsLeft = Timeout.Value;

  status =
      Uart_ValidatePortAccess(Port, UART_API_WAIT_TX_COMPLETE, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  status = Uart_ValidateTimeout(Port, UART_API_WAIT_TX_COMPLETE, Timeout);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.TxMode == UART_XFER_DISABLED) {
    Uart_ReportDet(Port, UART_API_WAIT_TX_COMPLETE, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  if (Timeout.Unit == UART_TIMEOUT_UNIT_TICKS) {
    startTick = s_UartTickProvider();
  }

  for (;;) {
    boolean done = TRUE;

    if ((ctx->Config.TxMode == UART_XFER_IRQ) &&
        (Uart_RingBuf_IsEmpty(&ctx->TxRing) == FALSE)) {
      done = FALSE;
    } else if ((ctx->Config.TxMode == UART_XFER_DMA) &&
               (ctx->DmaTx.State == UART_DMA_STATE_BUSY)) {
      done = FALSE;
    } else {
      /* Hàng đợi phần mềm / DMA đã xong, kiểm tra thanh ghi dịch phần cứng */
    }

    /*
     * Điểm mấu chốt:
     * - SR.TXE == 1 hay DMA TCIF == 1 chỉ báo dữ liệu cuối đã chép xong vào DR.
     * - SR.TC == 1 mới đảm bảo thanh ghi dịch đã đẩy xong bit stop cuối ra chân
     * TX.
     */
    if ((done != FALSE) && ((hw->Instance->SR & USART_SR_TC) != 0UL)) {
      return UART_STATUS_OK;
    }

    if (Uart_IsWaitExpired(Timeout, startTick, &loopsLeft) != FALSE) {
      return UART_STATUS_TIMEOUT;
    }
  }
}

/*============================================================================
 * NHÓM C: HIỆN THỰC TRUYỀN VÀ NHẬN NỀN BẰNG NGẮT + BỘ ĐỆM VÒNG
 *============================================================================*/

Uart_StatusType Uart_IrqWrite(Uart_PortIdType Port, const uint8 *Data,
                              uint16 Length, uint16 *WrittenOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint16 written = 0U;

  if (WrittenOut != NULL_PTR) {
    *WrittenOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_WRITE, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Data == NULL_PTR) || (Length == 0U)) {
    Uart_ReportDet(Port, UART_API_IRQ_WRITE, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  if ((ctx->Config.TxMode != UART_XFER_IRQ) ||
      (ctx->Config.DataBits == UART_DATA_BITS_9)) {
    Uart_ReportDet(Port, UART_API_IRQ_WRITE, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  while (written < Length) {
    Uart_RingSlotType slot =
        (Uart_RingSlotType)((uint16)Data[written] & ctx->DataMask);
    if (Uart_RingBuf_Push(&ctx->TxRing, slot) == FALSE) {
      break;
    }
    written++;
  }

  if (written > 0U) {
    Uart_KickIrqTx(hw, ctx);
  }

  if (WrittenOut != NULL_PTR) {
    *WrittenOut = written;
  }

  return (written == Length) ? UART_STATUS_OK : UART_STATUS_BUFFER_FULL;
}

Uart_StatusType Uart_IrqRead(Uart_PortIdType Port, uint8 *Buffer,
                             uint16 MaxLength, uint16 *ReadOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint16 count = 0U;

  if (ReadOut != NULL_PTR) {
    *ReadOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_READ, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Buffer == NULL_PTR) || (ReadOut == NULL_PTR) || (MaxLength == 0U)) {
    Uart_ReportDet(Port, UART_API_IRQ_READ, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  if ((ctx->Config.RxMode != UART_XFER_IRQ) ||
      (ctx->Config.DataBits == UART_DATA_BITS_9)) {
    Uart_ReportDet(Port, UART_API_IRQ_READ, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  while (count < MaxLength) {
    Uart_RingSlotType slot = 0U;
    if (Uart_RingBuf_Pop(&ctx->RxRing, &slot) == FALSE) {
      break;
    }
    Buffer[count] = (uint8)(slot & 0x00FFU);
    count++;
  }

  *ReadOut = count;
  return (count > 0U) ? UART_STATUS_OK : UART_STATUS_BUFFER_EMPTY;
}

Uart_StatusType Uart_IrqWriteFrames(Uart_PortIdType Port, const uint16 *Frames,
                                    uint16 Count, uint16 *WrittenOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint16 written = 0U;

  if (WrittenOut != NULL_PTR) {
    *WrittenOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_WRITE, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Frames == NULL_PTR) || (Count == 0U)) {
    Uart_ReportDet(Port, UART_API_IRQ_WRITE, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  if (ctx->Config.TxMode != UART_XFER_IRQ) {
    Uart_ReportDet(Port, UART_API_IRQ_WRITE, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  while (written < Count) {
    Uart_RingSlotType slot =
        (Uart_RingSlotType)(Frames[written] & ctx->DataMask);
    if (Uart_RingBuf_Push(&ctx->TxRing, slot) == FALSE) {
      break;
    }
    written++;
  }

  if (written > 0U) {
    Uart_KickIrqTx(hw, ctx);
  }

  if (WrittenOut != NULL_PTR) {
    *WrittenOut = written;
  }

  return (written == Count) ? UART_STATUS_OK : UART_STATUS_BUFFER_FULL;
}

Uart_StatusType Uart_IrqReadFrames(Uart_PortIdType Port, uint16 *Buffer,
                                   uint16 MaxCount, uint16 *ReadOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint16 count = 0U;

  if (ReadOut != NULL_PTR) {
    *ReadOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_READ, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if ((Buffer == NULL_PTR) || (ReadOut == NULL_PTR) || (MaxCount == 0U)) {
    Uart_ReportDet(Port, UART_API_IRQ_READ, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  if (ctx->Config.RxMode != UART_XFER_IRQ) {
    Uart_ReportDet(Port, UART_API_IRQ_READ, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  while (count < MaxCount) {
    Uart_RingSlotType slot = 0U;
    if (Uart_RingBuf_Pop(&ctx->RxRing, &slot) == FALSE) {
      break;
    }
    Buffer[count] = (uint16)slot;
    count++;
  }

  *ReadOut = count;
  return (count > 0U) ? UART_STATUS_OK : UART_STATUS_BUFFER_EMPTY;
}

Uart_StatusType Uart_IrqGetTxFree(Uart_PortIdType Port, uint16 *FreeOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  if (FreeOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_IRQ_QUERY, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_QUERY, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.TxMode != UART_XFER_IRQ) {
    Uart_ReportDet(Port, UART_API_IRQ_QUERY, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  *FreeOut = Uart_RingBuf_GetFree(&ctx->TxRing);
  return UART_STATUS_OK;
}

Uart_StatusType Uart_IrqGetRxAvailable(Uart_PortIdType Port,
                                       uint16 *AvailableOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  if (AvailableOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_IRQ_QUERY, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_QUERY, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.RxMode != UART_XFER_IRQ) {
    Uart_ReportDet(Port, UART_API_IRQ_QUERY, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  *AvailableOut = Uart_RingBuf_GetCount(&ctx->RxRing);
  return UART_STATUS_OK;
}

Uart_StatusType Uart_IrqFlushRx(Uart_PortIdType Port) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32_t primask;

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_FLUSH, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.RxMode != UART_XFER_IRQ) {
    Uart_ReportDet(Port, UART_API_IRQ_FLUSH, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  /* Reset cả Head và Tail trong vùng găng để không xung đột với ISR RX đang ghi
   * Head */
  primask = Uart_EnterCritical();
  Uart_RingBuf_Reset(&ctx->RxRing);
  Uart_ExitCritical(primask);

  return UART_STATUS_OK;
}

Uart_StatusType Uart_IsTxBusy(Uart_PortIdType Port, boolean *BusyOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32_t primask;
  boolean busy = FALSE;

  if (BusyOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_IRQ_QUERY, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_QUERY, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  primask = Uart_EnterCritical();
  if (ctx->Config.TxMode != UART_XFER_DISABLED) {
    if ((ctx->Config.TxMode == UART_XFER_IRQ) &&
        (Uart_RingBuf_IsEmpty(&ctx->TxRing) == FALSE)) {
      busy = TRUE;
    } else if ((ctx->Config.TxMode == UART_XFER_DMA) &&
               (ctx->DmaTx.State == UART_DMA_STATE_BUSY)) {
      busy = TRUE;
    } else if ((hw->Instance->SR & USART_SR_TC) == 0UL) {
      busy = TRUE;
    } else {
      busy = FALSE;
    }
  }
  Uart_ExitCritical(primask);

  *BusyOut = busy;
  return UART_STATUS_OK;
}

Uart_StatusType Uart_IsRxBusy(Uart_PortIdType Port, boolean *BusyOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  boolean busy = FALSE;

  if (BusyOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_IRQ_QUERY, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_IRQ_QUERY, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  if (ctx->Config.RxMode == UART_XFER_DMA) {
    busy = (ctx->DmaRx.State == UART_DMA_STATE_BUSY) ? TRUE : FALSE;
  } else if (ctx->Config.RxMode == UART_XFER_IRQ) {
    busy = (Uart_RingBuf_IsEmpty(&ctx->RxRing) == FALSE) ? TRUE : FALSE;
  } else {
    busy = FALSE;
  }

  *BusyOut = busy;
  return UART_STATUS_OK;
}

/*============================================================================
 * NHÓM D: HIỆN THỰC TRUYỀN VÀ NHẬN KHỐI BẰNG DMA
 *============================================================================*/

Uart_StatusType Uart_DmaTransmit(Uart_PortIdType Port, const uint8 *Data,
                                 uint16 Length) {
  return Uart_DmaStartTxInternal(Port, Data, Length, FALSE);
}

Uart_StatusType Uart_DmaReceive(Uart_PortIdType Port, uint8 *Buffer,
                                uint16 Length) {
  return Uart_DmaStartRxInternal(Port, Buffer, Length, FALSE);
}

Uart_StatusType Uart_DmaTransmitFrames(Uart_PortIdType Port,
                                       const uint16 *Frames, uint16 Count) {
  return Uart_DmaStartTxInternal(Port, Frames, Count, TRUE);
}

Uart_StatusType Uart_DmaReceiveFrames(Uart_PortIdType Port, uint16 *Buffer,
                                      uint16 Count) {
  return Uart_DmaStartRxInternal(Port, Buffer, Count, TRUE);
}

Uart_StatusType Uart_DmaGetTxStatus(Uart_PortIdType Port,
                                    Uart_DmaTransferInfoType *InfoOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  if (InfoOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_DMA_STATUS, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_DMA_STATUS, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

#if (UART_CFG_DMA_ENABLE == 0U)
  Uart_ReportDet(Port, UART_API_DMA_STATUS, UART_DET_E_UNSUPPORTED);
  return UART_STATUS_UNSUPPORTED;
#else
  uint32_t primask;
  if (ctx->Config.TxMode != UART_XFER_DMA) {
    Uart_ReportDet(Port, UART_API_DMA_STATUS, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  primask = Uart_EnterCritical();
  InfoOut->State = ctx->DmaTx.State;
  InfoOut->LastStatus = ctx->DmaTx.LastStatus;
  InfoOut->Requested = ctx->DmaTx.Requested;

  if (ctx->DmaTx.State == UART_DMA_STATE_BUSY) {
    uint16 rem = Uart_DmaIf_GetRemaining(Port, TRUE);
    InfoOut->Transferred = (rem <= ctx->DmaTx.Requested)
                               ? (uint16)(ctx->DmaTx.Requested - rem)
                               : 0U;
  } else {
    InfoOut->Transferred = ctx->DmaTx.Transferred;
  }
  Uart_ExitCritical(primask);

  return UART_STATUS_OK;
#endif
}

Uart_StatusType Uart_DmaGetRxStatus(Uart_PortIdType Port,
                                    Uart_DmaTransferInfoType *InfoOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;

  if (InfoOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_DMA_STATUS, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_DMA_STATUS, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

#if (UART_CFG_DMA_ENABLE == 0U)
  Uart_ReportDet(Port, UART_API_DMA_STATUS, UART_DET_E_UNSUPPORTED);
  return UART_STATUS_UNSUPPORTED;
#else
  uint32_t primask;
  if (ctx->Config.RxMode != UART_XFER_DMA) {
    Uart_ReportDet(Port, UART_API_DMA_STATUS, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  primask = Uart_EnterCritical();
  InfoOut->State = ctx->DmaRx.State;
  InfoOut->LastStatus = ctx->DmaRx.LastStatus;
  InfoOut->Requested = ctx->DmaRx.Requested;

  if (ctx->DmaRx.State == UART_DMA_STATE_BUSY) {
    uint16 rem = Uart_DmaIf_GetRemaining(Port, FALSE);
    InfoOut->Transferred = (rem <= ctx->DmaRx.Requested)
                               ? (uint16)(ctx->DmaRx.Requested - rem)
                               : 0U;
  } else {
    InfoOut->Transferred = ctx->DmaRx.Transferred;
  }
  Uart_ExitCritical(primask);

  return UART_STATUS_OK;
#endif
}

Uart_StatusType Uart_DmaAbortTx(Uart_PortIdType Port, uint16 *TransferredOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32_t primask;
  uint16 remaining = 0U;
  uint16 transferred = 0U;

  if (TransferredOut != NULL_PTR) {
    *TransferredOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_DMA_ABORT, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

#if (UART_CFG_DMA_ENABLE == 0U)
  (void)primask;
  (void)remaining;
  (void)transferred;
  Uart_ReportDet(Port, UART_API_DMA_ABORT, UART_DET_E_UNSUPPORTED);
  return UART_STATUS_UNSUPPORTED;
#else
  if (ctx->Config.TxMode != UART_XFER_DMA) {
    Uart_ReportDet(Port, UART_API_DMA_ABORT, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  primask = Uart_EnterCritical();
  if (ctx->DmaTx.State == UART_DMA_STATE_BUSY) {
    /*
     * Trình tự dừng DMA TX an toàn:
     * 1. Ngắt nguồn yêu cầu CR3.DMAT trên USART trước.
     * 2. Tắt kênh DMA và đọc CNDTR đã đóng băng.
     * 3. Chốt trạng thái ABORTED -> buffer nguồn của ứng dụng đã an toàn dùng
     * lại.
     */
    hw->Instance->CR3 &= ~USART_CR3_DMAT;
    (void)Uart_DmaIf_StopChannel(Port, TRUE, &remaining);

    transferred = (remaining <= ctx->DmaTx.Requested)
                      ? (uint16)(ctx->DmaTx.Requested - remaining)
                      : 0U;
    ctx->DmaTx.Transferred = transferred;
    ctx->DmaTx.State = UART_DMA_STATE_ABORTED;
    ctx->DmaTx.LastStatus = UART_STATUS_ABORTED;
  } else {
    transferred = ctx->DmaTx.Transferred;
  }
  Uart_ExitCritical(primask);

  if (TransferredOut != NULL_PTR) {
    *TransferredOut = transferred;
  }

  return UART_STATUS_OK;
#endif
}

Uart_StatusType Uart_DmaAbortRx(Uart_PortIdType Port, uint16 *TransferredOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32_t primask;
  uint16 remaining = 0U;
  uint16 transferred = 0U;

  if (TransferredOut != NULL_PTR) {
    *TransferredOut = 0U;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_DMA_ABORT, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

#if (UART_CFG_DMA_ENABLE == 0U)
  (void)primask;
  (void)remaining;
  (void)transferred;
  Uart_ReportDet(Port, UART_API_DMA_ABORT, UART_DET_E_UNSUPPORTED);
  return UART_STATUS_UNSUPPORTED;
#else
  if (ctx->Config.RxMode != UART_XFER_DMA) {
    Uart_ReportDet(Port, UART_API_DMA_ABORT, UART_DET_E_MODE_CONFLICT);
    return UART_STATUS_MODE_CONFLICT;
  }

  primask = Uart_EnterCritical();
  if (ctx->DmaRx.State == UART_DMA_STATE_BUSY) {
    /*
     * Trình tự dừng DMA RX an toàn:
     * 1. Tắt CR3.DMAR, CR3.EIE và CR1.PEIE để không còn request mới hay ngắt
     * lỗi chen ngang.
     * 2. Tắt kênh DMA và đọc CNDTR đã đóng băng.
     * 3. Xóa cờ lỗi tồn đọng bằng SR -> DR (lúc này DMA đã dừng nên không tranh
     * DR).
     */
    hw->Instance->CR3 &= ~(USART_CR3_DMAR | USART_CR3_EIE);
    hw->Instance->CR1 &= ~USART_CR1_PEIE;
    (void)Uart_DmaIf_StopChannel(Port, FALSE, &remaining);

    if ((hw->Instance->SR &
         (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) != 0UL) {
      volatile uint32_t discard = hw->Instance->DR;
      (void)discard;
    }

    transferred = (remaining <= ctx->DmaRx.Requested)
                      ? (uint16)(ctx->DmaRx.Requested - remaining)
                      : 0U;
    ctx->DmaRx.Transferred = transferred;
    ctx->DmaRx.State = UART_DMA_STATE_ABORTED;
    ctx->DmaRx.LastStatus = UART_STATUS_ABORTED;
  } else {
    transferred = ctx->DmaRx.Transferred;
  }
  Uart_ExitCritical(primask);

  if (TransferredOut != NULL_PTR) {
    *TransferredOut = transferred;
  }

  return UART_STATUS_OK;
#endif
}

/*============================================================================
 * NHÓM E: CHẨN ĐOÁN LỖI VÀ TÍNH TOÁN BAUD
 *============================================================================*/

Uart_StatusType Uart_GetErrorStats(Uart_PortIdType Port,
                                   Uart_ErrorStatsType *StatsOut) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32_t primask;

  if (StatsOut == NULL_PTR) {
    Uart_ReportDet(Port, UART_API_ERROR_STATS, UART_DET_E_PARAM_POINTER);
    return UART_STATUS_INVALID_ARG;
  }

  status = Uart_ValidatePortAccess(Port, UART_API_ERROR_STATS, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  /* Chụp ảnh nhất quán trong vùng găng vì ISR có thể đang tăng các bộ đếm lỗi
   */
  primask = Uart_EnterCritical();
  *StatsOut = ctx->ErrorStats;
  Uart_ExitCritical(primask);

  return UART_STATUS_OK;
}

Uart_StatusType Uart_ClearErrorStats(Uart_PortIdType Port) {
  const Uart_HwPortDescType *hw = NULL_PTR;
  Uart_PortCtxType *ctx = NULL_PTR;
  Uart_StatusType status;
  uint32_t primask;

  status = Uart_ValidatePortAccess(Port, UART_API_ERROR_STATS, TRUE, &hw, &ctx);
  if (status != UART_STATUS_OK) {
    return status;
  }

  primask = Uart_EnterCritical();
  ctx->ErrorStats.ParityErrorCount = 0UL;
  ctx->ErrorStats.FramingErrorCount = 0UL;
  ctx->ErrorStats.NoiseErrorCount = 0UL;
  ctx->ErrorStats.HwOverrunCount = 0UL;
  ctx->ErrorStats.SwRxOverflowCount = 0UL;
  ctx->ErrorStats.DmaTxErrorCount = 0UL;
  ctx->ErrorStats.DmaRxErrorCount = 0UL;
  ctx->ErrorStats.LastErrorFlags = UART_ERR_NONE;
  ctx->ErrorStats.StickyErrorFlags = UART_ERR_NONE;
  Uart_ExitCritical(primask);

  return UART_STATUS_OK;
}

Uart_StatusType Uart_ComputeBaudRate(uint32 BusClockHz, uint32 BaudRate,
                                     uint16 *BrrOut, uint32 *ActualBaudOut) {
  Uart_StatusType status =
      Uart_Calc_BaudBrr(BusClockHz, BaudRate, BrrOut, ActualBaudOut);
  if (status != UART_STATUS_OK) {
    Uart_ReportStatusIfProgError(UART_PORT_1, UART_API_COMPUTE_BAUD, status);
  }
  return status;
}

/*============================================================================
 * NHÓM F: TRÌNH PHỤC VỤ NGẮT USART VÀ DMA1 (PROJECT GỌI TỪ BẢNG VECTOR)
 *============================================================================*/

void Uart_IrqHandler(Uart_PortIdType Port) {
  const Uart_HwPortDescType *hw = Uart_Hw_GetPortDesc(Port);
  Uart_PortCtxType *ctx;
  USART_TypeDef *usart;
  uint32_t sr;
  uint32_t cr1;
  uint32_t cr3;
  const uint32_t errMask =
      (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE);

  if ((hw == NULL_PTR) || (hw->Available == FALSE)) {
    return;
  }

  ctx = &s_UartCtx[Port];
  if (ctx->State != UART_STATE_READY) {
    return;
  }

  usart = hw->Instance;
  sr = usart->SR;
  cr1 = usart->CR1;
  cr3 = usart->CR3;

  /*------------------------------------------------------------------------
   * NHÁNH 1: XỬ LÝ LỖI ĐƯỜNG TRUYỀN KHI ĐANG NHẬN BẰNG DMA RX
   *------------------------------------------------------------------------*/
#if (UART_CFG_DMA_ENABLE == 1U)
  if ((ctx->Config.RxMode == UART_XFER_DMA) &&
      (ctx->DmaRx.State == UART_DMA_STATE_BUSY) && ((sr & errMask) != 0UL) &&
      (((cr3 & USART_CR3_EIE) != 0UL) || ((cr1 & USART_CR1_PEIE) != 0UL))) {
    uint16 remaining = 0U;
    uint16 errFlags;
    volatile uint32_t discard;

    /*
     * BẮT BUỘC dừng DMA RX TRƯỚC khi đọc DR để xóa cờ lỗi:
     * nếu đọc DR khi DMA đang bật, CPU và DMA sẽ tranh nhau lấy byte từ DR.
     */
    usart->CR3 &= ~(USART_CR3_DMAR | USART_CR3_EIE);
    usart->CR1 &= ~USART_CR1_PEIE;
    (void)Uart_DmaIf_StopChannel(Port, FALSE, &remaining);

    /* Lúc này DMA đã tắt hẳn -> hoàn tất chuỗi SR (đọc ở trên) -> DR để xóa cờ
     * lỗi */
    discard = usart->DR;
    (void)discard;

    errFlags = Uart_RecordLineErrors(ctx, sr);
    ctx->DmaRx.Transferred = (remaining <= ctx->DmaRx.Requested)
                                 ? (uint16)(ctx->DmaRx.Requested - remaining)
                                 : 0U;
    ctx->DmaRx.State = UART_DMA_STATE_ERROR;
    ctx->DmaRx.LastStatus = UART_STATUS_LINE_ERROR;

    if (ctx->Config.Callback != NULL_PTR) {
      ctx->Config.Callback(Port, UART_EVENT_ERROR, errFlags,
                           ctx->Config.CallbackCtx);
    }
    return;
  }
#else
  (void)cr3;
#endif

  /*------------------------------------------------------------------------
   * NHÁNH 2: XỬ LÝ NHẬN DỮ LIỆU VÀ LỖI ĐƯỜNG TRUYỀN Ở CHẾ ĐỘ IRQ RX
   *------------------------------------------------------------------------*/
  if (ctx->Config.RxMode == UART_XFER_IRQ) {
    if ((sr & errMask) != 0UL) {
      /*
       * Chính sách khung lỗi:
       * 1. Đọc DR (sau khi đã đọc SR ở đầu hàm) để xóa sạch PE/FE/NE/ORE,
       *    ngăn ngắt lặp vô hạn.
       * 2. BỎ khung lỗi (không đẩy vào RxRing), tăng bộ đếm lỗi tương ứng.
       * 3. Báo callback UART_EVENT_ERROR.
       */
      volatile uint32_t discard = usart->DR;
      uint16 errFlags = Uart_RecordLineErrors(ctx, sr);
      (void)discard;

      if (ctx->Config.Callback != NULL_PTR) {
        ctx->Config.Callback(Port, UART_EVENT_ERROR, errFlags,
                             ctx->Config.CallbackCtx);
      }
    } else if (((sr & USART_SR_RXNE) != 0UL) &&
               ((cr1 & USART_CR1_RXNEIE) != 0UL)) {
      /* Đọc DR xóa luôn cờ RXNE; áp mặt nạ DataMask để tách đúng 7/8/9 bit hữu
       * ích */
      uint16 rawFrame = (uint16)(usart->DR & (uint32_t)ctx->DataMask);
      Uart_RingSlotType slot = (Uart_RingSlotType)rawFrame;

      if (Uart_RingBuf_Push(&ctx->RxRing, slot) != FALSE) {
        if (ctx->Config.Callback != NULL_PTR) {
          ctx->Config.Callback(Port, UART_EVENT_RX_DATA_READY, UART_ERR_NONE,
                               ctx->Config.CallbackCtx);
        }
      } else {
        /*
         * Chính sách khi bộ đệm vòng phần mềm RX đầy:
         * BỎ khung mới nhất (giữ nguyên dữ liệu cũ chưa kịp đọc), tăng bộ đếm
         * SwRxOverflowCount (phân biệt rõ với HwOverrunCount của phần cứng).
         */
        Uart_RecordMiscError(ctx, UART_ERR_SW_RX_OVERFLOW);
        if (ctx->Config.Callback != NULL_PTR) {
          ctx->Config.Callback(Port, UART_EVENT_ERROR, UART_ERR_SW_RX_OVERFLOW,
                               ctx->Config.CallbackCtx);
        }
      }
    } else {
      /* Không có sự kiện RX */
    }
  } else if ((sr & errMask) != 0UL) {
    /*
     * Chốt an toàn chống vòng ngắt lặp vô hạn nếu NVIC đang bật và phát sinh cờ
     * lỗi khi RX không ở chế độ IRQ (hoặc DMA RX vừa kết thúc trước đó 1 nhịp).
     */
    usart->CR3 &= ~USART_CR3_EIE;
    usart->CR1 &= ~(USART_CR1_PEIE | USART_CR1_RXNEIE);
    {
      volatile uint32_t discard = usart->DR;
      (void)discard;
    }
    (void)Uart_RecordLineErrors(ctx, sr);
  } else {
    /* Không có sự kiện RX */
  }

  /*------------------------------------------------------------------------
   * NHÁNH 3: XỬ LÝ PHÁT NỀN BẰNG NGẮT TXEIE VÀ HOÀN TẤT KHUNG CUỐI TCIE
   *------------------------------------------------------------------------*/
  if (ctx->Config.TxMode == UART_XFER_IRQ) {
    if (((sr & USART_SR_TXE) != 0UL) && ((cr1 & USART_CR1_TXEIE) != 0UL)) {
      Uart_RingSlotType nextSlot = 0U;

      if (Uart_RingBuf_Pop(&ctx->TxRing, &nextSlot) != FALSE) {
        usart->DR = (uint32_t)((uint16)nextSlot & ctx->DataMask);
      } else {
        /* Hàng đợi TX đã cạn: tắt TXEIE để không lặp ngắt và bật TCIE chờ bit
         * stop cuối */
        usart->CR1 &= ~USART_CR1_TXEIE;
        usart->CR1 |= USART_CR1_TCIE;
      }
    }

    if (((sr & USART_SR_TC) != 0UL) && ((cr1 & USART_CR1_TCIE) != 0UL)) {
      /*
       * Chỉ tắt TCIE, KHÔNG xóa SR.TC để Uart_WaitTxComplete() và
       * Uart_IsTxBusy() vẫn thấy đường truyền đã rảnh hoàn toàn. SR.TC sẽ tự
       * xóa khi có byte mới ghi vào DR.
       */
      usart->CR1 &= ~USART_CR1_TCIE;

      if (ctx->Config.Callback != NULL_PTR) {
        ctx->Config.Callback(Port, UART_EVENT_TX_COMPLETE, UART_ERR_NONE,
                             ctx->Config.CallbackCtx);
      }
    }
  }
}

void Uart_DmaTxIrqHandler(Uart_PortIdType Port) {
#if (UART_CFG_DMA_ENABLE == 1U)
  const Uart_HwPortDescType *hw = Uart_Hw_GetPortDesc(Port);
  Uart_PortCtxType *ctx;
  Uart_DmaIrqEventType ev;
  uint16 remaining = 0U;

  if ((hw == NULL_PTR) || (hw->Available == FALSE)) {
    return;
  }

  ctx = &s_UartCtx[Port];
  if ((ctx->State != UART_STATE_READY) ||
      (ctx->Config.TxMode != UART_XFER_DMA)) {
    return;
  }

  ev = Uart_DmaIf_HandleIrq(Port, TRUE, &remaining);
  if (ev == UART_DMA_IRQ_NONE) {
    return;
  }

  /* Tắt yêu cầu DMA TX trên USART */
  hw->Instance->CR3 &= ~USART_CR3_DMAT;

  ctx->DmaTx.Transferred = (remaining <= ctx->DmaTx.Requested)
                               ? (uint16)(ctx->DmaTx.Requested - remaining)
                               : 0U;

  if (ev == UART_DMA_IRQ_COMPLETE) {
    /*
     * LƯU Ý QUAN TRỌNG:
     * Tại thời điểm này DMA đã đọc xong phần tử cuối trong RAM và ghi vào
     * USART_DR, nên buffer RAM của ứng dụng ĐÃ AN TOÀN để dùng lại. Tuy nhiên,
     * thanh ghi dịch USART vẫn đang đẩy byte cuối ra chân TX -> nếu cần tắt
     * cổng hoặc đổi baud, ứng dụng phải gọi thêm Uart_WaitTxComplete().
     */
    ctx->DmaTx.State = UART_DMA_STATE_DONE;
    ctx->DmaTx.LastStatus = UART_STATUS_OK;

    if (ctx->Config.Callback != NULL_PTR) {
      ctx->Config.Callback(Port, UART_EVENT_DMA_TX_DONE, UART_ERR_NONE,
                           ctx->Config.CallbackCtx);
    }
  } else {
    ctx->DmaTx.State = UART_DMA_STATE_ERROR;
    ctx->DmaTx.LastStatus = UART_STATUS_DMA_ERROR;
    Uart_RecordMiscError(ctx, UART_ERR_DMA_TX_TRANSFER);

    if (ctx->Config.Callback != NULL_PTR) {
      ctx->Config.Callback(Port, UART_EVENT_ERROR, UART_ERR_DMA_TX_TRANSFER,
                           ctx->Config.CallbackCtx);
    }
  }
#else
  (void)Port;
#endif
}

void Uart_DmaRxIrqHandler(Uart_PortIdType Port) {
#if (UART_CFG_DMA_ENABLE == 1U)
  const Uart_HwPortDescType *hw = Uart_Hw_GetPortDesc(Port);
  Uart_PortCtxType *ctx;
  Uart_DmaIrqEventType ev;
  uint16 remaining = 0U;

  if ((hw == NULL_PTR) || (hw->Available == FALSE)) {
    return;
  }

  ctx = &s_UartCtx[Port];
  if ((ctx->State != UART_STATE_READY) ||
      (ctx->Config.RxMode != UART_XFER_DMA)) {
    return;
  }

  ev = Uart_DmaIf_HandleIrq(Port, FALSE, &remaining);
  if (ev == UART_DMA_IRQ_NONE) {
    return;
  }

  /* Tắt yêu cầu DMA RX và ngắt lỗi đường truyền của phiên DMA này */
  hw->Instance->CR3 &= ~(USART_CR3_DMAR | USART_CR3_EIE);
  hw->Instance->CR1 &= ~USART_CR1_PEIE;

  ctx->DmaRx.Transferred = (remaining <= ctx->DmaRx.Requested)
                               ? (uint16)(ctx->DmaRx.Requested - remaining)
                               : 0U;

  if (ev == UART_DMA_IRQ_COMPLETE) {
    ctx->DmaRx.State = UART_DMA_STATE_DONE;
    ctx->DmaRx.LastStatus = UART_STATUS_OK;

    if (ctx->Config.Callback != NULL_PTR) {
      ctx->Config.Callback(Port, UART_EVENT_DMA_RX_DONE, UART_ERR_NONE,
                           ctx->Config.CallbackCtx);
    }
  } else {
    ctx->DmaRx.State = UART_DMA_STATE_ERROR;
    ctx->DmaRx.LastStatus = UART_STATUS_DMA_ERROR;
    Uart_RecordMiscError(ctx, UART_ERR_DMA_RX_TRANSFER);

    if (ctx->Config.Callback != NULL_PTR) {
      ctx->Config.Callback(Port, UART_EVENT_ERROR, UART_ERR_DMA_RX_TRANSFER,
                           ctx->Config.CallbackCtx);
    }
  }
#else
  (void)Port;
#endif
}