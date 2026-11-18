#ifndef FDCAN_PORT_H
#define FDCAN_PORT_H

#include "FDCAN.h"

extern FDCAN_Handle_t g_fdcan1_handle;
FDCAN_Result_t FDCAN_Port_Init(void);
bool FDCAN_Port_TakeRxReady(void);
void FDCAN1_IT0_IRQHandler(void);

#endif
