#include "pomprt.h"

int main(void) {
  const char *input;
  pomprt_t prompt = pomprt_new(">> ");

  while ((input = pomprt_read(&prompt))) {
    printf("%s\n", input);
  }

  if (pomprt_interrupt(&prompt)) {
    printf("^C\n");
  } else if (pomprt_eof(&prompt)) {
    printf("^D\n");
  }

  pomprt_destroy(prompt);
}
