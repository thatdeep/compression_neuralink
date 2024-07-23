#include "ans.h"

int reversed_compare_indexed_occurrences(const void *a, const void *b) {
    return (int)(((indexedOccurrence *)a)->value < ((indexedOccurrence *)b)->value) - (((indexedOccurrence *)a)->value > ((indexedOccurrence *)b)->value);
}

int reversed_double_compare(const void *a, const void *b) {
    return (*(double *)a < *(double *)b) - (*(double *)a > *(double *)b);
}

int reversed_int32_t_compare(const void *a, const void *b) {
    return (int)((*(int32_t *)a < *(int32_t *)b) - (*(int32_t *)a > *(int32_t *)b));
}

int reversed_then_direct_dummy_double_int_pair_compare(void *a, void *b) {
    DummyDoubleIntPair *a_pair = (DummyDoubleIntPair *)a;
    DummyDoubleIntPair *b_pair = (DummyDoubleIntPair *)b;
    if (a_pair->x > b_pair->x) return -1;
    if (a_pair->x < b_pair->x) return 1;
    if (a_pair->y > b_pair->y) return 1;
    if (a_pair->y < b_pair->y) return -1;
    return 0;
}

unsigned int nextPowerOf2(uint32_t n) {
    if (n == 0)
        return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

uint8_t powerOfTwoExponent(uint32_t x) {
    if (x == 0)
        return -1;
    uint8_t exponent = 31 - __builtin_clz(x); // 7/15/31/63 for 8/16/32/64-bit inputs
    return exponent;
}

uint8_t logfloor(uint32_t x) {
    uint32_t np2x = nextPowerOf2(x);
    if (x == np2x) {
        return powerOfTwoExponent(x);
    }
    return powerOfTwoExponent(np2x >> 1);
}

uint32_t reverse_bits_uint32_t(uint32_t x, int nbits) {
    uint32_t reversed = 0;
    for (int i = 0; i < nbits; ++i) {
        if (x & ((uint32_t)1 << i)) {
            reversed |= (uint32_t)1 << (nbits - i - 1);
        }
    }
    return reversed;
}

double kl_divergence_factor(int32_t *p_occ, int32_t *q_occ, int p_total, int q_total, int n) {
    double result = 0.0;
    for (int i = 0; i < n; ++i) {
        result += (double)p_occ[i] * log((double)(p_occ[i]) * q_total /  (double)q_occ[i] / p_total) / p_total;
    }
    return result;
}

int32_t *quantize_occurences(int32_t *occ, int alphabet_size, int quant_size) {
    int total = 0;
    int quant_total = 0;
    int i, difference;
    int32_t *quant_occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);

    for (int i = 0; i < alphabet_size; ++i) {
        total += occ[i];
    }
    for (i = 0; i < alphabet_size; i++) {
        float probability = (float)occ[i] / total;
        quant_occ[i] = (int)(probability * quant_size + 0.5);
        if (quant_occ[i] == 0) quant_occ[i] = 1;
        //printf("prob[%d]=%f, quant_occ[%d]=%d\n", i, probability, i, quant_occ[i]);
        quant_total += quant_occ[i];
    }

    // Adjust quant values to sum exactly to QUANT_SIZE
    difference = quant_size - quant_total;
    // printf("quant_total=%d, difference=%d\n", quant_total, difference);
    int prev_difference = difference;
    while (difference > 0) {
        prev_difference = difference;
        //printf("difference: %d\n", difference);
        for (i = 0; i < alphabet_size && difference > 0; i++) {
            if (quant_occ[i] < (int)((float)occ[i] / total * quant_size + 0.5)) {
                quant_occ[i]++;
                difference--;
            }
        }
        if (prev_difference == difference) {
            quant_occ[0]++;
            difference--;
        }
    }
    while (difference < 0) {
        for (i = 0; i < alphabet_size && difference < 0; i++) {
            //printf("difference2: %d\n", difference);
            if (quant_occ[i] > 1) {
                quant_occ[i]--;
                difference++;
            }
        }
    }
    int quant_sum = 0;
    for (int i = 0; i < alphabet_size; ++i) {
        quant_sum += quant_occ[i];
    }
    // printf("quant_sum: %d\n", quant_sum);
    // TODO DO WE NEED INPLACE HERE?
    // for (i = 0; i < ASIZE; i++) {
    //     occ[i] = quant_occ[i];
    //     //printf("Symbol %d: Quantized value = %d\n", i, quant[i]);
    // }
    return quant_occ;
}

