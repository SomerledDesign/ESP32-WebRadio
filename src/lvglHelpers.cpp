#include <Arduino.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "lvglHelpers.h"

namespace {
constexpr uint16_t kLvglHorRes = 320;
constexpr uint16_t kLvglVerRes = 240;
constexpr uint32_t kLvglBufPixels = kLvglHorRes * 40;

constexpr int kPanelMargin = 6;
constexpr int kPanelY = 98;
constexpr int kPanelH = 82;
constexpr int kPanelGap = 6;
constexpr int kGenreBoxSize = kPanelH;
constexpr int kGenreIconPad = 10;
constexpr int kGenreIconSize = kGenreBoxSize - (kGenreIconPad * 2);
constexpr int kGenreBoxX = kLvglHorRes - kPanelMargin - kGenreBoxSize;
constexpr int kTrackPanelX = kPanelMargin;
constexpr int kTrackPanelW = kGenreBoxX - kPanelGap - kTrackPanelX;
constexpr int kTrackPanelPad = 8;
constexpr int kTrackLineSpacing = 20;

lv_display_t *g_display = nullptr;
lv_indev_t *g_touch = nullptr;

lv_obj_t *g_track_panel = nullptr;
lv_obj_t *g_track_label = nullptr;
lv_obj_t *g_artist_label = nullptr;
lv_obj_t *g_album_label = nullptr;

lv_obj_t *g_genre_box = nullptr;
lv_obj_t *g_genre_icon = nullptr;

enum GenreIcon {
    kIconNote,
    kIconGuitar,
    kIconMetal,
    kIconClassical,
    kIconMic,
};

struct GenreStyle {
    const char *key;
    const char *label;
    uint32_t bg;
    uint32_t fg;
};

const GenreStyle kGenreStyles[] = {
    {"hard rock", "METAL", 0x4A0000, 0xFF2A2A},
    {"heavy metal", "METAL", 0x4A0000, 0xFF2A2A},
    {"metal", "METAL", 0x4A0000, 0xFF2A2A},
    {"rock", "ROCK", 0x8B0000, 0xFF4C4C},
    {"classical", "CLASS", 0x123B7A, 0xCFE3FF},
    {"jazz", "JAZZ", 0x7A4B00, 0xFFE2B8},
    {"blues", "BLUES", 0x1A3D6D, 0xCFE1FF},
    {"pop", "POP", 0x8C4B00, 0xFFE0B3},
    {"country", "CNTRY", 0x4A2D12, 0xF5D7B0},
    {"news", "NEWS", 0x303030, 0xE0E0E0},
    {"talk", "TALK", 0x303030, 0xE0E0E0},
    {"ambient", "AMBIENT", 0x1C3F4A, 0xCFEFF7},
    {"chill", "CHILL", 0x1C3F4A, 0xCFEFF7},
    {nullptr, "GEN", 0x202020, 0xE0E0E0},
};

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

void toLower(const char *src, char *dst, size_t dst_len)
{
    if (!dst_len)
    {
        return;
    }
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    for (; i + 1 < dst_len && src[i]; ++i)
    {
        dst[i] = static_cast<char>(tolower(static_cast<unsigned char>(src[i])));
    }
    dst[i] = '\0';
}

void createTrackPanel()
{
    lv_obj_t *screen = lv_screen_active();

    g_track_panel = lv_obj_create(screen);
    lv_obj_set_size(g_track_panel, kTrackPanelW, kPanelH);
    lv_obj_set_pos(g_track_panel, kTrackPanelX, kPanelY);
    lv_obj_set_style_radius(g_track_panel, 8, 0);
    lv_obj_set_style_bg_color(g_track_panel, lv_color_hex(0x000033), 0);
    lv_obj_set_style_bg_opa(g_track_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_track_panel, 2, 0);
    lv_obj_set_style_border_color(g_track_panel, lv_color_hex(0x00AA66), 0);
    lv_obj_clear_flag(g_track_panel, LV_OBJ_FLAG_SCROLLABLE);

    const int label_width = kTrackPanelW - (kTrackPanelPad * 2);

    g_track_label = lv_label_create(g_track_panel);
    lv_obj_set_width(g_track_label, label_width);
    lv_obj_set_pos(g_track_label, kTrackPanelPad, kTrackPanelPad);
    lv_label_set_long_mode(g_track_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(g_track_label, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(g_track_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_track_label, "-");

    g_artist_label = lv_label_create(g_track_panel);
    lv_obj_set_width(g_artist_label, label_width);
    lv_obj_set_pos(g_artist_label, kTrackPanelPad, kTrackPanelPad + kTrackLineSpacing);
    lv_label_set_long_mode(g_artist_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(g_artist_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_artist_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_artist_label, "-");

    g_album_label = lv_label_create(g_track_panel);
    lv_obj_set_width(g_album_label, label_width);
    lv_obj_set_pos(g_album_label, kTrackPanelPad, kTrackPanelPad + (kTrackLineSpacing * 2));
    lv_label_set_long_mode(g_album_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(g_album_label, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_font(g_album_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_album_label, "-");

    g_genre_box = lv_obj_create(screen);
    lv_obj_set_size(g_genre_box, kGenreBoxSize, kPanelH);
    lv_obj_set_pos(g_genre_box, kGenreBoxX, kPanelY);
    lv_obj_set_style_radius(g_genre_box, 8, 0);
    lv_obj_set_style_bg_color(g_genre_box, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(g_genre_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_genre_box, 2, 0);
    lv_obj_set_style_border_color(g_genre_box, lv_color_hex(0x00AA66), 0);
    lv_obj_clear_flag(g_genre_box, LV_OBJ_FLAG_SCROLLABLE);

    g_genre_icon = lv_obj_create(g_genre_box);
    lv_obj_set_size(g_genre_icon, kGenreIconSize, kGenreIconSize);
    lv_obj_center(g_genre_icon);
    lv_obj_set_style_bg_opa(g_genre_icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_genre_icon, 0, 0);
    lv_obj_clear_flag(g_genre_icon, LV_OBJ_FLAG_SCROLLABLE);
}

void setLabelText(lv_obj_t *label, const char *value)
{
    if (!label)
    {
        return;
    }

    const char *safe_value = (value && value[0]) ? value : "-";
    lv_label_set_text(label, safe_value);
}

const GenreStyle *matchGenreStyle(const char *genre)
{
    char lower[64];
    toLower(genre, lower, sizeof(lower));
    for (size_t i = 0; i < sizeof(kGenreStyles) / sizeof(kGenreStyles[0]); ++i)
    {
        if (!kGenreStyles[i].key)
        {
            return &kGenreStyles[i];
        }
        if (lower[0] != '\0' && strstr(lower, kGenreStyles[i].key))
        {
            return &kGenreStyles[i];
        }
    }
    return &kGenreStyles[sizeof(kGenreStyles) / sizeof(kGenreStyles[0]) - 1];
}

GenreIcon matchGenreIcon(const char *genre)
{
    char lower[64];
    toLower(genre, lower, sizeof(lower));
    if (strstr(lower, "hard rock") || strstr(lower, "heavy metal") || strstr(lower, "metal"))
    {
        return kIconMetal;
    }
    if (strstr(lower, "rock"))
    {
        return kIconGuitar;
    }
    if (strstr(lower, "classical"))
    {
        return kIconClassical;
    }
    if (strstr(lower, "news") || strstr(lower, "talk"))
    {
        return kIconMic;
    }
    return kIconNote;
}

lv_obj_t *createIconRect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color, int radius)
{
    if (!parent || w <= 0 || h <= 0)
    {
        return nullptr;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

void drawGuitarIcon(lv_obj_t *parent, lv_color_t color, bool spiky)
{
    const int size = lv_obj_get_width(parent);
    const int body = (size * 48) / 100;
    const int body_x = (size * 12) / 100;
    const int body_y = size - body - (size * 8) / 100;
    const int neck_w = (size * 10) / 100;
    const int neck_h = (size * 45) / 100;
    const int neck_x = body_x + body - (neck_w / 2);
    const int neck_y = body_y - neck_h + (size * 6) / 100;
    int head_w = (size * 18) / 100;
    int head_h = (size * 10) / 100;
    int head_x = neck_x + neck_w - (head_w / 3);
    int head_y = neck_y - head_h + 2;
    if (head_y < 0)
    {
        head_y = 0;
    }

    createIconRect(parent, body_x, body_y, body, body, color, body / 2);
    createIconRect(parent, neck_x, neck_y, neck_w, neck_h, color, neck_w / 2);
    createIconRect(parent, head_x, head_y, head_w, head_h, color, head_h / 2);

    if (spiky)
    {
        int spike_w = (head_w < 6) ? head_w : head_w / 3;
        int spike_h = (head_h < 4) ? head_h : head_h / 2;
        createIconRect(parent, head_x, head_y - spike_h, spike_w, spike_h, color, 0);
        createIconRect(parent, head_x + head_w - spike_w, head_y - spike_h, spike_w, spike_h, color, 0);
    }
}

void drawViolinIcon(lv_obj_t *parent, lv_color_t color)
{
    const int size = lv_obj_get_width(parent);
    const int body_w = (size * 40) / 100;
    const int body_h = (size * 65) / 100;
    const int body_x = (size - body_w) / 2;
    const int body_y = size - body_h - (size * 8) / 100;
    const int neck_w = (size * 10) / 100;
    const int neck_h = (size * 30) / 100;
    const int neck_x = (size - neck_w) / 2;
    const int neck_y = body_y - neck_h + (body_w / 4);
    int scroll = neck_w + 4;
    int scroll_x = neck_x - (scroll - neck_w) / 2;
    int scroll_y = neck_y - scroll + 2;
    if (scroll_y < 0)
    {
        scroll_y = 0;
    }

    createIconRect(parent, body_x, body_y, body_w, body_h, color, body_w / 2);
    createIconRect(parent, neck_x, neck_y, neck_w, neck_h, color, neck_w / 2);
    createIconRect(parent, scroll_x, scroll_y, scroll, scroll, color, scroll / 2);
}

void drawNoteIcon(lv_obj_t *parent, lv_color_t color)
{
    const int size = lv_obj_get_width(parent);
    const int head = (size * 28) / 100;
    const int head_x = (size * 20) / 100;
    const int head_y = size - head - (size * 12) / 100;
    const int stem_w = (head / 5) ? (head / 5) : 2;
    const int stem_h = (size * 55) / 100;
    const int stem_x = head_x + head - (stem_w / 2);
    int stem_y = head_y - stem_h + (head / 3);
    if (stem_y < 0)
    {
        stem_y = 0;
    }
    const int flag_w = (size * 35) / 100;
    const int flag_h = (size * 12) / 100;
    const int flag_x = stem_x + stem_w - (flag_w / 4);
    const int flag_y = stem_y;

    createIconRect(parent, head_x, head_y, head, head, color, head / 2);
    createIconRect(parent, stem_x, stem_y, stem_w, stem_h, color, stem_w / 2);
    createIconRect(parent, flag_x, flag_y, flag_w, flag_h, color, flag_h / 2);
}

void drawMicIcon(lv_obj_t *parent, lv_color_t color)
{
    const int size = lv_obj_get_width(parent);
    const int head = (size * 40) / 100;
    const int head_x = (size - head) / 2;
    const int head_y = (size * 10) / 100;
    const int handle_w = (size * 20) / 100;
    const int handle_h = (size * 35) / 100;
    const int handle_x = (size - handle_w) / 2;
    const int handle_y = head_y + head - (handle_h / 4);
    const int base_w = (size * 30) / 100;
    const int base_h = (size * 8) / 100;
    const int base_x = (size - base_w) / 2;
    const int base_y = handle_y + handle_h - (base_h / 2);

    createIconRect(parent, head_x, head_y, head, head, color, head / 2);
    createIconRect(parent, handle_x, handle_y, handle_w, handle_h, color, handle_w / 2);
    createIconRect(parent, base_x, base_y, base_w, base_h, color, base_h / 2);
}

void drawGenreIcon(GenreIcon icon, lv_color_t color)
{
    if (!g_genre_icon)
    {
        return;
    }

    lv_obj_clean(g_genre_icon);

    switch (icon)
    {
    case kIconMetal:
        drawGuitarIcon(g_genre_icon, color, true);
        break;
    case kIconGuitar:
        drawGuitarIcon(g_genre_icon, color, false);
        break;
    case kIconClassical:
        drawViolinIcon(g_genre_icon, color);
        break;
    case kIconMic:
        drawMicIcon(g_genre_icon, color);
        break;
    case kIconNote:
    default:
        drawNoteIcon(g_genre_icon, color);
        break;
    }
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

    createTrackPanel();
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

void lvglUpdateTrackInfo(const char *track, const char *artist, const char *album)
{
    if (!g_track_panel)
    {
        return;
    }

    setLabelText(g_track_label, track);
    setLabelText(g_artist_label, artist);
    setLabelText(g_album_label, album);
}

void lvglUpdateGenre(const char *genre)
{
    if (!g_genre_box || !g_genre_icon)
    {
        return;
    }

    const GenreStyle *style = matchGenreStyle(genre);
    lv_obj_set_style_bg_color(g_genre_box, lv_color_hex(style->bg), 0);
    lv_obj_set_style_border_color(g_genre_box, lv_color_hex(style->fg), 0);
    drawGenreIcon(matchGenreIcon(genre), lv_color_hex(style->fg));
    lv_obj_invalidate(g_genre_box);
}
