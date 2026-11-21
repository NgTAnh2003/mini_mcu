/**
 * @file    Uart_RingBuf.c
 * @brief   [Nội bộ] Hiện thực bộ đệm vòng SPSC có rào chắn bộ nhớ (memory
 * barrier).
 */

#include "Uart_RingBuf.h"

/*============================================================================
 * RÀO CHẮN BỘ NHỚ (MEMORY BARRIER)
 *============================================================================*/

/*
 * Tại sao chỉ dùng "volatile" là CHƯA ĐỦ cho bộ đệm vòng giữa main và ISR?
 * - volatile đảm bảo trình biên dịch không cache Head/Tail vào thanh ghi CPU,
 *   nhưng KHÔNG cấm trình biên dịch sắp xếp lại lệnh ghi vào mảng Storage[head]
 *   (không volatile) ra SAU lệnh cập nhật Head!
 * - Nếu Head được cập nhật trước khi dữ liệu thực sự nằm trong Storage[head] và
 *   ISR ngắt đúng giữa hai lệnh đó, Consumer sẽ đọc phải dữ liệu rác cũ.
 * - Rào chắn bộ nhớ (dmb sy trên Cortex-M3 / compiler barrier trên host test)
 *   bắt buộc mọi thao tác đọc/ghi vào Storage hoàn tất TRƯỚC khi công bố chỉ số
 *   Head hoặc Tail mới.
 */
#if defined(__ARM_ARCH)
#define UART_RINGBUF_DMB() __asm__ volatile("dmb sy" ::: "memory")
#else
#define UART_RINGBUF_DMB() __asm__ volatile("" ::: "memory")
#endif

/**
 * @brief Tính chỉ số kế tiếp theo vòng tròn mà không dùng phép chia %.
 * @details Dùng uint32 trung gian để tránh tràn uint16 ngay cả khi Capacity ==
 * 65535.
 */
static uint16 Uart_RingBuf_NextIndex(uint16 Index, uint16 Capacity) {
  uint32 next = (uint32)Index + 1UL;
  if (next >= (uint32)Capacity) {
    next = 0UL;
  }
  return (uint16)next;
}

boolean Uart_RingBuf_Init(Uart_RingBufType *Rb, Uart_RingSlotType *Storage,
                          uint16 Capacity) {
  if ((Rb == NULL_PTR) || (Storage == NULL_PTR) || (Capacity < 2U)) {
    return FALSE;
  }

  Rb->Storage = Storage;
  Rb->Capacity = Capacity;
  Rb->Head = 0U;
  Rb->Tail = 0U;
  UART_RINGBUF_DMB();

  return TRUE;
}

void Uart_RingBuf_Reset(Uart_RingBufType *Rb) {
  if (Rb == NULL_PTR) {
    return;
  }

  Rb->Head = 0U;
  Rb->Tail = 0U;
  UART_RINGBUF_DMB();
}

boolean Uart_RingBuf_Push(Uart_RingBufType *Rb, Uart_RingSlotType Item) {
  uint16 head;
  uint16 nextHead;
  uint16 tail;

  if ((Rb == NULL_PTR) || (Rb->Storage == NULL_PTR) || (Rb->Capacity < 2U)) {
    return FALSE;
  }

  head = Rb->Head;
  nextHead = Uart_RingBuf_NextIndex(head, Rb->Capacity);
  tail = Rb->Tail;

  /* Bộ đệm đầy khi vị trí kế tiếp của Head trùng với Tail */
  if (nextHead == tail) {
    return FALSE;
  }

  /* Bước 1: Ghi dữ liệu vào ô nhớ trước */
  Rb->Storage[head] = Item;

  /* Bước 2: Rào chắn đảm bảo dữ liệu đã nằm yên trong RAM trước khi công bố
   * Head mới */
  UART_RINGBUF_DMB();

  /* Bước 3: Công bố Head mới cho Consumer thấy */
  Rb->Head = nextHead;

  return TRUE;
}

boolean Uart_RingBuf_Pop(Uart_RingBufType *Rb, Uart_RingSlotType *ItemOut) {
  uint16 tail;
  uint16 head;
  uint16 nextTail;

  if ((Rb == NULL_PTR) || (ItemOut == NULL_PTR) || (Rb->Storage == NULL_PTR) ||
      (Rb->Capacity < 2U)) {
    return FALSE;
  }

  tail = Rb->Tail;
  head = Rb->Head;

  /* Bộ đệm rỗng khi Tail == Head */
  if (tail == head) {
    return FALSE;
  }

  /* Bước 1: Rào chắn đảm bảo dữ liệu Producer ghi trước Head đều đã nhìn thấy
   */
  UART_RINGBUF_DMB();

  /* Bước 2: Đọc dữ liệu ra trước khi trả ô nhớ về cho Producer */
  *ItemOut = Rb->Storage[tail];

  /* Bước 3: Rào chắn đảm bảo đã đọc xong trước khi công bố Tail mới */
  UART_RINGBUF_DMB();

  /* Bước 4: Công bố Tail mới để Producer được phép ghi lại ô vừa đọc */
  nextTail = Uart_RingBuf_NextIndex(tail, Rb->Capacity);
  Rb->Tail = nextTail;

  return TRUE;
}

uint16 Uart_RingBuf_GetCount(const Uart_RingBufType *Rb) {
  uint16 head;
  uint16 tail;

  if ((Rb == NULL_PTR) || (Rb->Capacity < 2U)) {
    return 0U;
  }

  head = Rb->Head;
  tail = Rb->Tail;

  if (head >= tail) {
    return (uint16)(head - tail);
  }

  return (uint16)((Rb->Capacity - tail) + head);
}

uint16 Uart_RingBuf_GetFree(const Uart_RingBufType *Rb) {
  uint16 usable;
  uint16 count;

  if ((Rb == NULL_PTR) || (Rb->Capacity < 2U)) {
    return 0U;
  }

  usable = (uint16)(Rb->Capacity - 1U);
  count = Uart_RingBuf_GetCount(Rb);

  return (count <= usable) ? (uint16)(usable - count) : 0U;
}

boolean Uart_RingBuf_IsEmpty(const Uart_RingBufType *Rb) {
  if ((Rb == NULL_PTR) || (Rb->Capacity < 2U)) {
    return TRUE;
  }

  return (Rb->Head == Rb->Tail) ? TRUE : FALSE;
}