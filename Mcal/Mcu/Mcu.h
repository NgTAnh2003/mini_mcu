/**
 * @file    Mcu.h
 * @brief   Giao diện công khai (Public API) của module Mcal/Mcu cho dự án học
 * AUTOSAR Classic.
 * @details File này chỉ công khai các kiểu dữ liệu trừu tượng, mã định danh,
 * bảng cấu hình và các hàm khởi tạo, chọn và đọc nguồn xung. File
 * này KHÔNG include "stm32f1xx.h" và không để lộ địa chỉ hay kiểu thanh ghi của
 * STM32 ra các tầng gọi bên trên. Phạm vi chỉ quản lý nguồn xung hệ thống
 * (không PLL, không reset, không sleep/RAM).
 */

#ifndef MCU_H
#define MCU_H

#include "Mcu_Cfg.h"
#include "Std_Types.h"

/* ========================================================================== */
/*  Mã định danh Module, Thể hiện, Dịch vụ API và Lỗi phát triển (Det)        */
/*  Tham chiếu: AUTOSAR Classic R24-11, Specification of MCU Driver           */
/* ========================================================================== */

/**
 * @brief   Mã định danh chuẩn của MCU Driver trong AUTOSAR Classic (Module ID =
 * 101).
 */
#define MCU_MODULE_ID ((uint16)101U)

/**
 * @brief   Mã định danh thể hiện mặc định của MCU Driver (đơn thể hiện = 0).
 */
#define MCU_INSTANCE_ID ((uint8)0U)

/**
 * @brief   Mã dịch vụ API của hàm Mcu_Init() theo AUTOSAR SWS MCU Driver
 * (0x00).
 */
#define MCU_SID_INIT ((uint8)0x00U)

/**
 * @brief   Mã dịch vụ API của hàm Mcu_InitClock() theo AUTOSAR SWS MCU Driver
 * (0x02).
 */
#define MCU_SID_INIT_CLOCK ((uint8)0x02U)

/**
 * @brief   Mã lỗi Det theo SWS_Mcu_00012 (0x0A): tham số cấu hình trong
 * Mcu_Init() không hợp lệ.
 * @details [Chính sách riêng của dự án] Được báo khi bảng cấu hình không NULL
 * nhưng nội dung bên trong sai (mảng cấu hình NULL, số mục bằng 0, mã ID không
 * khớp, nguồn xung/chế độ chưa được hỗ trợ hoặc tần số không khớp HSE_VALUE).
 */
#define MCU_E_PARAM_CONFIG ((uint8)0x0AU)

/**
 * @brief   Mã lỗi Det theo SWS_Mcu_00012 (0x0B): mã cấu hình xung ClockSetting
 * nằm ngoài phạm vi.
 */
#define MCU_E_PARAM_CLOCK ((uint8)0x0BU)

/**
 * @brief   Mã lỗi Det theo SWS_Mcu_00012 (0x0F): gọi API khi module Mcu chưa
 * được khởi tạo hợp lệ.
 */
#define MCU_E_UNINIT ((uint8)0x0FU)

/**
 * @brief   Mã lỗi Det theo SWS_Mcu_00012 (0x10): truyền con trỏ NULL_PTR vào
 * API.
 * @details [Chính sách riêng của dự án] Sử dụng mã chuẩn 0x10 để phân biệt rõ
 * lỗi truyền ConfigPtr == NULL_PTR vào Mcu_Init() với lỗi sai nội dung bảng cấu
 * hình (0x0A).
 */
#define MCU_E_PARAM_POINTER ((uint8)0x10U)

/* ========================================================================== */
/*  Kiểu dữ liệu công khai                                                    */
/* ========================================================================== */

/**
 * @brief   Kiểu mã chọn một mục cấu hình xung trong bảng cấu hình Mcu.
 * @details Giá trị là chỉ số (ID) của cấu hình (ví dụ
 * McuConf_McuClockSettingConfig_HSE_8MHz = 0U), không phải tần số tính bằng Hz.
 */
typedef uint8 Mcu_ClockType;

/**
 * @brief   Danh sách nguồn xung hệ thống mức trừu tượng (không phụ thuộc tên
 * thanh ghi STM32).
 * @details [Kiểu cấu hình riêng của dự án] Giúp tách biệt Mcu_PBcfg.c khỏi
 * header phần cứng.
 */
typedef enum {
  MCU_CLOCK_SOURCE_HSI =
      0U, /**< Bộ dao động RC nội (HSI) tích hợp trong chip. */
  MCU_CLOCK_SOURCE_HSE = 1U /**< Bộ dao động ngoài (HSE) nối trực tiếp làm xung
                               hệ thống, không qua PLL. */
} Mcu_ClockSourceType;

/**
 * @brief   Cấu trúc mô tả một mục cấu hình nguồn xung hệ thống.
 * @details [Kiểu cấu hình riêng của dự án] Mỗi phần tử trong mảng cấu hình
 * tương ứng với một giá trị Mcu_ClockType hợp lệ.
 */
typedef struct {
  Mcu_ClockType ClockId; /**< Mã nhận diện mục cấu hình, phải khớp chỉ số phần
                            tử trong mảng. */
  Mcu_ClockSourceType ClockSource; /**< Nguồn xung hệ thống mong muốn (bài học
                                      này hỗ trợ MCU_CLOCK_SOURCE_HSE). */
  boolean EnableHseBypass; /**< FALSE: dùng thạch anh ngoài 2 chân (Blue Pill);
                              TRUE: cấp xung vuông trực tiếp vào OSC_IN. */
  uint32 TargetFrequencyHz; /**< Tần số xung mục tiêu tính bằng Hz (ví dụ
                               8000000UL), phân biệt với ClockId. */
} Mcu_ClockSettingConfigType;

