#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define ASIZE 256
#define R 12
#define RSMALL (R + 1)
#define L (1 << R)
#define SPREADSTEP ((L * 5 / 8) + 3)
#define BUFSIZE 1000000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

typedef struct {
    uint32_t new_x;
    uint8_t symbol;
    uint8_t nbbits;
} tableEntry;

static int32_t occ[ASIZE];
static int32_t quant[ASIZE];
static int32_t next[ASIZE];
static uint8_t symbol[L];
static uint8_t kdiff[ASIZE];
static int32_t nb[ASIZE];
static int32_t start[ASIZE];
static uint32_t encoding_table[L];
static tableEntry decoding_table[L];
static uint8_t memory_buffer[BUFSIZE];


typedef struct {
    uint64_t bit_buffer;
    size_t bits_in_buffer;
    size_t current_bytesize;
    size_t total_bitsize;
    FILE *stream;
} bitStream;


void print_uint64_t(uint64_t bbuf, int nbits) {
    for (int i = nbits - 1; i >= 0; --i) {
        printf("%d", ((bbuf & ((uint64_t)1 << i)) == ((uint64_t)1 << i)) ? 1 : 0);
    }
}

void print_uint32_t(uint32_t bbuf, int nbits) {
    for (int i = nbits - 1; i >= 0; --i) {
        printf("%d", ((bbuf & ((uint32_t)1 << i)) == ((uint32_t)1 << i)) ? 1 : 0);
    }
}

void print_uint8_t(uint8_t bbuf, int nbits) {
    for (int i = nbits - 1; i >= 0; --i) {
        printf("%d", ((bbuf & (1 << i)) == (1 << i)) ? 1 : 0);
    }
}


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


// bits input:  last bits -> [{zero_padding}{higher}...{lower}] <- first bits
void write_bits_bitstream(bitStream *bs, uint32_t bits, int n) {
    if (bs->bits_in_buffer + n > 64) {
        flush_buffer_bitstream(bs);
    }
    // CHECK IF BITS NEED TO BE TRIMMED VIA MASK TO EXACT N BITS
    bits = (bits & (((uint32_t)1 << n) - 1));
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
    // printf("inside fill_buffer_bitstream:\n");
    // printf("bit_buffer:\n");
    // print_uint64_t(bs->bit_buffer, 64);
    // printf("\n");
    size_t total_bytesize = (bs->total_bitsize % 8 == 0) ? (bs->total_bitsize / 8) : (bs->total_bitsize / 8 + 1);
    while ((bs->bits_in_buffer <= 56) && (bs->current_bytesize < total_bytesize)) {
        uint64_t next_uint64_t = 0;
        uint8_t next_byte = fgetc(bs->stream);
        next_uint64_t |= (next_byte & 0xFF);
        //if (next_byte == (uint8_t)EOF) break;
        // printf("wtf next byte uint64?\n");
        // printf("%llu\n", next_uint64_t);
        // print_uint64_t(next_uint64_t, 64);
        // printf("\n");
        // print_uint64_t((next_uint64_t << bs->bits_in_buffer), 64);
        // printf("\n");
        bs->bit_buffer |= ((uint64_t)next_byte << bs->bits_in_buffer);
        bs->bits_in_buffer += 8;
        bs->current_bytesize++;
        // printf("after reading byte:\n");
        // printf("bits in buffer: %lu\n", bs->bits_in_buffer);
        // printf("byte:\n");
        // print_uint8_t(next_byte, 8);
        // printf("\n");
        // printf("bit_buffer:\n");
        // print_uint64_t(bs->bit_buffer, 64);
        // printf("\n");
    }
    // printf("exiting fill_buffer_bitstream\n");
}

