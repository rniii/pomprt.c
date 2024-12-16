#include <stdbool.h>
#include <stdlib.h>

#include <io.h> // what??
#include <windows.h>

static DWORD pomprt__conin_mode;
static DWORD pomprt__conout_mode;
static HANDLE pomprt__conin;
static HANDLE pomprt__conout;

static bool pomprt__term_init(void) {
  pomprt__conin = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  pomprt__conout = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  return GetConsoleMode(pomprt__conin, &pomprt__conin_mode) &&
    GetConsoleMode(pomprt__conout, &pomprt__conout_mode);
}

static void pomprt__term_raw(void) {
  DWORD i = pomprt__conin_mode, o = pomprt__conout_mode;
  i &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
  i |= ENABLE_VIRTUAL_TERMINAL_INPUT;
  o |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;

  SetConsoleMode(pomprt__conin, i);
  SetConsoleMode(pomprt__conout, o);
}

static void pomprt__term_restore(void) {
  SetConsoleMode(pomprt__conin, pomprt__conout_mode);
  SetConsoleMode(pomprt__conout, pomprt__conout_mode);
}