int32_t *quantize_unsorted_occurences_precise(int32_t *occ, int alphabet_size, int quant_size) {
    // occ has alphabet_size length
    // need to sort occ array, while keeping reverse permutation of indices so I can restore it from sorted version
    indexedOccurrence *occurrences = (indexedOccurrence *)malloc(sizeof(indexedOccurrence) * alphabet_size);
    for (int i = 0; i < alphabet_size; ++i) {
        occurrences[i].value = occ[i];
        occurrences[i].index = i;
    }
    qsort(occurrences, alphabet_size, sizeof(indexedOccurrence), reversed_compare_indexed_occurrences);
    int32_t *sorted_occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *permutation = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    for (int i = 0; i < alphabet_size; ++i) {
        sorted_occ[i] = occurrences[i].value;
        permutation[occurrences[i].index] = i;
    }
    int32_t *sorted_quant_occ = quantize_occurences_precise(sorted_occ, alphabet_size, quant_size);
    int32_t *unsorted_quant_occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    for (int i = 0; i < alphabet_size; ++i) {
        unsorted_quant_occ[i] = sorted_quant_occ[permutation[i]];
        //printf("%5d -- %5d\n", occ[i], unsorted_quant_occ[i]);
    }
    free(occurrences);
    free(sorted_occ);
    free(permutation);
    free(sorted_quant_occ);
    return unsorted_quant_occ;
}

int32_t *quantize_occurences_precise(int32_t *occ, int alphabet_size, int quant_size) {
    int total = 0;
    int quant_total = 0;
    int i, difference;
    int32_t *quant_occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    for (int i = 0; i < alphabet_size; ++i) {
        total += occ[i];
    }
    double *prob = (double *)malloc(sizeof(double) * alphabet_size);
    double *probn = (double *)malloc(sizeof(double) * alphabet_size);
    double *probr = (double *)malloc(sizeof(double) * alphabet_size);
    double *cost = (double *)malloc(sizeof(double) * alphabet_size);
    uint8_t *nnz_mask = (uint8_t *)malloc(sizeof(uint8_t) * alphabet_size);
    int32_t *arange = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    // I dont care about occ[i] = 0, let them be 0 in quant as well
    int used = 0;
    for (int i = 0; i < alphabet_size; ++i) {
        arange[i] = i;
        if (occ[i] > 0) {
            nnz_mask[i] = 1;
            prob[i] = 1.0 * occ[i] / total;
            probn[i] = prob[i] * quant_size;
            quant_occ[i] = (int32_t)round(probn[i]);
            if (quant_occ[i] == 0) quant_occ[i] = 1;
            probr[i] = 1.0 / prob[i];
            cost[i] = (probn[i] - quant_occ[i]) * (probn[i] - quant_occ[i]) * probr[i];
            used += quant_occ[i];
        } else {
            nnz_mask[i] = 0;
            prob[i] = 0;
            probn[i] = 0;
            quant_occ[i] = 0;
            probr[i] = 0;
            quant_occ[i] = 0;
            cost[i] = 0;
        }
    }
    // double *upd_cost = (double *)malloc(sizeof(double) * alphabet_size);
    DummyDoubleIntPair *ranged_upd_cost = (DummyDoubleIntPair *)malloc(sizeof(DummyDoubleIntPair) * alphabet_size);
    for (int i = 0; i < alphabet_size; ++i) {
        ranged_upd_cost[i].x = 0.0;//cost[i];
        ranged_upd_cost[i].y = i;
    }
    if (used != quant_size) {
        int sgn = 1;
        if (used > quant_size) sgn = -1;
        Heap heap = (Heap) {
            .size = 0,
            .capacity = alphabet_size,
            .data = (HeapEntry *)malloc(sizeof(HeapEntry) * alphabet_size),
            .key_compare = reversed_then_direct_dummy_double_int_pair_compare
        };
        for (int i = 0; i < alphabet_size; ++i) {
            if (!nnz_mask[i]) continue; // skip occ[i] == 0
            if (quant_occ[i] + sgn) {
                ranged_upd_cost[i].x = cost[i] - ((probn[i] - (quant_occ[i] + sgn)) * (probn[i] - (quant_occ[i] + sgn)) * probr[i]);
                //printf("pushing %f at index %d to heap\n", upd_cost[i], i);
                push_heap(&heap, (void *)(ranged_upd_cost + i), (void *)NULL);
            }
        }
        for (; used != quant_size; used += sgn) {
            DummyDoubleIntPair *dummy_upd_ref_pair = (DummyDoubleIntPair *)find_extreme_key_heap(&heap);
            double *upd_ref = &(dummy_upd_ref_pair->x);
            int *i_ref = &(dummy_upd_ref_pair->y);
            pop_heap(&heap);
            int i = *i_ref;
            cost[i] -= *(upd_ref);
            quant_occ[i] += sgn;
            if (quant_occ[i] + sgn) {
                *(upd_ref) = cost[i] - ((probn[i] - (quant_occ[i] + sgn)) * (probn[i] - (quant_occ[i] + sgn)) * probr[i]);
                //printf("rushing %f(%f) at index %d to heap\n", upd_cost[i], *upd_ref, i);
                push_heap(&heap, (void *)dummy_upd_ref_pair, (void *)NULL);
            }
        }
        free(heap.data);
    }
    // to maintain quant_occ order we can sort quant_occ inside of each class of equivalence with i~j <=> occ[i] = occ[j]
    for (int eq_start = 0; eq_start < alphabet_size;) {
        int eq_end = eq_start;
        while (eq_end < alphabet_size && occ[eq_start] == occ[eq_end]) {
            eq_end++; // determine ending of equivalence class
        }
        if (eq_end - eq_start > 1) {
            qsort(quant_occ + eq_start, eq_end - eq_start, sizeof(int32_t), reversed_int32_t_compare);
        }
        eq_start = eq_end;
    }
    // printf("quant occ:\n");
    // for (int i = 0; i < alphabet_size; ++i) {
    //     printf("%d: o: %d, q: %d\n", i, occ[i], quant_occ[i]);
    // }
    // kind of a debug print, quant_occ should be sorted as a result of an algorithm
    for (int i = 0; i < alphabet_size - 1; ++i) assert(occ[i] >= occ[i + 1]);
    for (int i = 0; i < alphabet_size - 1; ++i) {
        if (quant_occ[i] < quant_occ[i + 1]) printf("q[%d]=%d, q[%d]=%d. Assertion will fail\n", i, quant_occ[i], i + 1, quant_occ[i + 1]);
        assert(quant_occ[i] >= quant_occ[i + 1]);
    }

    free(prob);
    free(probn);
    free(probr);
    free(cost);
    free(ranged_upd_cost);
    free(arange);
    free(nnz_mask);
    // printf("quant occ:\n");
    // for (int i = 0; i < alphabet_size; ++i) {
    //     printf("%d: o: %d, q: %d\n", i, occ[i], quant_occ[i]);
    // }
    return quant_occ;
}

