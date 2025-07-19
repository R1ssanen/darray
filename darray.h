#ifndef DARRAY_H_
#define DARRAY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DINLINE static inline

#define DGROWTH_FACTOR (1.5f)

#define DRUNTIME_ASSERT(expr, msg)                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "darray runtime assert at %s, line %d: %s\n", __func__,  \
              __LINE__, msg);                                                  \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#ifndef NDEBUG
#define DRUNTIME_DEBUG_ASSERT(expr, msg) DRUNTIME_ASSERT(expr, msg)
#else
#define DRUNTIME_DEBUG_ASSERT()                                                \
  do {                                                                         \
  } while (0)
#endif

enum {
  _DARRAY_TAG_STRIDE,
  _DARRAY_TAG_COUNT,
  _DARRAY_TAG_BYTES,
  _DARRAY_TAG_CAPACITY,
  _DARRAY_TAG_ENUM_END,
};

#define _DARRAY_HEADER_STRIDE (_DARRAY_TAG_ENUM_END * sizeof(size_t))

DINLINE size_t darray_fetch_field_impl(size_t *array, int field) {
  return *(array - field);
}

DINLINE void darray_set_field_impl(void *array, int field, size_t value) {
  *((size_t *)array - field) = value;
}

#define darray_stride(array)                                                   \
  darray_fetch_field_impl((size_t *)(array), _DARRAY_TAG_STRIDE)

#define darray_count(array)                                                    \
  darray_fetch_field_impl((size_t *)(array), _DARRAY_TAG_COUNT)

#define darray_bytes(array)                                                    \
  darray_fetch_field_impl((size_t *)(array), _DARRAY_TAG_BYTES)

#define darray_capacity(array)                                                 \
  darray_fetch_field_impl((size_t *)(array), _DARRAY_TAG_CAPACITY)

DINLINE
void *darray_create_impl(size_t stride, size_t count) {
  DRUNTIME_DEBUG_ASSERT(stride != 0, "cannot create array with stride of 0");
  if (count == 0)
    count = 1;

  size_t byte_count = stride * count;
  void *head = malloc(byte_count + _DARRAY_HEADER_STRIDE);
  DRUNTIME_ASSERT(head != NULL, "no memory to allocate array block.");

  void *block = (void *)((size_t *)head + _DARRAY_TAG_ENUM_END);
  darray_set_field_impl(block, _DARRAY_TAG_STRIDE, stride);
  darray_set_field_impl(block, _DARRAY_TAG_COUNT, count);
  darray_set_field_impl(block, _DARRAY_TAG_BYTES, byte_count);
  darray_set_field_impl(block, _DARRAY_TAG_CAPACITY, count * DGROWTH_FACTOR);
  return block;
}

#define darray_last(array)                                                     \
  (*((char *)(array) + darray_count(array) * darray_stride(array)))

#define darray_create(type) ((type *)(darray_create_impl(sizeof(type), 1)))

#define darray_create_reserved(type, count)                                    \
  ((type *)(darray_create_impl(sizeof(type), (count))))

DINLINE void darray_destroy(void *array) {
  size_t *head = (size_t *)array - _DARRAY_TAG_ENUM_END;
  free(head);
}

DINLINE void *darray_resize_impl(void *array, size_t new_capacity) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");

  size_t *old_head = (size_t *)array - _DARRAY_TAG_ENUM_END;
  size_t stride = darray_stride(array);

  void *new_head =
      realloc((void *)old_head, new_capacity * stride + _DARRAY_HEADER_STRIDE);
  DRUNTIME_ASSERT(new_head != NULL, "no memory to reallocate array block");

  void *new_block = (size_t *)new_head + _DARRAY_TAG_ENUM_END;
  size_t count = darray_count(array);

  // downsizing
  if (new_capacity < count) {
    darray_set_field_impl(new_block, _DARRAY_TAG_COUNT, new_capacity);
    darray_set_field_impl(new_block, _DARRAY_TAG_BYTES, new_capacity * stride);
  }

  darray_set_field_impl(new_block, _DARRAY_TAG_CAPACITY, new_capacity);
  return new_block;
}

