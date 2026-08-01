#include "GUI.h"

#include <stdio.h>
#include <time.h>

#include "Lunar.h"
#include "fonts.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define GFX_printf_styled(gfx, fg, bg, font, ...) \
    GFX_setTextColor(gfx, fg, bg);                \
    GFX_setFont(gfx, font);                       \
    GFX_printf(gfx, __VA_ARGS__);

typedef struct {
    uint8_t month;
    uint8_t day;
    char name[10];  // 3x3+1
} Festival;

static const Festival festivals[] = {
    {1, 1, "元旦节"},  {2, 14, "情人节"}, {3, 8, "妇女节"},  {3, 12, "植树节"},  {4, 1, "愚人节"},
    {5, 1, "劳动节"},  {5, 4, "青年节"},  {6, 1, "儿童节"},  {7, 1, "建党节"},   {8, 1, "建军节"},
    {9, 10, "教师节"}, {10, 1, "国庆节"}, {11, 1, "万圣节"}, {12, 24, "平安夜"}, {12, 25, "圣诞节"},
};

static const Festival festivals_lunar[] = {
    {1, 1, "春节"},    {1, 15, "元宵节"}, {2, 2, "龙抬头"},  {5, 5, "端午节"},  {7, 7, "七夕节"}, {7, 15, "中元节"},
    {8, 15, "中秋节"}, {9, 9, "重阳节"},  {10, 1, "寒衣节"}, {12, 8, "腊八节"}, {12, 30, "除夕"},
};

// 放假和调休数据，每年更新
#define HOLIDAY_YEAR 2026
static const uint16_t holidays[] = {
    0x0101, 0x0102, 0x0103, 0x1104, 0x120E, 0x020F, 0x0210, 0x0211, 0x0212, 0x0213, 0x0214, 0x0215, 0x0216,
    0x0217, 0x121C, 0x0404, 0x0405, 0x0406, 0x0501, 0x0502, 0x0503, 0x0504, 0x0505, 0x1509, 0x0613, 0x0614,
    0x0615, 0x0919, 0x091A, 0x091B, 0x1914, 0x0A01, 0x0A02, 0x0A03, 0x0A04, 0x0A05, 0x0A06, 0x0A07, 0x1A0A,
};

static bool GetHoliday(uint8_t mon, uint8_t day, bool* work) {
    for (uint8_t i = 0; i < ARRAY_SIZE(holidays); i++) {
        if (((holidays[i] >> 8) & 0xF) == mon && (holidays[i] & 0xFF) == day) {
            *work = ((holidays[i] >> 12) & 0xF) > 0;
            return true;
        }
    }
    return false;
}

static bool GetFestival(uint16_t year, uint8_t mon, uint8_t day, uint8_t week, struct Lunar_Date* Lunar,
                        char* festival) {
    // 农历节日
    for (uint8_t i = 0; i < ARRAY_SIZE(festivals_lunar); i++) {
        if (Lunar->Month == festivals_lunar[i].month && Lunar->Date == festivals_lunar[i].day) {
            strcpy(festival, festivals_lunar[i].name);
            return true;
        }
    }

    // 除夕：春节前一天（12/29 或 12/30），12/30 已在上面判断
    if (Lunar->Month == 12 && Lunar->Date == 29) {
        struct Lunar_Date nextLunar;
        struct devtm tm = {year, mon, day, 0, 0, 0, week};
        transformTime(transformTimeStruct(&tm) + 86400, &tm);
        LUNAR_SolarToLunar(&nextLunar, tm.tm_year + YEAR0, tm.tm_mon + 1, tm.tm_mday);
        if (nextLunar.Month == 1 && nextLunar.Date == 1) {
            strcpy(festival, "除夕");
            return true;
        }
    }
    // 母亲节: 五月第二个星期日
    if (mon == 5 && week == 0 && day >= 8 && day <= 14) {
        strcpy(festival, "母亲节");
        return true;
    }
    // 父亲节: 六月第三个星期日
    if (mon == 6 && week == 0 && day >= 15 && day <= 21) {
        strcpy(festival, "父亲节");
        return true;
    }
    // 感恩节：十一月第四个星期四
    if (mon == 11 && week == 4 && day >= 22 && day <= 28) {
        strcpy(festival, "感恩节");
        return true;
    }

    // 公历节日
    for (uint8_t i = 0; i < ARRAY_SIZE(festivals); i++) {
        if (mon == festivals[i].month && day == festivals[i].day) {
            strcpy(festival, festivals[i].name);
            return true;
        }
    }

    // 二十四节气
    uint8_t JQdate;
    if (GetJieQi(year, mon, day, &JQdate) && JQdate == day) {
        uint8_t JQ = (mon - 1) * 2;
        if (day >= 15) JQ++;
        strcpy(festival, JieQiStr[JQ]);
        if (JQ == 6)  // 清明
            strcat(festival, "节");

        return true;
    }

    return false;
}

