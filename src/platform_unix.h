#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdbool.h>

#include <signal.h>
#include <termios.h>
#include <unistd.h>

static struct termios pomprt__tty;

static bool pomprt__term_init(void) {
  return tcgetattr(STDIN_FILENO, &pomprt__tty) != -1;
}

static void pomprt__term_raw(void) {
  struct termios raw = pomprt__tty;
  cfmakeraw(&raw);
  raw.c_oflag |= OPOST;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void pomprt__term_restore(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &pomprt__tty);
}
