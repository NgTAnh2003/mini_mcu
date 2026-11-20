/**
 * @file Port.c
 * @brief Chuyển bảng cấu hình chân thành thao tác bật xung nhịp cổng và ghi
 * thanh ghi cấu hình GPIO của STM32F1.
 */
#include "Port.h"
#include "stm32f1xx.h"

#if (PORT_DEV_ERROR_DETECT == STD_ON)
#include "Det.h"
#endif

/*==============================================================================
 * INTERNAL CONSTANTS & TYPES
 *============================================================================*/
/** @brief Số lượng cổng GPIO phần cứng được quản lý trên STM32F103C8T6 (Port A,
 * B, C, D). */
#define PORT_NUM_OF_HW_PORTS (4u)
/** @brief Trạng thái driver chưa được khởi tạo (chưa gọi Port_Init). */
#define PORT_UNINIT (0u)
/** @brief Trạng thái driver đã nạp xong bảng cấu hình và sẵn sàng hoạt động. */
#define PORT_INITIALIZED (1u)

/** @brief Bảng ánh xạ từ chỉ số cổng (0..3) sang địa chỉ thanh ghi gốc của từng
 * Port phần cứng. */
static GPIO_TypeDef *const Port_GpioBase[PORT_NUM_OF_HW_PORTS] = {GPIOA, GPIOB,
                                                                  GPIOC, GPIOD};

/*==============================================================================
 * MODULE INTERNAL STATE
 *============================================================================*/
/** @brief Biến lưu trạng thái hoạt động hiện tại của module Port (PORT_UNINIT
 * hoặc PORT_INITIALIZED). */
static uint8 Port_ModuleState = PORT_UNINIT;

/** @brief Con trỏ lưu địa chỉ bộ cấu hình đang được sử dụng (thường trỏ tới
 * Port_Config trong Flash). */
static const Port_ConfigType *Port_ActiveConfigPtr = NULL_PTR;

/**
 * @brief Hai mảng lưu trạng thái thực tế (hướng và chế độ) của từng chân trong
 * lúc chạy (Runtime).
 * @note  Chỉ số (index) của mảng này khớp với chỉ số của chân trong bảng cấu
 * hình PinConfigs[], không dùng trực tiếp mã PinId (ví dụ 0x2D của PC13) để
 * tiết kiệm RAM.
 */
static Port_PinDirectionType Port_RuntimeDirection[PORT_NUM_OF_CONFIGURED_PINS];
static Port_PinModeType Port_RuntimeMode[PORT_NUM_OF_CONFIGURED_PINS];

/*==============================================================================
 * PRIVATE HELPER FUNCTIONS
 *============================================================================*/
/**
 * @brief  Kiểm tra mã cổng và số thứ tự chân có tồn tại vật lý trên chip
 * STM32F103C8T6 (vỏ LQFP48) hay không.
 * @param  Pin Mã định danh chân (4 bit cao là Port: 0=A, 1=B, 2=C, 3=D; 4 bit
 * thấp là số chân: 0..15).
 * @return TRUE nếu chân hợp lệ trên phần cứng, FALSE nếu chân không tồn tại.
 */
static boolean Port_IsHwPinValid(Port_PinType Pin) {
  /* Tách 4 bit cao để lấy chỉ số Port (0..3) và 4 bit thấp để lấy vị trí chân
   * (0..15) */
  uint8 portIdx = (uint8)(Pin >> 4u);
  uint8 pinIdx = (uint8)(Pin & 0x0Fu);
  boolean isValid = FALSE;

  /* Kiểm tra chân vật lý tồn tại trên gói LQFP48 của STM32F103C8T6 */
  if ((portIdx == 0u) || (portIdx == 1u)) {
    isValid = TRUE; /* Port A và Port B có đủ 16 chân: PA0..PA15, PB0..PB15 */
  } else if ((portIdx == 2u) && (pinIdx >= 13u)) {
    isValid = TRUE; /* Port C trên vỏ 48 chân chỉ đưa ra ngoài 3 chân: PC13,
                       PC14, PC15 */
  } else if ((portIdx == 3u) && (pinIdx <= 1u)) {
    isValid = TRUE; /* Port D trên vỏ 48 chân chỉ có 2 chân dao động thạch anh:
                       PD0, PD1 */
  } else {
    isValid = FALSE;
  }

  return isValid;
}