static void DrawTimeSyncTip(Adafruit_GFX* gfx, gui_data_t* data) {
    const char* title = "SYNC TIME!";
    const char* url = "https://ycd12.github.io/EPD-nRF5_DYC/";

    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);

    int16_t fh = GFX_getFontHeight(gfx);
    int16_t box_w = GFX_getUTF8Width(gfx, url) + 20;
    int16_t box_h = fh * 2 + 20;
    int16_t box_x = (data->width - box_w) / 2;
    int16_t box_y = data->height / 2 - box_h / 2;

    GFX_fillRect(gfx, box_x, box_y, box_w, box_h, GFX_WHITE);
    GFX_drawRoundRect(gfx, box_x, box_y, box_w, box_h, 5, GFX_BLACK);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_setCursor(gfx, box_x + (box_w - GFX_getUTF8Width(gfx, title)) / 2, box_y + 5 + fh);
    GFX_printf(gfx, title);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setCursor(gfx, box_x + 10, box_y + box_h - GFX_getFontAscent(gfx));
    GFX_printf(gfx, url);
}

static void DrawBattery(Adafruit_GFX* gfx, int16_t x, int16_t y, uint8_t iw, float voltage) {
    x -= iw;
    uint8_t level = (uint8_t)(voltage * 100 / 3.6f);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, x - GFX_getUTF8Width(gfx, "3.2V") - 2, y + 9);
    GFX_printf(gfx, "%.1fV", voltage);
    GFX_fillRect(gfx, x, y, iw, 10, GFX_WHITE);
    GFX_drawRect(gfx, x, y, iw, 10, GFX_BLACK);
    GFX_fillRect(gfx, x + iw, y + 4, 2, 2, GFX_BLACK);
    GFX_fillRect(gfx, x + 2, y + 2, 16 * level / 100, 6, GFX_BLACK);
}

static uint8_t GetWeekOfYear(uint8_t year, uint8_t mon, uint8_t mday, uint8_t wday) {
    struct tm tm = {0};
    tm.tm_year = year;
    tm.tm_mon = mon;
    tm.tm_mday = mday;
    tm.tm_wday = wday;
    tm.tm_isdst = -1;
    mktime(&tm);
    char buffer[3] = {0};
    strftime(buffer, 3, "%V", &tm);
    return atoi(buffer);
}

static void DrawDateHeader(Adafruit_GFX* gfx, int16_t x, int16_t y, tm_t* tm, struct Lunar_Date* Lunar,
                           gui_data_t* data) {
    GFX_setCursor(gfx, x, y - 2);
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%d", tm->tm_year + YEAR0);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "年");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%d", tm->tm_mon + 1);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "月");

    int16_t tx = gfx->tx;
    int16_t ty = y;

    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, tx, ty);
    if (Lunar->IsLeap) GFX_printf(gfx, " ");
    GFX_printf(gfx, "%s%s%s", Lunar_MonthLeapString[Lunar->IsLeap], Lunar_MonthString[Lunar->Month],
               Lunar_DateString[Lunar->Date]);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_printf(gfx, " [%d周]", GetWeekOfYear(tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_wday));

    GFX_setCursor(gfx, tx, ty - 14);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_printf(gfx, " %s%s年", Lunar_StemStrig[LUNAR_GetStem(Lunar)], Lunar_BranchStrig[LUNAR_GetBranch(Lunar)]);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_printf(gfx, " [%s]", Lunar_ZodiacString[LUNAR_GetZodiac(Lunar)]);

    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    DrawBattery(gfx, data->width - 10 - 2, data->height > 300 ? 16 : 6, 20, data->voltage);
    GFX_setCursor(gfx, data->width - GFX_getUTF8Width(gfx, data->ssid) - 10, y);
    GFX_printf(gfx, "%s", data->ssid);
}

