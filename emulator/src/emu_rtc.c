/**
 * @file    emu_rtc.c
 * @brief   Host version of src/plc_rtc.c: the PLC clock follows the PC clock.
 *
 * The emulated RTC is the local time of the PC plus an offset. RTC_Set() (TWR instruction,
 * or M8015 with D8013..D8018) changes only the offset, never the PC clock. The validity
 * rules and return codes are those of the firmware driver: dates 2016-01-01 .. 2136-12-31,
 * 0 = written, 1 = invalid date/time, 2 = RTC unavailable.
 */
#include <time.h>

#include "emu_log.h"
#include "plc_memory_map.h"
#include "plc_rtc.h"
#include "plc_runtime.h"
#include "plc_special_devices.h"

static const u8 month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const u8 week_tab[] = {0, 1, 4, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6};

static bool rtc_available;
static long long offset_seconds; /* PLC time minus PC local time */

u8 Is_Leap_Year(u16 year) {
  return (u8)((year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U);
}

u8 RTC_Get_Week(u16 year, u8 month, u8 day) {
  if (month < 1 || month > 12 || day < 1 || day > 31) return 0;
  if (month < 3 && Is_Leap_Year(year)) day--;
  return (u8)((day + year + year / 4U + year / 400U - year / 100U + week_tab[month] - 2U) % 7U);
}

/** Seconds since 1970-01-01 of a calendar date, treating it as UTC (only differences matter). */
static long long calendar_seconds(unsigned year, unsigned month, unsigned day, unsigned hour,
                                  unsigned minute, unsigned second) {
  long long days = 0;
  unsigned y, m;
  for (y = 1970; y < year; y++) days += Is_Leap_Year((u16)y) ? 366 : 365;
  for (m = 1; m < month; m++) days += month_days[m - 1] + (m == 2 && Is_Leap_Year((u16)year));
  days += day - 1;
  return ((days * 24 + hour) * 60 + minute) * 60 + second;
}

/** Current PC local time as calendar seconds. */
static long long host_seconds(void) {
  time_t now = time(NULL);
  struct tm local;
  localtime_s(&local, &now);
  return calendar_seconds((unsigned)local.tm_year + 1900U, (unsigned)local.tm_mon + 1U,
                          (unsigned)local.tm_mday, (unsigned)local.tm_hour,
                          (unsigned)local.tm_min, (unsigned)local.tm_sec);
}

void RTC_Init(void) {
  rtc_available = true;
  offset_seconds = 0;
  PLC_BIT_OFF(M8015);
  PLC_BIT_ON(M8018);
}

u8 RTC_Set(u16 year, u8 month, u8 day, u8 hour, u8 minute, u8 second) {
  u8 days;
  if (year < 2016 || year > 2136 || month < 1 || month > 12 || hour > 23 || minute > 59 ||
      second > 59) {
    LOG_WARN(EMU_CAT_RUNTIME, "RTC_Set rejected an invalid date/time %u-%02u-%02u %02u:%02u:%02u",
             year, month, day, hour, minute, second);
    return 1;
  }
  days = month_days[month - 1];
  if (month == 2 && Is_Leap_Year(year)) days++;
  if (day < 1 || day > days) {
    LOG_WARN(EMU_CAT_RUNTIME, "RTC_Set rejected an invalid day %u-%02u-%02u", year, month, day);
    return 1;
  }
  if (!rtc_available) return 2;
  offset_seconds = calendar_seconds(year, month, day, hour, minute, second) - host_seconds();
  LOG_INFO(EMU_CAT_RUNTIME, "PLC clock set to %u-%02u-%02u %02u:%02u:%02u", year, month, day, hour,
           minute, second);
  return 0;
}

void RTC_Get(void) {
  long long now, days;
  unsigned year = 1970, month = 1, day_seconds;

  if (PLC_BIT_TEST(M8015)) {
    u16 y = D8018, mo = D8017, d = D8016, h = D8015, mi = D8014, s = D8013;
    if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31 && h <= 23 && mi <= 59 && s <= 59)
      RTC_Set(y, (u8)mo, (u8)d, (u8)h, (u8)mi, (u8)s);
    return;
  }
  if (!rtc_available || !PLC_BIT_TEST(M8018)) return;
  now = host_seconds() + offset_seconds;
  if (now < 0) now = 0;
  days = now / 86400;
  day_seconds = (unsigned)(now % 86400);
  while (days >= (Is_Leap_Year((u16)year) ? 366 : 365)) {
    days -= Is_Leap_Year((u16)year) ? 366 : 365;
    year++;
  }
  for (;;) {
    unsigned length = month_days[month - 1] + (month == 2 && Is_Leap_Year((u16)year));
    if (days < (long long)length) break;
    days -= length;
    month++;
  }
  D8018 = (u16)year;
  D8017 = (u16)month;
  D8016 = (u16)(days + 1);
  D8015 = (u16)(day_seconds / 3600U);
  D8014 = (u16)(day_seconds % 3600U / 60U);
  D8013 = (u16)(day_seconds % 60U);
  D8019 = RTC_Get_Week((u16)year, (u8)month, (u8)(days + 1));
}