uint16_t *spread_fast16(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size) {
    int32_t xind = 0;
    size_t bitsize = 0;
    int spread_step = ((quant_size * 5 / 8) + 3);
    uint16_t *symbol = (uint16_t *)malloc(sizeof(uint16_t) * quant_size);


    for (size_t s = 0; s < alphabet_size; ++s) {
        for (uint32_t i = 0; i < quant_occ[s]; ++i) {
            symbol[xind] = s;
            xind = (xind + spread_step) % quant_size;
        }
    }
    return symbol;
}

uint8_t *spread_fast(int32_t *occ, int32_t *quant_occ, int alphabet_size, int quant_size) {
    int32_t xind = 0;
    size_t bitsize = 0;
    int spread_step = ((quant_size * 5 / 8) + 3);
    uint8_t *symbol = (uint8_t *)malloc(sizeof(uint8_t) * quant_size);


    for (size_t s = 0; s < alphabet_size; ++s) {
        for (uint32_t i = 0; i < quant_occ[s]; ++i) {
            symbol[xind] = s;
            xind = (xind + spread_step) % quant_size;
        }
    }
    return symbol;
}

uint32_t encode16(uint16_t *data, size_t dsize, vecStream *vs, int32_t *occ, int alphabet_size, int quant_pow) {
    int r_small = quant_pow + 1;
    int quant_size = 1 << quant_pow;
    int spread_step = ((quant_size * 5 / 8) + 3);
    int32_t *next = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *nb = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *start = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    uint8_t *kdiff = (uint8_t *)malloc(sizeof(uint8_t) * alphabet_size);
    uint32_t *encoding_table = (uint32_t *)malloc(sizeof(uint32_t) * quant_size);
    //printf("quantize:...\n");
    int32_t *quant_occ = quantize_unsorted_occurences_precise(occ, alphabet_size, quant_size);
    //printf("quantize done\n");
    uint8_t nbbits;
    size_t bitsize = 0;
    // spread
    uint16_t *symbol = spread_fast16(occ, quant_occ, alphabet_size, quant_size);
    // prepare
    int sumacc = 0;
    for (size_t s = 0; s < alphabet_size; ++s) {
        kdiff[s] = quant_pow - logfloor(quant_occ[s]);
        nb[s] = (kdiff[s] << r_small) - (quant_occ[s] << kdiff[s]);
        start[s] = -quant_occ[s] + sumacc;
        sumacc += quant_occ[s];
        next[s] = quant_occ[s];
    }
    int s;
    for (int x = quant_size; x < 2 * quant_size; ++x) {
        s = symbol[x - quant_size];
        encoding_table[start[s] + next[s]] = x;
        next[s]++;
    }
    uint32_t x = 1 + quant_size;

    for (size_t i = 0; i < dsize; ++i) {
        s = data[i];
        nbbits = (x + nb[s]) >> r_small;
        //printf("i=%lu, x=%d, nb[%d]=%d, (x + nb[s])=%d, nbbits=%d\n", i, x, s, nb[s], (x + nb[s]), (x + nb[s]) >> RSMALL);
        //printf("s = %d, x = %u, xx=%u, nbbits=%d\n", s, x, x - L, nbbits);
        // printf("writing %d bits into stream: ", nbbits);
        // print_uint32_t(x, nbbits);
        // printf("\n");
        write_bits_vecstream(vs, x, nbbits);
        bitsize += nbbits;
        x = encoding_table[start[s] + (x >> nbbits)];
    }
    // printf("total encoded bitsize=%lu\n", bitsize);
    vs->ms.total_bitsize = bitsize;
    free(next);
    free(nb);
    free(start);
    free(symbol);
    free(kdiff);
    free(encoding_table);
    free(quant_occ);

    return x;
}

