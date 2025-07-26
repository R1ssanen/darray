/**
 * @file darray.h
 * @author R1ssanen
 * @brief Dynamic array implementation for C99 projects. Arrays are
 * directly compatible with C standard-library functions, as they're
 * simply heap arrays with metadata headers.
 * @see http://www.github.com/R1ssanen/darray.git
 */

#ifndef DARRAY_H_
#define DARRAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DINLINE static inline
#define DGROWTH_FACTOR (1.5f)

#ifdef DARRAY_CUSTOM_ALLOC
extern void *darray_alloc(size_t count);
extern void *darray_realloc(void *ptr, size_t count);
extern void darray_dealloc(void *ptr);
#define DARRAY_ALLOC(count) darray_alloc(count)
#define DARRAY_REALLOC(ptr, count) darray_realloc(ptr, count)
#define DARRAY_DEALLOC(ptr) darray_dealloc(ptr)

#else
#define DARRAY_ALLOC(count) malloc(count)
#define DARRAY_REALLOC(ptr, count) realloc(ptr, count)
#define DARRAY_DEALLOC(ptr) free(ptr)
#endif

#define DASSERT(expr, msg)                                                     \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "darray runtime assert at %s, line %d: %s\n", __func__,  \
              __LINE__, msg);                                                  \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#ifndef NDEBUG
#define DASSERT_DEBUG(expr, msg) DASSERT(expr, msg)
#else
#define DASSERT_DEBUG() ((void)0)
#endif

enum {
  DARRAY_TAG_STRIDE_,
  DARRAY_TAG_COUNT_,
  DARRAY_TAG_CAPACITY_,
  DARRAY_ENUM_END_,
};

#define DARRAY_HEADER_STRIDE_ (DARRAY_ENUM_END_ * sizeof(size_t))

DINLINE size_t darray_fetch_field_(const size_t *block, int field) {
  return *(block - field - 1);
}

DINLINE void darray_set_field_(size_t *block, int field, size_t value) {
  *(block - field - 1) = value;
}

#define darray_stride(array)                                                   \
  darray_fetch_field_((const size_t *)(array), DARRAY_TAG_STRIDE_)

#define darray_count(array)                                                    \
  darray_fetch_field_((const size_t *)(array), DARRAY_TAG_COUNT_)

#define darray_capacity(array)                                                 \
  darray_fetch_field_((const size_t *)(array), DARRAY_TAG_CAPACITY_)

/*
 * IMPLEMENTATION
 */

DINLINE
void *darray_new_impl(size_t stride, size_t count) {
  DASSERT(stride != 0, "cannot create array with stride of 0.");

  size_t *head =
      (size_t *)DARRAY_ALLOC(stride * (count + 1) + DARRAY_HEADER_STRIDE_);
  DASSERT(head != NULL, "no memory to allocate array.");

  size_t *block = head + DARRAY_ENUM_END_;
  darray_set_field_(block, DARRAY_TAG_STRIDE_, stride);
  darray_set_field_(block, DARRAY_TAG_COUNT_, 0);
  darray_set_field_(block, DARRAY_TAG_CAPACITY_,
                    (count + 1) * DGROWTH_FACTOR + 0.5f);
  return block;
}

DINLINE void darray_free_impl(size_t *block) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");

  size_t *head = block - DARRAY_ENUM_END_;
  DARRAY_DEALLOC(head);
}

DINLINE void *darray_resize_impl(size_t *block, size_t new_capacity) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");

  size_t stride = darray_stride(block);
  size_t bytes = new_capacity * stride + DARRAY_HEADER_STRIDE_;

  size_t *new_head = (size_t *)DARRAY_REALLOC(block - DARRAY_ENUM_END_, bytes);
  DASSERT(new_head != NULL, "no memory to reallocate array.");

  size_t *new_block = new_head + DARRAY_ENUM_END_;

  // downsizing
  if (new_capacity < darray_count(new_block)) {
    darray_set_field_(new_block, DARRAY_TAG_COUNT_, new_capacity);
  }

  darray_set_field_(new_block, DARRAY_TAG_CAPACITY_, new_capacity);
  return new_block;
}