static void DrawWeekHeader(Adafruit_GFX* gfx, int16_t x, int16_t y, gui_data_t* data) {
    GFX_setFont(gfx, data->height > 300 ? u8g2_font_wqy12_t_lunar : u8g2_font_wqy9_t_lunar);
    uint8_t w = (data->width - 2 * x) / 7;
    uint8_t h = data->height > 300 ? 32 : 24;
    uint8_t r = (data->width - 2 * x) % 7;
    uint8_t fh = (h - GFX_getFontHeight(gfx)) / 2 + GFX_getFontAscent(gfx) + 1;
    int16_t cw = GFX_getUTF8Width(gfx, Lunar_DayString[0]);
    for (int i = 0; i < 7; i++) {
        uint8_t day = (data->week_start + i) % 7;
        uint16_t bg = (day == 0 || day == 6) ? GFX_RED : GFX_BLACK;
        GFX_fillRect(gfx, x + i * w, y, i == 6 ? (w + r) : w, h, bg);
        GFX_setTextColor(gfx, GFX_WHITE, bg);
        GFX_setCursor(gfx, x + (w - cw) / 2 + i * w, y + fh);
        GFX_printf(gfx, "%s", Lunar_DayString[day]);
    }
}

static void DrawMonthDays(Adafruit_GFX* gfx, int16_t x, int16_t y, tm_t* tm, struct Lunar_Date* Lunar,
                          gui_data_t* data) {
    uint8_t firstDayWeek = get_first_day_week(tm->tm_year + YEAR0, tm->tm_mon + 1);
    int8_t adjustedFirstDay = (firstDayWeek - data->week_start + 7) % 7;
    uint8_t monthMaxDays = thisMonthMaxDays(tm->tm_year + YEAR0, tm->tm_mon + 1);
    uint8_t monthDayRows = 1 + (monthMaxDays - (7 - adjustedFirstDay) + 6) / 7;

    int16_t bw = (data->width - x - 10) / 7;
    int16_t bh = (data->height - y - 10) / monthDayRows;
    bool large = data->height > 300;

    if (large) {
        for (uint8_t i = 1; i < monthDayRows; i++)
            GFX_drawDottedLine(gfx, x, y + i * bh, x + 7 * bw - 1, y + i * bh, GFX_BLACK, 1, 5);
        for (uint8_t i = 1; i < 7; i++)
            GFX_drawDottedLine(gfx, x + i * bw, y, x + i * bw, y + monthDayRows * bh - 1, GFX_BLACK, 1, 5);
    }

    for (uint8_t i = 0; i < monthMaxDays; i++) {
        uint16_t year = tm->tm_year + YEAR0;
        uint8_t month = tm->tm_mon + 1;
        uint8_t day = i + 1;

        int16_t actualWeek = (firstDayWeek + i) % 7;
        int16_t displayWeek = (adjustedFirstDay + i) % 7;
        bool weekend = (actualWeek == 0) || (actualWeek == 6);

        LUNAR_SolarToLunar(Lunar, year, month, day);

        int16_t cr = large ? 13 : 10;
        int16_t bx = x + (bw - 2 * cr) / 2 + displayWeek * bw;
        int16_t by = y + (bh - 2 * cr) / 2 + (i + adjustedFirstDay) / 7 * bh + 3;

        if (day == tm->tm_mday) {
            GFX_fillCircle(gfx, bx + cr, by + cr - 3, 2 * cr, GFX_RED);
            GFX_setTextColor(gfx, GFX_WHITE, GFX_RED);
        } else {
            GFX_setTextColor(gfx, weekend ? GFX_RED : GFX_BLACK, GFX_WHITE);
        }

        GFX_setFont(gfx, large ? u8g2_font_helvB18_tn : u8g2_font_helvB14_tn);
        GFX_setCursor(gfx, bx + (2 * cr - GFX_getUTF8Widthf(gfx, "%d", day)) / 2, by - (cr - GFX_getFontHeight(gfx)));
        GFX_printf(gfx, "%d", day);

        char festival[10] = {0};
        GFX_setFont(gfx, large ? u8g2_font_wqy12_t_lunar : u8g2_font_wqy9_t_lunar);
        GFX_setFontMode(gfx, 1);  // transparent
        if (GetFestival(year, month, day, actualWeek, Lunar, festival)) {
            if (day != tm->tm_mday) GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
        } else {
            if (Lunar->Date == 1)
                snprintf(festival, sizeof(festival), "%s%s", Lunar_MonthLeapString[Lunar->IsLeap],
                         Lunar_MonthString[Lunar->Month]);
            else
                snprintf(festival, sizeof(festival), "%s", Lunar_DateString[Lunar->Date]);
        }
        GFX_setCursor(gfx, bx + (2 * cr - GFX_getUTF8Width(gfx, festival)) / 2, gfx->ty + GFX_getFontHeight(gfx) + 3);
        GFX_printf(gfx, "%s", festival);

        bool work = false;
        if (year == HOLIDAY_YEAR && GetHoliday(month, day, &work)) {
            if (day == tm->tm_mday) {
                uint16_t rx = bx + (large ? 36 : 27);
                uint16_t ry = by - 2;
                uint8_t cr = large ? 10 : 8;
                GFX_fillCircle(gfx, rx, ry, cr, GFX_WHITE);
                GFX_drawCircle(gfx, rx, ry, cr, GFX_RED);
            }
            GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
            GFX_setTextColor(gfx, work ? GFX_BLACK : GFX_RED, GFX_WHITE);
            GFX_setCursor(gfx, bx + (large ? 31 : 22), by + 3);
            GFX_printf(gfx, "%s", work ? "班" : "休");
        }
    }
}

