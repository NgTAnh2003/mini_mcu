/**
 * @file    Uart_Types.h
 * @brief   Các kiểu dữ liệu, enum, mã lỗi và struct cấu hình công khai của
 * driver UART.
 * @details Header công khai này tuyệt đối KHÔNG include stm32f1xx.h và KHÔNG
 * làm lộ định nghĩa thanh ghi phần cứng ra tầng trên.
 */

#ifndef UART_TYPES_H
#define UART_TYPES_H

#include "Std_Types.h"
#include "Uart_Cfg.h"

/*============================================================================
 * 1. KIỂU PHẦN TỬ BỘ ĐỆM VÒNG THEO CẤU HÌNH 8-BIT / 9-BIT
 *============================================================================*/

#if (UART_CFG_SUPPORT_9BIT == 1U)
/**
 * @brief Kiểu một ô nhớ trong bộ đệm vòng khi bật hỗ trợ khung 9 bit dữ liệu
 * hữu ích.
 */
typedef uint16 Uart_RingSlotType;
#else
/**
 * @brief Kiểu một ô nhớ trong bộ đệm vòng khi tối ưu RAM cho khung 7/8 bit.
 */
typedef uint8 Uart_RingSlotType;
#endif

/*============================================================================
 * 2. MÃ ĐỊNH DANH CỔNG LOGIC VÀ BUS
 *============================================================================*/

/**
 * @brief Mã cổng UART logic dùng trong toàn bộ API công khai.
 */
typedef enum {
  UART_PORT_1 = 0U, /**< Cổng logic 1 ánh xạ tới USART1 (APB2) */
  UART_PORT_2 = 1U, /**< Cổng logic 2 ánh xạ tới USART2 (APB1) */
  UART_PORT_3 =
      2U, /**< Cổng logic 3 ánh xạ tới USART3 (APB1, chỉ có trên STM32F103xB) */
  UART_PORT_COUNT =
      3U /**< Tổng số mã cổng logic trong enum (không dùng làm cổng hợp lệ) */
} Uart_PortIdType;

/**
 * @brief Định danh bus ngoại vi cung cấp xung cho USART.
 * @details Truyền vào hàm callback lấy tần số xung (nếu người tích hợp cung
 * cấp) để phân biệt chính xác PCLK1 và PCLK2.
 */
typedef enum {
  UART_BUS_APB1 = 0U, /**< Bus APB1 (PCLK1) cấp xung cho USART2 và USART3 */
  UART_BUS_APB2 = 1U  /**< Bus APB2 (PCLK2) cấp xung cho USART1 */
} Uart_BusIdType;

/*============================================================================
 * 3. MÃ KẾT QUẢ VÀ PHÂN BIỆT LỖI LẬP TRÌNH VS HIỆN TƯỢNG VẬN HÀNH
 *============================================================================*/

/**
 * @brief Mã kết quả trả về của các API trong driver UART.
 * @details Thay vì chỉ dùng E_OK/E_NOT_OK của Std_ReturnType, kiểu riêng này
 * giúp ứng dụng xử lý chính xác từng tình huống (bận, timeout, lỗi đường
 * truyền...).
 */
