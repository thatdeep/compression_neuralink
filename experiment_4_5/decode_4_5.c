#include "../wavio.h"
#include "../stream.h"
#include "../vector.h"
#include "../ans.h"
#include "assert.h"
#include "math.h"

// quant/dequant funcs found by https://github.com/phoboslab/neuralink_brainwire/blob/master/bwenc.c
int16_t quant_encode_int(int16_t value) {
    return (int16_t)floor(value / 64.0);
}

int16_t dequant_decode_int(int16_t code) {
    if (code >= 0) {
        return (int16_t)round((double)code * 64.061577 + 31.034184);
    }
    else {
        return (int16_t)(-round((double)(- code - 1) * 64.061577 + 31.034184) - 1);
    }
}

int rice_read(uint32_t k, int *code_length, memStream *ms) {
    uint32_t msbs = 0;
    uint32_t content;
    while ((content = show_bits_memstream(ms, 32)) == 0) {
        msbs += 32;
        read_bits_memstream(ms, 32);
    }
    msbs  += __builtin_ctz(content);
    read_bits_memstream(ms, (msbs % 32) + 1);
    uint32_t lsbs = 1 + k;
    content = read_bits_memstream(ms, k);
    *code_length = msbs + lsbs;
    uint32_t uvalue = (msbs << k) | ((content & (((uint64_t)1 << (k)) - 1)));
    int value;
    if (uvalue & 1) {
        value = -((int)(uvalue >> 1)) - 1;
    } else {
        value = (int)(uvalue >> 1);
    }
    return value;
}

int16_t *read_compressed_file(const char *filename, size_t *num_samples) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }
    size_t total_bitsize;
    int temp_num_samples;
    fread(&temp_num_samples, sizeof(int), 1, file);
    *num_samples = (size_t)temp_num_samples;
    fread(&total_bitsize, sizeof(size_t), 1, file);
    size_t total_bytesize = (total_bitsize % 8 == 0) ? (total_bitsize / 8) : (total_bitsize / 8 + 1);
    vecStream diffs_reading_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = total_bitsize,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * total_bytesize)
        },
        .stream_capacity = total_bytesize
    };
    fread(diffs_reading_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    int16_t *idata = (int16_t *)malloc(sizeof(int16_t) * (*num_samples));
    float rice_k = 3;
    int encoded_len;
    int16_t prev_quantized = 0;
    for (int i = 0; i < *num_samples; ++i) {
        int residual = rice_read(rice_k, &encoded_len, &diffs_reading_stream.ms);
        int quantized = prev_quantized + residual;
        prev_quantized = quantized;
        idata[i] = dequant_decode_int(quantized);
        rice_k = rice_k * 0.99 + (encoded_len / 1.55) * 0.01;
    }
    free(diffs_reading_stream.ms.stream);
    return idata;
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














