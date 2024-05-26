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
    int8_t *data;
    size_t size;
} Vector8;

typedef struct {
    uint8_t *data;
    size_t size;
} UVector8;

// Function to unpack bits from the file
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


int16_t *decompress(Vector16 *samples, UVector8 *d64_data, UVector8 *dr_data, int bit_size) {
    int num_samples = dr_data->size + 1;
    int pow2 = 1 << (bit_size - 1);

    // Decompress data
    int16_t *output_data = (int16_t *)malloc(sizeof(int16_t) * (num_samples));
    output_data[0] = samples->data[0];

    size_t d64_index = 0, dr_index = 0, sample_index = 1;

    for (int i = 1; i < num_samples; ++i) {
        if (dr_data->data[dr_index] == 3) {
            output_data[i] = samples->data[sample_index++];
            dr_index++;
        } else {
            int d64 = d64_data->data[d64_index++] - pow2;
            int remainder = dr_data->data[dr_index++] - 1;
            output_data[i] = (int16_t)((int)output_data[i - 1] + (d64 * 64) + remainder);
        }
    }
    return output_data;
}


// Function to read compressed data from a file and decompress it
void read_compressed_file(const char *filename, int16_t **data, size_t *num_samples) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    size_t d64_data_size, dr_data_size;
    size_t samples2_size, d64_data2_size, dr_data2_size;

    // Read sizes of arrays
    fread(&samples2_size, sizeof(size_t), 1, file);
    fread(&d64_data2_size, sizeof(size_t), 1, file);
    fread(&dr_data2_size, sizeof(size_t), 1, file);
    //fread(&samples_size, sizeof(size_t), 1, file);
    fread(&d64_data_size, sizeof(size_t), 1, file);
    fread(&dr_data_size, sizeof(size_t), 1, file);

    // Allocate memory for arrays
    Vector16 samples2 = {.data = (int16_t *)malloc(sizeof(int16_t) * samples2_size), .size = samples2_size};
    UVector8 d64_data2 = {.data = (uint8_t *)malloc(sizeof(uint8_t) * d64_data2_size), .size = d64_data2_size};
    UVector8 dr_data2 = {.data = (uint8_t *)malloc(sizeof(uint8_t) * dr_data2_size), .size = dr_data2_size};
    UVector8 d64_data = {.data = (uint8_t *)malloc(sizeof(uint8_t) * d64_data_size), .size = d64_data_size};
    UVector8 dr_data = {.data = (uint8_t *)malloc(sizeof(uint8_t) * dr_data_size), .size = dr_data_size};

    // Read anchors
    fread(samples2.data, sizeof(int16_t), samples2.size, file);

    // Unpack 6-bit d64_data2
    unpack_bits(file, d64_data2.data, d64_data2.size, 6);

    // Unpack 2-bit dr_data2
    unpack_bits(file, dr_data2.data, dr_data2.size, 2);

    // Unpack 4-bit d64_data
    unpack_bits(file, d64_data.data, d64_data.size, 4);

    // Unpack 2-bit dr_data
    unpack_bits(file, dr_data.data, dr_data.size, 2);

    // printf("samples:\n");
    // for (int i = 0; i < 10; ++i) {
    //     for (int j = 0; j < 10; ++j) {
    //         printf("%7d ", samples.data[i * 10 + j]);
    //     }
    //     printf("\n");
    // }
    // printf("d64:\n");
    // for (int i = 0; i < 10; ++i) {
    //     for (int j = 0; j < 10; ++j) {
    //         printf("%7d ", d64_data.data[i * 10 + j]);
    //     }
    //     printf("\n");
    // }
    // printf("dr:\n");
    // for (int i = 0; i < 10; ++i) {
    //     for (int j = 0; j < 10; ++j) {
    //         printf("%7d ", dr_data.data[i * 10 + j]);
    //     }
    //     printf("\n");
    // }

    fclose(file);

    *num_samples = dr_data_size + 1;

    //printf("num_samples: %zu\n", *num_samples);

    // Decompress data
    Vector16 samples = {.data = decompress(&samples2, &d64_data2, &dr_data2, 6), .size = dr_data2.size + 1};
    //printf("dr_data2.size: %zu\n", dr_data2.size);

    *data = decompress(&samples, &d64_data, &dr_data, 4);
    //printf("dr_data.size: %zu\n", dr_data.size);

    // int16_t *output_data = (int16_t *)malloc(sizeof(int16_t) * (*num_samples));
    // output_data[0] = samples.data[0];

    // size_t d64_index = 0, dr_index = 0, sample_index = 1;

    // for (int i = 1; i < *num_samples; ++i) {
    //     if (dr_data.data[dr_index] == 3) {
    //         output_data[i] = samples.data[sample_index++];
    //         dr_index++;
    //     } else {
    //         int d64 = d64_data.data[d64_index++] - 16;
    //         int remainder = dr_data.data[dr_index++] - 1;
    //         output_data[i] = output_data[i - 1] + (d64 * 64) + remainder;
    //     }
    // }

    // Assign output data
    //*data = output_data;

    // Clean up
    free(samples.data);
    free(d64_data.data);
    free(dr_data.data);
    free(samples2.data);
    free(d64_data2.data);
    free(dr_data2.data);
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

// int main() {
//     // Sample data to write to the WAV file
//     int sample_rate = 19531;
//     int num_channels = 1;
//     int bits_per_sample = 16;
//     int data_size = 197398;
//     int num_samples = data_size / (bits_per_sample / 8);

//     int16_t *data = (int16_t *)malloc(num_samples * sizeof(int16_t));
//     if (!data) {
//         printf("Failed to allocate memory\n");
//         return 1;
//     }

//     // Fill the data array with some sample values for testing (sine wave, silence, etc.)
//     // Here we just fill with zeros as a placeholder
//     for (int i = 0; i < num_samples; i++) {
//         data[i] = 0; // Replace with actual data
//     }

//     write_wav_file("output.wav", data, num_samples, sample_rate, num_channels, bits_per_sample);

//     free(data);
//     return 0;
// }

int16_t *read_wav_file(const char *filename, WAVHeader *header, int *n) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open file '%s'\n", filename);
        exit(1);
    }

    //WAVHeader header;
    fread(header, sizeof(WAVHeader), 1, file);

    printf("Sample rate: %d\n", header->sampleRate);
    printf("Channels: %d\n", header->numChannels);
    printf("Bits per sample: %d\n", header->bitsPerSample);
    printf("Data size: %d\n", header->subchunk2Size);

    int num_samples = header->subchunk2Size / (header->bitsPerSample / 8);
    *n = num_samples;
    int16_t *data = (int16_t *) malloc(num_samples * sizeof(int16_t));
    if (!data) {
        printf("Failed to allocate memory\n");
        fclose(file);
        exit(1);
    }

    fread(data, header->bitsPerSample / 8, num_samples, file);

    // Process the data (placeholder for actual compression logic)
    // For now, just print the first 10 samples
    // for (int i = 0; i < 10 && i < num_samples; i++) {
    //     printf("%d\n", data[i]);
    // }

    fclose(file);
    return data;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <compressed-file> <output-wav-file>\n", argv[0]);
        return 1;
    }
    WAVHeader header;
    size_t num_samples;

    int16_t *data;// = read_wav_file(argv[1], &header, &num_samples);
    read_compressed_file(argv[1], &data, &num_samples);

    // for (int i = 0; i < 10; ++i) {
    //     for (int j = 0; j < 10; ++j) {
    //         printf("%7d ", data[i * 10 + j]);
    //     }
    //     printf("\n");
    // }

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