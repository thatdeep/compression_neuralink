#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>


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

void fill_buffer_bitstream(bitStream *bs) {
    size_t total_bytesize = (bs->total_bitsize % 8 == 0) ? (bs->total_bitsize / 8) : (bs->total_bitsize / 8 + 1);
    uint8_t next_byte;
    while ((bs->bits_in_buffer <= 56) && (bs->current_bytesize < total_bytesize)) {
        //uint8_t next_byte = fgetc(bs->stream);
        int xd = fread(&next_byte, sizeof(uint8_t), 1, bs->stream);
        if (xd != 1) printf("WTF xd=%d\n", xd);
        bs->bit_buffer |= ((uint64_t)next_byte << bs->bits_in_buffer);
        bs->bits_in_buffer += 8;
        bs->current_bytesize++;
    }
}

uint32_t read_bits_bitstream(bitStream *bs, int n) {
    if (bs->bits_in_buffer < n) fill_buffer_bitstream(bs);
    uint32_t result = (uint32_t)(bs->bit_buffer & (uint64_t)((1 << n) - 1));
    bs->bit_buffer >>= n;
    bs->bits_in_buffer -= n;
    return result;
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

void flush_buffer_vecstream(vecStream *vs) {
    while (vs->ms.bits_in_buffer >= 8) {
        if (vs->ms.current_bytesize + 1 >= vs->stream_capacity) {
            vs->stream_capacity = (vs->stream_capacity * 3) / 2;
            uint8_t *new_stream = (uint8_t *)malloc(sizeof(uint8_t) * vs->stream_capacity);
            memcpy(new_stream, vs->ms.stream, vs->ms.current_bytesize);
            free(vs->ms.stream);
            vs->ms.stream = new_stream;
        }
        vs->ms.stream[vs->ms.current_bytesize++] = (uint8_t)(vs->ms.bit_buffer & 0xFF);
        vs->ms.bit_buffer >>= 8;
        vs->ms.bits_in_buffer -= 8;
    }
}

// bits input:  last bits -> [{zero_padding}{higher}...{lower}] <- first bits
void write_bits_vecstream(vecStream *vs, uint32_t bits, int n) {
    if (vs->ms.bits_in_buffer + n > 64) {
        flush_buffer_vecstream(vs);
    }
    bits = (bits & (((uint32_t)1 << n) - 1));
    vs->ms.bit_buffer |= ((uint64_t)bits << vs->ms.bits_in_buffer);
    vs->ms.bits_in_buffer += n;
}

void finalize_vecstream(vecStream *vs) {
    flush_buffer_vecstream(vs); // specifically guarantees that last byte will be allocated
    if (vs->ms.bits_in_buffer > 0) {
        vs->ms.stream[vs->ms.current_bytesize++] = (uint8_t)(vs->ms.bit_buffer & ((1 << vs->ms.bits_in_buffer) - 1));
    }
}


void reverse_bits_memstream(memStream *ms) {
    // have         : [0:7|0:6|0:5|0:4|0:3|0:2|0:1|0:0][1:7|1:6|1:5|1:4|1:3|1:2|1:1|1:0]...[n:v|n:u|n:z|n:y|n:x|n:2|n:1|n:0]
    // revert bytes : [n:v|n:u|n:z|n:y|n:x|n:2|n:1|n:0]...[1:7|1:6|1:5|1:4|1:3|1:2|1:1|1:0][0:7|0:6|0:5|0:4|0:3|0:2|0:1|0:0]
    // revert bits  : [n:0|n:1|n:2|n:x|n:y|n:z|n:u|n:v]...[1:0|1:1|1:2|1:3|1:4|1:5|1:6|1:7][0:0|0:1|0:2|0:3|0:4|0:5|0:6|0:7]
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
    return;
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

uint32_t encode(uint8_t *data, size_t dsize, vecStream *vs) {
    uint8_t nbbits;
    // spread
    int32_t xind = 0;
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
        sumacc += occ[s];
        start[s] = -occ[s] + sumacc;
        next[s] = occ[s];
    }
    int s;
    for (int x = L; x < 2 * L; ++x) {
        s = symbol[x - L];
        encoding_table[start[s] + next[s]++] = x;
    }
    uint32_t x = 1 + L;

    for (size_t i = 0; i < dsize; ++i) {
        s = data[i];
        nbbits = (x + nb[s]) >> RSMALL;
        //printf("i=%lu, x=%d, nb[%d]=%d, (x + nb[s])=%d, nbbits=%d\n", i, x, s, nb[s], (x + nb[s]), (x + nb[s]) >> RSMALL);
        write_bits_vecstream(vs, x, nbbits);
        bitsize += nbbits;
        x = encoding_table[start[s] + (x >> nbbits)];
    }
    printf("total encoded bitsize=%lu\n", bitsize);
    vs->ms.total_bitsize = bitsize;
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
    // decode
    uint32_t xx = x - L;
    tableEntry t;
    int32_t bss = (int32_t)bitsize;
    int32_t tbm = 0;
    int32_t kek_counter = 0;
    //printf("remaining bitsize:%7d\n", bss);
    while (bss > 0) {
        kek_counter++;
        t = decoding_table[xx];
        data[di++] = t.symbol;
        if (t.nbbits >= 32) printf("WARNING t.nbbits=%d!\n", t.nbbits);
        if (t.nbbits > tbm) tbm = t.nbbits;
        xx = t.new_x + reverse_bits_uint32_t(read_bits_bitstream(bs, t.nbbits), t.nbbits);
        bss -= t.nbbits;
        //printf("bss:%d, xx:%u, di=%zu, sym=%d\n", bss, xx, di, data[di-1]);
        //printf("remaining bitsize:%7d\n", bss);
    }
    printf("MAX t.nbbits=%d, kek_counter=%d\n", tbm, kek_counter);
    return data;
}

void quantize_occurences() {
    int total = 0;
    int quant_total = 0;
    int i, difference;

    for (int i = 0; i < ASIZE; ++i) {
        total += occ[i];
    }
    for (i = 0; i < ASIZE; i++) {
        float probability = (float)occ[i] / total;
        quant[i] = (int)(probability * L + 0.5);
        if (quant[i] == 0) quant[i] = 1;
        //printf("prob[%d]=%f, quant[%d]=%d\n", i, probability, i, quant[i]);
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
    for (i = 0; i < ASIZE; i++) {
        occ[i] = quant[i];
        //printf("Symbol %d: Quantized value = %d\n", i, quant[i]);
    }
    return;
}

void test_write_read_streams() {
    int bitsizes[15] = {1, 3, 7, 15, 2, 30, 8, 2, 3, 5, 29, 15, 4, 3, 2};
    uint32_t bits[15] = {0xF, 0x2, 0x4, 0xA, 0x1, 0xFFA, 0x6, 0x1, 0x3, 0x5, 0x1C, 0xB, 0x2, 0x1, 0x3};
    uint32_t rbits[15] = {0};
    for (int i = 0; i < 15; ++i) {
        bits[i] &= ((1 << bitsizes[i]) - 1);
    }
    size_t streamsize = 15;
    char filename[] = "bitstream_testfile.bitstream";
    FILE *file = fopen(filename, "wb");
    size_t total_bytesize = 0, total_bitsize = 0;
    for (int i = 0; i < 15; ++i) {
        total_bitsize += bitsizes[i];
    }
    fwrite(&total_bitsize, sizeof(size_t), 1, file);
    total_bytesize = (total_bitsize % 8 == 0) ? (total_bitsize / 8) : (total_bitsize / 8 + 1);
    vecStream writing_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = total_bitsize,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * 10)
        },
        .stream_capacity = 10
    };
    for (int i = 0; i < 15; ++i) {
        write_bits_vecstream(&writing_stream, bits[i], bitsizes[i]);
    }
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.ms.current_bytesize, total_bytesize);
    finalize_vecstream(&writing_stream);
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.ms.current_bytesize, total_bytesize);
    printf("printing writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(writing_stream.ms.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");
    reverse_bits_memstream(&(writing_stream.ms));
    printf("printing reversed writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(writing_stream.ms.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");

    fwrite(writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);

    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    bitStream reading_stream = (bitStream){
        .bit_buffer = 0,
        .bits_in_buffer = 0,
        .current_bytesize = 0,
        .total_bitsize = read_total_bitsize,
        .stream=file
    };

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
        rbits[i] = read_bits_bitstream(&reading_stream, bitsizes[15 - i - 1]);
        printf("reading %d bits at reversed index %d\n", bitsizes[15 - i - 1], i);
        read_total_bitsize -= bitsizes[15 - i - 1];
        i++;
        //print_uint64_t(reading_stream.bit_buffer);
    }

    printf("r| current_bytesize: %lu, total_bytesize: %lu\n", reading_stream.current_bytesize, read_total_bytesize);
    for (int i = 0; i < 15; ++i) {
        printf("i = %d, b:\n", i);
        print_uint32_t(bits[i], bitsizes[i]);
        printf("\nrrb:\n");
        print_uint32_t(rbits[15 - i - 1], bitsizes[i]);
        //printf("b:%u --- rrb:%u\n", bits[i], rbits[15 - i - 1]);
        printf("\n");
    }
    fclose(file);
    free(writing_stream.ms.stream);
    return;
}


