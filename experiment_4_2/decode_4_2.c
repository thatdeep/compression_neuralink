#include "../wavio.h"
#include "../stream.h"
#include "../vector.h"
#include "../ans.h"
#include "assert.h"
#include "math.h"

static uint16_t codes_to_values[512] = {
    31,     95,    159,    223,    287,    351,    415,    479,    544,    608,    672,    736,    800,    864,    928,    992,   1056,   1120,   1184,   1248,   1312,   1376,   1440,   1504,   1569,   1633,   1697,   1761,   1825,   1889,   1953,   2017,   2081,   2145,   2209,   2273,   2337,   2401,   2465,   2529,   2593,   2658,   2722,   2786,   2850,   2914,   2978,   3042,   3106,   3170,   3234,   3298,   3362,   3426,   3490,   3554,   3618,   3683,   3747,   3811,   3875,   3939,   4003,   4067, 
  4131,   4195,   4259,   4323,   4387,   4451,   4515,   4579,   4643,   4708,   4772,   4836,   4900,   4964,   5028,   5092,   5156,   5220,   5284,   5348,   5412,   5476,   5540,   5604,   5668,   5733,   5797,   5861,   5925,   5989,   6053,   6117,   6181,   6245,   6309,   6373,   6437,   6501,   6565,   6629,   6693,   6757,   6822,   6886,   6950,   7014,   7078,   7142,   7206,   7270,   7334,   7398,   7462,   7526,   7590,   7654,   7718,   7782,   7847,   7911,   7975,   8039,   8103,   8167, 
  8231,   8295,   8359,   8423,   8487,   8551,   8615,   8679,   8743,   8807,   8872,   8936,   9000,   9064,   9128,   9192,   9256,   9320,   9384,   9448,   9512,   9576,   9640,   9704,   9768,   9832,   9897,   9961,  10025,  10089,  10153,  10217,  10281,  10345,  10409,  10473,  10537,  10601,  10665,  10729,  10793,  10857,  10922,  10986,  11050,  11114,  11178,  11242,  11306,  11370,  11434,  11498,  11562,  11626,  11690,  11754,  11818,  11882,  11946,  12011,  12075,  12139,  12203,  12267, 
 12331,  12395,  12459,  12523,  12587,  12651,  12715,  12779,  12843,  12907,  12971,  13036,  13100,  13164,  13228,  13292,  13356,  13420,  13484,  13548,  13612,  13676,  13740,  13804,  13868,  13932,  13996,  14061,  14125,  14189,  14253,  14317,  14381,  14445,  14509,  14573,  14637,  14701,  14765,  14829,  14893,  14957,  15021,  15086,  15150,  15214,  15278,  15342,  15406,  15470,  15534,  15598,  15662,  15726,  15790,  15854,  15918,  15982,  16046,  16110,  16175,  16239,  16303,  16367, 
 16431,  16495,  16559,  16623,  16687,  16751,  16815,  16879,  16943,  17007,  17071,  17135,  17200,  17264,  17328,  17392,  17456,  17520,  17584,  17648,  17712,  17776,  17840,  17904,  17968,  18032,  18096,  18160,  18225,  18289,  18353,  18417,  18481,  18545,  18609,  18673,  18737,  18801,  18865,  18929,  18993,  19057,  19121,  19185,  19250,  19314,  19378,  19442,  19506,  19570,  19634,  19698,  19762,  19826,  19890,  19954,  20018,  20082,  20146,  20210,  20274,  20339,  20403,  20467, 
 20531,  20595,  20659,  20723,  20787,  20851,  20915,  20979,  21043,  21107,  21171,  21235,  21299,  21364,  21428,  21492,  21556,  21620,  21684,  21748,  21812,  21876,  21940,  22004,  22068,  22132,  22196,  22260,  22324,  22389,  22453,  22517,  22581,  22645,  22709,  22773,  22837,  22901,  22965,  23029,  23093,  23157,  23221,  23285,  23349,  23414,  23478,  23542,  23606,  23670,  23734,  23798,  23862,  23926,  23990,  24054,  24118,  24182,  24246,  24310,  24374,  24438,  24503,  24567, 
 24631,  24695,  24759,  24823,  24887,  24951,  25015,  25079,  25143,  25207,  25271,  25335,  25399,  25463,  25528,  25592,  25656,  25720,  25784,  25848,  25912,  25976,  26040,  26104,  26168,  26232,  26296,  26360,  26424,  26488,  26553,  26617,  26681,  26745,  26809,  26873,  26937,  27001,  27065,  27129,  27193,  27257,  27321,  27385,  27449,  27513,  27578,  27642,  27706,  27770,  27834,  27898,  27962,  28026,  28090,  28154,  28218,  28282,  28346,  28410,  28474,  28538,  28602,  28667, 
 28731,  28795,  28859,  28923,  28987,  29051,  29115,  29179,  29243,  29307,  29371,  29435,  29499,  29563,  29627,  29692,  29756,  29820,  29884,  29948,  30012,  30076,  30140,  30204,  30268,  30332,  30396,  30460,  30524,  30588,  30652,  30717,  30781,  30845,  30909,  30973,  31037,  31101,  31165,  31229,  31293,  31357,  31421,  31485,  31549,  31613,  31677,  31742,  31806,  31870,  31934,  31998,  32062,  32126,  32190,  32254,  32318,  32382,  32446,  32510,  32574,  32638,  32702,  32767, 
};

