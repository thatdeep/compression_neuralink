#include "wavio.h"

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