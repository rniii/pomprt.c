#include "pomprt.h"

int main(void) {
  const char *input;
  pomprt_t prompt = pomprt_new(">> ");

  while ((input = pomprt_read(prompt))) {
    printf("%s\n", input);
  }

  pomprt_destroy(prompt);
}