static uint16_t values_to_codes[32768];

int16_t *read_compressed_file(const char *filename, size_t *num_samples) {
    const int alphabet_size = 512;
    const int binary_alphabet_size = 2;
    int quant_pow = R;
    for (int i = 0; i < alphabet_size - 1; ++i) {
        values_to_codes[codes_to_values[i]] = i;
    }
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }
    uint32_t final_state, sign_final_state;
    size_t read_total_bitsize, read_total_bytesize, garbage_bitsize;
    size_t sign_read_total_bitsize, sign_read_total_bytesize, sign_garbage_bitsize;
    int temp_num_samples;
    int32_t *occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    int32_t sign_change_occ[binary_alphabet_size] = {0};
    fread(&quant_pow, sizeof(int), 1, file);
    fread(occ, sizeof(int32_t), alphabet_size, file);
    fread(&final_state, sizeof(uint32_t), 1, file);
    fread(&temp_num_samples, sizeof(int), 1, file);
    *num_samples = (size_t)temp_num_samples;

    fread(&(read_total_bitsize), sizeof(size_t), 1, file);
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    vecStream sample_reading_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = read_total_bitsize,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * read_total_bytesize)
        },
        .stream_capacity = read_total_bytesize
    };
    fread(sample_reading_stream.ms.stream, sizeof(uint8_t), read_total_bytesize, file);
    fread(sign_change_occ, sizeof(int32_t), binary_alphabet_size, file);
    fread(&sign_final_state, sizeof(uint32_t), 1, file);
    fread(&(sign_read_total_bitsize), sizeof(size_t), 1, file);
    sign_read_total_bytesize = (sign_read_total_bitsize % 8 == 0) ? (sign_read_total_bitsize / 8) : (sign_read_total_bitsize / 8 + 1);
    vecStream sign_reading_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = sign_read_total_bitsize,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * sign_read_total_bytesize)
        },
        .stream_capacity = sign_read_total_bytesize
    };
    fread(sign_reading_stream.ms.stream, sizeof(uint8_t), sign_read_total_bytesize, file);
    fclose(file);
    garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        read_bits_memstream(&(sample_reading_stream.ms), garbage_bitsize);
    }
    uint16_t *samples_codes_recovered = decode16(*num_samples, read_total_bitsize, final_state, &(sample_reading_stream.ms), occ, alphabet_size, quant_pow);
    for (int i = 0; i < *num_samples / 2; ++i) {
        uint16_t temp = samples_codes_recovered[i];
        samples_codes_recovered[i] = samples_codes_recovered[*num_samples - i - 1];
        samples_codes_recovered[*num_samples - i - 1] = temp;
    }
    sign_garbage_bitsize = (sign_read_total_bitsize % 8 == 0) ? (0) : (8 - (sign_read_total_bitsize % 8));
    if (sign_garbage_bitsize) {
        read_bits_memstream(&(sign_reading_stream.ms), sign_garbage_bitsize);
    }
    uint8_t *signs_recovered = decode(*num_samples, sign_read_total_bitsize, sign_final_state, &(sign_reading_stream.ms), sign_change_occ, binary_alphabet_size, quant_pow);
    for (int i = 0; i < *num_samples / 2; ++i) {
        uint8_t temp = signs_recovered[i];
        signs_recovered[i] = signs_recovered[*num_samples - i - 1];
        signs_recovered[*num_samples - i - 1] = temp;
    }
    int16_t *data = (int16_t *)malloc(sizeof(int16_t) * (*num_samples));
    uint16_t code, value;
    uint8_t prev_sign = 1;
    uint8_t sign_changed = 0;
    for (int i = 0; i < *num_samples; ++i) {
        sign_changed = 0;
        //samples_codes_recovered[i] = codes_to_values[samples_codes_recovered[i]] - 32768;
        code = samples_codes_recovered[i];
        value = codes_to_values[code];
        // determine if we should flip sign of data
        if (signs_recovered[i] == 1) {
            prev_sign = (prev_sign == 1) ? 0 : 1;
            sign_changed = 1;
        }
        if (prev_sign) {
            data[i] = value;
        } else {
            data[i] = (-value) - 1;
        }
    }
    free(occ);
    free(sample_reading_stream.ms.stream);
    free(sign_reading_stream.ms.stream);
    free(signs_recovered);
    free(samples_codes_recovered);
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














