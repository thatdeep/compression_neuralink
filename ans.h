#ifndef ANS_H
#define ANS_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "stream.h"
#include "heap.h"

#define ASIZE 256
#define R 12
#define RSMALL (R + 1)
#define L (1 << R)
#define SPREADSTEP ((L * 5 / 8) + 3)
#define BUFSIZE 1000000

typedef struct {
    uint32_t new_x;
    uint8_t symbol;
    uint8_t nbbits;
} tableEntry;

unsigned int nextPowerOf2(uint32_t n);

uint8_t powerOfTwoExponent(uint32_t x);

uint8_t logfloor(uint32_t x);

uint32_t reverse_bits_uint32_t(uint32_t x, int nbits);

double kl_divergence_factor(int32_t *p_occ, int32_t *q_occ, int p_total, int q_total, int n);

int32_t *quantize_occurences(int32_t *occ, int alphabet_size, int quant_pow);

uint8_t *spread_fast(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size);

uint32_t encode(uint8_t *data, size_t dsize, vecStream *vs, int32_t *occ, int alphabet_size, int quant_pow);

uint8_t *decode(size_t dsize, size_t bitsize, uint32_t x, memStream *bs, int32_t *occ, int alphabet_size, int quant_pow);

#endif // ANS_H