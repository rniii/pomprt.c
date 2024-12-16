/*
 * buffer.h - simple resizable string buffer
 *
 * Part of https://github.com/rniii/99C
 *
 * COPYLEFT
 *    All copyright is waived to the public domain. This library is distributed
 *    without any warranty following the CC0 Public Domain Dedication:
 *
 *    <https://creativecommons.org/publicdomain/zero/1.0/>
 *
 * CREDITS
 *    rini <rini@rinici.de>
 */

#ifndef INCLUDE_BUFFER_H
#define INCLUDE_BUFFER_H
#include <stddef.h>

typedef struct {
  size_t capacity;
  size_t length;
  char *bytes;
} buffer_t;

/// Construct a new buffer with given capacity
buffer_t buffer_create(size_t capacity);
/// Deallocate a buffer
void buffer_destroy(buffer_t);

/// Reserve space for additional bytes, allocating more memory
void buffer_reserve(buffer_t *, size_t additional);
/// Shrink the buffer, deallocating some memory
void buffer_shrink(buffer_t *, size_t minimum);

/// Add a single byte to the end of the buffer
void buffer_push(buffer_t *, char value);
/// Extend the buffer with given bytes
void buffer_extend(buffer_t *, const void *value, size_t size);
/// Insert bytes at given index
void buffer_insert(buffer_t *, size_t idx, const void *value, size_t size);

/// Remove all data in the buffer
void buffer_clear(buffer_t *);
/// Limit the buffer to a size, removing bytes after the index
void buffer_truncate(buffer_t *, size_t size);
/// Remove bytes at given index
void buffer_remove(buffer_t *, size_t idx, size_t size);

/// Get a character at a specified index. -1 if out of bounds.
int buffer_get(buffer_t *, size_t idx);

// -------- Implementation

#ifdef BUFFER_IMPLEMENTATION
#undef BUFFER_IMPLEMENTATION
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUFFER_MIN
/// Minimum capacity for buffers, must be at least 1
#define BUFFER_MIN 8
#endif

inline buffer_t buffer_create(size_t capacity) {
  capacity = capacity < BUFFER_MIN ? BUFFER_MIN : capacity;
  char *data = malloc(capacity);
  data[0] = 0;
  return (buffer_t){capacity, 0, data};
}
inline void buffer_destroy(buffer_t buf) { free(buf.bytes); }

static void buffer__resize(buffer_t *buf, size_t cap) {
  buf->bytes = realloc(buf->bytes, cap);
  buf->capacity = cap;
}
void buffer_reserve(buffer_t *buf, size_t additional) {
  size_t req = buf->length + additional + 1;
  if (req <= buf->capacity)
    return;

  size_t cap = buf->capacity * 2;
  buffer__resize(buf, req > cap ? req : cap);
}
void buffer_shrink(buffer_t *buf, size_t minimum) {
  if (buf->capacity <= minimum)
    return;
  minimum = minimum < BUFFER_MIN ? BUFFER_MIN : minimum;
  buffer__resize(buf, buf->length > minimum ? buf->length : minimum);
}

void buffer_push(buffer_t *buf, char value) {
  buffer_reserve(buf, 1);
  buf->bytes[buf->length++] = value;
  buf->bytes[buf->length] = 0;
}
void buffer_extend(buffer_t *buf, const void *value, size_t size) {
  buffer_reserve(buf, size);
  memcpy(buf->bytes + buf->length, value, size);
  buf->bytes[buf->length + size] = 0;
  buf->length += size;
}
void buffer_insert(buffer_t *buf, size_t idx, const void *value, size_t size) {
  if (idx > buf->length) {
    assert(0 && "index out of bounds");
    return;
  }
  buffer_reserve(buf, size);
  memmove(buf->bytes + idx + size, buf->bytes + idx, buf->length - idx);
  memcpy(buf->bytes + idx, value, size);
  buf->length += size;
}

void buffer_clear(buffer_t *buf) {
  buf->bytes[0] = 0;
  buf->length = 0;
}
void buffer_truncate(buffer_t *buf, size_t size) {
  if (size >= buf->length)
    return;
  buf->bytes[size] = 0;
  buf->length = size;
}
void buffer_remove(buffer_t *buf, size_t idx, size_t size) {
  if (idx + size > buf->length) {
    assert(0 && "index out of bounds");
    return;
  }
  buf->length -= size;
  memcpy(buf->bytes + idx, buf->bytes + idx + size, size + 1);
}

int buffer_get(buffer_t *buf, size_t idx) {
  if (idx < 0 || idx >= buf->length)
    return -1;
  return buf->bytes[idx];
}

#endif
#endif
