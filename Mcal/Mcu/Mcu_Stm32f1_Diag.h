/**
 * @file    Mcu_Stm32f1_Diag.h
 * @brief   [Mở rộng tích hợp STM32F1] Giao diện chẩn đoán tách riêng khỏi 11
 * API chuẩn AUTOSAR.
 * @details Phân tách rõ hai nhóm dữ liệu để an toàn khi gọi từ ISR giữa quá
 * trình chuyển xung:
 *          1. Nhóm trạng thái phần mềm đã công bố (Published): cập nhật nguyên
 * tử trong vùng khóa ngắn.
 *          2. Nhóm quan sát thanh ghi tức thời (Live): đọc trực tiếp từ RCC tại
 * thời điểm gọi hàm. Tuyệt đối không include CMSIS (stm32f1xx.h).
 */

#ifndef MCU_STM32F1_DIAG_H
#define MCU_STM32F1_DIAG_H

#include "Mcu_Types.h"
#include "Std_Types.h"

/**
 * @brief Ảnh chụp nhất quán gồm trạng thái phần mềm đã công bố và quan sát
 * thanh ghi tức thời.
 */
typedef struct {
  /* --- Nhóm 1: Trạng thái phần mềm đã công bố nguyên tử (Published software
   * state) --- */
  Mcu_ClockStateType
      ClockState; /**< Trạng thái máy xung đã công bố gần nhất. */
  Mcu_ClockFailureReasonType
      LastFailureReason; /**< Nguyên nhân lỗi phần cứng gần nhất đã công bố. */
  Mcu_FailureClassType
      LastFailureClass; /**< Nhóm lỗi (CLOCK_SOURCE hay FLASH_INTERFACE). */
  Mcu_SysClkSourceType PublishedSwSource; /**< Giá trị SW đã chốt tại lần công
                                             bố trạng thái gần nhất. */
  Mcu_SysClkSourceType PublishedSysClkSource; /**< Giá trị SWS đã chốt tại lần
                                                 công bố trạng thái gần nhất. */
  boolean PllDistributed; /**< TRUE nếu PLL đã công bố phân phối ổn định. */
  boolean PllPending;     /**< TRUE nếu còn cấu hình PLL chờ phân phối/retry. */
  boolean ActiveConfigValid; /**< TRUE nếu có ActiveClockCfgPtr hợp lệ để khôi
                                phục sau STOP. */
  boolean StopWakeupRestoreFailed; /**< TRUE nếu lần thoát STOP gần nhất gặp lỗi
                                      khôi phục. */

  /* --- Nhóm 2: Quan sát thanh ghi phần cứng tại thời điểm đọc (Live HW
   * observation) --- */
  Mcu_SysClkSourceType
      LiveSwSource; /**< Giá trị RCC->CFGR.SW đọc tức thời khi gọi hàm. */
  Mcu_SysClkSourceType
      LiveSysClkSource; /**< Giá trị RCC->CFGR.SWS đọc tức thời khi gọi hàm. */
  boolean
      LivePllLocked; /**< Trạng thái RCC->CR.PLLRDY đọc tức thời khi gọi hàm. */
} Mcu_Stm32f1_DiagStatusType;

#define MCU_START_SEC_CODE
#include "Mcu_MemMap.h"

/**
 * @brief   [Mở rộng tích hợp] Đọc ảnh chụp nhất quán trạng thái chẩn đoán MCU.
 * @details Đồng bộ, tái nhập, an toàn khi gọi từ ISR kể cả lúc Mcu_InitClock()
 * hoặc Mcu_DistributePllClock() đang chạy vòng đợi phần cứng.
 * @param   StatusPtr Con trỏ struct nhận ảnh chụp (không được NULL_PTR).
 * @return  E_OK nếu thành công; E_NOT_OK nếu StatusPtr == NULL_PTR.
 */
Std_ReturnType Mcu_Stm32f1_GetDiagStatus(Mcu_Stm32f1_DiagStatusType *StatusPtr);

#define MCU_STOP_SEC_CODE
#include "Mcu_MemMap.h"

#endif /* MCU_STM32F1_DIAG_H */