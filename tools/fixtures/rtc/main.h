#include <stdint.h>
#include <stm32f103xb.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
extern volatile u16 plc_dates[7];
#define D8013 plc_dates[0]
#define D8014 plc_dates[1]
#define D8015 plc_dates[2]
#define D8016 plc_dates[3]
#define D8017 plc_dates[4]
#define D8018 plc_dates[5]
#define D8019 plc_dates[6]
#define M8015 15U
#define M8018 18U
