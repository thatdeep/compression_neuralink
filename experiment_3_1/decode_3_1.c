#include "../wavio.h"
#include "../stream.h"
#include "../vector.h"
#include "../ans.h"
#include "assert.h"
#include "math.h"

int16_t *read_compressed_file(const char *filename, size_t *num_samples) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }
    int alphabet_size;
    int quant_pow;
    fread(&alphabet_size, sizeof(int), 1, file);
    fread(&quant_pow, sizeof(int), 1, file);
    uint16_t temp16, *samples;
    int8_t *d64_table, *res_table;
    size_t samples_size, read_total_bitsize, read_total_bytesize, garbage_bitsize, diffs_size;
    uint32_t final_state;
    int32_t *occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);


    // for (int i = 0; i < alphabet_size; ++i) {
    //     fread(&temp16, sizeof(uint16_t), 1, file);
    //     occ[i] = (int32_t)temp16;
    // }
    fread(occ, sizeof(int32_t), alphabet_size, file);
    d64_table = (int8_t *)malloc(sizeof(int8_t) * alphabet_size);
    res_table = (int8_t *)malloc(sizeof(int8_t) * alphabet_size);
    d64_table[0] = 0;
    res_table[0] = 0;
    fread(d64_table + 1, sizeof(int8_t), (alphabet_size - 1), file);
    fread(res_table + 1, sizeof(int8_t), (alphabet_size - 1), file);
    fread(&samples_size, sizeof(size_t), 1, file);
    samples = (uint16_t *)malloc(sizeof(uint16_t) * samples_size);
    fread(samples, sizeof(uint16_t), samples_size, file);
    fread(&final_state, sizeof(uint32_t), 1, file);
    fread(&diffs_size, sizeof(size_t), 1, file);
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    vecStream diff_reading_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = read_total_bitsize,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * read_total_bytesize)
        },
        .stream_capacity = read_total_bytesize
    };
    fread(diff_reading_stream.ms.stream, sizeof(uint8_t), read_total_bytesize, file);
    garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        read_bits_memstream(&(diff_reading_stream.ms), garbage_bitsize);
    }
    uint8_t *diffs_recovered = decode(diffs_size, read_total_bitsize, final_state, &(diff_reading_stream.ms), occ, alphabet_size, quant_pow);
    for (int i = 0; i < diffs_size / 2; ++i) {
        uint8_t temp = diffs_recovered[i];
        diffs_recovered[i] = diffs_recovered[diffs_size - i - 1];
        diffs_recovered[diffs_size - i - 1] = temp;
    }
    *num_samples = diffs_size + 1;
    size_t diff_index = 0, sample_index = 1;
    uint16_t *udata = (uint16_t *)malloc(sizeof(uint16_t) * (size_t)(*num_samples));
    udata[0] = samples[0];
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
    int16_t *data = (int16_t *)udata;
    for (int i = 0; i < *num_samples; ++i) {
        data[i] -= 32768;
    }
    fclose(file);
    free(occ);
    free(diff_reading_stream.ms.stream);
    return data;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <compressed-file> <output-wav-file>\n", argv[0]);
        return 1;
    }

    size_t num_samples;
    int16_t *data = read_compressed_file(argv[1], &num_samples);
    write_wav_file(argv[2], data, num_samples);

    return 0;
}