uint32_t read_bits_bitstream(bitStream *bs, int n) {
    // printf("inside read_bits_bitstream\n");
    // printf("bit_buffer:\n");
    // print_uint64_t(bs->bit_buffer, 64);
    // printf("\n");
    if (bs->bits_in_buffer < n) fill_buffer_bitstream(bs);
    uint32_t result = (uint32_t)(bs->bit_buffer & (uint64_t)((1 << n) - 1));
    bs->bit_buffer >>= n;
    bs->bits_in_buffer -= n;
    // printf("bit_buffer:\n");
    // print_uint64_t(bs->bit_buffer, 64);
    // printf("\n");
    // printf("exiting read_bits_bitstream\n");
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

// bits input:  last bits -> [{zero_padding}{higher}...{lower}] <- first bits
void write_bits_memstream(memStream *ms, uint32_t bits, int n) {
    if (ms->bits_in_buffer + n > 64) {
        flush_buffer_memstream(ms);
    }
    // CHECK IF BITS NEED TO BE TRIMMED VIA MASK TO EXACT N BITS
    bits = (bits & (((uint32_t)1 << n) - 1));
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
    // have         : [0:7|0:6|0:5|0:4|0:3|0:2|0:1|0:0][1:7|1:6|1:5|1:4|1:3|1:2|1:1|1:0]...[n:v|n:u|n:z|n:y|n:x|n:2|n:1|n:0]
    // revert bytes : [n:v|n:u|n:z|n:y|n:x|n:2|n:1|n:0]...[1:7|1:6|1:5|1:4|1:3|1:2|1:1|1:0][0:7|0:6|0:5|0:4|0:3|0:2|0:1|0:0]
    // revert bits  : [n:0|n:1|n:2|n:x|n:y|n:z|n:u|n:v]...[1:0|1:1|1:2|1:3|1:4|1:5|1:6|1:7][0:0|0:1|0:2|0:3|0:4|0:5|0:6|0:7]
    // PROBABLY WONT NEED DUE TO BEING ABLE TO READ OUT FIRST GARBAGE BITS ON DECODING STAGE.
    // shift is performed by garbage size to the right, then {or} with high part of next byte (m=n-1, *=zero_padding)
    // shift garbage: [m:3|m:4|m:5|m:6|m:7|n:0|n:1|n:2]...[0:3|0:4|0:5|0:6|0:7|1:0|1:1|1:2][-:*|-:*|-:*|-:*|-:*|0:0|0:1|0:2]
    // now bits reads as:
    // n:2->n:1->n:0->m:7->m:6->...->m:1->m:0->...->1:7->1:6->...->1:2->1:1->1:0->0:7->0:6->...->0:3->0:2->0:1->0:0
    // revert bytes
    uint8_t temp;
    for (size_t i = 0; i < ms->current_bytesize / 2; ++i) {
        temp = ms->stream[i];
        ms->stream[i] = ms->stream[ms->current_bytesize - i - 1];
        ms->stream[ms->current_bytesize - i - 1] = temp;
    }
    // revert bits
    for (size_t i = 0; i < ms->current_bytesize; ++i) {
        temp = ms->stream[i];
        temp = (temp & 0xF0) >> 4 | (temp & 0x0F) << 4;
        temp = (temp & 0xCC) >> 2 | (temp & 0x33) << 2;
        temp = (temp & 0xAA) >> 1 | (temp & 0x55) << 1;
        ms->stream[i] = temp;
    }
    // shift garbage (DO WE NEED THIS? Let me explain, we know that garbage bits are simply first k bits, just read them as usual, then
    // read useful bits)
    // see, we write bitsize so amount of garbage bits should be known when reading starts.
    // if (ms->total_bitsize % 8 != 0) {
    //     uint8_t shift_size = 8 - (ms->total_bitsize % 8);
    //     for (size_t i = 1; i < ms->current_bytesize - 1; ++i) {
    //         ms->stream[i - 1] <<= shift_size;
    //         ms->stream[i - 1] |= (ms->stream[i] >> (8 - shift_size));
    //     }
    //     ms->stream[ms->current_bytesize - 1] <<= shift_size;
    // }
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
    return;
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
    uint32_t np2x = nextPowerOf2(x);
    if (x == np2x) {
        return powerOfTwoExponent(x);
    }
    return powerOfTwoExponent(np2x >> 1);
}


uint32_t encode(uint8_t *data, size_t dsize, memStream *ms) {
    uint8_t nbbits;
    // spread
    printf("encode | spreading...\n");
    int32_t xind = 0;
    size_t bitsize = 0;
    for (size_t s = 0; s < ASIZE; ++s) {
        for (uint32_t i = 0; i < occ[s]; ++i) {
            symbol[xind] = s;
            xind = (xind + SPREADSTEP) % L;
        }
    }
    printf("encode | spreading done.\n");
    printf("encode | preparing...\n");

    // prepare
    int sumacc = 0;
    for (size_t s = 0; s < ASIZE; ++s) {
        kdiff[s] = R - logfloor(occ[s]);
        nb[s] = (kdiff[s] << RSMALL) - (occ[s] << kdiff[s]);
        sumacc += occ[s];
        start[s] = -occ[s] + sumacc;
        next[s] = occ[s];
        printf("ggg: kdiff[s] << RSMALL: %d, occ[s] << kdiff[s]: %d\n", (kdiff[s] << RSMALL), (occ[s] << kdiff[s]));
        printf("s=%lu, occ[%lu]=%d, lf(occ[s])=%d, R-lf(occ[s])=%d, kdiff[%lu]=%d, nb[%lu]=%d, sumacc=%d, start[%lu]=%d, next[%lu]=%d\n", s, s, occ[s], logfloor(occ[s]), R - logfloor(occ[s]),  s, kdiff[s], s, nb[s], sumacc, s, start[s], s, next[s]);
    }
    int s;
    printf("symbols:\n");
    for (int i = 0; i < L; ++i) {
        printf("%3d, ", symbol[i]);
        if ((i + 1) % 32 == 0) {
            printf("\n");
        }
    }
    printf("\n");
    printf("encode | preparing encoding table...\n");
    printf("symbol[0]: %d\n", symbol[0]);
    printf("start[0]=%d, next[0]=%d, start[0]+next[0]=%d\n", start[0], next[0], start[0] + next[0]);
    printf("L=%d\n", L);
    int ll = L;
    for (int x = ll; x < 2 * ll; ++x) {
        printf("x=%d, ", x);
        s = symbol[x - ll];
        printf("s=%d, ", s);
        printf(", start[%d]=%d, next[%d]=%d, encoding table index: %u\n",s, start[s], s, next[s], start[s] + (next[s]));
        encoding_table[start[s] + next[s]] = x;
        next[s]++;
    }
    printf("encode | preparing done.\n");
    printf("encode | encoding...\n");

    // encode
    uint32_t x = 1 + L;

    for (size_t i = 0; i < dsize; ++i) {
        s = data[i];
        nbbits = (x + nb[s]) >> RSMALL;
        printf("i=%d, x=%d, nb[%d]=%d, (x + nb[s])=%d, nbbits=%d\n", i, x, s, nb[s], (x + nb[s]), (x + nb[s]) >> RSMALL);
        // REPLACE WRITEBITS WITH ACTUAL BIT WRITING FUNCTIONALITY
        write_bits_memstream(ms, x, nbbits);
        bitsize += nbbits;
        x = encoding_table[start[s] + (x >> nbbits)];
    }
    printf("total_bitsize=%lu\n", bitsize);
    ms->total_bitsize = bitsize;
    printf("encode | encoding done\n");

    return x;
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


uint8_t *decode(size_t dsize, size_t bitsize, uint32_t x, bitStream *bs) {
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * dsize);
    size_t di = 0;
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

    uint32_t xx = x - L;

    // decode
    tableEntry t;
    int32_t bss = (int32_t)bitsize;
    printf("bitsize:%d\n", bss);
    while (bss > 0) {
        printf("bitsize:%d\n", bss);
        t = decoding_table[xx];
        //useSymbol(t.symbol);
        data[di++] = t.symbol;
        // REPLACE READBITS WITH ACTUAL BIT READING FUNCTIONALITY
        //xx = t.new_x + readBits(t.nbbits);
        xx = t.new_x + reverse_bits_uint32_t(read_bits_bitstream(bs, t.nbbits), t.nbbits);
        bss -= t.nbbits;
    }
    return data;
}

void quantize_occurences() {
    int total = 0;
    int quant_total = 0;
    int i, difference;

    for (int i = 0; i < ASIZE; ++i) {
        total += occ[i];
    }

    // Calculate quantized values
    for (i = 0; i < ASIZE; i++) {
        float probability = (float)occ[i] / total;
        quant[i] = (int)(probability * L + 0.5); // Round to nearest integer
        if (quant[i] == 0) quant[i] = 1;
        printf("prob[%d]=%f, quant[%d]=%d\n", i, probability, i, quant[i]);
        quant_total += quant[i];
    }

    // Adjust quant values to sum exactly to QUANT_SIZE
    difference = L - quant_total;
    printf("quant_total=%d, difference=%d\n", quant_total, difference);

    while (difference > 0) {
        for (i = 0; i < ASIZE && difference > 0; i++) {
            if (quant[i] < (int)((float)occ[i] / total * L + 0.5)) {
                quant[i]++;
                difference--;
            }
        }
    }
    printf("kek\n");

    while (difference < 0) {
        for (i = 0; i < ASIZE && difference < 0; i++) {
            if (quant[i] > 1) {
                quant[i]--;
                difference++;
            }
        }
    }

    int quant_sum = 0;
    for (int i = 0; i < ASIZE; ++i) {
        quant_sum += quant[i];
    }
    printf("quant_sum: %d\n", quant_sum);

    // Output results
    for (i = 0; i < ASIZE; i++) {
        occ[i] = quant[i];
        printf("Symbol %d: Quantized value = %d\n", i, quant[i]);
    }


    return;
}

#define N 15

void test_write_read_streams() {
    int bitsizes[N] = {1, 3, 7, 15, 2, 30, 8, 2, 3, 5, 29, 15, 4, 3, 2};
    uint32_t bits[N] = {0xF, 0x2, 0x4, 0xA, 0x1, 0xFFA, 0x6, 0x1, 0x3, 0x5, 0x1C, 0xB, 0x2, 0x1, 0x3};
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
    memStream writing_stream = (memStream){.bit_buffer = 0, .bits_in_buffer = 0, .current_bytesize = 0, .total_bitsize = total_bitsize, .stream=memory_buffer};
    for (int i = 0; i < N; ++i) {
        write_bits_memstream(&writing_stream, bits[i], bitsizes[i]);
    }
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.current_bytesize, total_bytesize);
    finalize_memstream(&writing_stream);
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.current_bytesize, total_bytesize);
    printf("printing writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(writing_stream.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");
    reverse_bits_memstream(&writing_stream);
    printf("printing reversed writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(writing_stream.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");

    fwrite(writing_stream.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);

    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    bitStream reading_stream = (bitStream){.bit_buffer = 0, .bits_in_buffer = 0, .current_bytesize = 0, .total_bitsize = read_total_bitsize, .stream=file};

    int i = 0;
    size_t garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        //print_uint64_t(reading_stream.bit_buffer);
        printf("reading %lu garbage bits:\n", garbage_bitsize);
        read_bits_bitstream(&reading_stream, garbage_bitsize);
        //print_uint64_t(reading_stream.bit_buffer);
    }
    while (read_total_bitsize) {
        //print_uint64_t(reading_stream.bit_buffer);
        rbits[i] = read_bits_bitstream(&reading_stream, bitsizes[N - i - 1]);
        printf("reading %d bits at reversed index %d\n", bitsizes[N - i - 1], i);
        read_total_bitsize -= bitsizes[N - i - 1];
        i++;
        //print_uint64_t(reading_stream.bit_buffer);
    }

    printf("r| current_bytesize: %lu, total_bytesize: %lu\n", reading_stream.current_bytesize, read_total_bytesize);
    for (int i = 0; i < N; ++i) {
        printf("i = %d, b:\n", i);
        print_uint32_t(bits[i], bitsizes[i]);
        printf("\nrrb:\n");
        print_uint32_t(rbits[N - i - 1], bitsizes[i]);
        //printf("b:%u --- rrb:%u\n", bits[i], rbits[N - i - 1]);
        printf("\n");
    }
    fclose(file);
    return;
}

