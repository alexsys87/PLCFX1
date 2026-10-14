/**
 * @file plc_rtc.h
 * @brief Battery-backed real-time clock of the PLC (STM32F103 RTC with a 32.768 kHz LSE).
 *
 * The clock is mirrored into the FX special registers D8013..D8019 (seconds, minutes, hours,
 * day, month, year, weekday) so that ladder programs and the programming tool can read it.
 * Writing the clock from a program uses the TWR instruction, which calls RTC_Set().
 */
#ifndef PLC_RTC_H
#define PLC_RTC_H

#include "platform.h"

/** Start the RTC. Without a working LSE the clock is reported unavailable (no LSI fallback). */
void RTC_Init(void);

/** Copy the current time into D8013..D8019. Does nothing when the RTC is unavailable. */
void RTC_Get(void);

/**
 * Set the clock.
 * @return 0 on success, 1 for an invalid date/time, 2 when the RTC is unavailable or a write timed out.
 */
u8 RTC_Set(u16 year, u8 month, u8 day, u8 hour, u8 minute, u8 second);

/** Day of the week (0 = Sunday) of a Gregorian date. */
u8 RTC_Get_Week(u16 year, u8 month, u8 date);

/** Gregorian leap-year test. */
u8 Is_Leap_Year(u16 year);

#endif /* PLC_RTC_H */
