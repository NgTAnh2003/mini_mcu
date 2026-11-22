/**
 * @file    Mcu_MemMap.h
 * @brief   Ánh xạ phân vùng bộ nhớ GCC cho module MCAL MCU theo AUTOSAR CP
 * R20-11.
 * @details Không sử dụng include guard theo đặc tả AUTOSAR_SWS_MemoryMapping.
 *          Ánh xạ ở mức biên dịch object (.o):
 *          - MCU_START_SEC_CODE                    -> .text.mcu (gắn trực tiếp
 * lên định nghĩa hàm)
 *          - MCU_START_SEC_CONFIG_DATA_UNSPECIFIED -> .rodata.mcu
 *          - MCU_START_SEC_VAR_CLEARED_*           -> .bss.mcu (NOBITS, startup
 * xóa về 0)
 *          - MCU_START_SEC_VAR_NO_INIT_UNSPECIFIED -> .noinit.mcu (NOLOAD,
 * ngoài [_sbss, _ebss])
 */

#define MCU_MEMMAP_ERROR

#if defined(MCU_START_SEC_CODE)
#undef MCU_START_SEC_CODE
#undef MCU_MEMMAP_ERROR
#if defined(__GNUC__)
/** @brief Thuộc tính GCC đặt symbol hàm thực sự vào section .text.mcu. */
#define MCU_MEMMAP_CODE_ATTR __attribute__((section(".text.mcu")))
#else
#define MCU_MEMMAP_CODE_ATTR
#endif

#elif defined(MCU_STOP_SEC_CODE)
#undef MCU_STOP_SEC_CODE
#undef MCU_MEMMAP_ERROR
#undef MCU_MEMMAP_CODE_ATTR

#elif defined(MCU_START_SEC_CONFIG_DATA_UNSPECIFIED)
#undef MCU_START_SEC_CONFIG_DATA_UNSPECIFIED
#undef MCU_MEMMAP_ERROR
#if defined(__GNUC__)
        /** @brief Thuộc tính GCC đặt bảng cấu hình hằng vào section
         * .rodata.mcu. */
#define MCU_MEMMAP_CONST_ATTR __attribute__((section(".rodata.mcu")))
#else
#define MCU_MEMMAP_CONST_ATTR
#endif

#elif defined(MCU_STOP_SEC_CONFIG_DATA_UNSPECIFIED)
#undef MCU_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#undef MCU_MEMMAP_ERROR
#undef MCU_MEMMAP_CONST_ATTR

#elif defined(MCU_START_SEC_VAR_CLEARED_BOOLEAN)
#undef MCU_START_SEC_VAR_CLEARED_BOOLEAN
#undef MCU_MEMMAP_ERROR
#if defined(__GNUC__)
        /** @brief Thuộc tính GCC đặt biến xóa về 0 khi startup vào section
         * .bss.mcu. */
#define MCU_MEMMAP_VAR_CLEARED_ATTR __attribute__((section(".bss.mcu")))
#else
#define MCU_MEMMAP_VAR_CLEARED_ATTR
#endif

#elif defined(MCU_STOP_SEC_VAR_CLEARED_BOOLEAN)
#undef MCU_STOP_SEC_VAR_CLEARED_BOOLEAN
#undef MCU_MEMMAP_ERROR
#undef MCU_MEMMAP_VAR_CLEARED_ATTR

#elif defined(MCU_START_SEC_VAR_CLEARED_UNSPECIFIED)
#undef MCU_START_SEC_VAR_CLEARED_UNSPECIFIED
#undef MCU_MEMMAP_ERROR
#if defined(__GNUC__)
        /** @brief Thuộc tính GCC đặt biến xóa về 0 khi startup vào section
         * .bss.mcu. */
#define MCU_MEMMAP_VAR_CLEARED_ATTR __attribute__((section(".bss.mcu")))
#else
#define MCU_MEMMAP_VAR_CLEARED_ATTR
#endif

#elif defined(MCU_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
#undef MCU_STOP_SEC_VAR_CLEARED_UNSPECIFIED
#undef MCU_MEMMAP_ERROR
#undef MCU_MEMMAP_VAR_CLEARED_ATTR

#elif defined(MCU_START_SEC_VAR_NO_INIT_UNSPECIFIED)
#undef MCU_START_SEC_VAR_NO_INIT_UNSPECIFIED
#undef MCU_MEMMAP_ERROR
#if defined(__GNUC__)
        /** @brief Thuộc tính GCC đặt vùng RAM không khởi tạo bởi startup vào
         * .noinit.mcu. */
#define MCU_MEMMAP_VAR_NO_INIT_ATTR __attribute__((section(".noinit.mcu")))
#else
#define MCU_MEMMAP_VAR_NO_INIT_ATTR
#endif

#elif defined(MCU_STOP_SEC_VAR_NO_INIT_UNSPECIFIED)
#undef MCU_STOP_SEC_VAR_NO_INIT_UNSPECIFIED
#undef MCU_MEMMAP_ERROR
#undef MCU_MEMMAP_VAR_NO_INIT_ATTR
#endif

#if defined(MCU_MEMMAP_ERROR)
#error "Mcu_MemMap.h: Thiếu hoặc sai tên macro phân vùng bộ nhớ MCU."
#endif