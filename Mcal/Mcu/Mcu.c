/**
 * @file    Mcu.c
 * @brief   Triển khai module Mcal/Mcu cho vi điều khiển STM32F103 (C6T6 và
 * C8T6).
 * @details Đây là file duy nhất trong module Mcal/Mcu được phép include
 * "stm32f1xx.h" để truy cập trực tiếp các thanh ghi RCC của CMSIS. Không dùng
 * thư viện HAL, không cấp phát động và không có vòng chờ vô hạn.
 */

#include "Mcu.h"

#if (MCU_DEV_ERROR_DETECT == STD_ON)
#include "Det.h"
#endif

#include "stm32f1xx.h"

#ifndef HSE_VALUE
#error                                                                         \
    "HSE_VALUE phai duoc dinh nghia o buoc build (vi du -DHSE_VALUE=8000000U)."
#endif

/* ========================================================================== */
/*  Biến trạng thái nội bộ của module (chỉ dùng trong Mcu.c)                  */
/* ========================================================================== */

/**
 * @brief   Con trỏ lưu địa chỉ bảng cấu hình hợp lệ đã được truyền qua
 * Mcu_Init().
 * @details Luôn là NULL_PTR khi driver chưa khởi tạo hoặc lần gọi Mcu_Init() bị
 * lỗi.
 */
static const Mcu_ConfigType *Mcu_ConfigPtr = NULL_PTR;

/**
 * @brief   Cờ đánh dấu trạng thái khởi tạo của module Mcu.
 * @details Chỉ chuyển sang TRUE khi toàn bộ bảng cấu hình truyền vào Mcu_Init()
 * hợp lệ.
 */
static boolean Mcu_IsInitialized = FALSE;

/* ========================================================================== */
/*  Khai báo nguyên mẫu các hàm nội bộ (static)                               */
/* ========================================================================== */

static void Mcu_ReportDevError(uint8 ApiId, uint8 ErrorId);
static boolean Mcu_IsConfigTableValid(const Mcu_ConfigType *ConfigPtr);
static boolean Mcu_WaitForRegisterBits(const volatile uint32_t *RegisterPtr,
                                       uint32_t Mask, uint32_t ExpectedValue,
                                       uint32 MaxChecks);
static void Mcu_FallbackToHsi(void);

/* ========================================================================== */
/*  Triển khai các hàm nội bộ (static)                                        */
/* ========================================================================== */

/**
 * @brief   Gửi báo cáo lỗi phát triển (lỗi lập trình) sang module Det nếu được
 * bật.
 * @details Khi MCU_DEV_ERROR_DETECT == STD_OFF, hàm không làm gì nhưng vẫn ép
 * kiểu tham số sang (void) để tránh cảnh báo biên dịch về tham số không sử
 * dụng. Chỉ gọi hàm này cho lỗi lập trình (sai tham số, sai thứ tự gọi), KHÔNG
 * gọi cho lỗi phần cứng lúc chạy như thạch anh không dao động.
 *
 * @param[in] ApiId   Mã dịch vụ API nơi phát hiện lỗi (MCU_SID_INIT hoặc
 * MCU_SID_INIT_CLOCK).
 * @param[in] ErrorId Mã lỗi phát triển (MCU_E_PARAM_POINTER,
 * MCU_E_PARAM_CONFIG, MCU_E_UNINIT hoặc MCU_E_PARAM_CLOCK).
 */
static void Mcu_ReportDevError(uint8 ApiId, uint8 ErrorId) {
#if (MCU_DEV_ERROR_DETECT == STD_ON)
  (void)Det_ReportError(MCU_MODULE_ID, MCU_INSTANCE_ID, ApiId, ErrorId);
#else
  (void)ApiId;
  (void)ErrorId;
#endif
}

/**
 * @brief   Kiểm tra toàn diện tính hợp lệ của nội dung bảng cấu hình trước khi
 * lưu trạng thái.
 * @details Việc kiểm tra trọn gói giúp đảm bảo hàm Mcu_Init() không bao giờ tạo
 * ra trạng thái khởi tạo nửa chừng nếu một mục trong bảng cấu hình bị sai.
 *
 * @param[in] ConfigPtr Con trỏ tới bảng cấu hình (khác NULL_PTR).
 *
 * @pre     Người gọi đã kiểm tra ConfigPtr != NULL_PTR.
 * @return  boolean
 * @retval  TRUE  Toàn bộ bảng cấu hình và từng mục con bên trong đều hợp lệ và
 * được hỗ trợ.
 * @retval  FALSE Có ít nhất một trường không hợp lệ (mảng rỗng, sai ClockId,
 * nguồn xung chưa hỗ trợ, bật bypass không hỗ trợ, hoặc TargetFrequencyHz khác
 * với HSE_VALUE khi build).
 */
