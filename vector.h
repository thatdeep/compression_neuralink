#ifndef VECTOR_H
#define VECTOR_H

#include "stdint.h"
#include "stdlib.h"
#include "string.h"


typedef struct {
    size_t size;
    size_t capacity;
    int32_t *data;
} Vector32;

typedef struct {
    size_t size;
    size_t capacity;
    uint32_t *data;
} UVector32;

typedef struct {
    size_t size;
    size_t capacity;
    int16_t *data;
} Vector16;

typedef struct {
    size_t size;
    size_t capacity;
    uint16_t *data;
} UVector16;

typedef struct {
    size_t size;
    size_t capacity;
    int8_t *data;
} Vector8;

typedef struct {
    size_t size;
    size_t capacity;
    uint8_t *data;
} UVector8;

void push_vector32(Vector32 *vec, int32_t value);

void push_uvector32(UVector32 *vec, uint32_t value);

void push_vector16(Vector16 *vec, int16_t value);

void push_uvector16(UVector16 *vec, uint16_t value);

void push_vector8(Vector8 *vec, int8_t value);

void push_uvector8(UVector8 *vec, uint8_t value);

void free_vector32(Vector32 *vec);

void free_uvector32(UVector32 *vec);

void free_vector16(Vector16 *vec);

void free_uvector16(UVector16 *vec);

void free_vector8(Vector8 *vec);

void free_uvector8(UVector8 *vec);

#endif // VECTOR_H