#ifndef XPT2046_TOUCH_H
#define XPT2046_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Initial calibration for a typical XPT2046-compatible 240x320 module.
   Adjust these four raw limits after checking the two screen corners. */
#define XPT2046_RAW_X_MIN  300U
#define XPT2046_RAW_X_MAX  3800U
#define XPT2046_RAW_Y_MIN  300U
#define XPT2046_RAW_Y_MAX  3900U

void XPT2046_Init(void);
uint8_t XPT2046_Read(uint16_t *x, uint16_t *y);

typedef struct
{
  volatile uint16_t raw_x;
  volatile uint16_t raw_y;
  volatile uint16_t pressure;
  volatile uint32_t reads;
  volatile uint8_t irq_level;
} XPT2046_Diagnostics;

const XPT2046_Diagnostics *XPT2046_GetDiagnostics(void);

#ifdef __cplusplus
}
#endif

#endif
