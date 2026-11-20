/**
 * @file Port_PBcfg.c
 * @brief Bảng cấu hình được liên kết vào firmware. Board_Init truyền địa chỉ Port_Config cho Port_Init khi khởi động.
 */
#include "Port.h"

/*==============================================================================
 * POST-BUILD PIN CONFIGURATION ARRAY
 *============================================================================*/
/* Mỗi mục mô tả một chân. PC13 được nạp mức cao để đèn tắt trước khi chuyển sang đầu ra. */
static const Port_PinConfigType Port_PinConfigs[PORT_NUM_OF_CONFIGURED_PINS] =
{
    /* PC13: Onboard LED on STM32F103C8T6 Bluepill (Active-Low)
     * Datasheet STM32F103 khuyen cao toc do toi da 2MHz cho PC13/PC14/PC15.
     */
    {
        .PinId               = PortConf_PortPin_LED_PC13,
        .Direction           = PORT_PIN_OUT,
        .Mode                = PORT_PIN_MODE_DIO,
        .InitialLevel        = PORT_PIN_LEVEL_HIGH, /* Tat LED luc khoi tao */
        .Pull                = PORT_PIN_PULL_NONE,
        .OutputType          = PORT_PIN_OUT_PUSHPULL,
        .Speed               = PORT_PIN_SPEED_2MHZ,
        .DirectionChangeable = FALSE,
        .ModeChangeable      = FALSE
    }
};

/*==============================================================================
 * ROOT POST-BUILD CONFIGURATION STRUCTURE
 *============================================================================*/
/* Đối tượng board truyền vào Port_Init ghép địa chỉ bảng với số mục driver cần duyệt. */
const Port_ConfigType Port_Config =
{
    .PinConfigs = Port_PinConfigs,
    .NumOfPins  = PORT_NUM_OF_CONFIGURED_PINS
};
