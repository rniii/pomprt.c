# pomprt

A tiny and extensible readline implementation written from scratch. C99 rewrite of the [Rust version].

[Rust version]: https://github.com/rniii/pomprt

- UTF-8 support with high performance
- Easily bindable API with automatic allocation

```c
const char *input;
pomprt_t prompt = pomprt_new(">> ");
while ((input = pomprt_read(&prompt))) {
  printf("%s\n", input);
}

pomprt_destroy(prompt);
```

## Building

<!-- maid-tasks -->

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