/**
 * @brief  Tìm kiếm một chân trong bảng cấu hình hiện hành xem chân đó có được
 * khai báo sử dụng hay không.
 * @param  Pin      Mã chân cần tìm.
 * @param  IndexPtr Con trỏ nhận lại vị trí (index) của chân trong mảng
 * PinConfigs[] nếu tìm thấy.
 * @return TRUE nếu chân đã được cấu hình, FALSE nếu không tìm thấy.
 */
static boolean Port_FindConfiguredPin(Port_PinType Pin, uint16 *IndexPtr) {
  uint16 idx;
  boolean found = FALSE;

  if ((Port_ActiveConfigPtr != NULL_PTR) && (IndexPtr != NULL_PTR)) {
    /* Duyệt tuần tự qua danh sách các chân đã cấu hình */
    for (idx = 0u; idx < Port_ActiveConfigPtr->NumOfPins; idx++) {
      if (Port_ActiveConfigPtr->PinConfigs[idx].PinId == Pin) {
        /* Chỉ số tìm được dùng chung cho bảng cấu hình và hai bảng trạng thái
         * runtime */
        *IndexPtr = idx;
        found = TRUE;
        break;
      }
    }
  }

  return found;
}

/**
 * @brief Cấp xung nhịp (Clock) từ bus APB2 cho cổng GPIO tương ứng trước khi
 * truy cập thanh ghi.
 * @param PortIdx Chỉ số cổng (0=PortA, 1=PortB, 2=PortC, 3=PortD).
 * @param Mode    Chế độ hoạt động của chân (nếu dùng chức năng ngoại vi ALT thì
 * bật thêm AFIO).
 */
static void Port_EnablePortClock(uint8 PortIdx, Port_PinModeType Mode) {
  /* Bật bit cấp clock cho từng Port trên thanh ghi RCC_APB2ENR */
  switch (PortIdx) {
  case 0u:
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    break;
  case 1u:
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    break;
  case 2u:
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    break;
  case 3u:
    RCC->APB2ENR |= RCC_APB2ENR_IOPDEN;
    break;
  default:
    break;
  }

  /* Nếu chân dùng cho ngoại vi (UART, SPI, CAN, Timer...) cần bật thêm clock
   * cho khối AFIO */
  if (Mode == PORT_PIN_MODE_ALT) {
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  }

  /* Đọc giả thanh ghi APB2ENR để đảm bảo độ trễ đồng bộ 2 chu kỳ bus APB2 trước
   * khi ghi vào thanh ghi GPIO */
  (void)RCC->APB2ENR;
}

/**
 * @brief Ghi trực tiếp cấu hình chế độ và hướng vào thanh ghi phần cứng CRL/CRH
 * của STM32F1.
 * @note  Trên STM32F1, mỗi chân GPIO được điều khiển bởi 4 bit
 * [CNF1:CNF0:MODE1:MODE0].
 *        - 8 chân thấp (Pin 0..7) nằm ở thanh ghi CRL.
 *        - 8 chân cao (Pin 8..15) nằm ở thanh ghi CRH.
 * @param PinCfg            Con trỏ tới cấu trúc cấu hình gốc của chân.
 * @param Direction         Hướng chân cần áp dụng (Input / Output).
 * @param Mode              Chế độ chân cần áp dụng (DIO / ADC / ALT).
 * @param ApplyInitialLevel TRUE nếu cần nạp mức logic ban đầu (chỉ dùng khi
 * Port_Init), FALSE nếu giữ nguyên.
 */
