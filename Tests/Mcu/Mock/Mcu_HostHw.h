/** @file Mcu_HostHw.h
 * @brief Dùng kiểu thanh ghi và mặt nạ CMSIS của chip, chuyển truy cập sang RAM trên host.
 */
#ifndef MCU_HOST_HW_H
#define MCU_HOST_HW_H
#include "stm32f1xx.h"

extern RCC_TypeDef Mock_RccReg;
extern FLASH_TypeDef Mock_FlashReg;
extern PWR_TypeDef Mock_PwrReg;
extern SCB_Type Mock_ScbReg;

#undef RCC
#undef FLASH
#undef PWR
#undef SCB
#define RCC (&Mock_RccReg)     /**< Khối xung và reset mô phỏng. */
#define FLASH (&Mock_FlashReg) /**< Thanh ghi điều khiển Flash mô phỏng. */
#define PWR (&Mock_PwrReg)     /**< Điều khiển nguồn mô phỏng. */
#define SCB (&Mock_ScbReg)     /**< Khối điều khiển ngủ của lõi mô phỏng. */

/** @return Mặt nạ ngắt hiện tại của lõi mô phỏng. */
uint32_t Mock_GetPrimask(void);
/** @brief Chặn ngắt trong mô hình host. */
void Mock_DisableIrq(void);
/** @param value Mặt nạ ngắt cần khôi phục. */
void Mock_SetPrimask(uint32_t value);
/** @brief Cập nhật cờ phần cứng sau thao tác ghi thanh ghi. */
void Mock_DataBarrier(void);
/** @brief Mô phỏng một lần ngủ và thức dậy. */
void Mock_WaitForInterrupt(void);

#define __get_PRIMASK Mock_GetPrimask /**< Đọc mặt nạ ngắt host. */
#define __disable_irq Mock_DisableIrq /**< Khóa ngắt host. */
#define __set_PRIMASK Mock_SetPrimask /**< Khôi phục mặt nạ ngắt host. */
#undef __DSB
#define __DSB Mock_DataBarrier /**< Cho mô hình phản hồi thao tác thanh ghi. */
#undef __WFI
#define __WFI Mock_WaitForInterrupt /**< Ghi nhận trạng thái khi ngủ. */
#undef __ISB
#define __ISB() ((void)0) /**< Host không có pipeline Cortex-M. */
#undef __NOP
#define __NOP() ((void)0) /**< Lệnh rỗng trên host. */
#undef NVIC_SystemReset
#define NVIC_SystemReset() ((void)0) /**< Các ca hiện tại không yêu cầu reset host. */
#endif