typedef enum {
  /** Thao tác hoàn tất thành công. */
  UART_STATUS_OK = 0U,

  /** [Lỗi lập trình - báo Det] Tham số sai (NULL_PTR, mã cổng sai, kích thước
     0, thiếu tick provider...). */
  UART_STATUS_INVALID_ARG = 1U,

  /** [Lỗi lập trình - báo Det] Tổ hợp hoặc tính năng không hỗ trợ trên chip/bản
     build hiện tại. */
  UART_STATUS_UNSUPPORTED = 2U,

  /** [Lỗi lập trình - báo Det] Cổng chưa được khởi tạo bằng Uart_Init(). */
  UART_STATUS_NOT_INIT = 3U,

  /** [Lỗi lập trình - báo Det] Cổng đã khởi tạo; cần gọi Uart_DeInit() hoặc
     Uart_ReInit(). */
  UART_STATUS_ALREADY_INIT = 4U,

  /** [Lỗi lập trình - báo Det] Gọi API sai chế độ TX/RX đã cấu hình hoặc chiều
     truyền/nhận đang tắt. */
  UART_STATUS_MODE_CONFLICT = 5U,

  /** [Lỗi lập trình - báo Det] Giá trị BRR tính từ BusClockHz và BaudRate nằm
     ngoài [0x0010, 0xFFFF]. */
  UART_STATUS_BAUD_OUT_OF_RANGE = 6U,

  /** [Vận hành - không báo Det] Chiều truyền/nhận hoặc cổng đang có giao dịch
     chạy dở. */
  UART_STATUS_BUSY = 7U,

  /** [Vận hành - không báo Det] Hết giới hạn chờ trước khi hoàn tất; xem
     *ProcessedOut để biết phần đã xong. */
  UART_STATUS_TIMEOUT = 8U,

  /** [Vận hành - không báo Det] Bộ đệm vòng TX đã đầy, không ghi thêm được dữ
     liệu. */
  UART_STATUS_BUFFER_FULL = 9U,

  /** [Vận hành - không báo Det] Bộ đệm vòng RX đang rỗng, chưa có dữ liệu để
     đọc. */
  UART_STATUS_BUFFER_EMPTY = 10U,

  /** [Vận hành - không báo Det] Phát hiện lỗi vật lý đường truyền
     (PE/FE/NE/ORE). */
  UART_STATUS_LINE_ERROR = 11U,

  /** [Vận hành - không báo Det] Bộ điều khiển DMA báo lỗi truyền (Transfer
     Error). */
  UART_STATUS_DMA_ERROR = 12U,

  /** [Vận hành - không báo Det] Giao dịch DMA đã bị hủy chủ động qua
     Uart_DmaAbort*. */
  UART_STATUS_ABORTED = 13U
} Uart_StatusType;

/*============================================================================
 * 4. MÃ API ID VÀ ERROR ID PHỤC VỤ DET (KHI UART_CFG_DEV_ERROR_DETECT == 1U)
 *============================================================================*/

/** @brief Mã API dùng khi gọi Det_ReportError(). */
#define UART_API_INIT (0x01U)
#define UART_API_DEINIT (0x02U)
#define UART_API_REINIT (0x03U)
#define UART_API_GET_STATE (0x04U)
#define UART_API_TX_POLLING (0x10U)
#define UART_API_RX_POLLING (0x11U)
#define UART_API_WAIT_TX_COMPLETE (0x12U)
#define UART_API_IRQ_WRITE (0x20U)
#define UART_API_IRQ_READ (0x21U)
#define UART_API_IRQ_QUERY (0x22U)
#define UART_API_IRQ_FLUSH (0x23U)
#define UART_API_DMA_TX (0x30U)
#define UART_API_DMA_RX (0x31U)
#define UART_API_DMA_STATUS (0x32U)
#define UART_API_DMA_ABORT (0x33U)
#define UART_API_ERROR_STATS (0x40U)
#define UART_API_COMPUTE_BAUD (0x41U)

/** @brief Mã lỗi lập trình gửi lên Det_ReportError(). */
#define UART_DET_E_PARAM_PORT (0x01U)    /**< Mã cổng không hợp lệ */
#define UART_DET_E_PARAM_POINTER (0x02U) /**< Con trỏ NULL_PTR không hợp lệ */
#define UART_DET_E_PARAM_CONFIG                                                \
  (0x03U) /**< Tham số cấu hình hoặc độ dài không hợp lệ */
#define UART_DET_E_UNSUPPORTED                                                 \
  (0x04U) /**< Tính năng hoặc tổ hợp phần cứng không hỗ trợ */
#define UART_DET_E_UNINIT (0x05U) /**< Gọi API khi cổng chưa khởi tạo */
#define UART_DET_E_ALREADY_INIT                                                \
  (0x06U) /**< Khởi tạo lại cổng đang hoạt động bằng Uart_Init */
#define UART_DET_E_MODE_CONFLICT                                               \
  (0x07U) /**< Chế độ gọi xung đột với cấu hình chiều TX/RX */
#define UART_DET_E_BAUD_RANGE                                                  \
  (0x08U) /**< Giá trị baud nằm ngoài dải thanh ghi BRR */

/*============================================================================
 * 5. CÁC ENUM CẤU HÌNH KHUNG TRUYỀN, CHẾ ĐỘ VÀ TRẠNG THÁI
 *============================================================================*/