#define darray_resize(array, count)                                            \
  do {                                                                         \
    (array) = darray_resize_impl(array, count);                                \
  } while (0)

#define darray_shrink(array) darray_resize(array, darray_count(array))

#define darray_reserve(array, count)                                           \
  darray_resize(array, darray_count(array) + (count))

DINLINE void *darray_push_impl(void *array, void *value) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");
  DRUNTIME_DEBUG_ASSERT(value != NULL, "value argument is null");

  size_t count = darray_count(array);
  size_t capacity = darray_capacity(array);
  if (count >= capacity)
    darray_resize(array, capacity * DGROWTH_FACTOR);

  size_t stride = darray_stride(array);
  size_t last_index = count - 1;
  memcpy((char *)array + last_index * stride, value, stride);

  darray_set_field_impl(array, _DARRAY_TAG_COUNT, count + 1);
  darray_set_field_impl(array, _DARRAY_TAG_BYTES, count * stride + stride);
  return array;
}

#define darray_push(array, value)                                              \
  do {                                                                         \
    (array) = darray_push_impl(array, value);                                  \
  } while (0)

DINLINE void darray_pop(void *array) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");

  darray_set_field_impl(array, _DARRAY_TAG_COUNT, darray_count(array) - 1);
  darray_set_field_impl(array, _DARRAY_TAG_BYTES,
                        darray_bytes(array) - darray_stride(array));
}

// move all elements above 'index' up 'amount' places
DINLINE void *darray_shift_up_impl(void *array, size_t index, size_t amount) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");

  size_t new_count = darray_count(array) + amount;
  if (new_count >= darray_capacity(array))
    darray_resize(array, new_count);

  size_t stride = darray_stride(array);
  char *element = (char *)array + index * stride;
  memmove(element, element + amount, amount * stride);

  darray_set_field_impl(array, _DARRAY_TAG_COUNT, new_count);
  darray_set_field_impl(array, _DARRAY_TAG_BYTES, new_count * stride);
  return array;
}

// move all elements above 'index' down 'amount' places
DINLINE void darray_shift_down_impl(void *array, size_t index, size_t amount) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");

  size_t new_count = darray_count(array) - amount;
  size_t stride = darray_stride(array);

  char *element = (char *)array + index * stride;
  memmove(element - amount, element, amount * stride);

  darray_set_field_impl(array, _DARRAY_TAG_COUNT, new_count);
  darray_set_field_impl(array, _DARRAY_TAG_BYTES, new_count * stride);
}

DINLINE void *darray_insert_impl(void *array, size_t index, void *value) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");
  DRUNTIME_DEBUG_ASSERT(value != NULL, "value argument is null");

  size_t count = darray_count(array);
  DRUNTIME_DEBUG_ASSERT(index > count, "array index out of bounds");

  // make room
  array = darray_shift_up_impl(array, index, 1);

  size_t stride = darray_stride(array);
  memcpy((char *)array + index * stride, value, stride);
  return array;
}

#define darray_insert(array, index, value)                                     \
  do {                                                                         \
    array = darray_insert_impl(array, index, value);                           \
  } while (0)

DINLINE void darray_remove_impl(void *array, size_t index, size_t amount) {
  DRUNTIME_DEBUG_ASSERT(array != NULL, "array argument is null");

  size_t count = darray_count(array);
  DRUNTIME_DEBUG_ASSERT(index > count, "array index out of bounds");

  darray_shift_down_impl(array, index, amount);
}

#define darray_remove(array, index) darray_remove_impl(array, index, 1)

#define darray_remove_span(array, index, count)                                \
  darray_remove_impl(array, index, count)

#endif
