/**
 * @file Dio_Lcfg.c
 * @brief Định nghĩa nhóm chân được liên kết vào firmware. Nhóm đèn chỉ chọn bit 13 của cổng C, nên bit 0 trong giá trị nhóm ứng với chân 13.
 */
#include "Dio.h"

/*==============================================================================
 * LINK-TIME CHANNEL GROUP DEFINITIONS (Vi du cho PC13)
 *============================================================================*/
/* 0x2000 chọn duy nhất bit 13. Dịch trái giá trị nhóm 1 đúng 13 vị trí sẽ đặt chân đó lên cao. */
const Dio_ChannelGroupType DioConf_DioChannelGroup_PORTC_LED =
{
    .mask   = 0x2000u, /* Bit 13 */
    .offset = 13u,
    .port   = DioConf_DioPort_PORTC
};