int main(int argc, char **argv) {
    test_write_read_streams();
    uint8_t alphabet[ASIZE] = {0};
    for (int i = 0; i < ASIZE; ++i) {
        alphabet[i] = (uint8_t)i;
    }
    for (int i = 0; i < ASIZE; ++i) {
        occ[i] = (ASIZE - i);
    }
    quantize_occurences();
    uint8_t text[20] = {1, 2, 5, 10, 0, 0, 2, 2, 5, 5, 10, 10, 128, 3, 4, 7, 4, 4, 4, 2};
    size_t dsize = 20;
    memStream writing_stream = (memStream){.bit_buffer = 0, .bits_in_buffer = 0, .current_bytesize = 0, .total_bitsize = 0, .stream=memory_buffer};
    uint32_t final_state = encode(text, dsize, &writing_stream);
    printf("final state: %u\n", final_state);
    finalize_memstream(&writing_stream);
    printf("memstream finalized.\n");
    reverse_bits_memstream(&writing_stream);
    size_t total_bytesize = (writing_stream.total_bitsize % 8 == 0) ? (writing_stream.total_bitsize / 8) : (writing_stream.total_bitsize / 8 + 1);
    char filename[] = "bitstream_testfile.bitstream";
    FILE *file = fopen(filename, "wb");
    fwrite(&dsize, sizeof(size_t), 1, file);
    fwrite(&(writing_stream.total_bitsize), sizeof(size_t), 1, file);
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(writing_stream.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);

    size_t recovered_text_size = 0;
    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    uint32_t decode_initial_state;
    fread(&recovered_text_size, sizeof(size_t), 1, file);
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    fread(&decode_initial_state, sizeof(uint32_t), 1, file);
    bitStream reading_stream = (bitStream){.bit_buffer = 0, .bits_in_buffer = 0, .current_bytesize = 0, .total_bitsize = read_total_bitsize, .stream = file};
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    size_t garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        printf("read %d bits of garbage:\n", garbage_bitsize);
        read_bits_bitstream(&reading_stream, garbage_bitsize);
    }
    uint8_t *recovered_text = decode(recovered_text_size, read_total_bitsize, decode_initial_state, &reading_stream);
    for (int i = 0; i < recovered_text_size; ++i) {
        printf("%d --- %d\n", text[i], recovered_text[recovered_text_size - i - 1]);
    }
    free(recovered_text);
    return 0;
}













