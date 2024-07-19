#ifndef BWT_H
#define BWT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int32_t *sort_cyclic_shifts(int32_t *s, int n, int alphabet_size, int *original_index);

int32_t *bwt(int32_t *s, int n, int alphabet_size, int *original_index);

#endif // BWT_H