#ifndef BWT_H
#define BWT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int compare_uint32_t(const void* a, const void* b);

int32_t *sort_cyclic_shifts(int32_t *s, int n, int alphabet_size);

uint32_t *bwt(uint32_t *s, int n, int alphabet_size, int *sentinel_index);

#endif // BWT_H