#include "../stream.h"
#include "../vector.h"
#include "../wavio.h"
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

int rice_write(int val, uint32_t k, vecStream *vs) {
    uint32_t uval = val;
    uval <<= 1;
    uval ^= (val >> 31); // clamp [-n, n] to [0, 2n - 1] range, two-sided geometric to one-sided

    uint32_t msbs = uval >> k;
    uint32_t lsbs = 1 + k;
    uint32_t count = msbs + lsbs;
    for (int i = 0; i < msbs / 32; ++i) {
        write_bits_vecstream(vs, 0, 32);
    }
    uint32_t pattern = 1 << (msbs % 32); // mod 32 in [0..31] so we always have 1 placed inside uint32_t
    write_bits_vecstream(vs, pattern, (msbs % 32) + 1);
    write_bits_vecstream(vs, uval & (((uint32_t)1 << k) - 1), k);
    return msbs + lsbs;
}

void write_compressed_file(const char *filename, uint16_t *data, int num_samples) {
    for (int i = 0; i < num_samples; ++i) {
        data[i] -= 32768;
    }
    int16_t *idata = (int16_t *)data;
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
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    float rice_k = 3;
    fwrite(&num_samples, sizeof(int), 1, file);
    int16_t prev_quantized = 0;
    for (int i = 0; i < num_samples; ++i) {
        int quantized = quant_encode_int(idata[i]);
        int residual = quantized - prev_quantized;
        prev_quantized = quantized;
        int encoded_len = rice_write(residual, (uint32_t)rice_k, &diff_writing_stream);
        rice_k = rice_k * 0.99 + (encoded_len / 1.55) * 0.01;
    }

    finalize_vecstream(&diff_writing_stream);
    size_t total_bytesize = (diff_writing_stream.ms.total_bitsize % 8 == 0)
                            ? (diff_writing_stream.ms.total_bitsize / 8)
                            : (diff_writing_stream.ms.total_bitsize / 8 + 1);
    fwrite(&(diff_writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(diff_writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);
    free(diff_writing_stream.ms.stream);
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














