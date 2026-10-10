#ifndef PLC_RUNTIME_PROFILE_H
#define PLC_RUNTIME_PROFILE_H
/* Default is the checked 21-mnemonic subset. Experimental is a separate
 * legacy state machine, never an unsupported-opcode fallback. */
#ifndef PLC_RUNTIME_EXPERIMENTAL
#define PLC_RUNTIME_EXPERIMENTAL 0
#endif
#if PLC_RUNTIME_EXPERIMENTAL != 0 && PLC_RUNTIME_EXPERIMENTAL != 1
#error "PLC_RUNTIME_EXPERIMENTAL must be 0 or 1"
#endif
#endif