static void DrawCalendar(Adafruit_GFX* gfx, tm_t* tm, struct Lunar_Date* Lunar, gui_data_t* data) {
    bool large = data->height > 300;
    DrawDateHeader(gfx, 10, large ? 38 : 28, tm, Lunar, data);
    DrawWeekHeader(gfx, 10, large ? 44 : 32, data);
    DrawMonthDays(gfx, 10, large ? 84 : 64, tm, Lunar, data);
}

// clang-format off
/* Routine to Draw Large 7-Segment formated number
   Contributed by William Zaggle.

   int n - The number to be displayed
   int xLoc = The x location of the upper left corner of the number
   int yLoc = The y location of the upper left corner of the number
   int cS = The size of the number. 
   fC is the foreground color of the number
   bC is the background color of the number (prevents having to clear previous space)
   nD is the number of digit spaces to occupy (must include space for minus sign for numbers < 0).

   width: nD*(11*cS+2)-2*cS
   height: 20*cS+4

   https://forum.arduino.cc/t/fast-7-segment-number-display-for-tft/296619/4
*/
static void Draw7Number(Adafruit_GFX *gfx, int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC, int nD) {
    unsigned int num=abs(n),i,t,w,col,h,a,b,j=1,d=0,S2=5*cS,S3=2*cS,S4=7*cS,x1=cS+1,x2=S3+S2+1,y1=yLoc+x1,y3=yLoc+S3+S4+1;
    unsigned int seg[7][3]={{x1,yLoc,1},{x2,y1,0},{x2,y3+x1,0},{x1,(2*y3)-yLoc,1},{0,y3+x1,0},{0,y1,0},{x1,y3,1}};
    unsigned char nums[12]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x00,0x40},c=(c=abs(cS))>10?10:(c<1)?1:c,cnt=(cnt=abs(nD))>10?10:(cnt<1)?1:cnt;
    for (xLoc+=cnt*(d=S2+(3*S3)+2);cnt>0;cnt--){
      for (i=(num>9)?num%10:((!cnt)&&(n<0))?11:((nD<0)&&(!num))?10:num,xLoc-=d,num/=10,j=0;j<7;++j){
        col=(nums[i]&(1<<j))?fC:bC;
        if (seg[j][2])for(w=S2,t=seg[j][1]+S3,h=seg[j][1]+cS,a=xLoc+seg[j][0]+cS,b=seg[j][1];b<h;b++,a--,w+=2)GFX_drawFastHLine(gfx,a,b,w,col);
        else for(w=S4,t=xLoc+seg[j][0]+S3,h=xLoc+seg[j][0]+cS,b=xLoc+seg[j][0],a=seg[j][1]+cS;b<h;b++,a--,w+=2)GFX_drawFastVLine(gfx,b,a,w,col);
        for (;b<t;b++,a++,w-=2)seg[j][2]?GFX_drawFastHLine(gfx,a,b,w,col):GFX_drawFastVLine(gfx,b,a,w,col);
        }
    }
}
// clang-format on

