#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lvgl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr int kHorRes = 320;
constexpr int kVerRes = 240;
constexpr int kBufLines = 40;
constexpr size_t kBufPixels = static_cast<size_t>(kHorRes) * kBufLines;

constexpr int kPanelMargin = 6;
constexpr int kPanelY = 98;
constexpr int kPanelH = 82;
constexpr int kPanelGap = 6;
constexpr int kGenreBoxSize = kPanelH;
constexpr int kGenreIconPad = 10;
constexpr int kGenreIconSize = kGenreBoxSize - (kGenreIconPad * 2);
constexpr int kGenreBoxX = kHorRes - kPanelMargin - kGenreBoxSize;
constexpr int kTrackPanelX = kPanelMargin;
constexpr int kTrackPanelW = kGenreBoxX - kPanelGap - kTrackPanelX;
constexpr int kTrackPanelPad = 8;
constexpr int kTrackLineSpacing = 20;

SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;
SDL_Texture *g_texture = nullptr;

uint32_t *g_clear_buffer = nullptr;
uint32_t *g_convert_buffer = nullptr;
size_t g_convert_capacity = 0;

int g_mouse_x = 0;
int g_mouse_y = 0;
bool g_mouse_down = false;

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

void ensureConvertBuffer(size_t required);

void sdlFlushCb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    SDL_Rect rect = {area->x1, area->y1, w, h};

    size_t count = static_cast<size_t>(w) * h;
    ensureConvertBuffer(count);
    if (!g_convert_buffer)
    {
        lv_display_flush_ready(display);
        return;
    }

    const uint16_t *src = reinterpret_cast<const uint16_t *>(px_map);
    for (size_t i = 0; i < count; ++i)
    {
        uint16_t c = src[i];
        uint8_t r = static_cast<uint8_t>((c >> 11) & 0x1F);
        uint8_t g = static_cast<uint8_t>((c >> 5) & 0x3F);
        uint8_t b = static_cast<uint8_t>(c & 0x1F);
        r = static_cast<uint8_t>((r << 3) | (r >> 2));
        g = static_cast<uint8_t>((g << 2) | (g >> 4));
        b = static_cast<uint8_t>((b << 3) | (b >> 2));
        g_convert_buffer[i] = 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    }

    SDL_UpdateTexture(g_texture, &rect, g_convert_buffer, w * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);

    lv_display_flush_ready(display);
}

void ensureConvertBuffer(size_t required)
{
    if (required <= g_convert_capacity)
    {
        return;
    }

    uint32_t *next = static_cast<uint32_t *>(realloc(g_convert_buffer, required * sizeof(uint32_t)));
    if (!next)
    {
        return;
    }

    g_convert_buffer = next;
    g_convert_capacity = required;
}

void sdlPointerRead(lv_indev_t *indev, lv_indev_data_t *data)
{
    data->point.x = g_mouse_x;
    data->point.y = g_mouse_y;
    data->state = g_mouse_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
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

void createTrackPanel()
{
    lv_obj_t *screen = lv_screen_active();

    g_track_panel = lv_obj_create(screen);
    lv_obj_set_size(g_track_panel, kTrackPanelW, kPanelH);
    lv_obj_set_pos(g_track_panel, kTrackPanelX, kPanelY);
    lv_obj_set_style_radius(g_track_panel, 8, 0);
    lv_obj_set_style_bg_color(g_track_panel, lv_color_hex(0x101010), 0);
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

void updateGenre(const char *genre)
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

bool initSdl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow(
        "ESP32-WebRadio LVGL Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kHorRes * 2,
        kVerRes * 2,
        SDL_WINDOW_SHOWN);
    if (!g_window)
    {
        printf("SDL window failed: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer)
    {
        printf("SDL renderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_RenderSetLogicalSize(g_renderer, kHorRes, kVerRes);

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kHorRes, kVerRes);
    if (!g_texture)
    {
        printf("SDL texture failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    g_clear_buffer = static_cast<uint32_t *>(calloc(static_cast<size_t>(kHorRes) * kVerRes, sizeof(uint32_t)));
    if (g_clear_buffer)
    {
        SDL_UpdateTexture(g_texture, nullptr, g_clear_buffer, kHorRes * static_cast<int>(sizeof(uint32_t)));
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
        SDL_RenderPresent(g_renderer);
    }

    return true;
}

void processEvents(bool &running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_MOUSEMOTION:
            g_mouse_x = event.motion.x;
            g_mouse_y = event.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            g_mouse_down = true;
            g_mouse_x = event.button.x;
            g_mouse_y = event.button.y;
            break;
        case SDL_MOUSEBUTTONUP:
            g_mouse_down = false;
            g_mouse_x = event.button.x;
            g_mouse_y = event.button.y;
            break;
        default:
            break;
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!initSdl())
    {
        return 1;
    }

    lv_init();

    lv_display_t *display = lv_display_create(kHorRes, kVerRes);
    lv_display_set_flush_cb(display, sdlFlushCb);

    lv_color_t *buf1 = static_cast<lv_color_t *>(malloc(kBufPixels * sizeof(lv_color_t)));
    lv_color_t *buf2 = static_cast<lv_color_t *>(malloc(kBufPixels * sizeof(lv_color_t)));
    if (!buf1)
    {
        printf("LVGL buffer allocation failed\n");
        return 1;
    }
    if (!buf2)
    {
        buf2 = nullptr;
    }
    lv_display_set_buffers(display, buf1, buf2, kBufPixels, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_invalidate(screen);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, sdlPointerRead);

    createTrackPanel();
    setLabelText(g_track_label, "Sledgehammer");
    setLabelText(g_artist_label, "Peter Gabriel");
    setLabelText(g_album_label, "So");
    updateGenre("Rock");
    lv_refr_now(display);

    bool running = true;
    uint32_t last_tick = SDL_GetTicks();
    while (running)
    {
        processEvents(running);

        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_tick;
        if (elapsed > 0)
        {
            lv_tick_inc(elapsed);
            last_tick = now;
        }

        lv_timer_handler();
        SDL_Delay(5);
    }

    free(buf1);
    free(buf2);
    free(g_clear_buffer);
    free(g_convert_buffer);
    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
