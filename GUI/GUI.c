#include "GUI.h"

#include <stdio.h>

#include "fonts.h"

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
} dashboard_time_t;

static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 ? days[1] + is_leap_year(year) : days[month - 1];
}

static void dashboard_time_from_unix(uint32_t timestamp, dashboard_time_t* time) {
    uint32_t days = timestamp / 86400;
    uint32_t seconds = timestamp % 86400;

    time->year = 1970;
    while (days >= (is_leap_year(time->year) ? 366 : 365)) {
        days -= is_leap_year(time->year) ? 366 : 365;
        time->year++;
    }
    time->month = 1;
    while (days >= days_in_month(time->year, time->month)) {
        days -= days_in_month(time->year, time->month);
        time->month++;
    }
    time->day = days + 1;
    time->hour = seconds / 3600;
    time->minute = (seconds % 3600) / 60;
}

static void draw_tokens(Adafruit_GFX* gfx, int16_t x, int16_t y, uint16_t value, bool large) {
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setFont(gfx, large ? u8g2_font_helvB18_tn : u8g2_font_helvB14_tn);
    GFX_setCursor(gfx, x, y);
    GFX_printf(gfx, "%u.%u", value / 10, value % 10);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "M");
}

static void draw_cost(Adafruit_GFX* gfx, int16_t x, int16_t y, uint16_t cents, uint16_t color) {
    GFX_setTextColor(gfx, color, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, x, y);
    GFX_printf(gfx, "$%u.%02u", cents / 100, cents % 100);
}