static void DrawTime(Adafruit_GFX* gfx, tm_t* tm, int16_t x, int16_t y, uint16_t cS, uint16_t nD) {
    Draw7Number(gfx, tm->tm_hour, x, y, cS, GFX_BLACK, GFX_WHITE, nD);
    x += (nD * (11 * cS + 2) - 2 * cS) + 2 * cS;
    GFX_fillRect(gfx, x, y + 4.5 * cS + 1, 2 * cS, 2 * cS, GFX_BLACK);
    GFX_fillRect(gfx, x, y + 13.5 * cS + 3, 2 * cS, 2 * cS, GFX_BLACK);
    x += 4 * cS;
    Draw7Number(gfx, tm->tm_min, x, y, cS, GFX_BLACK, GFX_WHITE, nD);
}

static void DrawClock(Adafruit_GFX* gfx, tm_t* tm, struct Lunar_Date* Lunar, gui_data_t* data) {
    uint8_t padding = data->height > 300 ? 100 : 40;
    GFX_setCursor(gfx, padding, 36);
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%d", tm->tm_year + YEAR0);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "年");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%02d", tm->tm_mon + 1);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "月");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%02d", tm->tm_mday);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "日 ");

    GFX_setCursor(gfx, padding, 58);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "星期%s", Lunar_DayString[tm->tm_wday]);
    GFX_setCursor(gfx, 138, 58);
    GFX_printf(gfx, "%s%s%s", Lunar_MonthLeapString[Lunar->IsLeap], Lunar_MonthString[Lunar->Month],
               Lunar_DateString[Lunar->Date]);

    DrawBattery(gfx, data->width - padding, 25, 20, data->voltage);

    char ssid[5] = {0};
    int16_t ssid_len = strlen(data->ssid);
    int16_t sw = GFX_getUTF8Width(gfx, "25℃[1234]");
    memcpy(ssid, &data->ssid[ssid_len - 4], 4);
    GFX_setCursor(gfx, data->width - padding - sw - 2, 58);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "%d℃[%s]", data->temperature, ssid);

    GFX_drawFastHLine(gfx, padding - 10, 68, data->width - 2 * (padding - 10), GFX_BLACK);

    uint16_t cS = data->height / 45;
    uint16_t nD = 2;
    uint16_t time_width = 2 * (nD * (11 * cS + 2) - 2 * cS) + 4 * cS;
    uint16_t time_height = 20 * cS + 4;
    int16_t time_x = (data->width - time_width) / 2;
    int16_t time_y = (68 + (data->height - 68)) / 2 - time_height / 2;
    DrawTime(gfx, tm, time_x, time_y, cS, nD);

    GFX_drawFastHLine(gfx, padding - 10, data->height - 68, data->width - 2 * (padding - 10), GFX_BLACK);

    GFX_setCursor(gfx, padding, data->height - 68 + 30);
    GFX_setFont(gfx, u8g2_font_wqy12_t_lunar);
    GFX_printf(gfx, "%s%s", Lunar_StemStrig[LUNAR_GetStem(Lunar)], Lunar_BranchStrig[LUNAR_GetBranch(Lunar)]);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_printf(gfx, "%s", Lunar_ZodiacString[LUNAR_GetZodiac(Lunar)]);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_printf(gfx, "年");

    GFX_setCursor(gfx, padding, data->height - 68 + 30 + 20);
    GFX_printf(gfx, " %d周", GetWeekOfYear(tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_wday));

    uint8_t day = 0;
    uint8_t JQday = GetJieQiStr(tm->tm_year + YEAR0, tm->tm_mon + 1, tm->tm_mday, &day);
    if (day == 0) {
        GFX_setCursor(gfx, data->width - GFX_getUTF8Width(gfx, "小暑") - padding, data->height - 68 + 30);
        GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
        GFX_printf(gfx, "%s", JieQiStr[JQday % 24]);
    } else {
        GFX_setCursor(gfx, data->width - GFX_getUTF8Width(gfx, "离小暑") - padding, data->height - 68 + 30);
        GFX_printf(gfx, "离%");
        GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
        GFX_printf(gfx, "%s", JieQiStr[JQday % 24]);
        GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
        char buf[15] = {0};
        snprintf(buf, sizeof(buf), "还有%d天", day);
        GFX_setCursor(gfx, data->width - GFX_getUTF8Width(gfx, buf) - padding, data->height - 68 + 30 + 20);
        GFX_printf(gfx, buf);
    }
}