/**
 * @brief Số bit dữ liệu HỮU ÍCH mà người dùng gửi và nhận (không tính bit
 * parity).
 */
typedef enum {
  UART_DATA_BITS_7 = 7U, /**< 7 bit dữ liệu hữu ích — CHỈ hợp lệ khi bật parity
                            chẵn/lẻ (CR1.M=0, PCE=1) */
  UART_DATA_BITS_8 = 8U, /**< 8 bit dữ liệu hữu ích — hợp lệ với mọi chế độ
                            parity (M=0 nếu NONE, M=1 nếu EVEN/ODD) */
  UART_DATA_BITS_9 = 9U /**< 9 bit dữ liệu hữu ích — CHỈ hợp lệ khi không parity
                           (M=1, PCE=0) và UART_CFG_SUPPORT_9BIT=1 */
} Uart_DataBitsType;

/**
 * @brief Cấu hình bit kiểm tra chẵn/lẻ (parity).
 */
typedef enum {
  UART_PARITY_NONE = 0U, /**< Không dùng parity */
  UART_PARITY_EVEN = 1U, /**< Parity chẵn (CR1.PCE=1, CR1.PS=0) */
  UART_PARITY_ODD = 2U   /**< Parity lẻ (CR1.PCE=1, CR1.PS=1) */
} Uart_ParityType;

/**
 * @brief Số bit dừng (stop bit) cho UART bất đồng bộ.
 */
typedef enum {
  UART_STOP_BITS_1 = 0U, /**< 1 stop bit (CR2.STOP = 00b) */
  UART_STOP_BITS_2 = 1U  /**< 2 stop bit (CR2.STOP = 10b) */
} Uart_StopBitsType;

/**
 * @brief Điều khiển luồng phần cứng RTS/CTS.
 */
typedef enum {
  UART_FLOW_NONE = 0U,   /**< Không dùng RTS/CTS */
  UART_FLOW_RTS = 1U,    /**< Chỉ bật RTS (CR3.RTSE=1) — yêu cầu bật chiều RX */
  UART_FLOW_CTS = 2U,    /**< Chỉ bật CTS (CR3.CTSE=1) — yêu cầu bật chiều TX */
  UART_FLOW_RTS_CTS = 3U /**< Bật cả RTS và CTS — yêu cầu bật cả TX và RX */
} Uart_FlowControlType;

/**
 * @brief Chế độ truyền/nhận độc lập cho từng chiều TX và RX.
 * @details Mọi tổ hợp (TxMode, RxMode) đều được hỗ trợ miễn là ít nhất một
 * chiều khác UART_XFER_DISABLED. Driver chặn gọi nhầm API của chế độ khác.
 */
typedef enum {
  UART_XFER_DISABLED =
      0U, /**< Tắt hoàn toàn chiều này (không bật bit TE hoặc RE) */
  UART_XFER_POLLING = 1U, /**< Truyền/nhận đồng bộ bằng kiểm tra cờ thanh ghi */
  UART_XFER_IRQ = 2U,     /**< Truyền/nhận nền bằng ngắt USART và bộ đệm vòng */
  UART_XFER_DMA = 3U /**< Truyền/nhận khối dữ liệu bằng kênh DMA1 (cần
                        UART_CFG_DMA_ENABLE=1) */
} Uart_XferModeType;

/**
 * @brief Mức ưu tiên kênh DMA (ánh xạ trực tiếp vào DMA_CCR.PL).
 */
typedef enum {
  UART_DMA_PRIO_LOW = 0U,      /**< Ưu tiên thấp */
  UART_DMA_PRIO_MEDIUM = 1U,   /**< Ưu tiên trung bình */
  UART_DMA_PRIO_HIGH = 2U,     /**< Ưu tiên cao */
  UART_DMA_PRIO_VERY_HIGH = 3U /**< Ưu tiên rất cao */
} Uart_DmaPriorityType;

/**
 * @brief Trạng thái vòng đời của một cổng UART.
 */
typedef enum {
  UART_STATE_UNINIT = 0U, /**< Chưa khởi tạo hoặc đã Uart_DeInit() */
  UART_STATE_READY = 1U   /**< Đã khởi tạo và sẵn sàng hoạt động */
} Uart_PortStateType;

/**
 * @brief Trạng thái giao dịch DMA của một chiều (TX hoặc RX).
 */
