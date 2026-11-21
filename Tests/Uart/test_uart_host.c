/**
 * @file    test_uart_host.c
 * @brief   Bộ kiểm thử đơn vị trên PC cho phần độc lập phần cứng của thư viện
 * UART.
 * @details
 * Các nhóm kiểm thử:
 * 1. Tính thanh ghi BRR: ví dụ chuẩn 8 MHz / 115200 -> 0x0045, 72 MHz, làm tròn
 * nhớ từ Fraction sang Mantissa, chia cho 0, vượt biên [0x0010, 0xFFFF], chống
 * tràn uint32.
 * 2. Kiểm tra khung dữ liệu & cấu hình: 7N/9E/9O bị từ chối, 7E/7O mặt nạ 0x7F,
 * 8E/8O M=1 & mặt nạ 0xFF, 9N1 giữ bit thứ 9 và dùng DMA 16-bit, chặn USART3
 * trên C6T6, chặn DMA khi tắt biên dịch, chặn RTS/CTS thiếu chiều TX/RX, kiểm
 * tra ExtBuffer.
 * 3. Bộ đệm vòng SPSC: biên rỗng/đầy, sức chứa hữu ích (Capacity - 1), quay
 * vòng nhiều chu kỳ qua chỉ số 0, chính sách từ chối khi đầy, đặt lại Reset.
 * 4. Bảo toàn bit dữ liệu thứ 9 qua toàn bộ bộ đệm vòng khi bật
 * UART_CFG_SUPPORT_9BIT=1U.
 * 5. Kiểm tra hết hạn timeout tick khi bộ đếm 32-bit quay vòng từ 0xFFFFFFFF về
 * 0.
 */

#include "Uart_Calc.h"
#include "Uart_RingBuf.h"
#include <stdio.h>
#include <stdlib.h>

static uint32 s_TestsRun = 0UL;
static uint32 s_TestsFailed = 0UL;

#define TEST_ASSERT(cond)                                                      \
  do {                                                                         \
    s_TestsRun++;                                                              \
    if (!(cond)) {                                                             \
      s_TestsFailed++;                                                         \
      (void)fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
    }                                                                          \
  } while (0)

/** @brief Tạo struct cấu hình cơ sở hợp lệ để biến thể trong từng test case. */
static Uart_PortConfigType MakeValidBaseConfig(void) {
  Uart_PortConfigType cfg = {.BaudRate = 115200UL,
                             .BusClockHz = 8000000UL,
                             .GetBusClock = NULL_PTR,
                             .DataBits = UART_DATA_BITS_8,
                             .Parity = UART_PARITY_NONE,
                             .StopBits = UART_STOP_BITS_1,
                             .FlowControl = UART_FLOW_NONE,
                             .TxMode = UART_XFER_POLLING,
                             .RxMode = UART_XFER_POLLING,
                             .DmaTxPriority = UART_DMA_PRIO_LOW,
                             .DmaRxPriority = UART_DMA_PRIO_LOW,
                             .ExtTxBuffer = NULL_PTR,
                             .ExtTxBufferSlots = 0U,
                             .ExtRxBuffer = NULL_PTR,
                             .ExtRxBufferSlots = 0U,
                             .Callback = NULL_PTR,
                             .CallbackCtx = NULL_PTR};
  return cfg;
}

/** @brief Tạo năng lực phần cứng mẫu cho USART1 có đầy đủ tính năng. */
static Uart_HwCapsType MakeFullCaps(void) {
  Uart_HwCapsType caps = {.Available = TRUE,
                          .HasRtsCts = TRUE,
                          .HasDmaTx = TRUE,
                          .HasDmaRx = TRUE,
                          .DmaCompiledIn = TRUE,
                          .Support9Bit = TRUE,
                          .StaticTxSlots = 65U,
                          .StaticRxSlots = 65U};
  return caps;
}

/*============================================================================
 * 1. TEST TÍNH BRR (BAUD RATE REGISTER)
 *============================================================================*/