uint16_t *decode16(size_t dsize, size_t bitsize, uint32_t x, memStream *bs, int32_t *occ, int alphabet_size, int quant_pow) {
    int r_small = quant_pow + 1;
    int quant_size = 1 << quant_pow;
    int spread_step = ((quant_size * 5 / 8) + 3);
    int32_t *next = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    tableEntry16 *decoding_table = (tableEntry16 *)malloc(sizeof(tableEntry16) * quant_size);
    uint16_t *data = (uint16_t *)malloc(sizeof(uint16_t) * dsize);
    //printf("quantize:...\n");
    int32_t *quant_occ = quantize_unsorted_occurences_precise(occ, alphabet_size, quant_size);
    //printf("quantize done\n");

    size_t di = 0;
    // spread
    uint16_t *symbol = spread_fast16(occ, quant_occ, alphabet_size, quant_size);

    // prepare
    for (size_t s = 0; s < alphabet_size; ++s) {
        next[s] = quant_occ[s];
    }
    for (size_t xd = 0; xd < quant_size; ++xd) {
        tableEntry16 t;
        t.symbol = symbol[xd];
        uint32_t xx = next[t.symbol]++;
        t.nbbits = quant_pow - logfloor(xx);
        t.new_x = (xx << t.nbbits) - quant_size;
        decoding_table[xd] = t;
    }
    // decode
    uint32_t xx = x - quant_size;
    tableEntry16 t;
    int32_t bss = (int32_t)bitsize;
    int32_t tbm = 0;
    int32_t kek_counter = 0;
    //printf("remaining bitsize:%7d\n", bss);
    //while (bss > 0) {
    for (int i = 0; i < dsize; ++i) {
        kek_counter++;
        t = decoding_table[xx];
        data[i] = t.symbol;
        if (t.nbbits > tbm) tbm = t.nbbits;
        xx = t.new_x + reverse_bits_uint32_t(read_bits_memstream(bs, t.nbbits), t.nbbits);
        bss -= t.nbbits;
        // printf("bss:%d, xx:%u, di=%zu, sym=%d\n", bss, xx, di, data[di-1]);
        // printf("remaining bitsize:%7d\n", bss);
    }
    assert(bss == 0);
    //printf("MAX t.nbbits=%d, kek_counter=%d\n", tbm, kek_counter);
    free(next);
    free(symbol);
    free(decoding_table);
    free(quant_occ);
    return data;
}