/**
 * @brief   Cấu trúc cấu hình tổng (Post-Build style) truyền vào hàm Mcu_Init().
 * @details Bảng cấu hình và mảng con bên trong phải có thời gian sống suốt
 * chương trình (const/static).
 */
typedef struct {
  const Mcu_ClockSettingConfigType
      *ClockSettings; /**< Con trỏ tới mảng hằng chứa các mục cấu hình xung. */
  uint8 ClockSettingCount; /**< Số lượng mục cấu hình hợp lệ trong mảng
                              ClockSettings. */
} Mcu_ConfigType;

/* ========================================================================== */
/*  Khai báo bảng cấu hình mặc định (định nghĩa trong Mcu_PBcfg.c)            */
/* ========================================================================== */

/**
 * @brief   Đối tượng cấu hình hằng của module Mcu cho bo Blue Pill dùng thạch
 * anh ngoài 8 MHz.
 * @details Được định nghĩa với từ khóa const trong Mcu_PBcfg.c, lưu trong bộ
 * nhớ Flash và tồn tại suốt vòng đời chương trình.
 */
extern const Mcu_ConfigType Mcu_Config;

/* ========================================================================== */
/*  Khai báo các hàm API công khai theo AUTOSAR Classic                       */
/* ========================================================================== */

/**
 * @brief   Khởi tạo module Mcu bằng cách kiểm tra và lưu địa chỉ bảng cấu hình
 * hợp lệ.
 * @details Hàm này KHÔNG thực hiện chuyển nguồn xung hệ thống; việc áp dụng
 * xung diễn ra ở hàm Mcu_InitClock(). [Chính sách xử lý lỗi riêng của dự án]:
 * Nếu ConfigPtr là NULL_PTR hoặc bất kỳ trường nào trong bảng cấu hình không
 * hợp lệ, driver sẽ giữ/đưa trạng thái nội bộ về chưa khởi tạo (không bao giờ
 * rơi vào trạng thái khởi tạo nửa chừng), báo lỗi qua Det (nếu
 * MCU_DEV_ERROR_DETECT == STD_ON) và không truy cập thanh ghi RCC.
 *
 * @param[in] ConfigPtr Con trỏ tới bảng cấu hình hợp lệ có thời gian sống suốt
 * chương trình (thường truyền &Mcu_Config).
 *
 * @pre     Không yêu cầu Mcu đã khởi tạo trước đó. Nên gọi sau Port_Init() theo
 * thứ tự của bo.
 * @post    Nếu cấu hình hợp lệ, module chuyển sang trạng thái đã khởi tạo và
 * sẵn sàng cho Mcu_InitClock().
 */
void Mcu_Init(const Mcu_ConfigType *ConfigPtr);

/**
 * @brief   Áp dụng cấu hình nguồn xung hệ thống tương ứng với mã ClockSetting.
 * @details Thực hiện tuần tự các bước trên STM32F103:
 *          1. Kiểm tra trạng thái đã khởi tạo và phạm vi hợp lệ của
 * ClockSetting.
 *          2. Bật thạch anh ngoài (HSEON) và chờ cờ ổn định (HSERDY) trong giới
 * hạn vòng lặp.
 *          3. Yêu cầu chuyển nguồn xung hệ thống sang HSE (SW = HSE) và chờ
 * phần cứng xác nhận (SWS == HSE).
 *          4. Chỉ khi SWS xác nhận xong mới gọi SystemCoreClockUpdate() và trả
 * E_OK. Nếu xảy ra lỗi phần cứng (HSE không ổn định hoặc không xác nhận chuyển
 * nguồn đúng hạn), driver tự động quay về HSI theo trình tự an toàn nhưng vẫn
 * trả E_NOT_OK vì yêu cầu chuyển sang HSE đã thất bại. Lỗi phần cứng KHÔNG gọi
 * Det_ReportError().
 *
 * @param[in] ClockSetting Mã chọn cấu hình xung (ví dụ
 * McuConf_McuClockSettingConfig_HSE_8MHz).
 *
 * @pre     Mcu_Init() phải đã được gọi thành công với một bảng cấu hình hợp lệ.
 *
 * @return  Std_ReturnType
 * @retval  E_OK     Thạch anh ngoài đã ổn định, phần cứng đã xác nhận SWS ==
 * HSE và SystemCoreClock đã cập nhật.
 * @retval  E_NOT_OK Thất bại do một trong hai nhóm nguyên nhân:
 *                   - Lỗi lập trình (chưa gọi Mcu_Init hoặc ClockSetting sai):
 * có báo Det nếu bật Det, không đụng thanh ghi RCC.
 *                   - Lỗi phần cứng (HSE không sẵn sàng hoặc SWS không chuyển
 * sang HSE đúng hạn): không báo Det, driver đã thực hiện trình tự quay về HSI
 * an toàn.
 */
Std_ReturnType Mcu_InitClock(Mcu_ClockType ClockSetting);

/**
 * @brief Đọc nguồn xung hệ thống đang hoạt động. Có thể gọi từ lúc reset.
 * @return Mã trạng thái hai bit: 0 nguồn nội, 1 nguồn ngoài, 2 PLL (mạch
 * nhân xung), 3 dành riêng. Hàm chỉ đọc và không đổi cấu hình.
 */
uint8 Mcu_GetClockSourceStatus(void);

#endif /* MCU_H */