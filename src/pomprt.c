/*
 * Pomprt, minimal readline implementation
 *
 * Copyright (c) 2024 rini
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE

#define BUFFER_IMPLEMENTATION
#include "pomprt.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __unix__
#include "platform_unix.h"
#elif defined(_WIN32)
#include "platform_nt.h"
#else
#error "Unsupported platform"
#endif

struct pomprt_reader {
  FILE *input;
  buffer_t buf;
};

static pomprt_reader_t pomprt__create_reader(FILE *input) {
  return (pomprt_reader_t){input, buffer_create(8)};
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
      buffer_clear(&reader->buf);
      for (;;) {
        byte = fgetc(reader->input);
        if (byte <= 0x1f || byte >= 0x7f) // invalid, ignore it
          continue;
        buffer_push(&reader->buf, byte);
        if (byte >= 0x40)
          break;
      }
      buffer_push(&reader->buf, 0);

      return (pomprt_ansi_t){ANSI_CSI, {.str = reader->buf.bytes}};
    } else {
      return (pomprt_ansi_t){ANSI_ESC, {.byte = byte}};
    }
  } else if (byte <= 0x1f || byte == 0x7f) {
    return (pomprt_ansi_t){ANSI_CTRL, {.byte = byte}};
  } else {
    // we have enough capacity for a utf-8 char, don't reserve
    buffer_clear(&reader->buf);
    for (uint32_t state = 0;; byte = fgetc(reader->input)) {
      state = utf8s[state + utf8d[byte]];
      reader->buf.bytes[reader->buf.length++] = byte;
      if (state == 0)
        break;
      if (state == 12)
        return (pomprt_ansi_t){ANSI_CHAR, {.str = "\uFFFD"}};
    }
    reader->buf.bytes[reader->buf.length++] = 0;

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
    ['Z' ^ 0x40] = POMPRT_SUSPEND,
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

bool pomprt_is_keyword(void *_, const char *c) {
  return (*c < 0) || (*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') ||
    (*c >= 'a' && *c <= 'z');
}

static bool pomprt__is_term = false;

pomprt_t pomprt_new(const char *prompt) {
  static bool init = false;
  if (!init) {
    pomprt__is_term = pomprt__term_init();
  }
  init = true;

  return (pomprt_t){
    .prompt_len = strlen(prompt),
    .prompt = prompt,
    .editor = {NULL, pomprt_next_event_emacs, pomprt_is_keyword},
    .buffer = buffer_create(128),
    .state = POMPRT_STATE_READING,
  };
}

void pomprt_destroy(pomprt_t p) { buffer_destroy(p.buffer); }

const char *pomprt__read_dumb(pomprt_t *p) {
  buffer_clear(&p->buffer);
  int byte;
  while ((byte = fgetc(stdin)) != '\n') {
    if (byte == EOF) {
      p->state = POMPRT_STATE_EOF;
      return NULL;
    }
    buffer_push(&p->buffer, byte);
  }
  buffer_push(&p->buffer, 0);
  p->state = POMPRT_STATE_READING;
  return p->buffer.bytes;
}

const char *pomprt_read(pomprt_t *p) {
  if (!pomprt__is_term || !isatty(fileno(stdin)))
    return pomprt__read_dumb(p);
  return pomprt_read_from(p, stdin, isatty(fileno(stdout)) ? stdout : stderr);
}

static inline pomprt_event_t pomprt__next_event(
  pomprt_t *p, pomprt_reader_t *reader) {
  return p->editor.next_event(p->editor.self, reader);
}

static inline bool pomprt__is_keyword(pomprt_t *p, size_t cursor) {
  return p->editor.is_keyword(p->editor.self, &p->buffer.bytes[cursor]);
}

static void pomprt__redraw(pomprt_t *p, FILE *output) {
  fwrite("\r\x1b[J", 1, 4, output);
  fwrite(p->prompt, 1, p->prompt_len, output);
  fwrite(p->buffer.bytes, 1, p->buffer.length, output);
}

static size_t pomprt__count_chars(const char *buf, size_t end) {
  size_t i = 0;
  for (; end--; buf++)
    i += *buf > -0x40;
  return i;
}

const char *pomprt_read_from(pomprt_t *p, FILE *input, FILE *output) {
  if (pomprt__is_term)
    pomprt__term_raw();

  buffer_clear(&p->buffer);
  buffer_shrink(&p->buffer, 1 << 16); // limit buffer size

  size_t cursor = 0;
  size_t prompt_len = strlen(p->prompt);
  pomprt_reader_t reader = pomprt__create_reader(input);

  pomprt__redraw(p, output);

  for (;;) {
    pomprt_event_t event = pomprt__next_event(p, &reader);

    switch (event.type) {
    case POMPRT_INSERT: {
      size_t chr_len = strlen(event.str);
      buffer_insert(&p->buffer, cursor, event.str, chr_len);
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
        while (p->buffer.bytes[cursor - ++i] <= -0x40)
          ;
        buffer_remove(&p->buffer, cursor -= i, i);
        pomprt__redraw(p, output);
      }
      break;
    case POMPRT_TAB:
      break;
    case POMPRT_LEFT:
      if (cursor == 0)
        continue;
      while (p->buffer.bytes[--cursor] <= -0x40)
        ;
      break;
    case POMPRT_RIGHT:
      if (cursor >= p->buffer.length)
        continue;
      while (p->buffer.bytes[++cursor] <= -0x40)
        ;
      break;
    case POMPRT_HOME:
      cursor = 0;
      break;
    case POMPRT_END:
      cursor = p->buffer.length;
      break;
    case POMPRT_INTERRUPT:
      p->state = POMPRT_STATE_INTERRUPTED;
      goto end;
    case POMPRT_EOF:
      p->state = POMPRT_STATE_EOF;
      goto end;
    case POMPRT_SUSPEND:
#ifdef __unix__
      kill(getpid(), SIGTSTP);
      pomprt__redraw(p, output);
#endif
      break;
    case POMPRT_ABORT:
      pomprt__term_restore();
      abort();
    case POMPRT_UP:
      break;
    case POMPRT_DOWN:
      break;
    case POMPRT_CLEAR:
      break;
    case POMPRT_LEFT_WORD:
      while (cursor > 0 && pomprt__is_keyword(p, --cursor))
        ;
      break;
    case POMPRT_RIGHT_WORD:
      while (cursor < p->buffer.length && pomprt__is_keyword(p, ++cursor))
        ;
      break;
    }

    fprintf(output, "\r\x1b[%ziC",
      pomprt__count_chars(p->buffer.bytes, cursor) + prompt_len);
    fflush(output);
  };

end:
  if (pomprt__is_term)
    pomprt__term_restore();

  if (p->state != POMPRT_STATE_READING)
    return NULL;
  return p->buffer.bytes;
}

bool pomprt_eof(pomprt_t *p) { return p->state == POMPRT_STATE_EOF; }
bool pomprt_interrupt(pomprt_t *p) {
  return p->state == POMPRT_STATE_INTERRUPTED;
}
