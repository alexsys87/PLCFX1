/**
 * @file    plc_runtime_profile.h
 * @brief   Compile-time selection of the PLC instruction set.
 *
 * PLC_RUNTIME_EXPERIMENTAL = 0 (default) builds the checked 21-mnemonic subset
 * (src/plc_runtime_minimal.c). PLC_RUNTIME_EXPERIMENTAL = 1 builds the broad
 * FX1N-style instruction layer (src/plc_exp_*.c), a separate interpreter. The
 * experimental profile is never an automatic fallback for an opcode that the
 * minimal runtime rejects. Define the macro on the compiler command line (IAR:
 * Options > C/C++ Compiler > Preprocessor).
 */
#ifndef PLC_RUNTIME_PROFILE_H
#define PLC_RUNTIME_PROFILE_H

#ifndef PLC_RUNTIME_EXPERIMENTAL
#define PLC_RUNTIME_EXPERIMENTAL 0
#endif
#if PLC_RUNTIME_EXPERIMENTAL != 0 && PLC_RUNTIME_EXPERIMENTAL != 1
#error "PLC_RUNTIME_EXPERIMENTAL must be 0 or 1"
#endif
#endif
