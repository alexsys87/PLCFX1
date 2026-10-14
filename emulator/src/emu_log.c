/**
 * @file    emu_log.c
 * @brief   Log file of the PLC emulator (see emu_log.h).
 *
 * The log is shared by the main loop and by the console-control and crash handlers, which
 * run on other threads, so every write is serialised with a critical section.
 */
#include "emu_log.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static FILE *log_file;
static char log_file_path[MAX_PATH];
static emu_log_level console_level = EMU_LOG_INFO;
static emu_log_level file_level = EMU_LOG_DEBUG;
static unsigned long error_counts[EMU_CAT_COUNT];
static unsigned long warning_counts[EMU_CAT_COUNT];
static CRITICAL_SECTION log_lock;
static volatile LONG lock_ready;

static const char *const level_names[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};
static const char *const category_names[EMU_CAT_COUNT] = {
  "SYSTEM", "SERIAL", "PROTO ", "PLC   ", "FLASH ", "IO    "};

/** Create the lock on first use (the log may be used before emu_log_open()). */
static void ensure_lock(void) {
  if (InterlockedCompareExchange(&lock_ready, 1, 0) == 0) {
    InitializeCriticalSection(&log_lock);
    InterlockedExchange(&lock_ready, 2);
  }
  while (lock_ready != 2) Sleep(0);
}

/** Local wall-clock time with milliseconds, "YYYY-MM-DD hh:mm:ss.mmm". */
static void timestamp(char *out, size_t size) {
  SYSTEMTIME now;
  GetLocalTime(&now);
  snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
           now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
           now.wMilliseconds);
}

int emu_log_open(const char *path) {
  char stamp[32];
  ensure_lock();
  EnterCriticalSection(&log_lock);
  if (log_file) fclose(log_file);
  log_file = fopen(path, "a");
  if (log_file) {
    snprintf(log_file_path, sizeof log_file_path, "%s", path);
    timestamp(stamp, sizeof stamp);
    fprintf(log_file,
            "\n================================================================================\n"
            "%s  PLCFX1 emulator session started\n"
            "================================================================================\n",
            stamp);
    fflush(log_file);
  } else {
    log_file_path[0] = '\0';
  }
  LeaveCriticalSection(&log_lock);
  return log_file ? 0 : -1;
}

void emu_log_close(void) {
  int category;
  unsigned long errors = 0, warnings = 0;
  char stamp[32];

  ensure_lock();
  for (category = 0; category < EMU_CAT_COUNT; category++) {
    errors += error_counts[category];
    warnings += warning_counts[category];
  }
  emu_log(EMU_LOG_INFO, EMU_CAT_SYSTEM, "Session summary: %lu error(s), %lu warning(s)", errors,
          warnings);
  for (category = 0; category < EMU_CAT_COUNT; category++) {
    if (error_counts[category] || warning_counts[category])
      emu_log(EMU_LOG_INFO, EMU_CAT_SYSTEM, "  %s: %lu error(s), %lu warning(s)",
              category_names[category], error_counts[category], warning_counts[category]);
  }
  EnterCriticalSection(&log_lock);
  if (log_file) {
    timestamp(stamp, sizeof stamp);
    fprintf(log_file, "%s  PLCFX1 emulator session ended\n", stamp);
    fclose(log_file);
    log_file = NULL;
  }
  LeaveCriticalSection(&log_lock);
}

void emu_log_set_console_level(emu_log_level level) { console_level = level; }

void emu_log_set_file_level(emu_log_level level) { file_level = level; }

emu_log_level emu_log_threshold(void) {
  return console_level < file_level ? console_level : file_level;
}

void emu_log_v(emu_log_level level, emu_log_category category, const char *format,
               va_list args) {
  char message[1024];
  char stamp[32];

  if (category < 0 || category >= EMU_CAT_COUNT) category = EMU_CAT_SYSTEM;
  if (level >= EMU_LOG_ERROR) InterlockedIncrement((volatile LONG *)&error_counts[category]);
  else if (level == EMU_LOG_WARN) InterlockedIncrement((volatile LONG *)&warning_counts[category]);
  if (level < console_level && (level < file_level || !log_file)) return;

  vsnprintf(message, sizeof message, format, args);
  timestamp(stamp, sizeof stamp);
  ensure_lock();
  EnterCriticalSection(&log_lock);
  if (log_file && level >= file_level) {
    fprintf(log_file, "%s [%s] [%s] %s\n", stamp, level_names[level], category_names[category],
            message);
    if (level >= EMU_LOG_WARN) fflush(log_file);
  }
  if (level >= console_level) {
    FILE *stream = level >= EMU_LOG_ERROR ? stderr : stdout;
    fprintf(stream, "%s [%s] %s\n", stamp + 11, level_names[level], message);
    fflush(stream);
  }
  LeaveCriticalSection(&log_lock);
}

void emu_log(emu_log_level level, emu_log_category category, const char *format, ...) {
  va_list args;
  va_start(args, format);
  emu_log_v(level, category, format, args);
  va_end(args);
}

unsigned long emu_log_error_count(emu_log_category category) {
  return category < EMU_CAT_COUNT ? error_counts[category] : 0UL;
}

unsigned long emu_log_warning_count(emu_log_category category) {
  return category < EMU_CAT_COUNT ? warning_counts[category] : 0UL;
}

const char *emu_log_path(void) { return log_file_path; }

void emu_log_format_bytes(const unsigned char *data, size_t length, char *out, size_t out_size) {
  size_t i, used = 0;

  if (!out_size) return;
  out[0] = '\0';
  for (i = 0; i < length; i++) {
    char piece[8];
    unsigned char c = data[i];
    switch (c) {
      case 0x02: strcpy(piece, "<STX>"); break;
      case 0x03: strcpy(piece, "<ETX>"); break;
      case 0x05: strcpy(piece, "<ENQ>"); break;
      case 0x06: strcpy(piece, "<ACK>"); break;
      case 0x15: strcpy(piece, "<NAK>"); break;
      default:
        if (c >= 0x20 && c < 0x7F) { piece[0] = (char)c; piece[1] = '\0'; }
        else snprintf(piece, sizeof piece, "<%02X>", c);
        break;
    }
    if (used + strlen(piece) + 4 >= out_size) {
      strcpy(out + used, "...");
      return;
    }
    strcpy(out + used, piece);
    used += strlen(piece);
  }
}
