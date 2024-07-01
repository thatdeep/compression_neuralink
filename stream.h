#ifndef STREAM_H
#define STREAM_H

#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"


typedef struct {
    uint64_t bit_buffer;
    size_t bits_in_buffer;
    size_t current_bytesize;
    size_t total_bitsize;
    FILE *stream;
} bitStream;

typedef struct {
    uint64_t bit_buffer;
    size_t bits_in_buffer;
    size_t current_bytesize;
    size_t total_bitsize;
    uint8_t *stream;
} memStream;

typedef struct {
    memStream ms;
    size_t stream_capacity;
} vecStream;

void fill_buffer_bitstream(bitStream *bs);

uint32_t read_bits_bitstream(bitStream *bs, int n);

void flush_buffer_memstream(memStream *ms);

void write_bits_memstream(memStream *ms, uint32_t bits, int n);

void finalize_memstream(memStream *ms);

void fill_buffer_memstream(memStream *ms);

uint32_t read_bits_memstream(memStream *ms, int n);

void flush_buffer_vecstream(vecStream *vs);
void write_bits_vecstream(vecStream *vs, uint32_t bits, int n);

void finalize_vecstream(vecStream *vs);

void reverse_bits_memstream(memStream *ms);

#endif // STREAM_H