uint32_t encode(uint8_t *data, size_t dsize, vecStream *vs, int32_t *occ, int alphabet_size, int quant_pow) {
    int r_small = quant_pow + 1;
    int quant_size = 1 << quant_pow;
    int spread_step = ((quant_size * 5 / 8) + 3);
    int32_t *next = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *nb = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *start = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    uint8_t *kdiff = (uint8_t *)malloc(sizeof(uint8_t) * alphabet_size);
    uint32_t *encoding_table = (uint32_t *)malloc(sizeof(uint32_t) * quant_size);
    //printf("quantize:...\n");
    int32_t *quant_occ = quantize_occurences_precise(occ, alphabet_size, quant_size);
    //printf("quantize done\n");
    uint8_t nbbits;
    size_t bitsize = 0;
    // spread
    uint8_t *symbol = spread_fast(occ, quant_occ, alphabet_size, quant_size);
    // prepare
    int sumacc = 0;
    for (size_t s = 0; s < alphabet_size; ++s) {
        kdiff[s] = quant_pow - logfloor(quant_occ[s]);
        nb[s] = (kdiff[s] << r_small) - (quant_occ[s] << kdiff[s]);
        start[s] = -quant_occ[s] + sumacc;
        sumacc += quant_occ[s];
        next[s] = quant_occ[s];
    }
    int s;
    for (int x = quant_size; x < 2 * quant_size; ++x) {
        s = symbol[x - quant_size];
        encoding_table[start[s] + next[s]] = x;
        next[s]++;
    }
    uint32_t x = 1 + quant_size;

    for (size_t i = 0; i < dsize; ++i) {
        s = data[i];
        nbbits = (x + nb[s]) >> r_small;
        //printf("i=%lu, x=%d, nb[%d]=%d, (x + nb[s])=%d, nbbits=%d\n", i, x, s, nb[s], (x + nb[s]), (x + nb[s]) >> RSMALL);
        //printf("s = %d, x = %u, xx=%u, nbbits=%d\n", s, x, x - L, nbbits);
        // printf("writing %d bits into stream: ", nbbits);
        // print_uint32_t(x, nbbits);
        // printf("\n");
        write_bits_vecstream(vs, x, nbbits);
        bitsize += nbbits;
        x = encoding_table[start[s] + (x >> nbbits)];
    }
    // printf("total encoded bitsize=%lu\n", bitsize);
    vs->ms.total_bitsize = bitsize;
    free(next);
    free(nb);
    free(start);
    free(symbol);
    free(kdiff);
    free(encoding_table);
    free(quant_occ);

    return x;
}

uint8_t *decode(size_t dsize, size_t bitsize, uint32_t x, memStream *bs, int32_t *occ, int alphabet_size, int quant_pow) {
    int r_small = quant_pow + 1;
    int quant_size = 1 << quant_pow;
    int spread_step = ((quant_size * 5 / 8) + 3);
    int32_t *next = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    tableEntry *decoding_table = (tableEntry *)malloc(sizeof(tableEntry) * quant_size);
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * dsize);
    //printf("quantize:...\n");
    int32_t *quant_occ = quantize_occurences_precise(occ, alphabet_size, quant_size);
    //printf("quantize done\n");

    size_t di = 0;
    // spread
    uint8_t *symbol = spread_fast(occ, quant_occ, alphabet_size, quant_size);
    // prepare
    for (size_t s = 0; s < alphabet_size; ++s) {
        next[s] = quant_occ[s];
    }
    for (size_t xd = 0; xd < quant_size; ++xd) {
        tableEntry t;
        t.symbol = symbol[xd];
        uint32_t xx = next[t.symbol]++;
        t.nbbits = quant_pow - logfloor(xx);
        t.new_x = (xx << t.nbbits) - quant_size;
        decoding_table[xd] = t;
    }
    // decode
    uint32_t xx = x - quant_size;
    tableEntry t;
    int32_t bss = (int32_t)bitsize;
    int32_t tbm = 0;
    int32_t kek_counter = 0;
    //printf("remaining bitsize:%7d\n", bss);
    //while (bss > 0) {
    for (int i = 0; i < dsize; ++i) {
        kek_counter++;
        t = decoding_table[xx];
        data[i] = t.symbol;
        if (t.nbbits > tbm) tbm = t.nbbits;
        xx = t.new_x + reverse_bits_uint32_t(read_bits_memstream(bs, t.nbbits), t.nbbits);
        bss -= t.nbbits;
        //printf("bss:%d, xx:%u, di=%zu, sym=%d\n", bss, xx, di, data[di-1]);
        //printf("remaining bitsize:%7d\n", bss);
    }
    assert(bss == 0);
    //printf("MAX t.nbbits=%d, kek_counter=%d\n", tbm, kek_counter);
    free(next);
    free(symbol);
    free(decoding_table);
    free(quant_occ);
    return data;
}