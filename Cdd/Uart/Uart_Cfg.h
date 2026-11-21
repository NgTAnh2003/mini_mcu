/**
 * @file    Uart_Cfg.h
 * @brief   Cấu hình thời điểm biên dịch cho driver UART bất đồng bộ STM32F103.
 * @details Người tích hợp có thể sửa các macro trong file này hoặc ghi đè bằng
 * cờ biên dịch (-D...) trong Makefile mà không cần sửa mã nguồn lõi driver.
 *          File này không chứa kiểu thanh ghi phần cứng STM32.
 */

#ifndef UART_CFG_H
#define UART_CFG_H

/*============================================================================
 * 1. NHẬN DIỆN NĂNG LỰC BIẾN THỂ CHIP TẠI THỜI ĐIỂM BIÊN DỊCH
 *============================================================================*/

/**
 * @brief Khả năng tồn tại phần cứng của USART1 (có trên cả C6T6 và C8T6).
 */
#define UART_HAS_USART1 (1U)

/**
 * @brief Khả năng tồn tại phần cứng của USART2 (có trên cả C6T6 và C8T6).
 */
#define UART_HAS_USART2 (1U)

/**
 * @brief Khả năng tồn tại phần cứng của USART3.
 * @details Theo DS5792 (STM32F103x4/x6) và CMSIS stm32f103x6.h, STM32F103C6T6
 *          KHÔNG có USART3. Chỉ STM32F103xB (bao gồm C8T6) mới có USART3.
 *          Khi biên dịch test trên máy tính (không có macro STM32F103x6/xB),
 *          mặc định cho phép mô phỏng đủ 3 cổng để kiểm thử mọi nhánh logic.
 */
#if defined(STM32F103x6)
#define UART_HAS_USART3 (0U)
#define UART_HW_PORT_COUNT (2U)
#elif defined(STM32F103xB)
#define UART_HAS_USART3 (1U)
#define UART_HW_PORT_COUNT (3U)
#else
#define UART_HAS_USART3 (1U)
#define UART_HW_PORT_COUNT (3U)
#endif

/*============================================================================
 * 2. BẬT / TẮT TÍNH NĂNG Ở THỜI ĐIỂM BIÊN DỊCH
 *============================================================================*/

/**
 * @brief Bật (1U) hoặc tắt (0U) báo lỗi phát triển qua Services/Det.
 * @details Khi đặt 0U, driver không include Det.h và không gọi Det_ReportError,
 *          nhưng MỌI bước kiểm tra tham số, biên bộ nhớ và trạng thái vẫn chạy.
 */
#ifndef UART_CFG_DEV_ERROR_DETECT
#define UART_CFG_DEV_ERROR_DETECT (1U)
#endif

/**
 * @brief Mã module nội bộ của dự án dùng khi báo lỗi Det.
 * @note  Đây là mã riêng do dự án tự định nghĩa trong tầng MCAL, KHÔNG phải
 *        mã module AUTOSAR chính thức.
 */
#ifndef UART_CFG_MODULE_ID
#define UART_CFG_MODULE_ID (0xF101U)
#endif

/**
 * @brief Bật (1U) hoặc tắt (0U) backend truyền/nhận DMA.
 * @details Mặc định tắt (0U) để tiết kiệm Flash/RAM khi học và kiểm chứng
 *          polling + ngắt trước. Đặt 1U (hoặc -DUART_CFG_DMA_ENABLE=1U) để bật
 * DMA.
 */
#ifndef UART_CFG_DMA_ENABLE
#define UART_CFG_DMA_ENABLE (0U)
#endif

/**
 * @brief Bật (1U) hoặc tắt (0U) hỗ trợ khung 9 bit dữ liệu hữu ích (9N1/9N2).
 * @details Đánh đổi RAM:
 *          - 0U (mặc định tiết kiệm RAM cho C6T6): mỗi ô bộ đệm vòng chiếm 1
 * byte (uint8). Cấu hình UART_DATA_BITS_9 sẽ bị từ chối với
 * UART_STATUS_UNSUPPORTED.
 *          - 1U: mỗi ô bộ đệm vòng chiếm 2 byte (uint16) để giữ nguyên bit thứ
 * 9 xuyên suốt polling, ngắt và DMA.
 */
#ifndef UART_CFG_SUPPORT_9BIT
#define UART_CFG_SUPPORT_9BIT (0U)
#endif

/**
 * @brief Số lượt chờ tối đa khi dừng kênh DMA (tránh vòng chờ vô hạn nếu phần
 * cứng treo).
 */
#ifndef UART_CFG_DMA_DISABLE_TIMEOUT_LOOPS
#define UART_CFG_DMA_DISABLE_TIMEOUT_LOOPS (5000UL)
#endif

/*============================================================================
 * 3. KÍCH THƯỚC BỘ ĐỆM VÒNG TĨNH MẶC ĐỊNH CHO TỪNG CỔNG (ĐƠN VỊ: SỐ PHẦN TỬ)
 *============================================================================*/

/**
 * @brief Số phần tử hữu ích trong bộ đệm vòng TX mặc định của UART_PORT_1
 * (USART1).
 * @details Đặt 0U nếu không cấp phát bộ đệm tĩnh cho cổng này (tiết kiệm RAM
 * khi chỉ dùng polling/DMA hoặc dùng bộ đệm ngoài do ứng dụng cấp). Khi > 0U,
 * mảng tĩnh thực tế sẽ có (SIZE + 1U) phần tử để dành 1 ô phân biệt đầy/rỗng mà
 * vẫn cho người dùng đủ đúng SIZE phần tử sử dụng.
 */
#ifndef UART_CFG_PORT1_TX_BUF_SIZE
#define UART_CFG_PORT1_TX_BUF_SIZE (64U)
#endif

/**
 * @brief Số phần tử hữu ích trong bộ đệm vòng RX mặc định của UART_PORT_1
 * (USART1).
 */
#ifndef UART_CFG_PORT1_RX_BUF_SIZE
#define UART_CFG_PORT1_RX_BUF_SIZE (64U)
#endif

/**
 * @brief Số phần tử hữu ích trong bộ đệm vòng TX mặc định của UART_PORT_2
 * (USART2).
 */
#ifndef UART_CFG_PORT2_TX_BUF_SIZE
#define UART_CFG_PORT2_TX_BUF_SIZE (32U)
#endif

/**
 * @brief Số phần tử hữu ích trong bộ đệm vòng RX mặc định của UART_PORT_2
 * (USART2).
 */
#ifndef UART_CFG_PORT2_RX_BUF_SIZE
#define UART_CFG_PORT2_RX_BUF_SIZE (32U)
#endif

#if (UART_HAS_USART3 == 1U)
/**
 * @brief Số phần tử hữu ích trong bộ đệm vòng TX mặc định của UART_PORT_3
 * (USART3).
 * @note  Chỉ tồn tại khi chip có USART3 (STM32F103xB / C8T6).
 */
#ifndef UART_CFG_PORT3_TX_BUF_SIZE
#define UART_CFG_PORT3_TX_BUF_SIZE (32U)
#endif

/**
 * @brief Số phần tử hữu ích trong bộ đệm vòng RX mặc định của UART_PORT_3
 * (USART3).
 */
#ifndef UART_CFG_PORT3_RX_BUF_SIZE
#define UART_CFG_PORT3_RX_BUF_SIZE (32U)
#endif
#endif /* (UART_HAS_USART3 == 1U) */

#endif /* UART_CFG_H */