typedef enum {
  UART_DMA_STATE_IDLE =
      0U, /**< Chưa có giao dịch hoặc đã sẵn sàng nhận lệnh mới */
  UART_DMA_STATE_BUSY = 1U, /**< Kênh DMA đang chuyển dữ liệu; ứng dụng KHÔNG
                               được sửa/giải phóng buffer */
  UART_DMA_STATE_DONE =
      2U, /**< DMA chuyển đủ số phần tử; buffer đã an toàn để tái sử dụng */
  UART_DMA_STATE_ERROR =
      3U, /**< Dừng do lỗi DMA (TEIF) hoặc lỗi đường truyền UART khi DMA RX */
  UART_DMA_STATE_ABORTED = 4U /**< Đã bị hủy chủ động qua Uart_DmaAbort*() */
} Uart_DmaStateType;

/*============================================================================
 * 6. CƠ CHẾ TIMEOUT RÕ ĐƠN VỊ (SỐ LƯỢT KIỂM TRA HOẶC TICK THỜI GIAN THỰC)
 *============================================================================*/

/**
 * @brief Đơn vị của giới hạn chờ trong các hàm polling.
 */
typedef enum {
  UART_TIMEOUT_UNIT_LOOPS =
      0U, /**< Số lượt lặp kiểm tra cờ thanh ghi (không cần SysTick) */
  UART_TIMEOUT_UNIT_TICKS = 1U /**< Số tick từ nguồn thời gian đăng ký qua
                                  Uart_RegisterTickProvider() */
} Uart_TimeoutUnitType;

/**
 * @brief Mô tả giới hạn chờ kèm đơn vị rõ ràng để tránh nhầm lẫn giữa vòng lặp
 * và mili-giây.
 */
typedef struct {
  Uart_TimeoutUnitType Unit; /**< Đơn vị đo: LOOPS hoặc TICKS */
  uint32
      Value; /**< Giá trị giới hạn (0U = chỉ kiểm tra 1 lần, không chờ lặp) */
} Uart_TimeoutType;

/** @brief Macro tạo timeout theo số lượt kiểm tra cờ. */
#define UART_TIMEOUT_LOOPS(n)                                                  \
  ((Uart_TimeoutType){UART_TIMEOUT_UNIT_LOOPS, (uint32)(n)})

/** @brief Macro tạo timeout theo số tick của nguồn thời gian thực (ví dụ ms từ
 * SysTick). */
#define UART_TIMEOUT_TICKS(n)                                                  \
  ((Uart_TimeoutType){UART_TIMEOUT_UNIT_TICKS, (uint32)(n)})

/*============================================================================
 * 7. SỰ KIỆN CALLBACK, LỖI VÀ THÔNG TIN GIAO DỊCH DMA
 *============================================================================*/

/**
 * @brief Các sự kiện thông báo qua callback.
 * @warning Callback chạy TRONG NGỮ CẢNH NGẮT (USARTx_IRQHandler hoặc
 * DMA1_Channelx_IRQHandler). Phải giữ callback thật ngắn, không gọi hàm
 * polling/Init/DeInit/ReInit.
 */
typedef enum {
  UART_EVENT_RX_DATA_READY =
      0U, /**< [IRQ RX] Vừa nhận thêm một khung hợp lệ vào bộ đệm vòng RX */
  UART_EVENT_TX_COMPLETE = 1U, /**< [IRQ TX] Bộ đệm vòng TX đã cạn và bit stop
                                  cuối đã ra khỏi chân (SR.TC=1) */
  UART_EVENT_DMA_TX_DONE = 2U, /**< [DMA TX] DMA đã chuyển xong phần tử cuối vào
                                  DR (buffer TX đã dùng lại được) */
  UART_EVENT_DMA_RX_DONE =
      3U, /**< [DMA RX] DMA đã nhận đủ số phần tử yêu cầu vào buffer RX */
  UART_EVENT_ERROR = 4U /**< [IRQ/DMA] Phát hiện lỗi đường truyền
                           (PE/FE/NE/ORE), tràn bộ đệm mềm hoặc lỗi DMA */
} Uart_EventType;

/** @brief Cờ bit chi tiết lỗi trong Uart_ErrorStatsType.ActiveFlags và tham số
 * callback. */