static void Test_BaudBrrCalculation(void) {
  uint16 brr = 0U;
  uint32 actual = 0UL;
  Uart_StatusType st;

  /* Ví dụ chuẩn tính tay: USART1, 8 MHz, 115200 baud -> BRR = 0x0045 (69),
   * ~115942 bps */
  st = Uart_Calc_BaudBrr(8000000UL, 115200UL, &brr, &actual);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 0x0045U);
  TEST_ASSERT(actual == 115942UL);

  /* 72 MHz, 115200 baud -> 72000000 / 115200 = 625 = 0x0271 chính xác */
  st = Uart_Calc_BaudBrr(72000000UL, 115200UL, &brr, &actual);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 0x0271U);
  TEST_ASSERT(actual == 115200UL);

  /* 36 MHz (APB1 khi SYSCLK=72 MHz), 9600 baud -> 36000000 / 9600 = 3750 =
   * 0x0EA6 */
  st = Uart_Calc_BaudBrr(36000000UL, 9600UL, &brr, &actual);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 0x0EA6U);
  TEST_ASSERT(actual == 9600UL);

  /*
   * Test nhớ (carry) từ DIV_Fraction sang DIV_Mantissa:
   * 8,000,000 / 250,500 = 31.9361 -> Mantissa=1, Fraction=15.9361
   * Làm tròn lên 32 = 0x0020 (Mantissa=2, Fraction=0).
   */
  st = Uart_Calc_BaudBrr(8000000UL, 250500UL, &brr, NULL_PTR);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 0x0020U);

  /* Kiểm tra biên nhỏ nhất BRR = 0x0010 (f_CK / 16) */
  st = Uart_Calc_BaudBrr(8000000UL, 500000UL, &brr, NULL_PTR);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 0x0010U);

  /* Quá cao: 8,000,000 / 550,000 = 14.54 -> 15 (0x000F < 0x0010) ->
   * OUT_OF_RANGE */
  st = Uart_Calc_BaudBrr(8000000UL, 550000UL, &brr, NULL_PTR);
  TEST_ASSERT(st == UART_STATUS_BAUD_OUT_OF_RANGE);

  /* Biên lớn nhất BRR <= 0xFFFF: 72,000,000 / 1100 = 65455 (0xFFAF) -> OK */
  st = Uart_Calc_BaudBrr(72000000UL, 1100UL, &brr, NULL_PTR);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 65455U);

  /* Quá thấp: 72,000,000 / 1098 = 65574 > 65535 -> OUT_OF_RANGE */
  st = Uart_Calc_BaudBrr(72000000UL, 1098UL, &brr, NULL_PTR);
  TEST_ASSERT(st == UART_STATUS_BAUD_OUT_OF_RANGE);

  /* Kiểm tra chia cho 0 và con trỏ NULL_PTR */
  TEST_ASSERT(Uart_Calc_BaudBrr(0UL, 115200UL, &brr, NULL_PTR) ==
              UART_STATUS_INVALID_ARG);
  TEST_ASSERT(Uart_Calc_BaudBrr(8000000UL, 0UL, &brr, NULL_PTR) ==
              UART_STATUS_INVALID_ARG);
  TEST_ASSERT(Uart_Calc_BaudBrr(8000000UL, 115200UL, NULL_PTR, NULL_PTR) ==
              UART_STATUS_INVALID_ARG);

  /*
   * Kiểm tra chống tràn số uint32 gần UINT32_MAX:
   * 0xFFFFFFFF / 1,000,000 = 4294.967295 -> phải làm tròn đúng thành 4295
   * (0x10C7), không bị tràn nếu cộng (BusClockHz + BaudRate / 2).
   */
  st = Uart_Calc_BaudBrr(0xFFFFFFFFUL, 1000000UL, &brr, NULL_PTR);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(brr == 4295U);
}

/*============================================================================
 * 2. TEST KIỂM TRA KHUNG DỮ LIỆU VÀ RÀNG BUỘC CẤU HÌNH
 *============================================================================*/
