# pomprt

A tiny and extensible readline implementation written from scratch. C99 rewrite of the [Rust version].

[Rust version]: https://github.com/rniii/pomprt

```c
const char *input;
pomprt_t prompt = pomprt_new(">> ");
while ((input = pomprt_read(&prompt))) {
  printf("%s\n", input);
}

pomprt_destroy(prompt);
```

## Why not [linenoise]?

This project is in similar scope to linenoise, providing some advantages:

- Unlike linenoise, it has UTF-8 support
- Multiline prompts are properly rendered with line wraps
- Windows terminals are also supported
- It's easier to use in FFI
- All in standard C99, and just 400 lines of code!

[linenoise]: https://github.com/antirez/linenoise

However, linenoise has likely been tested in more terminals. If you find problems with specific
keybinds with pomprt though, please [open an issue!](github.com/rniii/pomprt.c/issues/new)

## Building

<!-- maid-tasks -->

If you'd like to use C, meson and clang are recommended, but just about any compiler will do— all
you need is to compile `src/pomprt.c` and use `include/pomprt.h`.

### setup

```sh
CC=clang meson setup build
```

### build

```sh
[ -d build ] || maid setup
ninja -C build
```

### run

```sh
maid build && ./build/example
```