DINLINE void *darray_grow_impl(size_t *block, unsigned amount) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");

  size_t count = darray_count(block);
  size_t capacity = darray_capacity(block);
  size_t grown_size = count + amount;

  if (grown_size >= capacity) {
    size_t new_capacity = amount * capacity * DGROWTH_FACTOR + 0.5f;
    block = (size_t *)darray_resize_impl(block, new_capacity);
  }

  darray_set_field_(block, DARRAY_TAG_COUNT_, grown_size);
  return block;
}

DINLINE void darray_pop_impl(size_t *block) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");
  darray_set_field_(block, DARRAY_TAG_COUNT_, darray_count(block) - 1);
}

// move all elements above 'index' up 'amount' places
DINLINE void *darray_shift_up_impl(size_t *block, size_t index, size_t amount) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");

  size_t old_count = darray_count(block);
  block = (size_t *)darray_grow_impl(block, amount);

  size_t stride = darray_stride(block);
  char *element = (char *)block + index * stride;
  memmove(element + amount, element, amount * stride);

  darray_set_field_(block, DARRAY_TAG_COUNT_, old_count + amount);
  return block;
}

// move all elements above 'index' down 'amount' places
DINLINE void darray_shift_down_impl(size_t *block, size_t index,
                                    size_t amount) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");

  size_t new_count = darray_count(block) - amount;
  size_t stride = darray_stride(block);

  char *element = (char *)block + index * stride;
  memmove(element - amount, element, amount * stride);

  darray_set_field_(block, DARRAY_TAG_COUNT_, new_count);
}

DINLINE void *darray_insert_impl(size_t *block, size_t index,
                                 const void *value) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");
  DASSERT_DEBUG(value != NULL, "value argument is null.");

  size_t count = darray_count(block);
  DASSERT_DEBUG(index < count, "array index out of bounds.");

  // make room
  block = (size_t *)darray_shift_up_impl(block, index, 1);

  size_t stride = darray_stride(block);
  memcpy((char *)block + index * stride, value, stride);
  return block;
}

DINLINE void darray_remove_impl(size_t *block, size_t index, size_t amount) {
  DASSERT_DEBUG(block != NULL, "array argument is null.");

  size_t count = darray_count(block);
  DASSERT_DEBUG(index < count, "array index out of bounds.");

  darray_shift_down_impl(block, index, amount);
}

/*
 * API
 */

#define darray_new(type) ((type *)(darray_new_impl(sizeof(type), 0)))

#define darray_new_reserved(type, count)                                       \
  ((type *)(darray_new_impl(sizeof(type), count)))

#define darray_free(array) darray_free_impl((size_t *)(array))

#define darray_clear(array) darray_set_field_impl(array, DARRAY_TAG_COUNT_, 0)

#define darray_resize(array, count)                                            \
  do {                                                                         \
    (array) = darray_resize_impl((size_t *)(array), count);                    \
  } while (0)

#define darray_shrink(array) darray_resize(array, darray_count(array))

#define darray_reserve(array, count)                                           \
  darray_resize(array, darray_count(array) + (count))

#define darray_push(array, value)                                              \
  do {                                                                         \
    (array) = darray_grow_impl((size_t *)(array), 1);                          \
    *darray_last(array) = (value);                                             \
  } while (0)

#define darray_pop(array) darray_pop_impl((size_t *)(array))

#define darray_last(array) ((array) + darray_count(array) - 1)

#define darray_insert(array, index, value)                                     \
  do {                                                                         \
    (array) = darray_insert_impl((size_t *)(array), index, value);             \
  } while (0)

#define darray_remove(array, index)                                            \
  darray_remove_impl((size_t *)(array), index, 1)

#define darray_remove_span(array, index, count)                                \
  darray_remove_impl((size_t *)(array), index, count)

#define darray_foreach(array, type, it)                                        \
  for (type *it = (array); it != darray_last(array); ++it)

#endif
