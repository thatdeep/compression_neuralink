#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define ASIZE 256
#define R 12
#define RSMALL (R + 1)
#define L (1 << R)
#define SPREADSTEP ((L * 5 / 8) + 3)

typedef struct {
    uint32_t new_x;
    uint8_t symbol;
    uint8_t nbbits;
} tableEntry;

static uint32_t occ[ASIZE];
static uint32_t next[ASIZE];
static uint8_t symbol[L];
static uint8_t kdiff[ASIZE];
static uint8_t nb[ASIZE];
static int32_t start[ASIZE];
static uint32_t encoding_table[L];
static tableEntry decoding_table[L];


typedef struct {
    uint64_t bit_buffer;
    size_t bits_in_buffer;
    size_t current_bytesize;
    size_t total_bitsize;
    FILE *stream;
} bitStream;

void initialize_bitstream(bitStream *bs, FILE *stream, size_t total_bitsize) {
    bs->bit_buffer = 0;
    bs->bits_in_buffer = 0;
    bs->current_bytesize = 0;
    bs->total_bitsize = total_bitsize;
    bs->stream = stream;
}

void flush_buffer_bitstream(bitStream *bs) {
    while (bs->bits_in_buffer >= 8) {
        uint8_t byte = bs->bit_buffer & 0xFF;
        fputc(byte, bs->stream);
        bs->bit_buffer >>= 8;
        bs->bits_in_buffer -= 8;
        bs->current_bytesize++;
    }
}

void write_bits_bitstream(bitStream *bs, uint32_t bits, int n) {
    if (bs->bits_in_buffer + n > 64) {
        flush_buffer_bitstream(bs);
    }
    // CHECK IF BITS NEED TO BE TRIMMED VIA MASK TO EXACT N BITS
    bs->bit_buffer |= ((uint64_t)bits << bs->bits_in_buffer);
    bs->bits_in_buffer += n;
}

void finalize_bitstream(bitStream *bs) {
    flush_buffer_bitstream(bs);
    if (bs->bits_in_buffer > 0) {
        uint8_t final_byte = bs->bit_buffer & ((1 << bs->bits_in_buffer) - 1);
        fputc(final_byte, bs->stream);
        bs->current_bytesize++;
    }
    fflush(bs->stream);
}


void fill_buffer_bitstream(bitStream *bs) {
    size_t total_bytesize = (bs->total_bitsize % 8 == 0) ? (bs->total_bitsize / 8) : (bs->total_bitsize / 8 + 1);
    while ((bs->bits_in_buffer <= 56) && (bs->current_bytesize < total_bytesize)) {
        uint8_t next_byte = fgetc(bs->stream);
        if (next_byte == (uint8_t)EOF) break;
        bs->bit_buffer |= (uint64_t)next_byte << bs->bits_in_buffer;
        bs->bits_in_buffer += 8;
        bs->current_bytesize++;
    }
}

uint32_t read_bits_bitstream(bitStream *bs, int n) {
    if (bs->bits_in_buffer < n) fill_buffer_bitstream(bs);
    uint32_t result = bs->bit_buffer & ((1 << n) - 1);
    bs->bit_buffer >>= n;
    bs->bits_in_buffer -= n;
    return result;
}


typedef struct {
    uint64_t bit_buffer;
    size_t bits_in_buffer;
    size_t current_bytesize;
    size_t total_bitsize;
    uint8_t *stream;
} memStream;


void initialize_memstream(memStream *ms, uint8_t *stream, size_t total_bitsize) {
    ms->bit_buffer = 0;
    ms->bits_in_buffer = 0;
    ms->current_bytesize = 0;
    ms->total_bitsize = total_bitsize;
    ms->stream = stream;
}

void flush_buffer_memstream(memStream *ms) {
    while (ms->bits_in_buffer >= 8) {
        ms->stream[ms->current_bytesize++] = (uint8_t)(ms->bit_buffer & 0xFF);
        ms->bit_buffer >>= 8;
        ms->bits_in_buffer -= 8;
    }
}

void write_bits_memstream(memStream *ms, uint32_t bits, int n) {
    if (ms->bits_in_buffer + n > 64) {
        flush_buffer_memstream(ms);
    }
    // CHECK IF BITS NEED TO BE TRIMMED VIA MASK TO EXACT N BITS
    ms->bit_buffer |= ((uint64_t)bits << ms->bits_in_buffer);
    ms->bits_in_buffer += n;
}

