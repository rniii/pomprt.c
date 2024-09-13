/*
 * Pomprt, minimal readline implementation
 *
 * Copyright (c) 2024 rini
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE

#include "pomprt.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __unix__
#include <termios.h>

static struct termios pomprt_tty;
static bool pomprt_tty_ok = false;

void pomprt__term_init(void) {
  static bool init = false;
  if (init)
    return;
  if (tcgetattr(0, &pomprt_tty) != -1)
    pomprt_tty_ok = true;
  init = true;
}

void pomprt__term_raw(void) {
  if (!pomprt_tty_ok)
    return;
  struct termios raw = pomprt_tty;
  cfmakeraw(&raw);
  raw.c_oflag |= OPOST;
  tcsetattr(0, OPOST, &raw);
}

void pomprt__term_restore(void) {
  if (!pomprt_tty_ok)
    return;
  tcsetattr(0, OPOST, &pomprt_tty);
}
#else
// TODO: winapi. may god have mercy
#error "Unsupported platform!"
#endif

static inline pomprt_buffer_t pomprt__create_buf(size_t capacity) {
  char *bytes = malloc(capacity);
  return (pomprt_buffer_t){0, capacity, bytes};
}

static inline void pomprt__destroy_buf(pomprt_buffer_t buf) { free(buf.bytes); }

static void pomprt__reserve_buf(pomprt_buffer_t buf, size_t additional) {
  size_t required = buf.len + additional;
  if (required <= buf.capacity)
    return;

  size_t new_cap;
  new_cap = buf.capacity * 2;
  new_cap = required > new_cap ? required : new_cap;

  buf.bytes = realloc(buf.bytes, new_cap);
  buf.capacity = new_cap;
}

static void pomprt__insert_buf(
  pomprt_buffer_t buf, size_t idx, const char *other) {
  size_t sz = strlen(other);
  pomprt__reserve_buf(buf, sz);
  memmove(buf.bytes + idx + sz, buf.bytes + idx, buf.len - sz);
  memcpy(buf.bytes + idx, other, sz);
  buf.len += sz;
}

static void pomprt__pushb_buf(pomprt_buffer_t buf, char b) {
  pomprt__reserve_buf(buf, 1);
  buf.bytes[buf.len++] = b;
}

static inline void pomprt__clear_buf(pomprt_buffer_t buf) { buf.len = 0; }

struct pomprt_reader {
  FILE *input;
  pomprt_buffer_t buf;
};

pomprt_reader_t pomprt__create_reader(FILE *input) {
  return (pomprt_reader_t){input, pomprt__create_buf(8)};
}

pomprt_ansi_t pomprt_reader_next(pomprt_reader_t *reader) {
  // http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
  // clang-format off
  static const uint8_t utf8d[] = {
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
  };
  static const uint8_t utf8s[] = {
     0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12,
  };
  // clang-format on

  unsigned char byte = fgetc(reader->input);
  if (byte == 0x1b) {
    byte = fgetc(reader->input);
    if (byte == '[') {
      reader->buf.len = 0;
      while ((byte = fgetc(reader->input)) < 0x40) {
        pomprt__pushb_buf(reader->buf, byte);
      }
      pomprt__pushb_buf(reader->buf, 0);

      return (pomprt_ansi_t){ANSI_CSI, {.str = reader->buf.bytes}};
    } else {
      return (pomprt_ansi_t){ANSI_ESC, {.byte = byte}};
    }
  } else if (byte <= 0x1f || byte == 0x7f) {
    return (pomprt_ansi_t){ANSI_CTRL, {.byte = byte}};
  } else {
    // we have enough capacity for a utf-8 char
    reader->buf.len = 0;
    for (size_t state = 0; state != 0; byte = fgetc(reader->input)) {
      state = utf8s[state + utf8d[byte]];
      reader->buf.bytes[reader->buf.len++] = byte;
      if (state == 12)
        return (pomprt_ansi_t){ANSI_CHAR, {.str = "\uFFFD"}};
    }

    return (pomprt_ansi_t){ANSI_CHAR, {.str = reader->buf.bytes}};
  }
}

pomprt_event_t pomprt_next_event_emacs(void *_, pomprt_reader_t *reader) {
  for (;;) {
    pomprt_ansi_t ansi = pomprt_reader_next(reader);
    if (ansi.type == ANSI_CHAR)
      return (pomprt_event_t){POMPRT_INSERT, ansi.data.str};
    if (ansi.type == ANSI_ESC && ansi.data.byte == '\r')
      return (pomprt_event_t){POMPRT_INSERT, "\n"};

    enum pomprt_event_kind kind;
    if (ansi.type == ANSI_CTRL) {
      // clang-format off
      switch (ansi.data.byte ^ 0x40) {
      case 'M':
        kind = POMPRT_ENTER;      break;
      case '?':
      case 'H':
        kind = POMPRT_BACKSPACE;  break;
      case 'I':
        kind = POMPRT_TAB;        break;
      case 'B':
        kind = POMPRT_LEFT;       break;
      case 'F':
        kind = POMPRT_RIGHT;      break;
      case 'A':
        kind = POMPRT_HOME;       break;
      case 'E':
        kind = POMPRT_END;        break;
      case 'C':
        kind = POMPRT_INTERRUPT;  break;
      case 'D':
        kind = POMPRT_EOF;        break;
      case '\\':
        kind = POMPRT_ABORT;      break;
      case 'L':
        kind = POMPRT_CLEAR;      break;
      default:
        continue;
      }
      // clang-format on
    } else if (ansi.type == ANSI_CSI) {
      if (strlen(ansi.data.str) == 1) {
        // clang-format off
        switch (ansi.data.str[0]) {
        case 'D':
          kind = POMPRT_LEFT;   break;
        case 'C':
          kind = POMPRT_RIGHT;  break;
        case 'H':
          kind = POMPRT_HOME;   break;
        case 'F':
          kind = POMPRT_END;    break;
        case 'A':
          kind = POMPRT_UP;     break;
        case 'B':
          kind = POMPRT_DOWN;   break;
        }
        // clang-format on
      } else if (strcmp(ansi.data.str, "1;5D") == 0 ||
        strcmp(ansi.data.str, "1;3D") == 0) {
        kind = POMPRT_LEFT_WORD;
      } else if (strcmp(ansi.data.str, "1;5C") == 0 ||
        strcmp(ansi.data.str, "1;3D") == 0) {
        kind = POMPRT_RIGHT_WORD;
      } else {
        continue;
      }
    } else {
      continue;
    }

    return (pomprt_event_t){kind, 0};
  }
}

pomprt_t pomprt_new(const char *prompt) {
  pomprt__term_init();

  return (pomprt_t){
    prompt,
    pomprt_default_editor,
    .buffer = pomprt__create_buf(128),
  };
}

void pomprt_destroy(pomprt_t p) { pomprt__destroy_buf(p.buffer); }

static inline pomprt_event_t pomprt__next_event(
  pomprt_editor_t editor, pomprt_reader_t *reader) {
  return editor.next_event(editor.self, reader);
}

const char *pomprt__read_dumb(pomprt_t p) {
  pomprt__clear_buf(p.buffer);
  int byte;
  while ((byte = fgetc(stdin)) != '\n') {
    if (byte == EOF)
      return NULL;
    pomprt__pushb_buf(p.buffer, byte);
  }
  pomprt__pushb_buf(p.buffer, 0);
  return p.buffer.bytes;
}

const char *pomprt_read(pomprt_t p) {
  if (!isatty(STDIN_FILENO))
    return pomprt__read_dumb(p);
  return pomprt_read_from(p, stdin, isatty(STDOUT_FILENO) ? stdout : stderr);
}

void pomprt__redraw(pomprt_t p, FILE *output) {
  fprintf(output, "\r\x1b[J%s%s", p.prompt, p.buffer.bytes);
}

const char *pomprt_read_from(pomprt_t p, FILE *input, FILE *output) {
  pomprt__term_raw();

  pomprt__clear_buf(p.buffer);
  pomprt__pushb_buf(p.buffer, 0);

  size_t cursor = 0;
  size_t prompt_len = strlen(p.prompt);
  pomprt_reader_t reader = pomprt__create_reader(input);

  pomprt__redraw(p, output);

  for (;;) {
    pomprt_event_t event = pomprt__next_event(p.editor, &reader);

    switch (event.type) {
    case POMPRT_INSERT:
      pomprt__insert_buf(p.buffer, cursor, event.str);
      pomprt__redraw(p, output);
      break;
    case POMPRT_INTERRUPT:
      pomprt__term_restore();
      return NULL;
    default:
      pomprt__term_restore();
      return p.buffer.bytes;
    }

    fprintf(output, "\r\x1b[%ziC", cursor + prompt_len);
    fflush(output);
  };
}
