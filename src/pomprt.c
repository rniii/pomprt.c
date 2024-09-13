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

static struct termios pomprt__tty;
static bool pomprt__tty_ok = false;

static void pomprt__term_init(void) {
  static bool init = false;
  if (init)
    return;
  if (tcgetattr(STDIN_FILENO, &pomprt__tty) != -1)
    pomprt__tty_ok = true;
  init = true;
}

static void pomprt__term_raw(void) {
  if (!pomprt__tty_ok)
    return;
  struct termios raw = pomprt__tty;
  cfmakeraw(&raw);
  raw.c_oflag |= OPOST;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void pomprt__term_restore(void) {
  if (!pomprt__tty_ok)
    return;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &pomprt__tty);
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

static void pomprt__reserve_buf(pomprt_buffer_t *buf, size_t additional) {
  size_t required = buf->len + additional;
  if (required <= buf->capacity)
    return;

  size_t new_cap;
  new_cap = buf->capacity * 2;
  new_cap = required > new_cap ? required : new_cap;

  buf->bytes = realloc(buf->bytes, new_cap);
  buf->capacity = new_cap;
}

static void pomprt__insert_buf(
  pomprt_buffer_t *buf, size_t idx, const char *value, size_t sz) {
  pomprt__reserve_buf(buf, sz);
  memmove(&buf->bytes[idx + sz], &buf->bytes[idx], buf->len - idx);
  memcpy(&buf->bytes[idx], value, sz);
  buf->len += sz;
}

static void pomprt__remove_buf(pomprt_buffer_t *buf, size_t idx, size_t sz) {
  buf->len -= sz;
  memcpy(&buf->bytes[idx], &buf->bytes[idx + sz], sz);
}

static void pomprt__pushb_buf(pomprt_buffer_t *buf, char b) {
  pomprt__reserve_buf(buf, 1);
  buf->bytes[buf->len++] = b;
}

static inline void pomprt__clear_buf(pomprt_buffer_t *buf) { buf->len = 0; }

struct pomprt_reader {
  FILE *input;
  pomprt_buffer_t buf;
};

static pomprt_reader_t pomprt__create_reader(FILE *input) {
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
      pomprt__clear_buf(&reader->buf);
      while ((byte = fgetc(reader->input)) < 0x40) {
        if (byte <= 0x1f || byte > 0x7e) // invalid, ignore it
          continue;
        pomprt__pushb_buf(&reader->buf, byte);
      }
      pomprt__pushb_buf(&reader->buf, 0);

      return (pomprt_ansi_t){ANSI_CSI, {.str = reader->buf.bytes}};
    } else {
      return (pomprt_ansi_t){ANSI_ESC, {.byte = byte}};
    }
  } else if (byte <= 0x1f || byte == 0x7f) {
    return (pomprt_ansi_t){ANSI_CTRL, {.byte = byte}};
  } else {
    // we have enough capacity for a utf-8 char, don't reserve
    pomprt__clear_buf(&reader->buf);
    for (uint32_t state = 0;; byte = fgetc(reader->input)) {
      state = utf8s[state + utf8d[byte]];
      reader->buf.bytes[reader->buf.len++] = byte;
      if (state == 0)
        break;
      if (state == 12)
        return (pomprt_ansi_t){ANSI_CHAR, {.str = "\uFFFD"}};
    }
    reader->buf.bytes[reader->buf.len++] = 0;

    return (pomprt_ansi_t){ANSI_CHAR, {.str = reader->buf.bytes}};
  }
}

pomprt_event_t pomprt_next_event_emacs(void *_, pomprt_reader_t *reader) {
  static const uint8_t events[128] = {
    // ctrl chars 0x00..0x1f, 0x7f
    ['?' ^ 0x40] = POMPRT_BACKSPACE,
    ['A' ^ 0x40] = POMPRT_HOME,
    ['B' ^ 0x40] = POMPRT_LEFT,
    ['C' ^ 0x40] = POMPRT_INTERRUPT,
    ['D' ^ 0x40] = POMPRT_EOF,
    ['E' ^ 0x40] = POMPRT_END,
    ['F' ^ 0x40] = POMPRT_RIGHT,
    ['H' ^ 0x40] = POMPRT_BACKSPACE,
    ['I' ^ 0x40] = POMPRT_TAB,
    ['L' ^ 0x40] = POMPRT_CLEAR,
    ['M' ^ 0x40] = POMPRT_ENTER,
    ['\\' ^ 0x40] = POMPRT_ABORT,
    // csi 0x40..0x7e
    ['A'] = POMPRT_UP,
    ['B'] = POMPRT_DOWN,
    ['C'] = POMPRT_RIGHT,
    ['D'] = POMPRT_LEFT,
    ['F'] = POMPRT_END,
    ['H'] = POMPRT_HOME,
  };

  for (;;) {
    pomprt_ansi_t ansi = pomprt_reader_next(reader);
    enum pomprt_event_kind kind;

    switch (ansi.type) {
    case ANSI_CHAR:
      return (pomprt_event_t){POMPRT_INSERT, ansi.data.str};
    case ANSI_ESC:
      if (ansi.data.byte == '\r')
        return (pomprt_event_t){POMPRT_INSERT, "\n"};
      break;
    case ANSI_CTRL:
      if ((kind = events[(size_t)ansi.data.byte]))
        return (pomprt_event_t){kind, 0};
      break;
    case ANSI_CSI:
      if (strlen(ansi.data.str) == 1) {
        if ((kind = events[(size_t)ansi.data.str[0]]))
          return (pomprt_event_t){kind, 0};
        break;
      }
      if (strcmp(ansi.data.str, "1;5D") == 0 ||
        strcmp(ansi.data.str, "1;3D") == 0) {
        return (pomprt_event_t){POMPRT_LEFT_WORD, 0};
      }
      if (strcmp(ansi.data.str, "1;5C") == 0 ||
        strcmp(ansi.data.str, "1;3D") == 0) {
        return (pomprt_event_t){POMPRT_RIGHT_WORD, 0};
      }
      break;
    }
  }
}