void finalize_memstream(memStream *ms) {
    flush_buffer_memstream(ms);
    if (ms->bits_in_buffer > 0) {
        ms->stream[ms->current_bytesize++] = (uint8_t)(ms->bit_buffer & ((1 << ms->bits_in_buffer) - 1));
    }
}


void reverse_bits_memstream(memStream *ms) {
    // read_bits  [0...0{read_bits}{old_bits}]<-[to_be_read]<-[...]<-[...]<-...<-[...]
    // write_bits [0...0{new_bits}{old_bits}] => [...]->[...]->...->[...], no reverse of bits here
    // so byteas are reversed already, just
    // have         : [0:xyzuv012][1:01234567][2:01234567]...[8:01234567][9:01234567]
    // revert bytes : [9:01234567][8:01234567]...[2:01234567][1:01234567][0:012xyzuv]
    // revert bits  : [9:76543210][8:76543210]...[2:76543210][1:76543210][0:vuzyx210]
    // shift garbage: [9:21076543][8:21076543]...[2:21076543][1:21076543][0:21000000]
    uint8_t temp;
    for (size_t i = 0; i < ms->current_bytesize / 2; ++i) {
        temp = ms->stream[i];
        ms->stream[i] = ms->stream[ms->current_bytesize - i - 1];
        ms->stream[ms->current_bytesize - i - 1] = temp;
    }
    for (size_t i = 0; i < ms->current_bytesize; ++i) {
        temp = ms->stream[i];
        temp = (temp & 0xF0) >> 4 | (temp & 0x0F) << 4;
        temp = (temp & 0xCC) >> 2 | (temp & 0x33) << 2;
        temp = (temp & 0xAA) >> 1 | (temp & 0x55) << 1;
        ms->stream[i] = temp;
    }
    if (ms->total_bitsize % 8 != 0) {
        uint8_t shift_size = 8 - (ms->total_bitsize % 8);
        for (size_t i = 1; i < ms->current_bytesize - 1; ++i) {
            ms->stream[i - 1] <<= shift_size;
            ms->stream[i - 1] |= (ms->stream[i] >> (8 - shift_size));
        }
        ms->stream[ms->current_bytesize - 1] <<= shift_size;
    }
/*
    0xFF
    0b11111111

    0xF0
    0b11110000

    0x0F
    0b00001111

    0xCC
    0b11001100

    0x33
    0b00110011

    0xAA
    0b10101010

    0x55
    0b01010101

    bi01234567
    bi45670123
    bi67452301
    bi76543210

    0 0000
    1 0001
    2 0010
    3 0011
    4 0100
    5 0101
    6 0110
    7 0111
    8 1000
    9 1001
    A 1010
    B 1011
    C 1100
    D 1101
    E 1110
    F 1111
*/

}


void fill_buffer_memstream(memStream *ms) {
    size_t total_bytesize = (ms->total_bitsize % 8 == 0) ? (ms->total_bitsize / 8) : (ms->total_bitsize / 8 + 1);
    while ((ms->bits_in_buffer <= 56) && (ms->current_bytesize < total_bytesize)) {
        uint8_t next_byte = ms->stream[ms->current_bytesize++];
        ms->bit_buffer |= (uint64_t)next_byte << ms->bits_in_buffer;
        ms->bits_in_buffer += 8;
    }
}

uint32_t read_bits_memstream(memStream *ms, int n) {
    if (ms->bits_in_buffer < n) fill_buffer_memstream(ms);
    uint32_t result = ms->bit_buffer & ((1 << n) - 1);
    ms->bit_buffer >>= n;
    ms->bits_in_buffer -= n;
    return result;
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
    return powerOfTwoExponent(nextPowerOf2(x) >> 1);
}


size_t encode(uint8_t *data, size_t dsize) {
    uint8_t nbbits;
    // spread
    uint32_t xind = 0;
    size_t bitsize = 0;
    for (size_t s = 0; s < ASIZE; ++s) {
        for (uint32_t i = 0; i < occ[s]; ++i) {
            symbol[xind] = s;
            xind = (xind + SPREADSTEP) % L;
        }
    }

    // prepare
    int sumacc = 0;
    for (size_t s = 0; s < ASIZE; ++s) {
        kdiff[s] = R - logfloor(occ[s]);
        nb[s] = (kdiff[s] << RSMALL) - (occ[s] << kdiff[s]);
        sumacc -= occ[s];
        start[s] = sumacc;
        next[s] = occ[s];
    }
    size_t s;
    for (size_t x = L; x < 2 * L; ++x) {
        s = symbol[x - L];
        encoding_table[start[s] + (next[s]++)] = x;
    }

    // encode
    uint32_t x = 1 + L;

    for (size_t i = 0; i < dsize; ++i) {
        s = data[i];
        nbbits = (x + nb[s]) >> RSMALL;
        // REPLACE WRITEBITS WITH ACTUAL BIT WRITING FUNCTIONALITY
        //write_bits(x, nbbits);
        bitsize += nbbits;
        x = encoding_table[start[s] + (x >> nbbits)];
    }
    return bitsize;
}