static void DrawDashboardTokens(Adafruit_GFX* gfx, int16_t x, int16_t y, uint16_t value, bool large) {
    uint16_t whole = value / 10;
    uint8_t fraction = value % 10;

    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setFont(gfx, large ? u8g2_font_helvB18_tn : u8g2_font_helvB14_tn);
    GFX_setCursor(gfx, x, y);
    GFX_printf(gfx, "%u.%u", whole, fraction);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "M");
}

static void DrawDashboardCost(Adafruit_GFX* gfx, int16_t x, int16_t y, uint16_t cents, uint16_t color) {
    GFX_setTextColor(gfx, color, GFX_WHITE);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, x, y);
    GFX_printf(gfx, "$%u.%02u", cents / 100, cents % 100);
}

static void DrawDashboardCompact(Adafruit_GFX* gfx, tm_t* tm, gui_data_t* data) {
    int16_t padding = 8;
    int16_t divider = data->width / 2;

    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_setCursor(gfx, padding, 14);
    GFX_printf(gfx, "CODEX QUOTA");
    GFX_setCursor(gfx, data->width - 48, 14);
    GFX_printf(gfx, "%02d.%02d", tm->tm_mon + 1, tm->tm_mday);
    GFX_drawFastHLine(gfx, padding, 20, data->width - 2 * padding, GFX_BLACK);
    GFX_drawFastVLine(gfx, divider, 28, data->height - 48, GFX_BLACK);

    GFX_setCursor(gfx, padding, 39);
    GFX_printf(gfx, "TODAY");
    DrawDashboardTokens(gfx, padding, 62, data->dashboard.today_tokens_tenth_m, false);
    GFX_setCursor(gfx, padding, 79);
    GFX_printf(gfx, "%u REQS", data->dashboard.today_requests);
    DrawDashboardCost(gfx, padding, 94, data->dashboard.today_cost_cents, GFX_RED);

    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_setCursor(gfx, divider + padding, 39);
    GFX_printf(gfx, "MONTH");
    DrawDashboardTokens(gfx, divider + padding, 62, data->dashboard.month_tokens_tenth_m, false);
    GFX_setCursor(gfx, divider + padding, 79);
    GFX_printf(gfx, "%u REQS", data->dashboard.month_requests);
    DrawDashboardCost(gfx, divider + padding, 94, data->dashboard.month_cost_cents, GFX_BLACK);

    GFX_drawFastHLine(gfx, padding, data->height - 31, data->width - 2 * padding, GFX_BLACK);
    GFX_setCursor(gfx, padding, data->height - 13);
    GFX_printf(gfx, "C:%u.%uM  CDX:%u.%uM", data->dashboard.claude_tokens_tenth_m / 10,
               data->dashboard.claude_tokens_tenth_m % 10, data->dashboard.codex_tokens_tenth_m / 10,
               data->dashboard.codex_tokens_tenth_m % 10);
}