static void Test_FrameAndConfigValidation(void) {
  Uart_PortConfigType cfg;
  Uart_HwCapsType caps;
  Uart_ValidatedParamsType params;
  Uart_RingSlotType extBuf[4];
  Uart_StatusType st;

  /* 7N1 không tồn tại trên STM32F1 -> UNSUPPORTED */
  cfg = MakeValidBaseConfig();
  caps = MakeFullCaps();
  cfg.DataBits = UART_DATA_BITS_7;
  cfg.Parity = UART_PARITY_NONE;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_UNSUPPORTED);

  /* 7E1 hợp lệ: M=0 (8 bit tổng), mặt nạ 0x007F */
  cfg.Parity = UART_PARITY_EVEN;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(params.Use9BitFrame == FALSE);
  TEST_ASSERT(params.DataMask == 0x007FU);
  TEST_ASSERT(params.Use16BitDma == FALSE);

  /* 8E1 hợp lệ: M=1 (9 bit tổng gồm parity), mặt nạ 0x00FF, DMA 8-bit */
  cfg.DataBits = UART_DATA_BITS_8;
  cfg.Parity = UART_PARITY_ODD;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(params.Use9BitFrame == TRUE);
  TEST_ASSERT(params.DataMask == 0x00FFU);
  TEST_ASSERT(params.Use16BitDma == FALSE);

  /* 9E1 cần 10 bit tổng -> phần cứng không biểu diễn được -> UNSUPPORTED */
  cfg.DataBits = UART_DATA_BITS_9;
  cfg.Parity = UART_PARITY_EVEN;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_UNSUPPORTED);

  /* 9N1 khi Support9Bit=TRUE -> OK, mặt nạ 0x01FF, M=1, DMA 16-bit */
  cfg.Parity = UART_PARITY_NONE;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_OK);
  TEST_ASSERT(params.Use9BitFrame == TRUE);
  TEST_ASSERT(params.DataMask == 0x01FFU);
  TEST_ASSERT(params.Use16BitDma == TRUE);

  /* 9N1 khi Support9Bit=FALSE (bản build tiết kiệm RAM) -> UNSUPPORTED */
  caps.Support9Bit = FALSE;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_UNSUPPORTED);

  /* Mô phỏng USART3 trên STM32F103C6T6 (Available = FALSE) -> UNSUPPORTED */
  cfg = MakeValidBaseConfig();
  caps = MakeFullCaps();
  caps.Available = FALSE;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_UNSUPPORTED);

  /* Tắt cả TX và RX -> INVALID_ARG */
  cfg = MakeValidBaseConfig();
  caps = MakeFullCaps();
  cfg.TxMode = UART_XFER_DISABLED;
  cfg.RxMode = UART_XFER_DISABLED;
  st = Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params);
  TEST_ASSERT(st == UART_STATUS_INVALID_ARG);

  /* Bật RTS nhưng tắt RX -> INVALID_ARG; bật CTS nhưng tắt TX -> INVALID_ARG */
  cfg = MakeValidBaseConfig();
  cfg.RxMode = UART_XFER_DISABLED;
  cfg.FlowControl = UART_FLOW_RTS;
  TEST_ASSERT(Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params) ==
              UART_STATUS_INVALID_ARG);

  cfg = MakeValidBaseConfig();
  cfg.TxMode = UART_XFER_DISABLED;
  cfg.FlowControl = UART_FLOW_CTS;
  TEST_ASSERT(Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params) ==
              UART_STATUS_INVALID_ARG);

  /* Chọn DMA nhưng biên dịch với UART_CFG_DMA_ENABLE=0 -> UNSUPPORTED */
  cfg = MakeValidBaseConfig();
  caps = MakeFullCaps();
  caps.DmaCompiledIn = FALSE;
  cfg.TxMode = UART_XFER_DMA;
  TEST_ASSERT(Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params) ==
              UART_STATUS_UNSUPPORTED);

  /* Chế độ IRQ nhưng StaticSlots=0 và không cấp ExtBuffer -> INVALID_ARG */
  cfg = MakeValidBaseConfig();
  caps = MakeFullCaps();
  caps.StaticRxSlots = 0U;
  cfg.RxMode = UART_XFER_IRQ;
  TEST_ASSERT(Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params) ==
              UART_STATUS_INVALID_ARG);

  /* Cấp ExtBuffer nhưng Slots < 2 -> INVALID_ARG; Slots >= 2 -> OK */
  cfg.ExtRxBuffer = extBuf;
  cfg.ExtRxBufferSlots = 1U;
  TEST_ASSERT(Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params) ==
              UART_STATUS_INVALID_ARG);

  cfg.ExtRxBufferSlots = 4U;
  TEST_ASSERT(Uart_Calc_ValidateConfig(&cfg, &caps, cfg.BusClockHz, &params) ==
              UART_STATUS_OK);
}

