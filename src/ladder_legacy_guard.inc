/* Translation-unit-local containment for the experimental legacy interpreter.
 * This does not validate operands, loop latency, arithmetic or device semantics. */
static void legacy_fault(plc_runtime_error error);
static u16 legacy_word(void);
static volatile void *legacy_memory(uintptr_t address, u32 size, u32 alignment);
static u32 legacy_pulse_test(u32 step);
static void legacy_pulse_put(u32 step, u8 on);
static void legacy_bit_put(u32 bit, u8 on);
static void PLC_Legacy_Missing(void);
static void PLC_Legacy_Incomplete(void);

/* Unlike the old raw/bit-band accesses, these cannot intentionally reach
 * registers or ordinary C state outside the reserved PLC image. */
#undef PLC_Bu
#undef PLC_Hu
#undef PLC_Wu
#undef PLC_Hs
#undef PLC_Ws
#undef PLC_Du
#undef PLC_Wf
#define PLC_Bu(x) (*(volatile u8 *)legacy_memory((uintptr_t)(x),1U,1U))
#define PLC_Hu(x) (*(volatile u16 *)legacy_memory((uintptr_t)(x),2U,2U))
#define PLC_Wu(x) (*(volatile u32 *)legacy_memory((uintptr_t)(x),4U,4U))
#define PLC_Hs(x) (*(volatile s16 *)legacy_memory((uintptr_t)(x),2U,2U))
#define PLC_Ws(x) (*(volatile s32 *)legacy_memory((uintptr_t)(x),4U,4U))
#define PLC_Du(x) (*(volatile int64_t *)legacy_memory((uintptr_t)(x),8U,8U))
#define PLC_Wf(x) (*(volatile float *)legacy_memory((uintptr_t)(x),4U,4U))
