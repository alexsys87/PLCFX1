/**
 * @file    emu_log.h
 * @brief   Log file of the PLC emulator.
 *
 * Every message goes to the log file with a millisecond timestamp, its level and its
 * category. Errors and warnings are flushed at once, so the file is complete even if
 * the emulator is killed. Messages at or above the console level are echoed to the
 * console. A per-category error counter feeds the summary written at exit.
 */
#ifndef EMU_LOG_H
#define EMU_LOG_H

#include <stdarg.h>
#include <stddef.h>

typedef enum {
  EMU_LOG_TRACE = 0,  /**< Wire traffic and other high-volume details (only with --trace). */
  EMU_LOG_DEBUG,      /**< Diagnostics that help to understand an error. */
  EMU_LOG_INFO,       /**< Normal events: start, RUN/STOP, program download. */
  EMU_LOG_WARN,       /**< Suspicious but recoverable: noise bytes, slow scans. */
  EMU_LOG_ERROR,      /**< Something failed: NAK, PLC fault, serial or flash error. */
  EMU_LOG_FATAL       /**< The emulator cannot continue. */
} emu_log_level;

/** Source of a message; errors are counted per category for the exit summary. */
typedef enum {
  EMU_CAT_SYSTEM = 0, /**< Emulator start-up, configuration, memory mapping, crashes. */
  EMU_CAT_SERIAL,     /**< COM port: open, read, write, line errors. */
  EMU_CAT_PROTOCOL,   /**< Programming protocol: frames, NAK replies and their reasons. */
  EMU_CAT_RUNTIME,    /**< PLC interpreter: RUN/STOP, faults, scan time. */
  EMU_CAT_FLASH,      /**< Program/retained flash emulation and its image file. */
  EMU_CAT_IO,         /**< Virtual X inputs and Y outputs. */
  EMU_CAT_COUNT
} emu_log_category;

/**
 * Open (append to) the log file and write a session header.
 * @return 0 on success, -1 when the file cannot be opened (messages then go to the console only).
 */
int emu_log_open(const char *path);

/** Write the error summary and close the log file. */
void emu_log_close(void);

/** Echo messages of at least @p level to the console (default: EMU_LOG_INFO). */
void emu_log_set_console_level(emu_log_level level);

/** Write messages of at least @p level to the file (default: EMU_LOG_DEBUG). */
void emu_log_set_file_level(emu_log_level level);

/** Lowest level currently written anywhere; lets callers skip expensive formatting. */
emu_log_level emu_log_threshold(void);

/** printf-style message. */
void emu_log(emu_log_level level, emu_log_category category, const char *format, ...);
void emu_log_v(emu_log_level level, emu_log_category category, const char *format, va_list args);

/** Number of errors (ERROR and FATAL) logged in @p category since start. */
unsigned long emu_log_error_count(emu_log_category category);

/** Number of warnings logged in @p category since start. */
unsigned long emu_log_warning_count(emu_log_category category);

/** Path of the open log file, or an empty string. */
const char *emu_log_path(void);

/**
 * Render bytes for the log: printable ASCII as is, control characters as <STX>, <ETX>,
 * <ENQ>, <ACK>, <NAK> or <xx>. The result is always NUL-terminated and truncated to fit.
 */
void emu_log_format_bytes(const unsigned char *data, size_t length, char *out, size_t out_size);

#define LOG_TRACE(cat, ...) emu_log(EMU_LOG_TRACE, (cat), __VA_ARGS__)
#define LOG_DEBUG(cat, ...) emu_log(EMU_LOG_DEBUG, (cat), __VA_ARGS__)
#define LOG_INFO(cat, ...)  emu_log(EMU_LOG_INFO, (cat), __VA_ARGS__)
#define LOG_WARN(cat, ...)  emu_log(EMU_LOG_WARN, (cat), __VA_ARGS__)
#define LOG_ERROR(cat, ...) emu_log(EMU_LOG_ERROR, (cat), __VA_ARGS__)
#define LOG_FATAL(cat, ...) emu_log(EMU_LOG_FATAL, (cat), __VA_ARGS__)

#endif /* EMU_LOG_H */
