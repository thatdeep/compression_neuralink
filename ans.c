#include "ans.h"

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

int32_t *quantize_occurences(int32_t *occ, int alphabet_size, int quant_pow) {
    int quant_size = 1 << quant_pow;
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
    while (difference > 0) {
        for (i = 0; i < alphabet_size && difference > 0; i++) {
            if (quant_occ[i] < (int)((float)occ[i] / total * quant_size + 0.5)) {
                quant_occ[i]++;
                difference--;
            }
        }
    }
    while (difference < 0) {
        for (i = 0; i < alphabet_size && difference < 0; i++) {
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

uint32_t encode(uint8_t *data, size_t dsize, vecStream *vs, int32_t *occ, int alphabet_size, int quant_pow) {
    int r_small = quant_pow + 1;
    int quant_size = 1 << quant_pow;
    int spread_step = ((quant_size * 5 / 8) + 3);
    int32_t *next = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *nb = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t *start = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    uint8_t *kdiff = (uint8_t *)malloc(sizeof(uint8_t) * alphabet_size);
    uint32_t *encoding_table = (uint32_t *)malloc(sizeof(uint32_t) * quant_size);
    int32_t *quant_occ = quantize_occurences(occ, alphabet_size, quant_pow);

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
    int32_t *quant_occ = quantize_occurences(occ, alphabet_size, quant_pow);

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