#include "lv_port_disp.h"

#include "lcd_ili9341.h"
#include "xpt2046_touch.h"
#include "lvgl.h"

#define LVGL_BUF_LINES 20U

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[LCD_WIDTH * LVGL_BUF_LINES];
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  uint16_t x;
  uint16_t y;
  (void)drv;

  if (XPT2046_Read(&x, &y) != 0U)
  {
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = LV_INDEV_STATE_PR;
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

static void lcd_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
  uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);

  LCD_SetAddressWindow((uint16_t)area->x1, (uint16_t)area->y1,
                       (uint16_t)area->x2, (uint16_t)area->y2);
  LCD_WritePixels((const uint16_t *)color_p, width * height);

  lv_disp_flush_ready(drv);
}

void lv_port_indev_init(void)
{
  XPT2046_Init();
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read_cb;
  lv_indev_drv_register(&indev_drv);
}

void lv_port_disp_init(void)
{
  lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, LCD_WIDTH * LVGL_BUF_LINES);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = lcd_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
}
