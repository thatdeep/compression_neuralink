#include "../stream.h"
// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <string.h>
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
static uint8_t symbol_backup[L];

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
    printf("occ encode:\n");
    for (int i = 0; i < ASIZE; ++i) {
        printf("%4d ", occ[i]);
        if (i && (i % 16 == 0)) printf("\n");
    }
    printf("\n");

    for (size_t s = 0; s < ASIZE; ++s) {
        for (uint32_t i = 0; i < occ[s]; ++i) {
            symbol[xind] = s;
            symbol_backup[xind] = s;
            xind = (xind + SPREADSTEP) % L;
        }
    }
    // prepare
    int sumacc = 0;
    for (size_t s = 0; s < ASIZE; ++s) {
        kdiff[s] = R - logfloor(occ[s]);
        nb[s] = (kdiff[s] << RSMALL) - (occ[s] << kdiff[s]);
        start[s] = -occ[s] + sumacc;
        sumacc += occ[s];
        next[s] = occ[s];
    }
    int s;
    for (int x = L; x < 2 * L; ++x) {
        s = symbol[x - L];
        encoding_table[start[s] + next[s]] = x;
        next[s]++;
    }
    uint32_t x = 1 + L;

    for (size_t i = 0; i < dsize; ++i) {
        s = data[i];
        nbbits = (x + nb[s]) >> RSMALL;
        //printf("i=%lu, x=%d, nb[%d]=%d, (x + nb[s])=%d, nbbits=%d\n", i, x, s, nb[s], (x + nb[s]), (x + nb[s]) >> RSMALL);
        printf("s = %d, x = %u, xx=%u, nbbits=%d\n", s, x, x - L, nbbits);
        printf("writing %d bits into stream: ", nbbits);
        print_uint32_t(x, nbbits);
        printf("\n");
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

uint8_t *decode(size_t dsize, size_t bitsize, uint32_t x, memStream *bs) {
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * dsize);
    size_t di = 0;
    // spread
    uint32_t xind = 0;
    printf("occ decode:\n");
    for (int i = 0; i < ASIZE; ++i) {
        printf("%4d ", occ[i]);
        if (i && (i % 16 == 0)) printf("\n");
    }
    printf("\n");
    for (size_t s = 0; s < ASIZE; ++s) {
        for (uint32_t i = 0; i < occ[s]; ++i) {
            symbol[xind] = s;
            if (symbol[xind] != symbol_backup[xind]) {
                printf("symbol build differs on xind=%u\n", xind);
            }
            xind = (xind + SPREADSTEP) % L;
        }
    }
    // prepare
    for (size_t s = 0; s < ASIZE; ++s) {
        next[s] = occ[s];
    }
    for (size_t xd = 0; xd < L; ++xd) {
        tableEntry t;
        t.symbol = symbol[xd];
        uint32_t xx = next[t.symbol];
        next[t.symbol]++;
        t.nbbits = R - logfloor(xx);
        t.new_x = (xx << t.nbbits) - L;
        decoding_table[xd] = t;
    }
    // decode
    uint32_t xx = x - L;
    tableEntry t;
    int32_t bss = (int32_t)bitsize;
    //printf("remaining bitsize:%7d\n", bss);
    while (bss > 0) {
        t = decoding_table[xx];
        data[di++] = t.symbol;
        printf("s = %d, x = %u, xx=%u, nbbits=%d\n", data[di - 1], xx + L, xx, t.nbbits);
        printf("reading %d bits from stream: ", t.nbbits);
        uint32_t kekw = read_bits_memstream(bs, t.nbbits);
        print_uint32_t(kekw, t.nbbits);
        printf(", plus reverse: ");
        print_uint32_t(reverse_bits_uint32_t(kekw, t.nbbits), t.nbbits);
        printf("\n");
        xx = t.new_x + reverse_bits_uint32_t(kekw, t.nbbits);
        bss -= t.nbbits;
        //printf("remaining bitsize:%7d\n", bss);
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
    uint8_t *recovered_text = decode(recovered_text_size, read_total_bitsize, decode_initial_state, (memStream *)(&reading_stream));
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


#define SAMPLE_TABLE_SIZE 4096
#define SAMPLE_TABLE_EMPTY_KEY -1

static TableEntry sample_table[SAMPLE_TABLE_SIZE];
static uint16_t sample_stack[SAMPLE_TABLE_SIZE];


uint16_t *read_wav_file(const char *filename, WAVHeader *header, size_t *num_samples) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open file '%s'\n", filename);
        exit(1);
    }

    //WAVHeader header;
    fread(header, sizeof(WAVHeader), 1, file);

    *num_samples = header->subchunk2Size / (header->bitsPerSample / 8);
    int16_t *data = (int16_t *) malloc((*num_samples) * sizeof(int16_t));
    if (!data) {
        printf("Failed to allocate memory\n");
        fclose(file);
        exit(1);
    }

    fread(data, header->bitsPerSample / 8, *num_samples, file);
    uint16_t *udata = (uint16_t *)data;
    for (int i = 0; i < *num_samples; ++i) {
        udata[i] += 32768;
    }
    fclose(file);
    return udata;
}