static void DrawCodexDashboard(Adafruit_GFX* gfx, tm_t* tm, gui_data_t* data) {
    const int16_t padding = 12;

    if (data->width < 360 || data->height < 250) {
        DrawDashboardCompact(gfx, tm, data);
        return;
    }

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
    GFX_printf(gfx, "%02d.%02d", tm->tm_mon + 1, tm->tm_mday);
    GFX_drawFastHLine(gfx, padding, 22, data->width - 2 * padding, GFX_BLACK);
    GFX_drawFastVLine(gfx, divider, 30, 127, GFX_BLACK);

    GFX_setCursor(gfx, padding, 43);
    GFX_printf(gfx, "TODAY TOKENS");
    DrawDashboardTokens(gfx, padding, 70, data->dashboard.today_tokens_tenth_m, true);
    GFX_setCursor(gfx, padding, 91);
    GFX_printf(gfx, "%u REQUESTS", data->dashboard.today_requests);
    DrawDashboardCost(gfx, divider - 66, 91, data->dashboard.today_cost_cents, GFX_RED);
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
    DrawDashboardTokens(gfx, right_x, 69, data->dashboard.month_tokens_tenth_m, false);
    GFX_setCursor(gfx, right_x, 89);
    GFX_printf(gfx, "COST");
    DrawDashboardCost(gfx, data->width - 61, 89, data->dashboard.month_cost_cents, GFX_RED);
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

        tm_t chart_tm = {0};
        transformTime(data->timestamp - (6 - i) * 86400, &chart_tm);
        GFX_setCursor(gfx, x - 6, chart_bottom + 14);
        GFX_printf(gfx, "%02d", chart_tm.tm_mday);
    }

    GFX_drawFastHLine(gfx, padding, data->height - 27, data->width - 2 * padding, GFX_BLACK);
    GFX_setCursor(gfx, padding, data->height - 10);
    GFX_printf(gfx, "EPD - %.1fV", data->voltage);
    GFX_setCursor(gfx, data->width - 82, data->height - 10);
    GFX_printf(gfx, "%02d:%02d UPDATED", tm->tm_hour, tm->tm_min);
}

void DrawGUI(gui_data_t* data, buffer_callback callback, void* callback_data) {
    if (data->week_start > 6) data->week_start = 0;

    tm_t tm = {0};
    struct Lunar_Date Lunar;

    transformTime(data->timestamp, &tm);

    Adafruit_GFX gfx;
    int16_t ph = (__HEAP_SIZE - 512) / (data->width / 8);

    if (data->color == 2)
        GFX_begin_3c(&gfx, data->width, data->height, ph);
    else if (data->color == 3)
        GFX_begin_4c(&gfx, data->width, data->height, ph);
    else
        GFX_begin(&gfx, data->width, data->height, ph);

    GFX_firstPage(&gfx);
    do {
        GFX_fillScreen(&gfx, GFX_WHITE);

        LUNAR_SolarToLunar(&Lunar, tm.tm_year + YEAR0, tm.tm_mon + 1, tm.tm_mday);

        switch (data->mode) {
            case MODE_CALENDAR:
                DrawCalendar(&gfx, &tm, &Lunar, data);
                break;
            case MODE_CLOCK:
                DrawClock(&gfx, &tm, &Lunar, data);
                break;
            case MODE_CODEX_DASHBOARD:
                DrawCodexDashboard(&gfx, &tm, data);
                break;
            default:
                break;
        }
        if ((data->mode == MODE_CALENDAR || data->mode == MODE_CLOCK) &&
            (tm.tm_year + YEAR0 == 2025 && tm.tm_mon + 1 == 1)) {
            DrawTimeSyncTip(&gfx, data);
        }
    } while (GFX_nextPage(&gfx, callback, callback_data));

    GFX_end(&gfx);
}