void test_tans_algorithm() {
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
    vecStream writing_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = 0,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * 10)
        },
        .stream_capacity = 10
    };
    uint32_t final_state = encode(text, dsize, &writing_stream);
    printf("final state: %u\n", final_state);
    finalize_vecstream(&writing_stream);
    printf("memstream finalized.\n");
    reverse_bits_memstream(&(writing_stream.ms));
    size_t total_bytesize = (writing_stream.ms.total_bitsize % 8 == 0) ? (writing_stream.ms.total_bitsize / 8) : (writing_stream.ms.total_bitsize / 8 + 1);
    char filename[] = "bitstream_testfile.bitstream";
    FILE *file = fopen(filename, "wb");
    fwrite(&dsize, sizeof(size_t), 1, file);
    fwrite(&(writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);

    size_t recovered_text_size = 0;
    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    uint32_t decode_initial_state;
    fread(&recovered_text_size, sizeof(size_t), 1, file);
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    fread(&decode_initial_state, sizeof(uint32_t), 1, file);
    bitStream reading_stream = (bitStream){
        .bit_buffer = 0,
        .bits_in_buffer = 0,
        .current_bytesize = 0,
        .total_bitsize = read_total_bitsize,
        .stream = file
    };
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    size_t garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        printf("read %lu bits of garbage:\n", garbage_bitsize);
        read_bits_bitstream(&reading_stream, garbage_bitsize);
    }
    uint8_t *recovered_text = decode(recovered_text_size, read_total_bitsize, decode_initial_state, &reading_stream);
    for (int i = 0; i < recovered_text_size; ++i) {
        printf("%d --- %d\n", text[i], recovered_text[recovered_text_size - i - 1]);
    }
    free(recovered_text);
    free(writing_stream.ms.stream);
}