static boolean Mcu_IsConfigTableValid(const Mcu_ConfigType *ConfigPtr) {
  uint8 index;

  if ((ConfigPtr->ClockSettings == NULL_PTR) ||
      (ConfigPtr->ClockSettingCount == 0U)) {
    return FALSE;
  }

  for (index = 0U; index < ConfigPtr->ClockSettingCount; index++) {
    const Mcu_ClockSettingConfigType *entry = &ConfigPtr->ClockSettings[index];

    /* Đảm bảo ClockId khớp đúng vị trí phần tử để tra bảng O(1) theo chỉ số an
     * toàn. */
    if (entry->ClockId != (Mcu_ClockType)index) {
      return FALSE;
    }

    /* Phạm vi bài học chỉ hỗ trợ trực tiếp thạch anh ngoài HSE (không bypass,
     * không PLL). */
    if ((entry->ClockSource != MCU_CLOCK_SOURCE_HSE) ||
        (entry->EnableHseBypass != FALSE)) {
      return FALSE;
    }

    /*
     * SystemCoreClockUpdate() của CMSIS tính tần số dựa theo macro HSE_VALUE
     * khi build. Kiểm tra khớp tần số giúp phát hiện sớm sai lệch giữa
     * Mcu_PBcfg.c và Makefile.
     */
    if (entry->TargetFrequencyHz != (uint32)HSE_VALUE) {
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * @brief   Chờ có giới hạn cho đến khi các bit được che bởi Mask trong thanh
 * ghi đạt ExpectedValue.
 * @details Dùng kiểu con trỏ volatile uint32_t khớp với định nghĩa thanh ghi
 * CMSIS (__IO uint32_t) để trình biên dịch luôn đọc trực tiếp từ phần cứng ở
 * mỗi vòng lặp. Mọi vòng chờ phần cứng trong driver đều đi qua hàm này nhằm
 * loại bỏ vòng lặp vô hạn.
 *
 * @param[in] RegisterPtr   Địa chỉ thanh ghi phần cứng cần theo dõi.
 * @param[in] Mask          Mặt nạ chọn các bit cần kiểm tra.
 * @param[in] ExpectedValue Giá trị mong đợi sau khi áp mặt nạ Mask.
 * @param[in] MaxChecks     Số lượt đọc thanh ghi tối đa (KHÔNG phải micro
 * giây).
 *
 * @return  boolean
 * @retval  TRUE  Thanh ghi đạt đúng giá trị mong đợi trong giới hạn MaxChecks
 * lượt kiểm tra.
 * @retval  FALSE Hết số lượt kiểm tra nhưng phần cứng vẫn chưa đạt trạng thái
 * mong đợi, hoặc RegisterPtr là NULL_PTR.
 */
static boolean Mcu_WaitForRegisterBits(const volatile uint32_t *RegisterPtr,
                                       uint32_t Mask, uint32_t ExpectedValue,
                                       uint32 MaxChecks) {
  uint32 checkCount;

  if (RegisterPtr == NULL_PTR) {
    return FALSE;
  }

  for (checkCount = 0U; checkCount < MaxChecks; checkCount++) {
    if (((*RegisterPtr) & Mask) == ExpectedValue) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * @brief   Đưa hệ thống quay về bộ dao động nội HSI theo trình tự an toàn của
 * STM32F103.
 * @details Gọi trong nhánh xử lý lỗi phần cứng khi HSE không ổn định hoặc không
 * xác nhận chuyển nguồn đúng hạn. Mọi vòng chờ trong hàm đều có giới hạn số
 * lượt kiểm tra. Trình tự bắt buộc:
 *          1. Bật HSI (HSION) và chờ HSIRDY.
 *          2. Chỉ khi HSI đã sẵn sàng mới ghi SW = HSI và chờ phần cứng xác
 * nhận SWS == HSI.
 *          3. Chỉ khi đọc được SWS == HSI mới được phép tắt HSEON. Nếu chưa xác
 * nhận về được HSI, tuyệt đối không tắt HSE vì CPU có thể vẫn đang phụ thuộc
 * nguồn đó.
 *          4. Gọi SystemCoreClockUpdate() để biến CMSIS phản ánh đúng nguồn
 * xung hiện tại.
 */
static void Mcu_FallbackToHsi(void) {
  boolean hsiReady;
  boolean hsiSwitched = FALSE;
  uint32_t cfgrValue;

  /* Bước 1: Đảm bảo HSI được bật (giữ nguyên các bit khác) và chờ ổn định. */
  RCC->CR |= RCC_CR_HSION;
  hsiReady = Mcu_WaitForRegisterBits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY,
                                     MCU_CLOCK_READY_MAX_CHECKS);

  /* Bước 2: Chỉ yêu cầu chuyển mạch sang HSI nếu HSI đã thực sự ổn định. */
  if (hsiReady == TRUE) {
    /* Đọc-sửa-ghi trường SW [1:0] để bảo toàn toàn bộ bộ chia bus và các bit
     * khác. */
    cfgrValue = RCC->CFGR;
    cfgrValue &= ~RCC_CFGR_SW;
    cfgrValue |= RCC_CFGR_SW_HSI;
    RCC->CFGR = cfgrValue;

    /* Chỉ kết luận đã về HSI khi đọc trường SWS [3:2] xác nhận đúng
     * RCC_CFGR_SWS_HSI. */
    hsiSwitched =
        Mcu_WaitForRegisterBits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI,
                                MCU_CLOCK_SWITCH_MAX_CHECKS);
  }

  /*
   * Bước 3: Chỉ tắt HSEON sau khi SWS đã xác nhận CPU đang chạy hoàn toàn bằng
   * HSI. Nếu hsiSwitched == FALSE, không chạm vào HSEON để tránh làm mất xung
   * đột ngột của CPU.
   */
  if (hsiSwitched == TRUE) {
    RCC->CR &= ~RCC_CR_HSEON;
  }

  /* Cập nhật lại SystemCoreClock dựa trên trường SWS thực tế của chip. */
  SystemCoreClockUpdate();
}

/* ========================================================================== */
/*  Triển khai các hàm API công khai                                          */
/* ========================================================================== */

/**
 * @brief   Khởi tạo module Mcu và ghi nhớ bảng cấu hình hợp lệ.
 * @details Xem đầy đủ đặc tả tham số, điều kiện gọi và chính sách lỗi tại
 * Mcu.h.
 */
void Mcu_Init(const Mcu_ConfigType *ConfigPtr) {
  /* Kiểm tra con trỏ rỗng trước khi truy cập bất kỳ trường nào của bảng cấu
   * hình. */
  if (ConfigPtr == NULL_PTR) {
    /* Đưa về trạng thái chưa khởi tạo để không tồn tại trạng thái khởi tạo nửa
     * chừng. */
    Mcu_ConfigPtr = NULL_PTR;
    Mcu_IsInitialized = FALSE;
    Mcu_ReportDevError(MCU_SID_INIT, MCU_E_PARAM_POINTER);
    return;
  }

  /* Kiểm tra toàn bộ nội dung bảng cấu hình trước khi cam kết lưu trạng thái.
   */
  if (Mcu_IsConfigTableValid(ConfigPtr) == FALSE) {
    Mcu_ConfigPtr = NULL_PTR;
    Mcu_IsInitialized = FALSE;
    Mcu_ReportDevError(MCU_SID_INIT, MCU_E_PARAM_CONFIG);
    return;
  }

  /* Chỉ ghi nhận địa chỉ cấu hình và bật cờ khởi tạo sau khi mọi kiểm tra đều
   * đạt. */
  Mcu_ConfigPtr = ConfigPtr;
  Mcu_IsInitialized = TRUE;
}

/**
 * @brief   Kích hoạt và chuyển nguồn xung hệ thống theo cấu hình ClockSetting.
 * @details Xem đầy đủ đặc tả tham số, điều kiện gọi và giá trị trả về tại
 * Mcu.h.
 */
Std_ReturnType Mcu_InitClock(Mcu_ClockType ClockSetting) {
  const Mcu_ClockSettingConfigType *clockCfg;
  boolean hseReady;
  boolean hseSwitched;
  uint32_t cfgrValue;

  /* 1. Kiểm tra trình tự khởi tạo trước khi đọc con trỏ cấu hình hoặc thanh
   * ghi. */
  if ((Mcu_IsInitialized == FALSE) || (Mcu_ConfigPtr == NULL_PTR)) {
    Mcu_ReportDevError(MCU_SID_INIT_CLOCK, MCU_E_UNINIT);
    return E_NOT_OK;
  }

  /* 2. Kiểm tra mã cấu hình nằm trong phạm vi trước khi tra bảng theo chỉ số.
   */
  if (ClockSetting >= Mcu_ConfigPtr->ClockSettingCount) {
    Mcu_ReportDevError(MCU_SID_INIT_CLOCK, MCU_E_PARAM_CLOCK);
    return E_NOT_OK;
  }

  clockCfg = &Mcu_ConfigPtr->ClockSettings[ClockSetting];

  /* Kiểm tra phòng vệ lại mục cấu hình trước khi tác động phần cứng. */
  if ((clockCfg->ClockId != ClockSetting) ||
      (clockCfg->ClockSource != MCU_CLOCK_SOURCE_HSE) ||
      (clockCfg->EnableHseBypass != FALSE)) {
    Mcu_ReportDevError(MCU_SID_INIT_CLOCK, MCU_E_PARAM_CLOCK);
    return E_NOT_OK;
  }

  /*
   * 3. Chuẩn bị chế độ thạch anh ngoài (không bypass) và bật bộ dao động HSE.
   * Theo tài liệu RM0008 (STM32F103), bit HSEBYP chỉ được phép ghi khi HSEON ==
   * 0. Nếu HSE đang bật sẵn (ví dụ gọi lại Mcu_InitClock khi đã chạy HSE),
   * không chạm vào HSEBYP.
   */
  if ((RCC->CR & RCC_CR_HSEON) == 0U) {
    RCC->CR &= ~RCC_CR_HSEBYP;
  }

  /* Đặt bit HSEON và bảo toàn toàn bộ các bit không liên quan trong RCC->CR
   * (như HSION). */
  RCC->CR |= RCC_CR_HSEON;

  /*
   * Giai đoạn 1 - Chờ thạch anh ổn định (HSERDY == 1):
   * Cờ HSERDY chỉ cho biết mạch dao động ngoài đã ổn định tần số, CHƯA đồng
   * nghĩa CPU đã chạy bằng HSE (lúc này CPU vẫn đang chạy bằng nguồn cũ, mặc
   * định là HSI).
   */
  hseReady =
      Mcu_WaitForRegisterBits(&RCC->CR, RCC_CR_HSERDY,
                              RCC_CR_HSERDY,
                              MCU_CLOCK_READY_MAX_CHECKS);

  if (hseReady == FALSE) {
    /* Lỗi phần cứng (không gọi Det): quay về HSI an toàn và báo thất bại
     * E_NOT_OK. */
    Mcu_FallbackToHsi();
    return E_NOT_OK;
  }

  /*
   * Giai đoạn 2 - Phần mềm gửi yêu cầu chuyển nguồn xung hệ thống sang HSE (SW
   * = 0b01): Thực hiện đọc-sửa-ghi chỉ trên 2 bit trường SW [1:0], bảo toàn
   * nguyên vẹn các bit bộ chia bus (HPRE, PPRE1, PPRE2), ADC, PLL và MCO trong
   * RCC->CFGR. Ghi xong SW mới chỉ là YÊU CẦU của phần mềm, phần cứng cần thêm
   * vài chu kỳ xung để chuyển mạch đồng bộ không tạo xung gai (glitch-free).
   */
  cfgrValue = RCC->CFGR;
  cfgrValue &= ~RCC_CFGR_SW;
  cfgrValue |= RCC_CFGR_SW_HSE;
  RCC->CFGR = cfgrValue;

  /*
   * Giai đoạn 3 - Chờ phần cứng xác nhận đã chuyển xong (SWS == 0b01):
   * Chỉ kiểm tra HSERDY là CHƯA ĐỦ vì HSERDY chỉ phản ánh bộ dao động đã chạy,
   * không phản ánh bộ chọn xung hệ thống (SYSCLK mux) đã chuyển sang HSE hay
   * chưa. Chỉ khi đọc trường SWS [3:2] khớp RCC_CFGR_SWS_HSE mới chắc chắn CPU
   * đang dùng HSE.
   */
  hseSwitched = Mcu_WaitForRegisterBits(
      &RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSE, MCU_CLOCK_SWITCH_MAX_CHECKS);

  if (hseSwitched == FALSE) {
    /*
     * Lỗi phần cứng: phần cứng không xác nhận chuyển sang HSE trong giới hạn
     * cho phép. Quay về HSI an toàn và vẫn trả E_NOT_OK vì cấu hình HSE được
     * yêu cầu đã thất bại.
     */
    Mcu_FallbackToHsi();
    return E_NOT_OK;
  }

  /* Chỉ cập nhật biến tần số CMSIS và trả E_OK sau khi bước xác nhận SWS thành
   * công. */
  SystemCoreClockUpdate();
  return E_OK;
}
uint8 Mcu_GetClockSourceStatus(void) {
  /* Hai bit SWS [3:2] phản ánh nguồn thực tế mà phần cứng đang dùng. */
  return (uint8)((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos);
}
