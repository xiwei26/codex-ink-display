#ifndef __GUI_H
#define __GUI_H

#include "Adafruit_GFX.h"

typedef enum {
    MODE_PICTURE = 0,
    MODE_CODEX_DASHBOARD = 3,
} display_mode_t;

/**
 * Codex usage values are sent over BLE in compact units so they fit on the
 * legacy 20-byte ATT payload too. Token values use 0.1M as their unit;
 * rate-limit fields use hundredths of a percent.
 */
typedef struct {
    uint16_t today_tokens_tenth_m;
    uint16_t month_tokens_tenth_m;
    uint16_t current_streak_days;
    uint16_t longest_streak_days;
    uint16_t primary_used_hundredths_percent;
    uint16_t secondary_used_hundredths_percent;
    uint16_t lifetime_tokens_tenth_m;
    uint16_t peak_daily_tokens_tenth_m;
    uint16_t history_tenth_m[7];
} codex_dashboard_t;

typedef struct {
    display_mode_t mode;
    uint16_t color;
    uint16_t width;
    uint16_t height;
    uint32_t timestamp;
    float voltage;
    codex_dashboard_t dashboard;
} gui_data_t;

void DrawGUI(gui_data_t* data, buffer_callback callback, void* callback_data);

#endif