static void Port_WriteHardwareConfig(const Port_PinConfigType *PinCfg,
                                     Port_PinDirectionType Direction,
                                     Port_PinModeType Mode,
                                     boolean ApplyInitialLevel) {
  uint8 portIdx = (uint8)(PinCfg->PinId >> 4u);
  uint8 pinIdx = (uint8)(PinCfg->PinId & 0x0Fu);
  GPIO_TypeDef *gpio = Port_GpioBase[portIdx];
  uint32 cfgBits = 0u; /* Giá trị 4 bit [CNF:MODE] sẽ ghi vào CRL hoặc CRH */
  uint32 shift;

  /* [SWS_Port_00055] Ghi mức logic ban đầu vào thanh ghi BSRR TRƯỚC KHI chuyển
   * chân sang Output để tránh tạo ra xung nhiễu (glitch) ngắn trên đường dây
   * khi chân vừa kích hoạt đầu ra. */
  if ((ApplyInitialLevel == TRUE) && (Direction == PORT_PIN_OUT) &&
      (Mode == PORT_PIN_MODE_DIO)) {
    if (PinCfg->InitialLevel == PORT_PIN_LEVEL_HIGH) {
      gpio->BSRR =
          (1uL << pinIdx); /* 16 bit thấp của BSRR dùng để Set bit lên mức 1 */
    } else {
      gpio->BSRR =
          (1uL << (pinIdx +
                   16u)); /* 16 bit cao của BSRR dùng để Reset bit về mức 0 */
    }
  }

  /* Tính toán giá trị 4 bit cấu hình [CNF1:CNF0:MODE1:MODE0] theo Reference
   * Manual của STM32F1 */
  if (Mode == PORT_PIN_MODE_ADC) {
    /* Chế độ Analog Input (đọc ADC): CNF = 00, MODE = 00 -> 0x00 */
    cfgBits = 0x00u;
  } else if (Direction == PORT_PIN_IN) {
    /* Chế độ Input (áp dụng cho cả DIO Input và Alternate Function Input) */
    if (PinCfg->Pull == PORT_PIN_PULL_NONE) {
      /* Floating Input (thả nổi, không trở kéo): CNF = 01, MODE = 00 -> 0b0100
       * = 0x04 */
      cfgBits = 0x04u;
    } else {
      /* Input có điện trở kéo lên/xuống: CNF = 10, MODE = 00 -> 0b1000 = 0x08
       */
      cfgBits = 0x08u;
      /* Đặc trưng của STM32F1: Khi CNF = 10, thanh ghi ODR quyết định kéo lên
       * (1) hay kéo xuống (0) */
      if (PinCfg->Pull == PORT_PIN_PULL_UP) {
        gpio->BSRR = (1uL << pinIdx); /* Kéo lên nguồn (Pull-up) */
      } else {
        gpio->BSRR = (1uL << (pinIdx + 16u)); /* Kéo xuống mass (Pull-down) */
      }
    }
  } else {
    /* Chế độ Output: 2 bit thấp MODE[1:0] quy định tốc độ đảo trạng thái (2MHz
     * / 10MHz / 50MHz) */
    uint32 modeBits = (uint32)PinCfg->Speed & 0x03u;
    uint32 cnfBits = 0u;

    if (Mode == PORT_PIN_MODE_DIO) {
      /* General Purpose Output (điều khiển bằng phần mềm): Push-Pull = 00
       * (0x00), Open-Drain = 01 (0x04) */
      cnfBits = (PinCfg->OutputType == PORT_PIN_OUT_OPENDRAIN) ? 0x04u : 0x00u;
    } else {
      /* Alternate Function Output (ngoại vi điều khiển): Push-Pull = 10 (0x08),
       * Open-Drain = 11 (0x0C) */
      cnfBits = (PinCfg->OutputType == PORT_PIN_OUT_OPENDRAIN) ? 0x0Cu : 0x08u;
    }

    /* Ghép 2 bit CNF ở trên với 2 bit MODE ở dưới thành cụm 4 bit hoàn chỉnh */
    cfgBits = cnfBits | modeBits;
  }

  /* Tính vị trí bit bắt đầu của chân trong thanh ghi CRL/CRH (mỗi chân chiếm 4
   * bit nên nhân 4) */
  shift = (uint32)(pinIdx & 0x07u) * 4u;

  /* Dùng phép toán Read-Modify-Write để xóa 4 bit cũ và nạp 4 bit mới, giữ
   * nguyên 7 chân còn lại */
  if (pinIdx < 8u) {
    gpio->CRL = (gpio->CRL & ~(0x0FuL << shift)) | (cfgBits << shift);
  } else {
    gpio->CRH = (gpio->CRH & ~(0x0FuL << shift)) | (cfgBits << shift);
  }
}

/*==============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/
/**
 * @brief  Khởi tạo toàn bộ các chân GPIO theo bảng cấu hình tĩnh được truyền
 * vào.
 * @param  ConfigPtr Con trỏ trỏ tới cấu trúc cấu hình Port (ví dụ:
 * &Port_Config).
 */
