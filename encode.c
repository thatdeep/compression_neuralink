#include <stdio.h>
#include <stdlib.h>
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
    size_t size;
    size_t capacity;
    int16_t *data;
} Vector16;

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

int16_t *read_wav_file(const char *filename, WAVHeader *header, size_t *num_samples) {
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
    fclose(file);
    return data;
}

void init_vector16(Vector16 *vec, size_t capacity) {
    vec->data = (int16_t *)malloc(sizeof(int16_t) * capacity);
    vec->size = 0;
    vec->capacity = capacity;
}

void init_vector8(Vector8 *vec, size_t capacity) {
    vec->data = (int8_t *)malloc(sizeof(int8_t) * capacity);
    vec->size = 0;
    vec->capacity = capacity;
}

void init_uvector8(UVector8 *vec, size_t capacity) {
    vec->data = (uint8_t *)malloc(sizeof(uint8_t) * capacity);
    vec->size = 0;
    vec->capacity = capacity;
}

void pack_and_write(FILE *file, uint8_t *data, size_t size, int bit_size) {
    int bit_buffer = 0;
    int bit_count = 0;
    uint8_t bitmask = (1 << bit_size) - 1;

    for (size_t i = 0; i < size; ++i) {
        bit_buffer = (bit_buffer << bit_size) | (data[i] & bitmask);
        bit_count += bit_size;

        while (bit_count >= 8) {
            fputc((bit_buffer >> (bit_count - 8)) & 0xFF, file);
            bit_count -= 8;
        }
    }

    if (bit_count > 0) {
        fputc((bit_buffer << (8 - bit_count)) & 0xFF, file);
    }
}


void compress(int16_t *data, Vector16 *samples, UVector8 *d64_data, UVector8 *dr_data, int num_samples, int bit_size) {
    init_vector16(samples, num_samples);
    init_uvector8(d64_data, num_samples);
    init_uvector8(dr_data, num_samples);
    int pow2 = 1 << (bit_size - 1);
    samples->data[samples->size++] = data[0];

    for (int i = 1; i < num_samples; ++i) {
        int diff = data[i] - data[i - 1];
        int d64 = round(1.0 * diff / 64);
        if ((d64 >= -pow2) && (d64 < pow2) && (abs(diff - d64 * 64) <= 1)) {
            d64_data->data[d64_data->size++] = (uint8_t)(d64 + pow2); // int8_t->uint8_t
            dr_data->data[dr_data->size++] = (uint8_t)(diff - d64 * 64 + 1); // -1/0/1 -> 0/1/2
        } else {
            dr_data->data[dr_data->size++] = 3; // 3 signals end of the mul64 diff chain, new anchor is needed
            samples->data[samples->size++] = data[i];
        }
    }
    return;
}


// Function to write compressed data to a file
void write_compressed_file(const char *filename, int16_t *data, int num_samples) {
    Vector16 samples, samples2;
    UVector8 d64_data, d64_data2;
    UVector8 dr_data, dr_data2;
    compress(data, &samples, &d64_data, &dr_data, num_samples, 4);
    compress(samples.data, &samples2, &d64_data2, &dr_data2, samples.size, 6);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    fwrite(&samples2.size, sizeof(size_t), 1, file);
    fwrite(&d64_data2.size, sizeof(size_t), 1, file);
    fwrite(&dr_data2.size, sizeof(size_t), 1, file);
    fwrite(&d64_data.size, sizeof(size_t), 1, file);
    fwrite(&dr_data.size, sizeof(size_t), 1, file);
    fwrite(samples2.data, sizeof(int16_t), samples2.size, file);
    pack_and_write(file, d64_data2.data, d64_data2.size, 6);
    pack_and_write(file, dr_data2.data, dr_data2.size, 2);
    pack_and_write(file, d64_data.data, d64_data.size, 4);
    pack_and_write(file, dr_data.data, dr_data.size, 2);
    fclose(file);

    free(samples.data);
    free(d64_data.data);
    free(dr_data.data);
    free(samples2.data);
    free(d64_data2.data);
    free(dr_data2.data);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <input-wav-file> <compressed-file>\n", argv[0]);
        return 1;
    }
    WAVHeader header;
    size_t num_samples;

    int16_t *data = read_wav_file(argv[1], &header, &num_samples);
    write_compressed_file(argv[2], data, num_samples);

    free(data);
    return 0;
}