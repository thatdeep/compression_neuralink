#include "../stream.h"
#include "../vector.h"
#include "../wavio.h"
#include "../ans.h"
#include "assert.h"
#include "math.h"

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
    // TODO IF ALPHABET_SIZE IS A VARIABLE THEN WRITE IT TO THE FILE
    int alphabet_size = ASIZE;
    int quant_pow = R;
    int32_t *occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    memset(occ, 0, sizeof(int32_t) * alphabet_size);
    // collect occurences, determine top (alphabet_size) pairs of (mul64, res) to encode into diffs alphabet.
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
    int32_t *quant_occ = quantize_occurences(occ, alphabet_size, quant_pow); // honestly, just send and serialize quantized occurences, plus we use it at encode
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
    int K = diffs.size;
    uint32_t final_state = encode(diffs.data, K, &diff_writing_stream, quant_occ, alphabet_size, quant_pow);
    finalize_vecstream(&diff_writing_stream);
    size_t total_bytesize = (diff_writing_stream.ms.total_bitsize % 8 == 0)
                            ? (diff_writing_stream.ms.total_bitsize / 8)
                            : (diff_writing_stream.ms.total_bitsize / 8 + 1);
    reverse_bits_memstream(&(diff_writing_stream.ms));
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    // what should be packed:
    // * encoded diff alphabet:
    // int alphabet_size
    // int quant_pow (TODO remove it if we dont plan to search optimal quantization)
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
    fwrite(&alphabet_size, sizeof(int), 1, file);
    fwrite(&quant_pow, sizeof(int), 1, file);
    for (int i = 0; i < ASIZE; ++i) {
        uint16_t quant_occ_value = (uint16_t)quant_occ[i];
        fwrite(&quant_occ_value, sizeof(uint16_t), 1, file);
    }
    for (int i = 1; i < ASIZE; ++i) {
        fwrite(&(chain_table[i - 1].d64), sizeof(int8_t), 1, file);
    }
    for (int i = 1; i < ASIZE; ++i) {
        fwrite(&(chain_table[i - 1].res), sizeof(int8_t), 1, file);
    }
    fwrite(&(samples.size), sizeof(size_t), 1, file);
    fwrite(samples.data, sizeof(uint16_t), samples.size, file);
    // saving diff would change result to simply choosing top occurence diffs (one of proposed improvements)
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(&(diffs.size), sizeof(size_t), 1, file);
    fwrite(&(diff_writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(diff_writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);
    free(occ);
    free(quant_occ);
    free(diff_writing_stream.ms.stream);
    free(samples.data);
    free(diffs.data);
    return;
}

int main(int argc, char **argv) {
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














