/**
 * @file Dio.c
 * @brief Thực hiện đọc/ghi chân số. IDR phản ánh mức điện ở đầu vào, ODR là chốt đầu ra, BSRR cho phép đặt hoặc xóa các bit được chọn.
 */
#include "Dio.h"
#include "stm32f1xx.h"

#if (DIO_DEV_ERROR_DETECT == STD_ON)
    #include "Det.h"
#endif

/*==============================================================================
 * INTERNAL CONSTANTS
 *============================================================================*/
/** @brief Số cổng trong bảng địa chỉ phần cứng của Dio. */
#define DIO_NUM_OF_HW_PORTS     (4u)

/* Chỉ số cổng trong mã kênh chọn địa chỉ khối thanh ghi: A = 0, B = 1, C = 2, D = 3. */
static GPIO_TypeDef * const Dio_GpioBase[DIO_NUM_OF_HW_PORTS] =
{
    GPIOA,
    GPIOB,
    GPIOC,
    GPIOD
};

/*==============================================================================
 * PRIVATE HELPER FUNCTIONS
 *============================================================================*/
/** @brief Kiểm tra chỉ số trước khi dùng làm chỉ số mảng địa chỉ cổng. */
static boolean Dio_IsPortValid(Dio_PortType PortId)
{
    return (PortId < DIO_NUM_OF_HW_PORTS) ? TRUE : FALSE;
}

/** @brief Tách mã kênh thành cổng và số chân rồi kiểm tra theo sơ đồ chân của chip. */
static boolean Dio_IsChannelValid(Dio_ChannelType ChannelId)
{
    /* Ví dụ mã 45 (0x2D): dịch phải 4 bit lấy cổng C, giữ 4 bit thấp lấy chân 13. */
    uint8 portIdx = (uint8)(ChannelId >> 4u);
    uint8 pinIdx  = (uint8)(ChannelId & 0x0Fu);
    boolean isValid = FALSE;

    if ((portIdx == 0u) || (portIdx == 1u))
    {
        isValid = TRUE;
    }
    else if ((portIdx == 2u) && (pinIdx >= 13u))
    {
        isValid = TRUE;
    }
    else if ((portIdx == 3u) && (pinIdx <= 1u))
    {
        isValid = TRUE;
    }
    else
    {
        isValid = FALSE;
    }

    return isValid;
}

/* Kiem tra truc tiep MODE[1:0] trong CRL/CRH: != 0 nghia la Output */
/** @brief Đọc hai bit MODE của chân; giá trị khác 0 biểu thị đầu ra trên STM32F1. */
static boolean Dio_IsPinConfiguredAsOutput(const GPIO_TypeDef *Gpio, uint8 PinIdx)
{
    uint32 shift = (uint32)(PinIdx & 0x07u) * 4u;
    uint32 regVal = (PinIdx < 8u) ? Gpio->CRL : Gpio->CRH;
    uint32 modeBits = (regVal >> shift) & 0x03u;

    return (modeBits != 0u) ? TRUE : FALSE;
}

/*==============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId)
{
    uint8 portIdx;
    uint8 pinIdx;
    const GPIO_TypeDef *gpio;
    Dio_LevelType level = STD_LOW;

    if (Dio_IsChannelValid(ChannelId) == FALSE)
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_READ_CHANNEL_SID, DIO_E_PARAM_INVALID_CHANNEL_ID);
#endif
        return STD_LOW;
    }

    portIdx = (uint8)(ChannelId >> 4u);
    pinIdx  = (uint8)(ChannelId & 0x0Fu);
    gpio    = Dio_GpioBase[portIdx];

    /* Mặt nạ chỉ giữ bit của chân cần đọc, kể cả khi những chân khác cùng cổng đang ở mức cao. */
    if ((gpio->IDR & (1uL << pinIdx)) != 0u)
    {
        level = STD_HIGH;
    }

    return level;
}

void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level)
{
    uint8 portIdx;
    uint8 pinIdx;
    GPIO_TypeDef *gpio;

    if (Dio_IsChannelValid(ChannelId) == FALSE)
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_WRITE_CHANNEL_SID, DIO_E_PARAM_INVALID_CHANNEL_ID);
#endif
        return;
    }

    portIdx = (uint8)(ChannelId >> 4u);
    pinIdx  = (uint8)(ChannelId & 0x0Fu);
    gpio    = Dio_GpioBase[portIdx];

    /* [SWS_Dio_00029] Khong ghi len chan Input (tranh lam thay doi Pull-Up/Pull-Down qua ODR tren STM32F1) */
    if (Dio_IsPinConfiguredAsOutput(gpio, pinIdx) == TRUE)
    {
        if (Level == STD_HIGH)
        {
            gpio->BSRR = (1uL << pinIdx);
        }
        else
        {
            /* Nửa cao của BSRR kéo chân xuống thấp; bit 29 ứng với chân 13. */
            gpio->BSRR = (1uL << (pinIdx + 16u));
        }
    }
}

Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId)
{
    if (Dio_IsPortValid(PortId) == FALSE)
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_READ_PORT_SID, DIO_E_PARAM_INVALID_PORT_ID);
#endif
        return (Dio_PortLevelType)0u;
    }

    return (Dio_PortLevelType)(Dio_GpioBase[PortId]->IDR & 0xFFFFu);
}

