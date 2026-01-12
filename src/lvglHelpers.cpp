#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "main.h"
#include "lvglHelpers.h"

namespace {
constexpr uint16_t kLvglHorRes = 320;
constexpr uint16_t kLvglVerRes = 240;
constexpr uint32_t kLvglBufPixels = kLvglHorRes * 40;

lv_display_t *g_display = nullptr;
lv_indev_t *g_touch = nullptr;

void lvglFlushCb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
	uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
	uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);

	tft.startWrite();
	tft.setAddrWindow(area->x1, area->y1, w, h);
	tft.pushColors(reinterpret_cast<uint16_t *>(px_map), w * h, true);
	tft.endWrite();

	lv_display_flush_ready(display);
}

void lvglTouchRead(lv_indev_t *indev, lv_indev_data_t *data)
{
	uint16_t x = 0;
	uint16_t y = 0;

	bool touched = tft.getTouch(&x, &y);
	if (touched)
	{
		data->state = LV_INDEV_STATE_PRESSED;
		data->point.x = x;
		data->point.y = y;
	}
	else
	{
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

void createOverlay()
{
	lv_obj_t *label = lv_label_create(lv_screen_active());
	lv_label_set_text(label, "LVGL");
	lv_obj_set_style_text_color(label, lv_color_hex(0x00FF66), 0);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -6, 4);
}

} // namespace

void initLvgl()
{
	lv_init();

	lv_color_t *buf1 = static_cast<lv_color_t *>(
		heap_caps_malloc(kLvglBufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	lv_color_t *buf2 = static_cast<lv_color_t *>(
		heap_caps_malloc(kLvglBufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

	if (!buf1)
	{
		buf1 = static_cast<lv_color_t *>(malloc(kLvglBufPixels * sizeof(lv_color_t)));
	}

	if (!buf1)
	{
		Serial.println("LVGL buffer allocation failed");
		return;
	}

	if (!buf2)
	{
		buf2 = nullptr;
	}

	g_display = lv_display_create(kLvglHorRes, kLvglVerRes);
	lv_display_set_flush_cb(g_display, lvglFlushCb);
	lv_display_set_buffers(g_display, buf1, buf2, kLvglBufPixels, LV_DISPLAY_RENDER_MODE_PARTIAL);

	g_touch = lv_indev_create();
	lv_indev_set_type(g_touch, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(g_touch, lvglTouchRead);

	createOverlay();
}

void lvglTaskHandler()
{
	if (!g_display)
	{
		return;
	}

	static uint32_t last_tick = 0;
	static uint32_t last_handler = 0;

	uint32_t now = millis();
	if (last_tick == 0)
	{
		last_tick = now;
	}

	uint32_t elapsed = now - last_tick;
	if (elapsed > 0)
	{
		lv_tick_inc(elapsed);
		last_tick = now;
	}

	if (now - last_handler >= 5)
	{
		lv_timer_handler();
		last_handler = now;
	}
}
