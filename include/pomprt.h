/*
 * Pomprt, minimal readline implementation
 *
 * Copyright (c) 2024 rini
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_POMPRT_H
#define INCLUDE_POMPRT_H

#include <stdint.h>
#include <stdio.h>

typedef struct pomprt pomprt_t;
typedef struct pomprt_editor pomprt_editor_t;

/** Creation functions */

pomprt_t pomprt_new(const char *prompt);
pomprt_t pomprt_with(pomprt_editor_t, const char *prompt);

/** Cleanup */

void pomprt_destroy(pomprt_t);

/** Input functions */

const char *pomprt_read(pomprt_t prompt);
const char *pomprt_read_from(pomprt_t prompt, FILE *input, FILE *output);

/** ANSI escape parsing */

enum pomprt_ansi_kind {
  ANSI_CHAR, /**< An UTF-8 character as a multi-byte sequence */
  ANSI_CTRL, /**< An ASCII control character, including DEL */
  ANSI_ESC,  /**< An extended control character, `ESC x` */
  ANSI_CSI,  /**< A Control Sequence Introducer sequence, `ESC [ x` */
};

typedef struct {
  enum pomprt_ansi_kind type;
  union {
    char byte; /**< Used if type is ANSI_CTRL or ANSI_ESC */
    char *str; /**< Used if type is ANSI_CHAR or ANSI_CSI */
  } data;
} pomprt_ansi_t;

typedef struct pomprt_reader pomprt_reader_t;

pomprt_ansi_t pomprt_reader_next(pomprt_reader_t *reader);

/** Editor callback API */

enum pomprt_event_kind {
  POMPRT_INSERT,     /**< Inserts a character and moves the cursor. */
  POMPRT_ENTER,      /**< Enter key. Submits the input. */
  POMPRT_BACKSPACE,  /**< Backspace key. Deletes character below the cursor. */
  POMPRT_TAB,        /**< Tab key. Indents the input or triggers completion. */
  POMPRT_LEFT,       /**< Left arrow. Moves cursor backwards. */
  POMPRT_RIGHT,      /**< Right arrow. Moves cursor forwards. */
  POMPRT_HOME,       /**< Home key. Moves cursor to start of input. */
  POMPRT_END,        /**< End key. Moves cursor to end of input. */
  POMPRT_INTERRUPT,  /**< Ctrl+C. Aborts if there's no input, or clears. */
  POMPRT_EOF,        /**< Ctrl+D. Aborts if there's no input. */
  POMPRT_SUSPEND,    /**< Ctrl+Z. Suspends current process (Unix only). */
  POMPRT_ABORT,      /**< Ctrl+\. Coredumps current process (Unix only). */
  POMPRT_UP,         /**< Up arrow. Selects previous entry in history. */
  POMPRT_DOWN,       /**< Down arrow. Selects next entry in history. */
  POMPRT_CLEAR,      /**< CTRL+L. Clears the terminal. */
  POMPRT_LEFT_WORD,  /**< Alt+Left. Moves cursor back a word. */
  POMPRT_RIGHT_WORD, /**< Alt+Right. Moves cursor forward a word. */
};

typedef struct {
  enum pomprt_event_kind type;
  char *str; /**< Character insterted by POMPRT_INSERT, NULL otherwise. */
} pomprt_event_t;

struct pomprt_editor {
  void *self; /**< Data passed to callbacks */
  pomprt_event_t (*next_event)(void *self, pomprt_reader_t *reader);
};

pomprt_event_t pomprt_next_event_emacs(void *self, pomprt_reader_t *reader);

const pomprt_editor_t pomprt_default_editor = {NULL, pomprt_next_event_emacs};

/** Implementation details. Subject to change! */

typedef struct {
  size_t len;
  size_t capacity;
  char *bytes;
} pomprt_buffer_t;

struct pomprt {
  const char *prompt;
  pomprt_editor_t editor;
  pomprt_buffer_t buffer; /**< Input buffer. Modified every read. */
};

#endif // INCLUDE_POMPRT_H
