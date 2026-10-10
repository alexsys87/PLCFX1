/********************************************************/
// CPU requirements: STM32F103, at least 64 KB RAM and 128 KB Flash.
// Original firmware tested on STM32F103RDT6 and STM32F103VET6.
// Edit date: 20150909
// Editor: Xiao Xiaosheng
// Online shop: shop182385147.taobao.com
/********************************************************/

#include "plc_io.h"

u16 X_DIY = 10;

/* The original firmware disables board-specific input filtering.
 * Keep the runtime hook without re-enabling the obsolete pin mappings.
 */
void X_filter(void) {
}

/* Board-specific output refresh was disabled in the original firmware. */
void PLC_IO_Refresh(void) {
}

/* Board-specific I/O configuration was disabled in the original firmware. */
void PLC_IO_config(void) {
}