#define UART_ERR_NONE (0x0000U)   /**< Không có lỗi */
#define UART_ERR_PARITY (0x0001U) /**< Lỗi parity phần cứng (USART_SR.PE) */
#define UART_ERR_FRAMING                                                       \
  (0x0002U) /**< Lỗi khung / sai stop bit / sai baud (USART_SR.FE) */
#define UART_ERR_NOISE (0x0004U) /**< Nhiễu trên đường RX (USART_SR.NE) */
#define UART_ERR_HW_OVERRUN                                                     \
  (0x0008U) /**< Tràn thanh ghi phần cứng do đọc DR không kịp (USART_SR.ORE) \
             */
#define UART_ERR_SW_RX_OVERFLOW                                                  \
  (0x0010U) /**< Tràn bộ đệm vòng phần mềm RX (chính sách: bỏ khung mới nhất) \
             */
#define UART_ERR_DMA_TX_TRANSFER                                               \
  (0x0020U) /**< Lỗi truyền DMA kênh TX (DMA_ISR.TEIF) */
#define UART_ERR_DMA_RX_TRANSFER                                               \
  (0x0040U) /**< Lỗi truyền DMA kênh RX (DMA_ISR.TEIF) */

/**
 * @brief Con trỏ hàm cung cấp bộ đếm tick tăng đơn điệu (tự quay vòng ở
 * 0xFFFFFFFF -> 0).
 * @return Giá trị tick hiện tại (ví dụ: số mili-giây từ SysTick).
 */
typedef uint32 (*Uart_GetTickFn)(void);

/**
 * @brief Con trỏ hàm cung cấp tần số xung bus hiện tại (Hz) theo bus APB1 hoặc
 * APB2.
 * @param Bus Bus mà ngoại vi USART thuộc về (UART_BUS_APB1 hoặc UART_BUS_APB2).
 * @return Tần số bus tính bằng Hz (ví dụ 8000000UL hoặc 36000000UL /
 * 72000000UL).
 */
typedef uint32 (*Uart_GetBusClockFn)(Uart_BusIdType Bus);

/**
 * @brief Con trỏ hàm callback thông báo sự kiện bất đồng bộ của một cổng UART.
 * @param Port       Mã cổng logic phát sinh sự kiện.
 * @param Event      Loại sự kiện (UART_EVENT_*).
 * @param ErrorFlags Mặt nạ cờ lỗi (UART_ERR_*) nếu Event == UART_EVENT_ERROR,
 * ngược lại là 0U.
 * @param UserCtx    Con trỏ ngữ cảnh người dùng truyền lúc Uart_Init().
 */
typedef void (*Uart_CallbackFn)(Uart_PortIdType Port, Uart_EventType Event,
                                uint16 ErrorFlags, void *UserCtx);

/**
 * @brief Thống kê lỗi tích lũy theo từng cổng.
 */
typedef struct {
  uint32 ParityErrorCount;  /**< Số khung bị lỗi parity (PE) */
  uint32 FramingErrorCount; /**< Số khung bị lỗi framing (FE) */
  uint32 NoiseErrorCount;   /**< Số lần phát hiện nhiễu (NE) */
  uint32 HwOverrunCount;    /**< Số lần tràn thanh ghi nhận phần cứng (ORE) */
  uint32 SwRxOverflowCount; /**< Số khung hợp lệ bị bỏ do bộ đệm vòng phần mềm
                               RX đầy */
  uint32 DmaTxErrorCount; /**< Số lần lỗi DMA chiều TX (TEIF) */
  uint32 DmaRxErrorCount; /**< Số lần lỗi DMA chiều RX (TEIF) */
  uint16 LastErrorFlags;  /**< Mặt nạ cờ lỗi của sự cố gần nhất */
  uint16 StickyErrorFlags; /**< Tập hợp mọi cờ lỗi đã từng xảy ra kể từ lần
                              Clear gần nhất */
} Uart_ErrorStatsType;

/**
 * @brief Thông tin chi tiết trạng thái giao dịch DMA của một chiều.
 */
typedef struct {
  Uart_DmaStateType State;    /**< Trạng thái hiện tại của giao dịch DMA */
  Uart_StatusType LastStatus; /**< Mã kết quả cuối cùng (OK, LINE_ERROR,
                                 DMA_ERROR, ABORTED, BUSY) */
  uint16 Requested; /**< Tổng số phần tử (byte hoặc khung 16-bit) đã yêu cầu */
  uint16 Transferred; /**< Số phần tử đã chuyển thành công (chính xác cả khi
                         đang chạy hoặc bị hủy) */
} Uart_DmaTransferInfoType;