void Port_Init(const Port_ConfigType *ConfigPtr) {
  uint16 idx;

  /* Bước 1: Kiểm tra tính hợp lệ của con trỏ cấu hình và số lượng chân trước
   * khi thao tác */
  if ((ConfigPtr == NULL_PTR) || (ConfigPtr->PinConfigs == NULL_PTR) ||
      (ConfigPtr->NumOfPins == 0u) ||
      (ConfigPtr->NumOfPins > PORT_NUM_OF_CONFIGURED_PINS)) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_INIT_SID,
                          PORT_E_INIT_FAILED);
#endif
    return;
  }

  /* Bước 2: Duyệt qua từng chân trong bảng cấu hình để kiểm tra và nạp xuống
   * phần cứng */
  for (idx = 0u; idx < ConfigPtr->NumOfPins; idx++) {
    const Port_PinConfigType *pinCfg = &ConfigPtr->PinConfigs[idx];

    /* Kiểm tra chân có tồn tại trên vỏ chip vật lý hay không */
    if (Port_IsHwPinValid(pinCfg->PinId) == FALSE) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
      (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_INIT_SID,
                            PORT_E_PARAM_PIN);
#endif
      return;
    }

    /* Bật xung nhịp cho Port và ghi thanh ghi cấu hình (kèm áp dụng mức logic
     * ban đầu) */
    Port_EnablePortClock((uint8)(pinCfg->PinId >> 4u), pinCfg->Mode);
    Port_WriteHardwareConfig(pinCfg, pinCfg->Direction, pinCfg->Mode, TRUE);

    /* Lưu lại trạng thái khởi tạo vào mảng RAM để theo dõi nếu có thay đổi lúc
     * runtime */
    Port_RuntimeDirection[idx] = pinCfg->Direction;
    Port_RuntimeMode[idx] = pinCfg->Mode;
  }

  /* Bước 3: Lưu lại con trỏ cấu hình và đánh dấu module Port đã khởi tạo thành
   * công */
  Port_ActiveConfigPtr = ConfigPtr;
  Port_ModuleState = PORT_INITIALIZED;
}

#if (PORT_SET_PIN_DIRECTION_API == STD_ON)
/**
 * @brief  Thay đổi hướng (Input/Output) của một chân trong lúc chương trình
 * đang chạy.
 * @param  Pin       Mã chân cần đổi hướng.
 * @param  Direction Hướng mới muốn thiết lập (PORT_PIN_IN hoặc PORT_PIN_OUT).
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction) {
  uint16 idx = 0u;

  /* Kiểm tra xem Port_Init đã được gọi chưa */
  if (Port_ModuleState != PORT_INITIALIZED) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_DIRECTION_SID, PORT_E_UNINIT);
#endif
    return;
  }

  /* Kiểm tra chân có hợp lệ và có nằm trong danh sách cấu hình của dự án không
   */
  if ((Port_IsHwPinValid(Pin) == FALSE) ||
      (Port_FindConfiguredPin(Pin, &idx) == FALSE)) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_DIRECTION_SID, PORT_E_PARAM_PIN);
#endif
    return;
  }

  /* Kiểm tra chân này có được phép đổi hướng lúc runtime không
   * (DirectionChangeable == TRUE) */
  if (Port_ActiveConfigPtr->PinConfigs[idx].DirectionChangeable == FALSE) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_DIRECTION_SID,
                          PORT_E_DIRECTION_UNCHANGEABLE);
#endif
    return;
  }

  /* Ghi hướng mới xuống thanh ghi phần cứng (giữ nguyên Mode hiện tại, không
   * ghi đè mức InitialLevel) */
  Port_WriteHardwareConfig(&Port_ActiveConfigPtr->PinConfigs[idx], Direction,
                           Port_RuntimeMode[idx], FALSE);
  Port_RuntimeDirection[idx] = Direction;
}
#endif /* PORT_SET_PIN_DIRECTION_API == STD_ON */

/**
 * @brief  Khôi phục lại hướng ban đầu cho tất cả các chân có hướng cố định
 * (DirectionChangeable == FALSE).
 * @note   Dùng để phục hồi thanh ghi phần cứng nếu bị nhiễu điện từ hoặc lỗi
 * ghi đè bộ nhớ.
 */
