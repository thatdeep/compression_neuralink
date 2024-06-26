#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

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
    int16_t *data;
    size_t size;
} Vector16;

typedef struct {
    uint16_t *data;
    size_t size;
} UVector16;

typedef struct {
    int8_t *data;
    size_t size;
} Vector8;

typedef struct {
    uint8_t *data;
    size_t size;
} UVector8;


#define SAMPLE_TABLE_SIZE 4096
//#define SAMPLE_TABLE_EMPTY_KEY -1

//static TableEntry sample_table[SAMPLE_TABLE_SIZE];
static uint16_t sample_stack[SAMPLE_TABLE_SIZE];


unsigned int nextPowerOf2(unsigned int n) {
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

int powerOfTwoExponent(unsigned int x) {
    if (x == 0)
        return -1;

    int exponent = 31 - __builtin_clz(x); // 7/15/31/63 for 8/16/32/64-bit inputs
    return exponent;
}

void unpack_bits(FILE *file, uint8_t *data, size_t size, int bit_size) {
    int bit_buffer = 0;
    int bit_count = 0;
    uint8_t byte;

    for (size_t i = 0; i < size; ++i) {
        while (bit_count < bit_size) {
            byte = fgetc(file);
            // if (byte == EOF) {
            //     fprintf(stderr, "Unexpected end of file.\n");
            //     exit(EXIT_FAILURE);
            // }
            bit_buffer = (bit_buffer << 8) | (byte & 0xFF);
            bit_count += 8;
        }
        data[i] = (bit_buffer >> (bit_count - bit_size)) & ((1 << bit_size) - 1);

        bit_count -= bit_size;
    }
}

void unpack_bits16(FILE *file, uint16_t *data, size_t size, int bit_size) {
    int bit_buffer = 0;
    int bit_count = 0;
    uint8_t byte;

    for (size_t i = 0; i < size; ++i) {
        while (bit_count < bit_size) {
            byte = fgetc(file);
            // if (byte == EOF) {
            //     fprintf(stderr, "Unexpected end of file.\n");
            //     exit(EXIT_FAILURE);
            // }
            bit_buffer = (bit_buffer << 8) | (byte & 0xFF);
            bit_count += 8;
        }
        data[i] = (bit_buffer >> (bit_count - bit_size)) & ((1 << bit_size) - 1);

        bit_count -= bit_size;
    }
}

uint16_t *decompress(UVector16 *samples, UVector8 *d64_data, UVector8 *dr_data, int bit_size) {
    int num_samples = dr_data->size + 1;
    int pow2 = 1 << (bit_size - 1);

    uint16_t *output_data = (uint16_t *)malloc(sizeof(uint16_t) * (num_samples));
    output_data[0] = samples->data[0];

    size_t d64_index = 0, dr_index = 0, sample_index = 1;

    for (int i = 1; i < num_samples; ++i) {
        if (dr_data->data[dr_index] == 3) {
            output_data[i] = samples->data[sample_index++];
            dr_index++;
        } else {
            int d64 = d64_data->data[d64_index++] - pow2;
            int remainder = dr_data->data[dr_index++] - 1;
            output_data[i] = (uint16_t)((int)output_data[i - 1] + (d64 * 64) + remainder);
        }
    }
    return output_data;
}


void read_compressed_file(const char *filename, int16_t **data, size_t *num_samples) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    size_t d64_data_size, dr_data_size, samples_size, sample_stacksize;

    fread(&sample_stacksize, sizeof(size_t), 1, file);
    fread(&samples_size, sizeof(size_t), 1, file);
    fread(&d64_data_size, sizeof(size_t), 1, file);
    fread(&dr_data_size, sizeof(size_t), 1, file);

    UVector16 samples = {.data = (uint16_t *)malloc(sizeof(uint16_t) * samples_size), .size = samples_size};
    UVector8 d64_data = {.data = (uint8_t *)malloc(sizeof(uint8_t) * d64_data_size), .size = d64_data_size};
    UVector8 dr_data = {.data = (uint8_t *)malloc(sizeof(uint8_t) * dr_data_size), .size = dr_data_size};

    //fread(samples2.data, sizeof(int16_t), samples2.size, file);
    fread(sample_stack, sizeof(uint16_t), sample_stacksize, file);
    unpack_bits16(file, samples.data, samples.size, powerOfTwoExponent(nextPowerOf2(sample_stacksize)));
    unpack_bits(file, d64_data.data, d64_data.size, 4);
    unpack_bits(file, dr_data.data, dr_data.size, 2);
    fclose(file);

    for (int i = 0; i < samples.size; ++i) {
        samples.data[i] = sample_stack[samples.data[i]];
    }

    *num_samples = dr_data_size + 1;
    *data = (int16_t *)decompress(&samples, &d64_data, &dr_data, 4);
    for (int i = 0; i < *num_samples; ++i) {
        (*data)[i] -= 32768;
    }

    free(samples.data);
    free(d64_data.data);
    free(dr_data.data);
    return;
}

void write_wav_file(const char *filename, int16_t *data, WAVHeader header, int num_samples) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to open file '%s'\n", filename);
        exit(1);
    }
    fwrite(&header, sizeof(WAVHeader), 1, file);
    fwrite(data, sizeof(int16_t), num_samples, file);

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <compressed-file> <output-wav-file>\n", argv[0]);
        return 1;
    }
    WAVHeader header;
    size_t num_samples;

    int16_t *data;
    read_compressed_file(argv[1], &data, &num_samples);

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

    write_wav_file(argv[2], data, header, num_samples);

    free(data);
    return 0;
}