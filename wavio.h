#ifndef WAVIO_H
#define WAVIO_H

#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"

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
#pragma pack() // Reset to default packing


uint16_t *read_wav_file(const char *filename, WAVHeader *header, size_t *num_samples);
void write_wav_file(const char *filename, int16_t *data, int num_samples);

#endif // WAVIO_H