void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level)
{
    if (Dio_IsPortValid(PortId) == FALSE)
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_WRITE_PORT_SID, DIO_E_PARAM_INVALID_PORT_ID);
#endif
        return;
    }

    /* Ghi đè cả 16 chốt: các chân có bit 0 trong Level cũng bị tác động. */
    Dio_GpioBase[PortId]->ODR = (uint32)Level;
}

Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr)
{
    const GPIO_TypeDef *gpio;
    uint32 portVal;

    if ((ChannelGroupIdPtr == NULL_PTR) ||
        (Dio_IsPortValid(ChannelGroupIdPtr->port) == FALSE) ||
        (ChannelGroupIdPtr->offset >= 16u))
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_READ_CHANNEL_GROUP_SID, DIO_E_PARAM_INVALID_GROUP);
#endif
        return (Dio_PortLevelType)0u;
    }

    gpio    = Dio_GpioBase[ChannelGroupIdPtr->port];
    /* Ví dụ nhóm đèn: giữ bit 13 rồi dịch phải 13 để bên gọi nhận giá trị 0 hoặc 1. */
    portVal = gpio->IDR & (uint32)ChannelGroupIdPtr->mask;

    return (Dio_PortLevelType)(portVal >> ChannelGroupIdPtr->offset);
}

void Dio_WriteChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr, Dio_PortLevelType Level)
{
    GPIO_TypeDef *gpio;
    uint32 mask;
    uint32 setBits;
    uint32 resetBits;

    if ((ChannelGroupIdPtr == NULL_PTR) ||
        (Dio_IsPortValid(ChannelGroupIdPtr->port) == FALSE) ||
        (ChannelGroupIdPtr->offset >= 16u))
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_WRITE_CHANNEL_GROUP_SID, DIO_E_PARAM_INVALID_GROUP);
#endif
        return;
    }

    gpio      = Dio_GpioBase[ChannelGroupIdPtr->port];
    mask      = (uint32)ChannelGroupIdPtr->mask;
    /* Dịch giá trị nhóm vào đúng vị trí; chỉ các bit trong mặt nạ được phép thay đổi. */
    setBits   = ((uint32)Level << ChannelGroupIdPtr->offset) & mask;
    /* Các chân còn lại trong nhóm cần kéo thấp. Bit ngoài nhóm bằng 0 ở cả hai nửa lệnh ghi. */
    resetBits = (~setBits) & mask;

    /* Thao tac atomic qua thanh ghi BSRR - an toan voi ngat (ISR-safe) */
    gpio->BSRR = setBits | (resetBits << 16u);
}

#if (DIO_VERSION_INFO_API == STD_ON)
void Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo)
{
    if (VersionInfo == NULL_PTR)
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_GET_VERSION_INFO_SID, DIO_E_PARAM_POINTER);
#endif
        return;
    }

    VersionInfo->vendorID         = DIO_VENDOR_ID;
    VersionInfo->moduleID         = DIO_MODULE_ID;
    VersionInfo->sw_major_version = DIO_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = DIO_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = DIO_SW_PATCH_VERSION;
}
#endif /* DIO_VERSION_INFO_API == STD_ON */

#if (DIO_FLIP_CHANNEL_API == STD_ON)
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId)
{
    uint8 portIdx;
    uint8 pinIdx;
    GPIO_TypeDef *gpio;
    Dio_LevelType newLevel;

    if (Dio_IsChannelValid(ChannelId) == FALSE)
    {
#if (DIO_DEV_ERROR_DETECT == STD_ON)
        (void)Det_ReportError(DIO_MODULE_ID, DIO_INSTANCE_ID, DIO_FLIP_CHANNEL_SID, DIO_E_PARAM_INVALID_CHANNEL_ID);
#endif
        return STD_LOW;
    }

    portIdx = (uint8)(ChannelId >> 4u);
    pinIdx  = (uint8)(ChannelId & 0x0Fu);
    gpio    = Dio_GpioBase[portIdx];

    /* [SWS_Dio_00191] & [SWS_Dio_00192]: Chi dao trang thai neu la Output; neu la Input thi tra ve muc hien tai */
    if (Dio_IsPinConfiguredAsOutput(gpio, pinIdx) == TRUE)
    {
        /* Đảo theo chốt đầu ra. Đọc rồi ghi là hai thao tác; nếu hai bên cùng đảo một chân, có thể mất một lần đảo. */
        if ((gpio->ODR & (1uL << pinIdx)) != 0u)
        {
            /* Nửa cao của BSRR kéo chân xuống thấp; bit 29 ứng với chân 13. */
            gpio->BSRR = (1uL << (pinIdx + 16u));
            newLevel = STD_LOW;
        }
        else
        {
            gpio->BSRR = (1uL << pinIdx);
            newLevel = STD_HIGH;
        }
    }
    else
    {
        newLevel = ((gpio->IDR & (1uL << pinIdx)) != 0u) ? STD_HIGH : STD_LOW;
    }

    return newLevel;
}
#endif /* DIO_FLIP_CHANNEL_API == STD_ON */