#pragma pack(1) // Ensure no padding
typedef struct {
    char chunkID[4];
    unsigned int chunkSize;
    char format[4];
    char subchunk1ID[4];
    unsigned int subchunk1Size;
    unsigned short audioFormat;
    unsigned short numChannels;
    unsigned int sampleRate;
    unsigned int byteRate;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
    char subchunk2ID[4];
    unsigned int subchunk2Size;
} WAVHeader;

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

typedef struct {
     int key;
     int stack_index;
} TableEntry;

void push_vector16(Vector16 *vec, int16_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        int16_t *new_data = (int16_t *)malloc(sizeof(int16_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(int16_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_uvector16(UVector16 *vec, uint16_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        uint16_t *new_data = (uint16_t *)malloc(sizeof(uint16_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(uint16_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_vector8(Vector8 *vec, int8_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        int8_t *new_data = (int8_t *)malloc(sizeof(int8_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(int8_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_uvector8(UVector8 *vec, uint8_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        uint8_t *new_data = (uint8_t *)malloc(sizeof(uint8_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(uint8_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void read_compressed_file(const char *filename, int16_t **data, size_t *num_samples) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    uint16_t temp16, *samples;
    int8_t *d64_table, *res_table;
    size_t samples_size, read_total_bitsize, read_total_bytesize, garbage_bitsize, diffs_size;
    uint32_t final_state;

    for (int i = 0; i < ASIZE; ++i) {
        fread(&temp16, sizeof(uint16_t), 1, file);
        occ[i] = (uint32_t)temp16;
    }
    d64_table = (int8_t *)malloc(sizeof(int8_t) * ASIZE);
    res_table = (int8_t *)malloc(sizeof(int8_t) * ASIZE);
    d64_table[0] = 0;
    res_table[0] = 0;
    fread(d64_table + 1, sizeof(int8_t), (ASIZE - 1), file);
    fread(res_table + 1, sizeof(int8_t), (ASIZE - 1), file);
    for (int i = 1; i < 10; ++i) {
        printf("ct[%d].d64=%d, ct[%d].res=%d\n", i, d64_table[i], i, res_table[i]);
    }
    fread(&samples_size, sizeof(size_t), 1, file);
    samples = (uint16_t *)malloc(sizeof(uint16_t) * samples_size);
    fread(samples, sizeof(uint16_t), samples_size, file);
    fread(&final_state, sizeof(uint32_t), 1, file);
    fread(&diffs_size, sizeof(size_t), 1, file);
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    bitStream diff_reading_stream = (bitStream){
        .bit_buffer = 0,
        .bits_in_buffer = 0,
        .current_bytesize = 0,
        .total_bitsize = read_total_bitsize,
        .stream = file
    };
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    printf("read_final_state: %u\n", final_state);
    printf("read_total_bytesize:%zu, read_total_bitsize:%zu\n", read_total_bytesize, read_total_bitsize);
    garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        printf("read %lu bits of garbage:\n", garbage_bitsize);
        read_bits_bitstream(&diff_reading_stream, garbage_bitsize);
    }
    uint8_t *diffs_recovered = decode(diffs_size, read_total_bitsize, final_state, &diff_reading_stream);
    for (int i = 0; i < diffs_size / 2; ++i) {
        uint8_t temp = diffs_recovered[i];
        diffs_recovered[i] = diffs_recovered[diffs_size - i - 1];
        diffs_recovered[diffs_size - i - 1] = temp;
    }
    printf("diffs size: %zu\n", diffs_size);
    for (int i = 0; i < 50; ++i) {
        printf("%3d ", diffs_recovered[i]);
    }
    for (int i = 0; i < diffs_size; ++i) {
        if (diffs_recovered[i]) {
            printf("first non-zero diff is at %d\n", i);
            break;
        }
    }
    printf("\n");
    // restoration of initial samples
    *num_samples = diffs_size + 1;
    //UVector16 datavec = (UVector16){.size=0, .capacity=(size_t)(*num_samples), .data = (uint16_t *)malloc(sizeof(uint16_t) * (size_t)(*num_samples))};
    //push_uvector16(&datavec, samples[0]);
    size_t diff_index = 0, sample_index = 1;
    printf("samples size: %zu, num_samples: %zu, diff_size: %zu\n", samples_size, *num_samples, diffs_size);
    uint16_t *udata = (uint16_t *)malloc(sizeof(uint16_t) * (size_t)(*num_samples));
    for (int i = 1; i < (*num_samples); ++i) {
        uint8_t diff_char = diffs_recovered[diff_index++];
        if (diff_char == 0) {
            udata[i] = samples[sample_index++];
        } else {
            int d64 = d64_table[diff_char];
            int res = res_table[diff_char];
            udata[i] = (uint16_t)((int32_t)udata[i - 1] + (d64 * 64) + res);
        }
    }
    *data = (int16_t *)udata;
    for (int i = 0; i < *num_samples; ++i) {
        (*data)[i] -= 32768;
    }
    fclose(file);
    return;
}

void write_wav_file(const char *filename, int16_t *data, int num_samples) {
    WAVHeader header;
    int num_channels = 1;
    int sample_rate = 19531;
    int bits_per_sample = 16;
    memcpy(header.chunkID, "RIFF", 4);
    header.chunkSize = 36 + num_samples * sizeof(int16_t);
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1ID, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1; // PCM
    header.numChannels = num_channels;
    header.sampleRate = sample_rate;
    header.byteRate = sample_rate * num_channels * bits_per_sample / 8;
    header.blockAlign = num_channels * bits_per_sample / 8;
    header.bitsPerSample = bits_per_sample;
    memcpy(header.subchunk2ID, "data", 4);
    header.subchunk2Size = num_samples * sizeof(int16_t);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to open file '%s'\n", filename);
        exit(1);
    }
    fwrite(&header, sizeof(WAVHeader), 1, file);
    fwrite(data, sizeof(int16_t), num_samples, file);

    fclose(file);
}

int main(int argc, char **argv) {
    test_write_read_streams();
    test_tans_algorithm();
    if (argc < 3) {
        printf("Usage: %s <compressed-file> <output-wav-file>\n", argv[0]);
        return 1;
    }

    int16_t *data;
    size_t num_samples;
    read_compressed_file(argv[1], &data, &num_samples);
    write_wav_file(argv[2], data, num_samples);

    return 0;
}