/*============================================================================
 * 3. TEST BỘ ĐỆM VÒNG SPSC TẠI BIÊN ĐẦY/RỖNG VÀ KHI QUAY VÒNG
 *============================================================================*/
static void Test_RingBufferBoundariesAndWrapAround(void) {
  Uart_RingBufType rb;
  Uart_RingSlotType storage[5]; /* Capacity = 5 -> sức chứa hữu ích = 4 */
  Uart_RingSlotType out = 0U;
  uint16 cycle;
  uint16 i;

  /* Kiểm tra lỗi tham số Init */
  TEST_ASSERT(Uart_RingBuf_Init(NULL_PTR, storage, 5U) == FALSE);
  TEST_ASSERT(Uart_RingBuf_Init(&rb, NULL_PTR, 5U) == FALSE);
  TEST_ASSERT(Uart_RingBuf_Init(&rb, storage, 1U) == FALSE);

  /* Khởi tạo hợp lệ */
  TEST_ASSERT(Uart_RingBuf_Init(&rb, storage, 5U) == TRUE);
  TEST_ASSERT(Uart_RingBuf_IsEmpty(&rb) == TRUE);
  TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == 0U);
  TEST_ASSERT(Uart_RingBuf_GetFree(&rb) == 4U);
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == FALSE);

  /* Đẩy đầy đúng 4 phần tử */
  for (i = 0U; i < 4U; i++) {
    TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)(0x10U + i)) == TRUE);
    TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == (uint16)(i + 1U));
    TEST_ASSERT(Uart_RingBuf_GetFree(&rb) == (uint16)(3U - i));
  }

  /* Phần tử thứ 5 phải bị từ chối, không ghi đè dữ liệu cũ */
  TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)0x99U) == FALSE);
  TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == 4U);
  TEST_ASSERT(Uart_RingBuf_GetFree(&rb) == 0U);

  /* Đọc ra 2 phần tử để tạo khoảng trống phía đầu, sau đó đẩy tiếp gây quay
   * vòng Head */
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
  TEST_ASSERT(out == (Uart_RingSlotType)0x10U);
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
  TEST_ASSERT(out == (Uart_RingSlotType)0x11U);
  TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == 2U);
  TEST_ASSERT(Uart_RingBuf_GetFree(&rb) == 2U);

  /* Đẩy 2 phần tử mới (Head quay vòng qua chỉ số 0) */
  TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)0x20U) == TRUE);
  TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)0x21U) == TRUE);
  TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == 4U);
  TEST_ASSERT(Uart_RingBuf_GetFree(&rb) == 0U);
  TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)0x22U) == FALSE);

  /* Xác minh thứ tự FIFO sau khi quay vòng (Tail đi qua biên 4 -> 0) */
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
  TEST_ASSERT(out == (Uart_RingSlotType)0x12U);
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
  TEST_ASSERT(out == (Uart_RingSlotType)0x13U);
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
  TEST_ASSERT(out == (Uart_RingSlotType)0x20U);
  TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
  TEST_ASSERT(out == (Uart_RingSlotType)0x21U);
  TEST_ASSERT(Uart_RingBuf_IsEmpty(&rb) == TRUE);

  /* Stress test quay vòng liên tục 50 chu kỳ với bước đẩy/rút lệch 3 phần tử */
  for (cycle = 0U; cycle < 50U; cycle++) {
    uint8 base = (uint8)((cycle * 3U) & 0xFFU);
    TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)base) == TRUE);
    TEST_ASSERT(Uart_RingBuf_Push(
                    &rb, (Uart_RingSlotType)((base + 1U) & 0xFFU)) == TRUE);
    TEST_ASSERT(Uart_RingBuf_Push(
                    &rb, (Uart_RingSlotType)((base + 2U) & 0xFFU)) == TRUE);
    TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == 3U);

    TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
    TEST_ASSERT(out == (Uart_RingSlotType)base);
    TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
    TEST_ASSERT(out == (Uart_RingSlotType)((base + 1U) & 0xFFU));
    TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
    TEST_ASSERT(out == (Uart_RingSlotType)((base + 2U) & 0xFFU));
    TEST_ASSERT(Uart_RingBuf_IsEmpty(&rb) == TRUE);
  }

  /* Kiểm tra Reset */
  TEST_ASSERT(Uart_RingBuf_Push(&rb, (Uart_RingSlotType)0xAAU) == TRUE);
  Uart_RingBuf_Reset(&rb);
  TEST_ASSERT(Uart_RingBuf_IsEmpty(&rb) == TRUE);
  TEST_ASSERT(Uart_RingBuf_GetCount(&rb) == 0U);
}