void Port_RefreshPortDirection(void) {
  uint16 idx;

  if (Port_ModuleState != PORT_INITIALIZED) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_REFRESH_PORT_DIRECTION_SID, PORT_E_UNINIT);
#endif
    return;
  }

  /* [SWS_Port_00061] Chỉ nạp lại hướng cho các chân KHÔNG cho phép đổi hướng
   * lúc runtime; bỏ qua các chân DirectionChangeable == TRUE để không làm hỏng
   * trạng thái giao tiếp động của chúng. */
  for (idx = 0u; idx < Port_ActiveConfigPtr->NumOfPins; idx++) {
    const Port_PinConfigType *pinCfg = &Port_ActiveConfigPtr->PinConfigs[idx];

    if (pinCfg->DirectionChangeable == FALSE) {
      Port_WriteHardwareConfig(pinCfg, pinCfg->Direction, Port_RuntimeMode[idx],
                               FALSE);
      Port_RuntimeDirection[idx] = pinCfg->Direction;
    }
  }
}

#if (PORT_VERSION_INFO_API == STD_ON)
/**
 * @brief  Lấy thông tin phiên bản phần mềm và mã nhà cung cấp của module Port.
 * @param  versioninfo Con trỏ tới cấu trúc nhận dữ liệu phiên bản.
 */
void Port_GetVersionInfo(Std_VersionInfoType *versioninfo) {
  if (versioninfo == NULL_PTR) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_GET_VERSION_INFO_SID, PORT_E_PARAM_POINTER);
#endif
    return;
  }

  versioninfo->vendorID = PORT_VENDOR_ID;
  versioninfo->moduleID = PORT_MODULE_ID;
  versioninfo->sw_major_version = PORT_SW_MAJOR_VERSION;
  versioninfo->sw_minor_version = PORT_SW_MINOR_VERSION;
  versioninfo->sw_patch_version = PORT_SW_PATCH_VERSION;
}
#endif /* PORT_VERSION_INFO_API == STD_ON */

#if (PORT_SET_PIN_MODE_API == STD_ON)
/**
 * @brief  Chuyển đổi chức năng (Mode) của một chân lúc runtime (ví dụ từ DIO
 * sang chân ngoại vi ALT).
 * @param  Pin  Mã chân cần đổi chế độ.
 * @param  Mode Chế độ mới muốn áp dụng (PORT_PIN_MODE_DIO, PORT_PIN_MODE_ADC,
 * PORT_PIN_MODE_ALT).
 */
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode) {
  uint16 idx = 0u;

  /* Kiểm tra trạng thái khởi tạo của module */
  if (Port_ModuleState != PORT_INITIALIZED) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_MODE_SID, PORT_E_UNINIT);
#endif
    return;
  }

  /* Kiểm tra chân có tồn tại và có trong bảng cấu hình không */
  if ((Port_IsHwPinValid(Pin) == FALSE) ||
      (Port_FindConfiguredPin(Pin, &idx) == FALSE)) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_MODE_SID, PORT_E_PARAM_PIN);
#endif
    return;
  }

  /* Kiểm tra chân này có cho phép đổi chế độ lúc runtime không (ModeChangeable
   * == TRUE) */
  if (Port_ActiveConfigPtr->PinConfigs[idx].ModeChangeable == FALSE) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_MODE_SID, PORT_E_MODE_UNCHANGEABLE);
#endif
    return;
  }

  /* Kiểm tra mã Mode truyền vào có nằm trong dải giá trị hợp lệ của driver
   * không */
  if (Mode > PORT_PIN_MODE_ALT) {
#if (PORT_DEV_ERROR_DETECT == STD_ON)
    (void)Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID,
                          PORT_SET_PIN_MODE_SID, PORT_E_PARAM_INVALID_MODE);
#endif
    return;
  }

  /* Bật thêm clock (nếu chuyển sang ALT cần AFIO) và ghi Mode mới (giữ nguyên
   * hướng hiện tại) */
  Port_EnablePortClock((uint8)(Pin >> 4u), Mode);
  Port_WriteHardwareConfig(&Port_ActiveConfigPtr->PinConfigs[idx],
                           Port_RuntimeDirection[idx], Mode, FALSE);
  Port_RuntimeMode[idx] = Mode;
}
#endif /* PORT_SET_PIN_MODE_API == STD_ON */