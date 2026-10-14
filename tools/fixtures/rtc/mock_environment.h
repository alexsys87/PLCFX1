/**
 * @file mock_environment.h
 * @brief Host environment of the RTC driver test (tools/test_rtc.py).
 *
 * The runner removes the #include lines of src/plc_rtc.c and puts harness_prefix.c in front of it,
 * so this header replaces plc_rtc.h, platform.h and the PLC device headers. It provides the
 * integer names, the CMSIS register definitions of the STM32F103 (the layouts of RTC, RCC, PWR and
 * BKP) and the FX clock registers D8013..D8019 (second, minute, hour, day, month, year, weekday),
 * which are mapped onto an array that the test can read and write. M8015 (write the clock) and
 * M8018 (RTC present) are bit numbers in the mock relay store of harness_prefix.c.
 */
#include <stdint.h>
#include <stm32f103xb.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
/* Storage behind the FX clock registers; defined in harness_prefix.c. */
extern volatile u16 plc_dates[7];
#define D8013 plc_dates[0]
#define D8014 plc_dates[1]
#define D8015 plc_dates[2]
#define D8016 plc_dates[3]
#define D8017 plc_dates[4]
#define D8018 plc_dates[5]
#define D8019 plc_dates[6]
/* Relay numbers: bit positions in the mock relay store, not real PLC addresses. */
#define M8015 15U
#define M8018 18U