/*============================================================================
 * 4. TEST BẢO TOÀN BIT DỮ LIỆU THỨ 9 (KHI UART_CFG_SUPPORT_9BIT == 1U)
 *============================================================================*/
static void Test_NineBitPreservation(void) {
#if (UART_CFG_SUPPORT_9BIT == 1U)
  Uart_RingBufType rb;
  Uart_RingSlotType storage[6];
  const uint16 vectors[5] = {0x0000U, 0x00FFU, 0x0100U, 0x015AU, 0x01FFU};
  Uart_RingSlotType out = 0U;
  uint16 i;

  TEST_ASSERT(sizeof(Uart_RingSlotType) == 2U);
  TEST_ASSERT(Uart_RingBuf_Init(&rb, storage, 6U) == TRUE);

  /* Đẩy 5 khung chứa đủ các tổ hợp bit thứ 9 (bit 8 = 0 và bit 8 = 1) */
  for (i = 0U; i < 5U; i++) {
    TEST_ASSERT(Uart_RingBuf_Push(
                    &rb, (Uart_RingSlotType)(vectors[i] & 0x01FFU)) == TRUE);
  }
  TEST_ASSERT(Uart_RingBuf_GetFree(&rb) == 0U);

  /* Đọc ra và xác minh bit thứ 9 không bị cắt cụt */
  for (i = 0U; i < 5U; i++) {
    TEST_ASSERT(Uart_RingBuf_Pop(&rb, &out) == TRUE);
    TEST_ASSERT(out == vectors[i]);
  }
#else
  /* Bản build tối ưu RAM: mỗi ô đệm đúng 1 byte */
  TEST_ASSERT(sizeof(Uart_RingSlotType) == 1U);
#endif
}

/*============================================================================
 * 5. TEST BỘ ĐẾM TICK TIMEOUT QUAY VÒNG 32-BIT
 *============================================================================*/
static void Test_TickWrapAround(void) {
  /* Chưa quay vòng */
  TEST_ASSERT(Uart_Calc_IsTickExpired(100UL, 109UL, 10UL) == FALSE);
  TEST_ASSERT(Uart_Calc_IsTickExpired(100UL, 110UL, 10UL) == TRUE);

  /* Quay vòng từ 0xFFFFFFFC qua 0x00000000 với Timeout = 10 tick */
  TEST_ASSERT(Uart_Calc_IsTickExpired(0xFFFFFFFCUL, 0xFFFFFFFFUL, 10UL) ==
              FALSE); /* elapsed = 3 */
  TEST_ASSERT(Uart_Calc_IsTickExpired(0xFFFFFFFCUL, 0x00000000UL, 10UL) ==
              FALSE); /* elapsed = 4 */
  TEST_ASSERT(Uart_Calc_IsTickExpired(0xFFFFFFFCUL, 0x00000005UL, 10UL) ==
              FALSE); /* elapsed = 9 */
  TEST_ASSERT(Uart_Calc_IsTickExpired(0xFFFFFFFCUL, 0x00000006UL, 10UL) ==
              TRUE); /* elapsed = 10 */

  /* Timeout = 0: luôn hết hạn ngay sau 1 lượt kiểm tra */
  TEST_ASSERT(Uart_Calc_IsTickExpired(500UL, 500UL, 0UL) == TRUE);
}

int main(void) {
  Test_BaudBrrCalculation();
  Test_FrameAndConfigValidation();
  Test_RingBufferBoundariesAndWrapAround();
  Test_NineBitPreservation();
  Test_TickWrapAround();

  (void)printf("Host UART tests (%s): %lu assertions, %lu failed.\n",
               (UART_CFG_SUPPORT_9BIT == 1U) ? "9-bit enabled"
                                             : "8-bit compact",
               (unsigned long)s_TestsRun, (unsigned long)s_TestsFailed);

  return (s_TestsFailed == 0UL) ? EXIT_SUCCESS : EXIT_FAILURE;
}