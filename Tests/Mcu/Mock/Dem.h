/**
 * @file    Dem.h
 * @brief   Mock DEM tối thiểu phục vụ kiểm thử ánh xạ MCU_E_CLOCK_FAILURE
 * (FAILED/PASSED).
 */
#ifndef DEM_H
#define DEM_H

#include "Std_Types.h"

typedef uint16 Dem_EventIdType;
typedef uint8 Dem_EventStatusType;

#define DEM_EVENT_STATUS_PASSED ((Dem_EventStatusType)0x00U)
#define DEM_EVENT_STATUS_FAILED ((Dem_EventStatusType)0x01U)
#define DemConf_DemEventParameter_MCU_E_CLOCK_FAILURE ((Dem_EventIdType)42U)

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId,
                                  Dem_EventStatusType EventStatus);

#endif /* DEM_H */