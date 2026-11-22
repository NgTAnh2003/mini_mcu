/**
 * @file    Mcu_PBcfg.c
 * @brief   Dữ liệu cấu hình Post-Build mẫu cho STM32F103C6T6 và STM32F103C8T6.
 * @details Cung cấp HSI 8 MHz, HSE 8 MHz -> PLL x9 (72 MHz), chế độ
 * RUN/SLEEP/STOP, và 3 vùng RAM mẫu (1/2/4 byte) chuẩn C11 đặt tại .noinit.mcu.
 */

#include "Mcu.h"

/*============================================================================*/
/*          CÁC PHÂN VÙNG RAM MẪU (.noinit.mcu, CĂN CHỈNH & ĐÚNG KIỂU C11)    */
/*============================================================================*/

#define MCU_START_SEC_VAR_NO_INIT_UNSPECIFIED
#include "Mcu_MemMap.h"

/** @brief Vùng RAM mẫu ghi 1 byte (16 byte, effective type uint8, nằm trong
 * .noinit.mcu). */
static _Alignas(1) uint8 Mcu_SampleRamSection1B
    [MCU_SAMPLE_RAM1B_SIZE_BYTES] MCU_MEMMAP_VAR_NO_INIT_ATTR;

/** @brief Vùng RAM mẫu ghi 2 byte (32 byte, effective type uint16, nằm trong
 * .noinit.mcu). */
static _Alignas(2) uint16
    Mcu_SampleRamSection2B[MCU_SAMPLE_RAM2B_SIZE_BYTES /
                           sizeof(uint16)] MCU_MEMMAP_VAR_NO_INIT_ATTR;

/** @brief Vùng RAM mẫu ghi 4 byte (64 byte, effective type uint32, nằm trong
 * .noinit.mcu). */
static _Alignas(4) uint32
    Mcu_SampleRamSection4B[MCU_SAMPLE_RAM4B_SIZE_BYTES /
                           sizeof(uint32)] MCU_MEMMAP_VAR_NO_INIT_ATTR;

#define MCU_STOP_SEC_VAR_NO_INIT_UNSPECIFIED
#include "Mcu_MemMap.h"

_Static_assert(
    sizeof(Mcu_SampleRamSection1B) == MCU_SAMPLE_RAM1B_SIZE_BYTES,
    "Kích thước Mcu_SampleRamSection1B phải khớp MCU_SAMPLE_RAM1B_SIZE_BYTES");

_Static_assert(
    (sizeof(Mcu_SampleRamSection2B) == MCU_SAMPLE_RAM2B_SIZE_BYTES) &&
        ((MCU_SAMPLE_RAM2B_SIZE_BYTES % 2UL) == 0UL) &&
        (_Alignof(uint16) >= 2U),
    "Mcu_SampleRamSection2B phải đúng 32 byte và căn chỉnh tối thiểu 2 byte");

_Static_assert(
    (sizeof(Mcu_SampleRamSection4B) == MCU_SAMPLE_RAM4B_SIZE_BYTES) &&
        ((MCU_SAMPLE_RAM4B_SIZE_BYTES % 4UL) == 0UL) &&
        (_Alignof(uint32) >= 4U),
    "Mcu_SampleRamSection4B phải đúng 64 byte và căn chỉnh tối thiểu 4 byte");

/*============================================================================*/
/*                    BẢNG DỮ LIỆU CẤU HÌNH POST-BUILD                        */
/*============================================================================*/

#define MCU_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "Mcu_MemMap.h"