uint8_t *decode(size_t dsize, size_t bitsize) {
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * dsize);
    size_t di = 1;
    // spread
    uint32_t xind = 0;
    for (size_t s = 0; s < ASIZE; ++s) {
        for (uint32_t i = 0; i < occ[s]; ++i) {
            symbol[xind] = s;
            xind = (xind + SPREADSTEP) % L;
        }
    }
    // prepare
    for (size_t s = 0; s < ASIZE; ++s) {
        next[s] = occ[s];
    }
    for (size_t x = 0; x < L; ++x) {
        tableEntry t;
        t.symbol = symbol[x];
        uint32_t xx = next[t.symbol]++;
        t.nbbits = R - logfloor(xx);
        t.new_x = (xx << t.nbbits) - L;
        decoding_table[x] = t;
    }

    uint32_t x, xx;

    // TODO read initial state X (read16bits and probably 8 more until we get x from L to 2L - 1?)
    // or just save number of bits to read
    xx = x - L;

    // decode
    tableEntry t;
    while (bitsize) {
        t = decoding_table[xx];
        //useSymbol(t.symbol);
        data[di++] = t.symbol;
        // REPLACE READBITS WITH ACTUAL BIT READING FUNCTIONALITY
        //xx = t.new_x + readBits(t.nbbits);
        bitsize -= t.nbbits;
    }
    return data;
}

#define N 15

int main(int argc, char **argv) {
    int bitsizes[N] = {1, 3, 7, 15, 2, 37, 8, 2, 3, 5, 29, 15, 4, 3, 1};
    uint32_t bits[N] = {0xF, 0x2, 0x4, 0xA, 0x1, 0xFA, 0x6, 0x1, 0x3, 0x5, 0x1C, 0xB, 0x2, 0x1, 0x0};
    uint32_t rbits[N] = {0};
    for (int i = 0; i < N; ++i) {
        bits[i] &= ((1 << bitsizes[i]) - 1);
    }
    size_t streamsize = N;
    char filename[] = "bitstream_testfile.bitstream";
    FILE *file = fopen(filename, "wb");
    size_t total_bytesize = 0, total_bitsize = 0;
    for (int i = 0; i < N; ++i) {
        total_bitsize += bitsizes[i];
    }
    fwrite(&total_bitsize, sizeof(size_t), 1, file);
    total_bytesize = (total_bitsize % 8 == 0) ? (total_bitsize / 8) : (total_bitsize / 8 + 1);
    bitStream writing_stream = (bitStream){.bit_buffer = 0, .bits_in_buffer = 0, .current_bytesize = 0, .total_bitsize = total_bitsize, .stream=file};
    for (int i = 0; i < N; ++i) {
        write_bits_bitstream(&writing_stream, bits[i], bitsizes[i]);
    }
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.current_bytesize, total_bytesize);
    finalize_bitstream(&writing_stream);
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.current_bytesize, total_bytesize);
    file = writing_stream.stream;
    fclose(file);

    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    bitStream reading_stream = (bitStream){.bit_buffer = 0, .bits_in_buffer = 0, .current_bytesize = 0, .total_bitsize = read_total_bitsize, .stream=file};

    // for (int i = 0; i < N; ++i) {
    //     rbits[i] = read_bits_bitstream(&reading_stream, bitsizes[i]);
    // }
    // what do we need here?
    // have         : [0:01234567][1:01234567][2:01234567]...[8:01234567][9:012xyzuv]
    // revert bytes : [9:012xyzuv][8:01234567]...[2:01234567][1:01234567][0:01234567]
    // revert bits  : [9:vuzyx210][8:76543210]...[2:76543210][1:76543210][0:76543210]
    // shift garbage: [9:21076543][8:21076543]...[2:21076543][1:21076543][0:21000000]
    // write to file


    printf("r| current_bytesize: %lu, total_bytesize: %lu\n", reading_stream.current_bytesize, read_total_bytesize);
    for (int i = 0; i < N; ++i) {
        printf("%u --- %u\n", bits[i], rbits[i]);
    }
    return 0;
}