pomprt_t pomprt_new(const char *prompt) {
  pomprt__term_init();

  return (pomprt_t){
    .prompt_len = strlen(prompt),
    .prompt = prompt,
    .editor = pomprt_default_editor,
    .buffer = pomprt__create_buf(128),
    .state = POMPRT_STATE_READING,
  };
}

void pomprt_destroy(pomprt_t p) { pomprt__destroy_buf(p.buffer); }

const char *pomprt__read_dumb(pomprt_t *p) {
  pomprt__clear_buf(&p->buffer);
  int byte;
  while ((byte = fgetc(stdin)) != '\n') {
    if (byte == EOF) {
      p->state = POMPRT_STATE_EOF;
      return NULL;
    }
    pomprt__pushb_buf(&p->buffer, byte);
  }
  pomprt__pushb_buf(&p->buffer, 0);
  p->state = POMPRT_STATE_READING;
  return p->buffer.bytes;
}

const char *pomprt_read(pomprt_t *p) {
  if (!isatty(STDIN_FILENO))
    return pomprt__read_dumb(p);
  return pomprt_read_from(p, stdin, isatty(STDOUT_FILENO) ? stdout : stderr);
}

static inline pomprt_event_t pomprt__next_event(
  pomprt_editor_t editor, pomprt_reader_t *reader) {
  return editor.next_event(editor.self, reader);
}

static void pomprt__redraw(pomprt_t *p, FILE *output) {
  fwrite("\r\x1b[J", 1, 4, output);
  fwrite(p->prompt, 1, p->prompt_len, output);
  fwrite(p->buffer.bytes, 1, p->buffer.len, output);
}

const char *pomprt_read_from(pomprt_t *p, FILE *input, FILE *output) {
  pomprt__term_raw();

  pomprt__clear_buf(&p->buffer);

  size_t cursor = 0;
  size_t prompt_len = strlen(p->prompt);
  pomprt_reader_t reader = pomprt__create_reader(input);

  pomprt__redraw(p, output);

  for (;;) {
    pomprt_event_t event = pomprt__next_event(p->editor, &reader);

    switch (event.type) {
    case POMPRT_INSERT: {
      size_t chr_len = strlen(event.str);
      pomprt__insert_buf(&p->buffer, cursor, event.str, chr_len);
      pomprt__redraw(p, output);
      cursor += chr_len;
      break;
    }
    case POMPRT_ENTER:
      pomprt__redraw(p, output);
      fputc('\n', output);
      fflush(output);
      p->state = POMPRT_STATE_READING;
      goto end;
    case POMPRT_BACKSPACE:
      if (cursor > 0) {
        size_t i = 0;
        while (p->buffer.bytes[cursor - ++i] < 0)
          ;
        pomprt__remove_buf(&p->buffer, cursor -= i, i);
        pomprt__redraw(p, output);
      }
      break;
    case POMPRT_INTERRUPT:
      p->state = POMPRT_STATE_INTERRUPTED;
      goto end;
    case POMPRT_EOF:
      p->state = POMPRT_STATE_EOF;
      goto end;
    default:
      p->state = POMPRT_STATE_READING;
      goto end;
    }

    fprintf(output, "\r\x1b[%ziC", cursor + prompt_len);
    fflush(output);
  };

end:
  pomprt__pushb_buf(&p->buffer, 0);

  pomprt__term_restore();

  if (p->state != POMPRT_STATE_READING)
    return NULL;
  return p->buffer.bytes;
}

bool pomprt_eof(pomprt_t *p) { return p->state == POMPRT_STATE_EOF; }
bool pomprt_interrupt(pomprt_t *p) {
  return p->state == POMPRT_STATE_INTERRUPTED;
}
