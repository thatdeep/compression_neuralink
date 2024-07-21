#include "../stream.h"
#include "../vector.h"
#include "../wavio.h"
#include "../ans.h"
#include "../bwt.h"
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

static const int d64_bound = 50;
static const int res_bound = 31;
static const int d64_range = 2 * d64_bound + 1;
static const int res_range = 2 * res_bound + 1;
static ChainElemInfo chain_table[res_range * d64_range];
static int chain_elem_to_alphabet[res_range * d64_range];


void write_diffs_freq_stat(const char *filename, const char *row_name, uint32_t *frequencies, int freq_size) {
    FILE *file = fopen(filename, "a");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    // Check if the file is empty
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    if (filesize == 0) {
        fprintf(file, "filename");
        for (int i = 0; i < freq_size; ++i) {
            fprintf(file, ",%d", i);    
        }
        fprintf(file, "\n");
    }
    // Write filename and frequencies to file
    fprintf(file, "%s", row_name);
    for (int i = 0; i < freq_size; ++i) {
        fprintf(file, ",%d", frequencies[i]);
    }
    fprintf(file, "\n");
    fclose(file);
    return;
}


void write_compressed_file(const char *filename, uint16_t *data, int num_samples) {
    int alphabet_size = ASIZE;
    int quant_pow = R;
    int32_t *occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    memset(occ, 0, sizeof(int32_t) * alphabet_size);
    // collect occurences, determine top (alphabet_size) pairs of (mul64, res) to encode into diffs alphabet.
    for (int d64 = -d64_bound; d64 < d64_bound + 1; ++d64) {
        for (int res = -res_bound; res  < res_bound + 1; ++res) {
            chain_table[(d64 + d64_bound) * res_range + (res + res_bound)] = (ChainElemInfo){.count = 0, .d64 = (int8_t)d64, .res = (int8_t)res};
            chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)] = -1;
        }
    }
    for (int i = 1; i < num_samples; ++i) {
        int diff = data[i] - data[i - 1];
        int d64 = round(1.0 * diff / 64);
        int res = diff - d64 * 64;
        if ((abs(d64) <= d64_bound) && (abs(res) <= res_bound)) {
            chain_table[(d64 + d64_bound) * res_range + (res + res_bound)].count++;
        }
    }
    // TODO THINK ABOUT CAPPING ALPHABET SIZE AT THIS POINT TO INCLUDE ONLY NON-ZERO OCC. WHY DO YOU EVEN NEED CODES FOR 0-PROBABILITY SYMBOLS?
    // THIS MESSES WITH FAST QUANTIZATION, WILL SEE IF IT WILL CRIPPLE PRECISE QUANTIZATION
    qsort(chain_table, res_range * d64_range, sizeof(ChainElemInfo), compare_chain_element_infos);
    //memset(occ, 0, sizeof(int32_t) * ASIZE);
    for (int i = 0; i < alphabet_size - 1; ++i) { // first symbol states end of chain
        int d64 = chain_table[i].d64;
        int res = chain_table[i].res;
        chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)] = i + 1;
        occ[i + 1] = chain_table[i].count;
    }
    // compute occ[0] - # of occurences of stop-symbol, then sink it into sorted array of frequencies while remembering its index.
    for (int i = 1; i < num_samples; ++i) {
        int diff = data[i] - data[i - 1];
        int d64 = round(1.0 * diff / 64);
        int res = diff - d64 * 64;
        if ((abs(d64) > d64_bound) || (abs(res) > res_bound) || (chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)] == -1)) {
            occ[0]++;
        }
    }
    // for (int i = 0; i < alphabet_size; ++i) {
    //     printf("o[%3d]=%4d, ", i, occ[i]);
    //     if (i && ((i + 1) % 14 == 0)) printf("\n");
    // }
    // printf("\n");
    int delimiter_index = 0;
    for (int i = 0; i < alphabet_size - 1; ++i) {
        if (occ[i + 1] <= occ[i]) break;
        int d64 = chain_table[i].d64;
        int res = chain_table[i].res;
        chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)] = i;
        int32_t temp = occ[i];
        occ[i] = occ[i + 1];
        occ[i + 1] = temp;
        delimiter_index = i + 1;
    }
    // printf("delimiter_index=%d\n", delimiter_index);
    // trim alphabet to contain only nonzero occ
    int trimmed_alphabet_size = alphabet_size;
    for (int i = 0; i < alphabet_size; ++i) {
        if (occ[i] == 0) {
            trimmed_alphabet_size = i;
            break;
        }
    }
    // printf("trimmed_alphabet_size=%d\n", trimmed_alphabet_size);

    alphabet_size = trimmed_alphabet_size;
    // diffs alphabet encoded, iterate samples and create anchors array as well as coded diffs
    UVector16 samples = (UVector16){.size=0, .capacity=1024, .data=(uint16_t *)malloc(sizeof(uint16_t) * 1024)};
    UVector8 diffs = (UVector8){.size=0, .capacity=4096, .data=(uint8_t *)malloc(sizeof(uint8_t) * 4096)};
    push_uvector16(&samples, data[0]);
    //printf("pek\n");
    for (int i = 1; i < num_samples; ++i) {
        int diff = (int)data[i] - (int)data[i - 1];
        int d64 = round(1.0 * diff / 64);
        int res = diff - d64 * 64;
        // if (i == 3026) {
        //     printf("at i=%d diff=%d, d64=%d, res=%d\n", i, diff, d64, res);
        //     printf("chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)]=%d\n", chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)]);
        //     int pek = chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)];
        //     printf("chain_table[%d].d64=%d, chain_table[%d].res=%d\n", pek, chain_table[pek].d64, pek, chain_table[pek].res);
        //     printf("chain_table[%d].d64=%d, chain_table[%d].res=%d\n", pek - 1, chain_table[pek - 1].d64, pek - 1, chain_table[pek - 1].res);
        //     printf("chain_table[%d].d64=%d, chain_table[%d].res=%d\n", pek + 1, chain_table[pek + 1].d64, pek + 1, chain_table[pek + 1].res);
        // }
        if ((abs(d64) <= d64_bound) && (abs(res) <= res_bound)) {
            if (chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)] != -1) { // can never index delimiter_index here as it explicitly requires == -1 inside d64 res bounds
                push_uvector8(&diffs, chain_elem_to_alphabet[(d64 + d64_bound) * res_range + (res + res_bound)]);
                // if ((i <= 50) || (num_samples - i <= 50)) printf("w| i: %d, sample: %d, diff: %d, d64: %d, res: %d\n", i, data[i], diff, d64, res);
            } else { // here we have that part of stop-symbols
                push_uvector8(&diffs, delimiter_index);
                push_uvector16(&samples, data[i]);
                // printf("w: in-bounds stop symbol at index %d\n", i);
            }
        } else {
            push_uvector8(&diffs, delimiter_index);
            push_uvector16(&samples, data[i]);
            // printf("w: out-bounds stop symbol at index %d\n", i);
        }
    }
    //printf("xd\n");
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
    uint32_t *udata_32 = (uint32_t *)malloc(sizeof(uint32_t) * diffs.size);
    for (int i = 0; i < diffs.size; ++i) {
        udata_32[i] = (uint32_t)(diffs.data[i]);
    }
    int sentinel_index;
    uint32_t *udata_32_bwt = bwt(udata_32, diffs.size, alphabet_size, &sentinel_index);
    for (int i = 0; i < num_samples; ++i) {
        diffs.data[i] = (uint8_t)(udata_32_bwt[i]);
    }
    free(udata_32);
    free(udata_32_bwt);

    char raw_diffs_rle_freq_csv[] = "outputs/raw_diffs_rle_freq.csv";
    char bwt_diffs_rle_freq_csv[] = "outputs/bwt_diffs_rle_freq.csv";
    const int freq_size = 256;
    uint32_t frequencies[freq_size] = {0};
    uint8_t prev1 = diffs.data[0];
    uint32_t cnt = 1;
    for (int i = 1; i < diffs.size; ++i) {
        if (diffs.data[i] == prev1) {
            cnt++;
            if (cnt == 256) {
                frequencies[cnt - 1]++;
                cnt = 1;
            }
        } else {
            frequencies[cnt]++;
            cnt = 1;
            prev1 = diffs.data[i];
        }
    }
    write_diffs_freq_stat(raw_diffs_rle_freq_csv, filename, frequencies, freq_size);

    memset(frequencies, 0, sizeof(uint32_t) * freq_size);
    uint32_t prev2 = udata_32_bwt[0];
    cnt = 1;
    for (int i = 1; i < diffs.size; ++i) {
        if (udata_32_bwt[i] == prev2) {
            cnt++;
            if (cnt == 256) {
                frequencies[cnt - 1]++;
                cnt = 1;
            }
        } else {
            frequencies[cnt]++;
            cnt = 1;
            prev2 = udata_32_bwt[i];
        }
    }
    write_diffs_freq_stat(bwt_diffs_rle_freq_csv, filename, frequencies, freq_size);


    uint32_t final_state = encode(diffs.data, K, &diff_writing_stream, occ, alphabet_size, quant_pow);
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
    fwrite(&sentinel_index, sizeof(int), 1, file);
    // what should be packed:
    // * encoded diff alphabet:
    // int alphabet_size
    // int quant_pow (TODO remove it if we dont plan to search optimal quantization)
    // uint32_t occ[ASIZE]
    // int8 d64[ASIZE - 1] (d64 has no meaning for our END_OF_CHAIN symbol 0)
    // int8 res[ASIZE - 1] (res has no meaning for our END_OF_CHAIN symbol 1) - can be compressed via vecstream/memstream to 2-bit
    // * anchors (samples)
    // size_t samples_size
    // uint16_t *samples
    // * coded diffs
    // size_t bitsize
    // (optionally number of diffs, but can be deduced at decoding stage after we decode all bitsize bits)
    // content of reversed diff_writing_stream
    //printf("kek\n");
    // for (int i = 0; i < alphabet_size; ++i) {
    //     printf("o[%3d]=%4d, ", i, occ[i]);
    //     if (i && ((i + 1) % 14 == 0)) printf("\n");
    // }
    // printf("\n");
    // printf("asize: %d, del_idx: %d\n", alphabet_size, delimiter_index);
    fwrite(&alphabet_size, sizeof(int), 1, file);
    fwrite(&delimiter_index, sizeof(int), 1, file);
    fwrite(&quant_pow, sizeof(int), 1, file);
    fwrite(occ, sizeof(int32_t), alphabet_size, file);
    // for (int i = 0; i < ASIZE; ++i) {
    //     uint16_t quant_occ_value = (uint16_t)quant_occ[i];
    //     fwrite(&quant_occ_value, sizeof(uint16_t), 1, file);
    // }
    // for (int i = 0; i < alphabet_size; ++i) {
    //     printf("d[%3d]=%4d, ", i, chain_table[i].d64);
    //     if (i && ((i + 1) % 14 == 0)) printf("\n");
    // }
    // printf("\n");
    // for (int i = 0; i < alphabet_size; ++i) {
    //     printf("r[%3d]=%4d, ", i, chain_table[i].res);
    //     if (i && ((i + 1) % 14 == 0)) printf("\n");
    // }
    // printf("\n");
    int bytes_written = 0;
    if (delimiter_index >= alphabet_size) {
        for (int i = 0; i < alphabet_size; ++i) {
            fwrite(&(chain_table[i].d64), sizeof(int8_t), 1, file);
            bytes_written++;
        }
        for (int i = 0; i < alphabet_size; ++i) {
            fwrite(&(chain_table[i].res), sizeof(int8_t), 1, file);
            bytes_written++;
        }
    } else {
        for (int i = 0; i < delimiter_index; ++i) {
            fwrite(&(chain_table[i].d64), sizeof(int8_t), 1, file);
            bytes_written++;
        }
        for (int i = delimiter_index + 1; i < alphabet_size; ++i) {
            fwrite(&(chain_table[i - 1].d64), sizeof(int8_t), 1, file);
            bytes_written++;
        }
        for (int i = 0; i < delimiter_index; ++i) {
            fwrite(&(chain_table[i].res), sizeof(int8_t), 1, file);
            bytes_written++;
        }
        for (int i = delimiter_index + 1; i < alphabet_size; ++i) {
            fwrite(&(chain_table[i - 1].res), sizeof(int8_t), 1, file);
            bytes_written++;
        }
    }
    // printf("bytes_written: %d\n", bytes_written);
    // printf("writing stream bitsize: %zu\n", diff_writing_stream.ms.total_bitsize);
    // printf("wnum_diffs: %zu, wnum_samples: %d\n", diffs.size, num_samples);
    fwrite(&(samples.size), sizeof(size_t), 1, file);
    fwrite(samples.data, sizeof(uint16_t), samples.size, file);
    // saving diff would change result to simply choosing top occurence diffs (one of proposed improvements)
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(&(diffs.size), sizeof(size_t), 1, file);
    fwrite(&(diff_writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(diff_writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);
    // FILE *dbg_file = fopen("encoded_raw_data.csv", "w+");
    // fprintf(dbg_file, "id,encoded,udiff,d64,res,diff\n");
    // for (int i = 1; i < num_samples; ++i) {
    //     uint8_t diff_char = diffs.data[i - 1];
    //     int alphabet_index = diff_char;
    //     if (diff_char > delimiter_index) alphabet_index--;
    //     int d64 = (diff_char != delimiter_index) ? chain_table[alphabet_index].d64 : -1111;
    //     int res = (diff_char != delimiter_index) ? chain_table[alphabet_index].res : -1111;
    //     fprintf(dbg_file, "%6d,%6d,%6d,%6d,%6d,%6d\n", i, (int16_t)(data[i] - 32768), diff_char, d64, res, (diff_char != delimiter_index) ? (d64 * 64 + res) : -1111);
    // }
    // fclose(dbg_file);
    free(occ);
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














