/**
 * @file    emu_protocol.c
 * @brief   Programming-port observer: request decoding, tracing and NAK diagnosis.
 *
 * The rules in nak_reason() mirror Process_switch() in src/plc_serial_protocol.c. They are
 * only used to explain a NAK in the log; the firmware alone decides the reply.
 */
#include "emu_protocol.h"

#include <stdio.h>
#include <string.h>

#include "emu_log.h"
#include "emu_memory.h"
#include "plc_flash.h"
#include "plc_memory.h"
#include "plc_memory_map.h"

/* Longest frame (STX up to the last checksum digit) that fits the firmware's rx_data[]. */
#define MAX_FRAME 142U

enum { PENDING_NONE, PENDING_ENQ, PENDING_FRAME, PENDING_OVERFLOW };

static uint8_t frame[MAX_FRAME + 1];
static size_t frame_length;      /* 0: outside a frame */
static uint8_t request[MAX_FRAME + 1];
static size_t request_length;
static int pending;              /* what the next reply answers */
static unsigned long noise_bytes;
static bool trace;
static unsigned long requests, naks;

void emu_proto_set_trace(bool on) { trace = on; }
bool emu_proto_get_trace(void) { return trace; }
unsigned long emu_proto_requests(void) { return requests; }
unsigned long emu_proto_naks(void) { return naks; }

