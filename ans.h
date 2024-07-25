#ifndef ANS_H
#define ANS_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include "stream.h"
#include "heap.h"

#define ASIZE 256
#define R 12
#define RSMALL (R + 1)
#define L (1 << R)
#define SPREADSTEP ((L * 5 / 8) + 3)
#define BUFSIZE 1000000

#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

typedef struct {
    int32_t value;
    int32_t index;
} indexedOccurrence;

typedef struct {
    uint32_t new_x;
    uint8_t symbol;
    uint8_t nbbits;
} tableEntry;

typedef struct {
    uint32_t new_x;
    uint16_t symbol;
    uint8_t nbbits;
} tableEntry16;

typedef struct {
    double x;
    int y;
}   DummyDoubleIntPair;

int reversed_compare_indexed_occurrences(const void *a, const void *b);

int reversed_double_compare(const void *a, const void *b);

int reversed_int32_t_compare(const void *a, const void *b);

int reversed_then_direct_dummy_double_int_pair_compare(void *a, void *b);

unsigned int nextPowerOf2(uint32_t n);

uint8_t powerOfTwoExponent(uint32_t x);

uint8_t logfloor(uint32_t x);

uint32_t reverse_bits_uint32_t(uint32_t x, int nbits);

double kl_divergence_factor(int32_t *p_occ, int32_t *q_occ, int p_total, int q_total, int n);

int32_t *quantize_unsorted_occurences_precise(int32_t *occ, int alphabet_size, int quant_size);

int32_t *quantize_occurences_precise(int32_t *occ, int alphabet_size, int quant_size);

int32_t *quantize_occurences(int32_t *occ, int alphabet_size, int quant_pow);

uint16_t *spread_fast16(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size);

uint8_t *spread_fast(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size);

uint8_t *spread_tuned(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size);

uint16_t *spread_tuned16(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size);

uint32_t encode16(uint16_t *data, size_t dsize, vecStream *vs, int32_t *occ, int alphabet_size, int quant_pow);

uint16_t *decode16(size_t dsize, size_t bitsize, uint32_t x, memStream *bs, int32_t *occ, int alphabet_size, int quant_pow);

uint32_t encode(uint8_t *data, size_t dsize, vecStream *vs, int32_t *occ, int alphabet_size, int quant_pow);

uint8_t *decode(size_t dsize, size_t bitsize, uint32_t x, memStream *bs, int32_t *occ, int alphabet_size, int quant_pow);

#endif // ANS_H