/** @brief Bảng cấu hình cây xung mẫu cho STM32F103. */
static const Mcu_ClockConfigType
    Mcu_ClockConfigTable[MCU_NUM_CLOCK_SETTINGS] MCU_MEMMAP_CONST_ATTR = {
        /* [0] HSI 8 MHz: AHB = 8 MHz, APB1 = 8 MHz, APB2 = 8 MHz, ADC = 4 MHz,
           Flash 0 WS */
        {.SysClkSource = MCU_SYSCLK_SRC_HSI,
         .HseEnable = FALSE,
         .PllEnable = FALSE,
         .PllSource = MCU_PLL_SRC_HSI_DIV2,
         .PllMul = MCU_PLL_MUL_2,
         .AhbPrescaler = MCU_AHB_DIV_1,
         .Apb1Prescaler = MCU_APB_DIV_1,
         .Apb2Prescaler = MCU_APB_DIV_1,
         .AdcPrescaler = MCU_ADC_DIV_2,
         .FlashLatency = MCU_FLASH_LATENCY_0WS,
         .FlashPrefetchEnable = TRUE},
        /* [1] HSE 8 MHz -> PLL x9 -> 72 MHz: AHB = 72, APB1 = 36, APB2 = 72,
           ADC = 12 MHz, Flash 2 WS */
        {.SysClkSource = MCU_SYSCLK_SRC_PLL,
         .HseEnable = TRUE,
         .PllEnable = TRUE,
         .PllSource = MCU_PLL_SRC_HSE_DIV1,
         .PllMul = MCU_PLL_MUL_9,
         .AhbPrescaler = MCU_AHB_DIV_1,
         .Apb1Prescaler = MCU_APB_DIV_2,
         .Apb2Prescaler = MCU_APB_DIV_1,
         .AdcPrescaler = MCU_ADC_DIV_6,
         .FlashLatency = MCU_FLASH_LATENCY_2WS,
         .FlashPrefetchEnable = TRUE}};

/** @brief Bảng cấu hình chế độ nguồn mẫu (RUN, SLEEP, STOP). */
static const Mcu_ModeConfigType
    Mcu_ModeConfigTable[MCU_NUM_MODE_SETTINGS] MCU_MEMMAP_CONST_ATTR = {
        {.PowerMode = MCU_POWER_MODE_RUN,
         .LowPowerRegulator = FALSE,
         .SleepOnExit = FALSE},
        {.PowerMode = MCU_POWER_MODE_SLEEP,
         .LowPowerRegulator = FALSE,
         .SleepOnExit = FALSE},
        {.PowerMode = MCU_POWER_MODE_STOP,
         .LowPowerRegulator = TRUE,
         .SleepOnExit = FALSE}};

/** @brief Bảng cấu hình 3 phân vùng RAM mẫu (1/2/4 byte). */
static const Mcu_RamSectionConfigType
    Mcu_RamSectionConfigTable[MCU_NUM_RAM_SECTIONS] MCU_MEMMAP_CONST_ATTR = {
        {.BaseAddress = (void *)&Mcu_SampleRamSection1B[0U],
         .SectionSize = MCU_SAMPLE_RAM1B_SIZE_BYTES,
         .DefaultValue = MCU_SAMPLE_RAM1B_DEFAULT_VALUE,
         .WriteSize = MCU_RAM_WRITE_SIZE_1BYTE},
        {.BaseAddress = (void *)&Mcu_SampleRamSection2B[0U],
         .SectionSize = MCU_SAMPLE_RAM2B_SIZE_BYTES,
         .DefaultValue = MCU_SAMPLE_RAM2B_DEFAULT_VALUE,
         .WriteSize = MCU_RAM_WRITE_SIZE_2BYTE},
        {.BaseAddress = (void *)&Mcu_SampleRamSection4B[0U],
         .SectionSize = MCU_SAMPLE_RAM4B_SIZE_BYTES,
         .DefaultValue = MCU_SAMPLE_RAM4B_DEFAULT_VALUE,
         .WriteSize = MCU_RAM_WRITE_SIZE_4BYTE}};

/** @brief Cấu trúc cấu hình Post-Build gốc truyền vào Mcu_Init(). */
const Mcu_ConfigType Mcu_Config MCU_MEMMAP_CONST_ATTR = {
    .ClockConfigPtr = &Mcu_ClockConfigTable[0U],
    .NumClockConfigs = MCU_NUM_CLOCK_SETTINGS,
    .ModeConfigPtr = &Mcu_ModeConfigTable[0U],
    .NumModeConfigs = MCU_NUM_MODE_SETTINGS,
    .RamSectionConfigPtr = &Mcu_RamSectionConfigTable[0U],
    .NumRamSections = MCU_NUM_RAM_SECTIONS,
    .ResetHwType = MCU_RESET_HW_SYSRESETREQ,
    .ClockFailureCallback = NULL_PTR};

#define MCU_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "Mcu_MemMap.h"