static int hex_value(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

void emu_proto_flush_noise(void) {
  if (noise_bytes) {
    LOG_WARN(EMU_CAT_PROTOCOL,
             "%lu byte(s) outside a frame ignored (noise, wrong baud rate or a client that does "
             "not use STX framing)",
             noise_bytes);
    noise_bytes = 0;
  }
}

void emu_proto_on_line_error(void) {
  if (frame_length)
    LOG_WARN(EMU_CAT_PROTOCOL, "Frame in progress (%u byte(s)) abandoned after a line error",
             (unsigned)frame_length);
  frame_length = 0;
}

void emu_proto_on_rx_byte(uint8_t byte) {
  if (byte == 0x05) { /* ENQ is answered at once, even inside a frame */
    if (frame_length)
      LOG_WARN(EMU_CAT_PROTOCOL, "ENQ inside a frame: %u byte(s) of the frame discarded",
               (unsigned)frame_length);
    frame_length = 0;
    pending = PENDING_ENQ;
    return;
  }
  if (byte == 0x02) {
    emu_proto_flush_noise();
    if (frame_length > 1) {
      char text[400];
      emu_log_format_bytes(frame, frame_length, text, sizeof text);
      LOG_WARN(EMU_CAT_PROTOCOL, "Incomplete frame restarted by a new STX, discarded: %s", text);
    }
    frame[0] = byte;
    frame_length = 1;
    return;
  }
  if (!frame_length) {
    noise_bytes++;
    return;
  }
  if (frame_length >= MAX_FRAME) {
    memcpy(request, frame, frame_length);
    request_length = frame_length;
    frame_length = 0;
    pending = PENDING_OVERFLOW;
    return;
  }
  frame[frame_length++] = byte;
  if (frame_length > 3 && frame[frame_length - 3] == 0x03) {
    memcpy(request, frame, frame_length);
    request_length = frame_length;
    frame_length = 0;
    pending = PENDING_FRAME;
  }
}

/* ---- Request decoding -------------------------------------------------------------------- */

/** Decoded payload of the last request (hex pairs between the command and ETX). */
static uint8_t payload[MAX_FRAME / 2];
static size_t payload_length;
static bool payload_valid;

static void decode_payload(void) {
  size_t i, digits = request_length >= 5 ? request_length - 5 : 0;
  payload_valid = !(digits & 1U);
  payload_length = 0;
  for (i = 0; payload_valid && i < digits; i += 2) {
    int hi = hex_value(request[2 + i]), lo = hex_value(request[3 + i]);
    if (hi < 0 || lo < 0) {
      payload_valid = false;
      break;
    }
    payload[payload_length++] = (uint8_t)((hi << 4) | lo);
  }
}

/** Is this the special E7/E8 bit-force frame (four hex digits, low byte first)? */
static bool is_force_frame(void) {
  return request_length == 10 && request[1] == 'E' && (request[2] == '7' || request[2] == '8');
}

static unsigned force_address(void) {
  return (unsigned)((hex_value(request[3]) << 4) | hex_value(request[4]) |
                    (hex_value(request[5]) << 12) | (hex_value(request[6]) << 8));
}

/** One-line human description of the last request. */
static void describe(char *out, size_t size) {
  unsigned address = payload_length >= 2 ? (unsigned)(payload[0] << 8 | payload[1]) : 0;
  unsigned length = payload_length >= 3 ? payload[2] : 0;
  unsigned e_address = payload_length >= 3 ? (unsigned)(payload[1] << 8 | payload[2]) : 0;
  unsigned e_length = payload_length >= 4 ? payload[3] : 0;

  if (request_length < 2) { snprintf(out, size, "empty frame"); return; }
  if (!is_force_frame() && !payload_valid) {
    snprintf(out, size, "command '%c' with a malformed payload",
             request[1] >= 0x20 && request[1] < 0x7F ? request[1] : '?');
    return;
  }
  if (is_force_frame()) {
    unsigned bit = force_address();
    const char *what = request[2] == '7' ? "set bit" : "reset bit";
    if (request[2] == '7' && bit == 0x0E24) what = "RUN request (E7 M8036)";
    if (request[2] == '7' && bit == 0x0E25) what = "STOP request (E7 M8037)";
    snprintf(out, size, "E%c %s, bit address 0x%04X", request[2], what, bit);
    return;
  }
  switch (request[1]) {
    case '0': snprintf(out, size, "0 read RAM 0x%04X, %u byte(s)", address, length); return;
    case '1': snprintf(out, size, "1 write RAM 0x%04X, %u byte(s)", address, length); return;
    case '4': snprintf(out, size, "4 legacy handshake"); return;
    case '7': snprintf(out, size, "7 set bit 0x%04X", address); return;
    case '8': snprintf(out, size, "8 reset bit 0x%04X", address); return;
    case 'B': snprintf(out, size, "B flush staged program block"); return;
    case 'E': break;
    default: snprintf(out, size, "unknown command '%c' (0x%02X)", request[1] >= 0x20 && request[1] < 0x7F ? request[1] : '?', request[1]); return;
  }
  if (!payload_length) { snprintf(out, size, "E without sub-command"); return; }
  switch (payload[0]) {
    case 0x00: snprintf(out, size, "E00 read RAM 0x%04X, %u byte(s)%s", e_address, e_length, e_address == 0x1790 ? " (monitor snapshot)" : ""); return;
    case 0x01: snprintf(out, size, "E01 read program 0x%04X, %u byte(s)", e_address, e_length); return;
    case 0x10: snprintf(out, size, "E10 write RAM 0x%04X, %u byte(s)", e_address, e_length); return;
    case 0x11: snprintf(out, size, "E11 write %s 0x%04X, %u byte(s)", e_address >= 0x8000 ? "program" : "RAM", e_address, e_length); return;
    case 0x41: snprintf(out, size, "E41 search program from 0x%04X", e_address); return;
    case 0x42: snprintf(out, size, "E42 flush staged program block"); return;
    case 0x60: snprintf(out, size, "E60 clear program memory"); return;
    case 0x61: snprintf(out, size, "E61 discard staged program block"); return;
    case 0x62: snprintf(out, size, "E62 clear M0..M3071"); return;
    case 0x63: snprintf(out, size, "E63 clear D0..D1999"); return;
    case 0x64: snprintf(out, size, "E64 clear D0..D999"); return;
    default: snprintf(out, size, "unknown extended command E%02X", payload[0]); return;
  }
}

/* ---- NAK diagnosis ----------------------------------------------------------------------- */

static bool ram_range(unsigned offset, unsigned length) {
  return offset <= PLC_RAM_SIZE && length <= PLC_RAM_SIZE - offset;
}

/** Reason why a RAM write of [offset, offset + length) is refused, or NULL. */
static const char *ram_write_problem(unsigned offset, unsigned length, char *out, size_t size) {
  if (!ram_range(offset, length)) {
    snprintf(out, size, "RAM range 0x%04X+%u lies outside the PLC image (0x0000..0x%04lX)", offset,
             length, (unsigned long)PLC_RAM_SIZE - 1U);
    return out;
  }
  if (length && offset <= Addr_M8 && offset + length > Addr_M8) {
    snprintf(out, size,
             "the write covers byte 0x%04X (M8000..M8007), which is protected; use E7 0E24 / "
             "E7 0E25 for RUN/STOP",
             (unsigned)Addr_M8);
    return out;
  }
  return NULL;
}

/** Reason why a read reply of @p length bytes at physical address @p physical is refused, or NULL. */
static const char *read_problem(unsigned long physical, unsigned length, char *out, size_t size) {
  if (length > 70U) {
    snprintf(out, size, "read length %u exceeds the 70-byte reply limit", length);
    return out;
  }
  if (physical >= RAM_ADDR) {
    if (!ram_range((unsigned)(physical - RAM_ADDR), length)) {
      snprintf(out, size, "RAM range 0x%04lX+%u lies outside the PLC image", physical - RAM_ADDR, length);
      return out;
    }
    return NULL;
  }
  if (physical < ROM_ADDR || physical > PLC_RETAIN_BASE || length > PLC_RETAIN_BASE - physical) {
    snprintf(out, size, "program range 0x%04lX+%u lies outside the program area (0x0000..0x%04lX)",
             physical - ROM_ADDR, length, (unsigned long)(PLC_RETAIN_BASE - ROM_ADDR - 1U));
    return out;
  }
  return NULL;
}

static const char *nak_reason(char *out, size_t size, bool flash_failed) {
  size_t i;
  unsigned sum = 0, address, length;
  int hi, lo;
  const char *problem;

  if (pending == PENDING_OVERFLOW) {
    snprintf(out, size, "frame longer than %u bytes (no ETX found)", MAX_FRAME);
    return out;
  }
  if (request_length < 4) return "frame too short";
  for (i = 1; i + 2 < request_length; i++) sum += request[i];
  hi = hex_value(request[request_length - 2]);
  lo = hex_value(request[request_length - 1]);
  if (hi < 0 || lo < 0) {
    snprintf(out, size, "checksum digits '%c%c' are not hexadecimal", request[request_length - 2],
             request[request_length - 1]);
    return out;
  }
  if ((unsigned)((hi << 4) | lo) != (sum & 0xFFU)) {
    snprintf(out, size, "checksum mismatch: frame carries %02X, sum of command..ETX is %02X",
             (unsigned)((hi << 4) | lo), sum & 0xFFU);
    return out;
  }
  if (request_length < 5) return "frame too short (no command)";
  if (flash_failed) return "flash write/erase failed while executing the request (see FLASH errors)";

  if (is_force_frame()) {
    unsigned bit;
    for (i = 3; i < 7; i++)
      if (hex_value(request[i]) < 0) return "bit address of E7/E8 contains a non-hex digit";
    bit = force_address();
    if (!ram_range(bit >> 3, 1U)) {
      snprintf(out, size, "bit address 0x%04X lies outside the PLC image", bit);
      return out;
    }
    if ((bit >> 3) == Addr_M8) return "M8000..M8007 cannot be forced; use E7 0E24 (RUN) / E7 0E25 (STOP)";
    return "rejected by the firmware (no specific reason found)";
  }
  if (!payload_valid) return "payload is not a sequence of hex digit pairs (odd length or non-hex character)";

  switch (request[1]) {
    case '0':
      if (payload_length != 3) return "command 0 expects 3 payload bytes: address(2) length(1)";
      address = (unsigned)(payload[0] << 8 | payload[1]);
      if ((problem = read_problem(RAM_ADDR + address, payload[2], out, size)) != NULL) return problem;
      break;
    case '1':
      if (payload_length < 3) return "command 1 expects address(2) length(1) data";
      address = (unsigned)(payload[0] << 8 | payload[1]);
      if ((problem = ram_write_problem(address, payload[2], out, size)) != NULL) return problem;
      if (payload_length != 3U + payload[2]) {
        snprintf(out, size, "length byte says %u data byte(s), the frame carries %u", payload[2], (unsigned)payload_length - 3U);
        return out;
      }
      break;
    case '7': case '8':
      if (payload_length != 2) return "commands 7/8 expect a 2-byte bit address";
      address = (unsigned)(payload[0] << 8 | payload[1]);
      if (!ram_range(address >> 3, 1U)) return "bit address lies outside the PLC image";
      if ((address >> 3) == Addr_M8) return "M8000..M8007 cannot be forced";
      break;
    case '4': break;
    case 'B': if (payload_length != 0) return "command B takes no payload"; break;
    case 'E': break;
    default:
      snprintf(out, size, "unknown command '%c' (0x%02X); supported: 0 1 4 7 8 B E", request[1] >= 0x20 && request[1] < 0x7F ? request[1] : '?', request[1]);
      return out;
  }
  if (request[1] != 'E') return "rejected by the firmware (no specific reason found)";

  if (payload_length < 1) return "extended command without a sub-command byte";
  switch (payload[0]) {
    case 0x00: case 0x01: if (payload_length != 4) return "E00/E01 expect sub(1) address(2) length(1)"; break;
    case 0x10: case 0x11: if (payload_length < 4) return "E10/E11 expect sub(1) address(2) length(1) data"; break;
    case 0x41: if (payload_length != 5) return "E41 expects sub(1) address(2) word(2)"; break;
    case 0x42: case 0x60: case 0x61: case 0x62: case 0x63: case 0x64:
      if (payload_length != 1) return "this extended command takes no arguments";
      break;
    default:
      snprintf(out, size, "unknown extended command E%02X; supported: E00 E01 E10 E11 E41 E42 E60..E64, E7/E8", payload[0]);
      return out;
  }
  address = payload_length >= 3 ? (unsigned)(payload[1] << 8 | payload[2]) : 0;
  length = payload_length >= 4 ? payload[3] : 0;
  switch (payload[0]) {
    case 0x00:
      if (address == 0x1790U)
        return "monitor snapshot (E00 0x1790) refused: the descriptor list at 0x1400 is invalid "
               "(bad address, odd word address, or too many entries)";
      if (address > 0x3FFFU) address -= 0x2000U;
      if ((problem = read_problem(RAM_ADDR + address, length, out, size)) != NULL) return problem;
      break;
    case 0x01:
      if (address >= 0x8000U) address -= 0x8000U;
      if (address >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE) {
        snprintf(out, size, "program address 0x%04X lies outside the program area", address);
        return out;
      }
      if ((problem = read_problem(ROM_ADDR + address, length, out, size)) != NULL) return problem;
      break;
    case 0x10:
      if (payload_length != 4U + length) {
        snprintf(out, size, "length byte says %u data byte(s), the frame carries %u", length, (unsigned)payload_length - 4U);
        return out;
      }
      if (address >= 0x8000U) return "E10 cannot write program memory (address >= 0x8000); use E11";
      if (address > 0x3FFFU) address -= 0x2000U;
      if ((problem = ram_write_problem(address, length, out, size)) != NULL) return problem;
      break;
    case 0x11:
      if (length == 0U) return "E11 with a zero length";
      if (payload_length != 4U + length) {
        snprintf(out, size, "length byte says %u data byte(s), the frame carries %u", length, (unsigned)payload_length - 4U);
        return out;
      }
      if (address >= 0x8000U) {
        address -= 0x8000U;
        if ((address & 1U) || (length & 1U)) return "program writes must start at an even address and have an even length";
        if (address >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE || length > PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE - address) {
          snprintf(out, size, "program range 0x%04X+%u lies outside the program area (%lu bytes)", address, length,
                   (unsigned long)(PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE));
          return out;
        }
      } else if ((problem = ram_write_problem(address, length, out, size)) != NULL) {
        return problem;
      }
      break;
    default: break;
  }
  return "rejected by the firmware (no specific reason found)";
}

void emu_proto_on_tx(const uint8_t *data, size_t length) {
  char request_text[400], reply_text[400], description[160], reason[200];
  unsigned flash_failures = emu_flash_take_failures();
  bool is_nak = length == 1 && data[0] == 0x15;

  if (pending == PENDING_NONE) {
    LOG_WARN(EMU_CAT_PROTOCOL, "Reply sent without a recognised request (%u byte(s))", (unsigned)length);
    return;
  }
  if (pending == PENDING_ENQ) {
    LOG_TRACE(EMU_CAT_PROTOCOL, "RX <ENQ>  ->  TX <ACK>");
    pending = PENDING_NONE;
    return;
  }
  requests++;
  decode_payload();
  if (request_length >= 5 || pending == PENDING_OVERFLOW) describe(description, sizeof description);
  else snprintf(description, sizeof description, "truncated frame");
  if (pending == PENDING_OVERFLOW) snprintf(description, sizeof description, "oversized frame");

  if (is_nak) {
    naks++;
    emu_log_format_bytes(request, request_length, request_text, sizeof request_text);
    LOG_ERROR(EMU_CAT_PROTOCOL, "NAK for request [%s]: %s", description,
              nak_reason(reason, sizeof reason, flash_failures > 0));
    LOG_ERROR(EMU_CAT_PROTOCOL, "  request bytes: %s", request_text);
  } else if (trace && emu_log_threshold() <= EMU_LOG_TRACE) {
    emu_log_format_bytes(request, request_length, request_text, sizeof request_text);
    emu_log_format_bytes(data, length, reply_text, sizeof reply_text);
    LOG_TRACE(EMU_CAT_PROTOCOL, "RX %s [%s]  ->  TX %s", request_text, description, reply_text);
  }
  if (!is_nak && request_length >= 5) {
    /* Events worth an INFO line even without tracing. */
    if (is_force_frame() && request[2] == '7' && force_address() == 0x0E24)
      LOG_INFO(EMU_CAT_PROTOCOL, "Host requested RUN");
    else if (is_force_frame() && request[2] == '7' && force_address() == 0x0E25)
      LOG_INFO(EMU_CAT_PROTOCOL, "Host requested STOP");
    else if (request[1] == 'E' && payload_length == 1 && payload[0] == 0x60)
      LOG_INFO(EMU_CAT_PROTOCOL, "Host cleared the program memory (E60)");
  }
  pending = PENDING_NONE;
}