static void draw_dashboard(Adafruit_GFX* gfx, gui_data_t* data) {
    const int16_t padding = 12;
    dashboard_time_t now;
    dashboard_time_from_unix(data->timestamp, &now);

    int16_t divider = data->width * 53 / 100;
    int16_t right_x = divider + 12;
    int16_t chart_left = padding + 3;
    int16_t chart_right = data->width - padding - 3;
    int16_t chart_top = data->height - 99;
    int16_t chart_bottom = data->height - 45;
    uint16_t highest = 1;

    for (uint8_t i = 0; i < 7; i++) {
        if (data->dashboard.history_tenth_m[i] > highest) highest = data->dashboard.history_tenth_m[i];
    }

    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setCursor(gfx, padding, 15);
    GFX_printf(gfx, "CODEX QUOTA");
    GFX_setCursor(gfx, data->width - 54, 15);
    GFX_printf(gfx, "%02u.%02u", now.month, now.day);
    GFX_drawFastHLine(gfx, padding, 22, data->width - 2 * padding, GFX_BLACK);
    GFX_drawFastVLine(gfx, divider, 30, 127, GFX_BLACK);

    GFX_setCursor(gfx, padding, 43);
    GFX_printf(gfx, "TODAY TOKENS");
    draw_tokens(gfx, padding, 70, data->dashboard.today_tokens_tenth_m, true);
    GFX_setCursor(gfx, padding, 91);
    GFX_printf(gfx, "%u REQUESTS", data->dashboard.today_requests);
    draw_cost(gfx, divider - 66, 91, data->dashboard.today_cost_cents, GFX_RED);
    GFX_drawFastHLine(gfx, padding, 99, divider - padding - 5, GFX_BLACK);
    GFX_setCursor(gfx, padding, 114);
    GFX_printf(gfx, "CLAUDE");
    GFX_setCursor(gfx, divider - 48, 114);
    GFX_printf(gfx, "%u.%uM", data->dashboard.claude_tokens_tenth_m / 10,
               data->dashboard.claude_tokens_tenth_m % 10);
    GFX_setCursor(gfx, padding, 128);
    GFX_printf(gfx, "CODEX");
    GFX_setCursor(gfx, divider - 48, 128);
    GFX_printf(gfx, "%u.%uM", data->dashboard.codex_tokens_tenth_m / 10,
               data->dashboard.codex_tokens_tenth_m % 10);

    GFX_setCursor(gfx, right_x, 43);
    GFX_printf(gfx, "MONTH TOKENS");
    draw_tokens(gfx, right_x, 69, data->dashboard.month_tokens_tenth_m, false);
    GFX_setCursor(gfx, right_x, 89);
    GFX_printf(gfx, "COST");
    draw_cost(gfx, data->width - 61, 89, data->dashboard.month_cost_cents, GFX_RED);
    GFX_setCursor(gfx, right_x, 106);
    GFX_printf(gfx, "REQUESTS");
    GFX_setCursor(gfx, data->width - 38, 106);
    GFX_printf(gfx, "%u", data->dashboard.month_requests);
    GFX_setCursor(gfx, right_x, 123);
    GFX_printf(gfx, "AVG / DAY");
    GFX_setCursor(gfx, data->width - 39, 123);
    GFX_printf(gfx, "%u.%uM", data->dashboard.month_tokens_tenth_m / 300,
               (data->dashboard.month_tokens_tenth_m / 30) % 10);

    GFX_drawFastHLine(gfx, padding, data->height - 116, data->width - 2 * padding, GFX_BLACK);
    GFX_setCursor(gfx, padding, data->height - 102);
    GFX_printf(gfx, "LAST 7 DAYS");
    GFX_setCursor(gfx, data->width - 91, data->height - 102);
    GFX_printf(gfx, "TOTAL %u.%uM", data->dashboard.month_tokens_tenth_m / 10,
               data->dashboard.month_tokens_tenth_m % 10);

    GFX_drawFastHLine(gfx, chart_left, chart_bottom, chart_right - chart_left, GFX_BLACK);
    for (uint8_t i = 0; i < 7; i++) {
        int16_t x = chart_left + (chart_right - chart_left) * i / 6;
        int16_t y = chart_bottom - (chart_bottom - chart_top) * data->dashboard.history_tenth_m[i] / highest;
        if (i > 0) {
            int16_t previous_x = chart_left + (chart_right - chart_left) * (i - 1) / 6;
            int16_t previous_y = chart_bottom - (chart_bottom - chart_top) * data->dashboard.history_tenth_m[i - 1] / highest;
            GFX_drawLine(gfx, previous_x, previous_y, x, y, GFX_BLACK);
        }
        GFX_fillCircle(gfx, x, y, 2, i == 6 ? GFX_RED : GFX_WHITE);
        GFX_drawCircle(gfx, x, y, 2, GFX_BLACK);

        dashboard_time_t day;
        dashboard_time_from_unix(data->timestamp - (6 - i) * 86400, &day);
        GFX_setCursor(gfx, x - 6, chart_bottom + 14);
        GFX_printf(gfx, "%02u", day.day);
    }

    GFX_drawFastHLine(gfx, padding, data->height - 27, data->width - 2 * padding, GFX_BLACK);
    GFX_setCursor(gfx, padding, data->height - 10);
    GFX_printf(gfx, "EPD - %.1fV", data->voltage);
    GFX_setCursor(gfx, data->width - 82, data->height - 10);
    GFX_printf(gfx, "%02u:%02u UPDATED", now.hour, now.minute);
}

void DrawGUI(gui_data_t* data, buffer_callback callback, void* callback_data) {
    Adafruit_GFX gfx;
    int16_t page_height = (__HEAP_SIZE - 512) / (data->width / 8);

    if (data->color == 2)
        GFX_begin_3c(&gfx, data->width, data->height, page_height);
    else if (data->color == 3)
        GFX_begin_4c(&gfx, data->width, data->height, page_height);
    else
        GFX_begin(&gfx, data->width, data->height, page_height);

    GFX_firstPage(&gfx);
    do {
        GFX_fillScreen(&gfx, GFX_WHITE);
        draw_dashboard(&gfx, data);
    } while (GFX_nextPage(&gfx, callback, callback_data));
    GFX_end(&gfx);
}