/*============================================================================
 * 8. STRUCT CẤU HÌNH KHỞI TẠO CỔNG UART
 *============================================================================*/

/**
 * @brief Cấu hình đầy đủ cho một cổng UART logic khi gọi Uart_Init() hoặc
 * Uart_ReInit().
 */
typedef struct {
  /** Tốc độ baud mong muốn tính bằng bps (ví dụ 9600UL, 115200UL). Phải > 0. */
  uint32 BaudRate;

  /**
   * @brief Tần số xung bus thực tế (Hz) của chính ngoại vi USART này (PCLK2 cho
   * USART1, PCLK1 cho USART2/3).
   * @details Hợp đồng cấp xung:
   *          - Nếu BusClockHz > 0U: driver dùng trực tiếp giá trị này.
   *          - Nếu BusClockHz == 0U: driver gọi GetBusClock(hw->BusId).
   *          - Nếu cả hai đều bằng 0 / NULL_PTR: trả UART_STATUS_INVALID_ARG.
   */
  uint32 BusClockHz;

  /** Hàm truy vấn tần số xung bus động (tùy chọn khi BusClockHz == 0U). */
  Uart_GetBusClockFn GetBusClock;

  /** Số bit dữ liệu hữu ích (7, 8 hoặc 9). */
  Uart_DataBitsType DataBits;

  /** Chế độ parity (NONE, EVEN, ODD). */
  Uart_ParityType Parity;

  /** Số bit dừng (1 hoặc 2). */
  Uart_StopBitsType StopBits;

  /** Chế độ điều khiển luồng phần cứng RTS/CTS. */
  Uart_FlowControlType FlowControl;

  /** Chế độ hoạt động chiều phát (DISABLED / POLLING / IRQ / DMA). */
  Uart_XferModeType TxMode;

  /** Chế độ hoạt động chiều thu (DISABLED / POLLING / IRQ / DMA). */
  Uart_XferModeType RxMode;

  /** Mức ưu tiên kênh DMA chiều TX (chỉ dùng khi TxMode == UART_XFER_DMA). */
  Uart_DmaPriorityType DmaTxPriority;

  /** Mức ưu tiên kênh DMA chiều RX (chỉ dùng khi RxMode == UART_XFER_DMA). */
  Uart_DmaPriorityType DmaRxPriority;

  /**
   * @brief Bộ đệm vòng TX do người gọi tự cấp phát (tùy chọn, dùng khi TxMode
   * == UART_XFER_IRQ).
   * @details Nếu NULL_PTR: driver dùng mảng tĩnh mặc định theo Uart_Cfg.h.
   *          Nếu khác NULL_PTR: mảng phải có ít nhất 2 phần tử (sức chứa hữu
   * ích = ExtTxBufferSlots - 1) và PHẢI tồn tại (static/global) trong suốt thời
   * gian cổng chưa DeInit.
   */
  Uart_RingSlotType *ExtTxBuffer;

  /** Tổng số phần tử (slots) của ExtTxBuffer (bỏ qua nếu ExtTxBuffer ==
   * NULL_PTR). */
  uint16 ExtTxBufferSlots;

  /**
   * @brief Bộ đệm vòng RX do người gọi tự cấp phát (tùy chọn, dùng khi RxMode
   * == UART_XFER_IRQ).
   * @details Quyền sở hữu và thời gian sống giống ExtTxBuffer.
   */
  Uart_RingSlotType *ExtRxBuffer;

  /** Tổng số phần tử (slots) của ExtRxBuffer (bỏ qua nếu ExtRxBuffer ==
   * NULL_PTR). */
  uint16 ExtRxBufferSlots;

  /** Callback nhận thông báo sự kiện từ ISR (có thể là NULL_PTR nếu không cần).
   */
  Uart_CallbackFn Callback;

  /** Con trỏ ngữ cảnh tùy ý truyền lại nguyên vẹn vào Callback. */
  void *CallbackCtx;
} Uart_PortConfigType;

#endif /* UART_TYPES_H */