typedef struct {
    uint32_t count;
    int8_t d64;
    int8_t res;
} ChainElemInfo;

int compare_chain_element_infos(const void *a, const void *b) {
    ChainElemInfo *elemA = (ChainElemInfo *)a;
    ChainElemInfo *elemB = (ChainElemInfo *)b;
    return (elemB->count - elemA->count); // Sorting in descending order of counts
}

static ChainElemInfo chain_table[303];
static uint8_t chain_elem_to_alphabet[303];

void write_compressed_file(const char *filename, uint16_t *data, int num_samples) {
    // collect occurences, determine top (255) pairs of (mul64, res) to encode into diffs alphabet.
    for (int d64 = -50; d64 < 51; ++d64) {
        for (int res = -1; res  < 2; ++res) {
            chain_table[(d64 + 50) * 3 + (res + 1)] = (ChainElemInfo){.count = 0, .d64 = (int8_t)d64, .res = (int8_t)res};
            chain_elem_to_alphabet[(d64 + 50) * 3 + (res + 1)] = 0;
        }
    }
    for (int i = 1; i < num_samples; ++i) {
        int diff = data[i] - data[i - 1];
        int d64 = round(1.0 * diff / 64);
        int res = diff - d64 * 64;
        if ((abs(d64) <= 50) && (abs(res) <= 1)) {
            chain_table[(d64 + 50) * 3 + (res + 1)].count++;
        }
    }
    qsort(chain_table, 303, sizeof(ChainElemInfo), compare_chain_element_infos);
    memset(occ, 0, sizeof(int32_t) * ASIZE);
    for (int i = 0; i < ASIZE - 1; ++i) { // first symbol states end of chain
        int d64 = chain_table[i].d64;
        int res = chain_table[i].res;
        chain_elem_to_alphabet[(d64 + 50) * 3 + (res + 1)] = i + 1;
        occ[i + 1] = chain_table[i].count;
    }
    // diffs alphabet encoded, iterate samples and create anchors array as well as coded diffs
    UVector16 samples = (UVector16){.size=0, .capacity=1024, .data=(uint16_t *)malloc(sizeof(uint16_t) * 1024)};
    UVector8 diffs = (UVector8){.size=0, .capacity=4096, .data=(uint8_t *)malloc(sizeof(uint8_t) * 4096)};
    push_uvector16(&samples, data[0]);
    for (int i = 1; i < num_samples; ++i) {
        int diff = data[i] - data[i - 1];
        int d64 = round(1.0 * diff / 64);
        int res = diff - d64 * 64;
        if ((abs(d64) <= 50) && (abs(res) <= 1)) {
            if (chain_elem_to_alphabet[(d64 + 50) * 3 + (res + 1)] != 0) {
                push_uvector8(&diffs, chain_elem_to_alphabet[(d64 + 50) * 3 + (res + 1)]);
            } else {
                push_uvector8(&diffs, 0);
                push_uvector16(&samples, data[i]);
                occ[0]++;
            }
        } else {
            push_uvector8(&diffs, 0);
            push_uvector16(&samples, data[i]);
            occ[0]++;
        }
    }
    quantize_occurences(); // honestly, just send serialize quantized occurences, plus we use it at encode
    vecStream diff_writing_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = 0,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * 8192)
        },
        .stream_capacity = 8192
    };
    for (int i = 909; i < 920; ++i) {
        printf("%3d ", diffs.data[i]);
    }
    int K = diffs.size;
    printf("\n");
    uint32_t final_state = encode(diffs.data, K, &diff_writing_stream);
    printf("final state: %u, diffs_size: %zu\n", final_state, diffs.size);
    finalize_vecstream(&diff_writing_stream);
    size_t total_bytesize = (diff_writing_stream.ms.total_bitsize % 8 == 0)
                            ? (diff_writing_stream.ms.total_bitsize / 8)
                            : (diff_writing_stream.ms.total_bitsize / 8 + 1);
    printf("vecstream finalized.\n");
    printf("printing writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(diff_writing_stream.ms.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");
    printf("printing reversed writing_stream byte-by-byte:\n");
    reverse_bits_memstream(&(diff_writing_stream.ms));
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(diff_writing_stream.ms.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");
    printf("dws current_bytesize=%zu, total_bytesize=%zu\n", diff_writing_stream.ms.current_bytesize, total_bytesize);
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    // what should be packed:
    // * encoded diff alphabet:
    // (TODO: ASIZE when gridsearch arrives, imagine testing different alphabet sizes to see which will get better comp rate)
    // uint16 quant_occ[ASIZE]
    // int8 d64[ASIZE - 1] (d64 has no meaning for our END_OF_CHAIN symbol 0)
    // int8 res[ASIZE - 1] (res has no meaning for our END_OF_CHAIN symbol 1) - can be compressed via vecstream/memstream to 2-bit
    // * anchors (samples)
    // size_t samples_size
    // uint16_t *samples
    // * coded diffs
    // size_t bitsize
    // (optionally number of diffs, but can be deduced at decoding stage after we decode all bitsize bits)
    // content of reversed diff_writing_stream
    for (int i = 0; i < ASIZE; ++i) {
        uint16_t quant_occ_value = (uint16_t)occ[i];
        // int d64 = chain_table[i].d64;
        // int res = chain_table[i].res;
        // chain_elem_to_alphabet[(d64 + 50) * 3 + res] = i + 1;
        // occ[i + 1] = chain_table[i].count;
        fwrite(&quant_occ_value, sizeof(uint16_t), 1, file);
    }
    for (int i = 1; i < ASIZE; ++i) {
        fwrite(&(chain_table[i - 1].d64), sizeof(int8_t), 1, file);
    }
    for (int i = 1; i < ASIZE; ++i) {
        fwrite(&(chain_table[i - 1].res), sizeof(int8_t), 1, file);
    }
    for (int i = 0; i < 10; ++i) {
        printf("ct[%d].d64=%d, ct[%d].res=%d\n", i, chain_table[i].d64, i, chain_table[i].res);
    }
    printf("# of anchor samples: %zu\n", samples.size);
    printf("# of diff samples: %zu\n", diffs.size);
    printf("total bytesize: %zu, total bitsize: %zu\n", total_bytesize, diff_writing_stream.ms.total_bitsize);
    fwrite(&(samples.size), sizeof(size_t), 1, file);
    fwrite(samples.data, sizeof(uint16_t), samples.size, file);
    // saving diff would change result to simply choosing top occurence diffs (one of proposed improvements)
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(&(diffs.size), sizeof(size_t), 1, file);
    fwrite(&(diff_writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(diff_writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);
    diff_writing_stream.ms.current_bytesize = 0;
    diff_writing_stream.ms.bit_buffer = 0;
    diff_writing_stream.ms.bits_in_buffer = 0;
    size_t garbage_bitsize = (diff_writing_stream.ms.total_bitsize % 8 == 0) ? (0) : (8 - (diff_writing_stream.ms.total_bitsize % 8));
    if (garbage_bitsize) {
        printf("read %lu bits of garbage:\n", garbage_bitsize);
        read_bits_memstream(&(diff_writing_stream.ms), garbage_bitsize);
    }
    uint8_t *diffs_recovered = decode(K, diff_writing_stream.ms.total_bitsize, final_state, &(diff_writing_stream.ms));
    for (int i = 0; i < K / 2; ++i) {
        uint8_t temp = diffs_recovered[i];
        diffs_recovered[i] = diffs_recovered[K - i - 1];
        diffs_recovered[K - i - 1] = temp;
    }
    int fi = -1;
    for (int i = K - 1; i >= 0; --i) {
        printf("od[%5d]=%3d -- rd[%5d]=%3d.", i, diffs.data[i], i, diffs_recovered[i]);
        if (diffs.data[i] != diffs_recovered[i]) {
            printf(" DIFFERENT!!!\n");
            if (fi == -1) fi = K - i - 1;
        } else {
            printf("\n");
        }
    }
    printf("first DIFF diverges at %d from end.\n", fi);
    //for (int i = fi; i < fi + 15; ++i) {
    //    printf("od[%5d]=%3d -- rd[%5d]=%3d\n", i, diffs.data[i], i, diffs_recovered[i]);
    //}
    free(diffs_recovered);
    return;
}

int main(int argc, char **argv) {
    //test_write_read_streams();
    //test_tans_algorithm();
    if (argc < 3) {
        printf("Usage: %s <input-wav-file> <compressed-file>\n", argv[0]);
        return 1;
    }
    WAVHeader header;
    size_t num_samples;

    uint16_t *data = read_wav_file(argv[1], &header, &num_samples);
    write_compressed_file(argv[2], data, num_samples);

    return 0;
}














