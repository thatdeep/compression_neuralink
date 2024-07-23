#include "../stream.h"
#include "../vector.h"
#include "../wavio.h"
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

static int32_t diff_occ_frame[1024];
static uint16_t smaller_diff_occ_frame[1024];

static uint16_t values_to_codes[32768];

void write_compressed_file(const char *filename, uint16_t *data, int num_samples) {
    const int alphabet_size = 512;
    const int binary_alphabet_size = 2;
    const int diff_codes_alphabet_size = 1024;
    int quant_pow = R + 2;
    for (int i = 0; i < alphabet_size; ++i) {
        values_to_codes[codes_to_values[i]] = i;
    }
    uint16_t *coded_data = (uint16_t *)malloc(sizeof(uint16_t) * num_samples);
    int32_t *occ = (int32_t *)malloc(sizeof(int32_t) * alphabet_size);
    memset(occ, 0, sizeof(int32_t) * alphabet_size);
    for (int i = 0; i < num_samples; ++i) {
        data[i] -= 32768;
    }
    int16_t *idata = (int16_t *)data;
    uint8_t prev_sign = 1;
    uint8_t *sign_change = (uint8_t *)malloc(sizeof(uint8_t) * num_samples);
    memset(sign_change, 0, sizeof(uint8_t) * num_samples);
    int32_t sign_change_occ[binary_alphabet_size] = {num_samples, 0};
    uint16_t value, code;
    for (int i = 0; i < num_samples; ++i) {
        if ((idata[i] < 0) && (prev_sign == 1)) {
            sign_change[i] = 1;
            prev_sign = 0;
            sign_change_occ[1]++;
            sign_change_occ[0]--;
        }
        if ((idata[i] > 0) && (prev_sign == 0)) {
            sign_change[i] = 1;
            prev_sign = 1;
            sign_change_occ[1]++;
            sign_change_occ[0]--;
        }
        value = (idata[i] < 0) ? -(idata[i] + 1) : idata[i];
        code = values_to_codes[value];
        occ[code]++;
        coded_data[i] = code;
    }
    memset(diff_occ_frame, 0, sizeof(int32_t) * diff_codes_alphabet_size);
    uint16_t *code_diffs = (uint16_t *)malloc(sizeof(uint16_t) * (num_samples - 1));
    //uint32_t *diff_occ_zero_code_ref = diff_occ_frame + diff_codes_alphabet_size / 2 - 1; // 511 - 511 = 0 and 511 + 511 = 1022, 1023 for stop-symbol
    int zero_index_ref = diff_codes_alphabet_size / 2 - 1;
    for (int i = 1; i < num_samples; ++i) {
        int diff_code_ref = (int)coded_data[i] - (int)coded_data[i - 1] + zero_index_ref;
        // printf("diff_code_ref[%4d]=%4d\n", i, diff_code_ref);
        diff_occ_frame[diff_code_ref]++;
        code_diffs[i - 1] = diff_code_ref;
    }
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
    uint32_t final_state = encode16(code_diffs, num_samples - 1, &diff_writing_stream, diff_occ_frame, diff_codes_alphabet_size, quant_pow);
    finalize_vecstream(&diff_writing_stream);
    size_t total_bytesize = (diff_writing_stream.ms.total_bitsize % 8 == 0)
                            ? (diff_writing_stream.ms.total_bitsize / 8)
                            : (diff_writing_stream.ms.total_bitsize / 8 + 1);
    reverse_bits_memstream(&(diff_writing_stream.ms));
    vecStream sign_writing_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = 0,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * 8192)
        },
        .stream_capacity = 8192
    };
    uint32_t sign_final_state = encode(sign_change, num_samples, &sign_writing_stream, sign_change_occ, binary_alphabet_size, quant_pow);
    finalize_vecstream(&sign_writing_stream);
    size_t sign_total_bytesize = (sign_writing_stream.ms.total_bitsize % 8 == 0)
                            ? (sign_writing_stream.ms.total_bitsize / 8)
                            : (sign_writing_stream.ms.total_bitsize / 8 + 1);
    reverse_bits_memstream(&(sign_writing_stream.ms));
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    // printf("num_samples=%d, final_state=%u\n", num_samples, final_state);
    fwrite(&quant_pow, sizeof(int), 1, file);
    uint16_t overhead_occ[4] = {0};
    uint8_t overhead_count = 0;
    for (int i = 0; i < diff_codes_alphabet_size; ++i) {
        if (diff_occ_frame[i] >= 65535) {
            // printf("big diff occ value=%d, overhead=%d\n", diff_occ_frame[i], (uint16_t)(diff_occ_frame[i] - 65535));
            overhead_occ[overhead_count++] = (uint16_t)(diff_occ_frame[i] - 65535);
            smaller_diff_occ_frame[i] = 65535;
        } else {
            smaller_diff_occ_frame[i] = (uint16_t)diff_occ_frame[i];    
        }
        
    }
    // printf("total overhead values=%d\n", overhead_count);
    fwrite(smaller_diff_occ_frame, sizeof(uint16_t), diff_codes_alphabet_size, file);
    if (overhead_count) {
        fwrite(overhead_occ, sizeof(uint16_t), overhead_count, file);
    }
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(&(num_samples), sizeof(int), 1, file);
    fwrite(coded_data, sizeof(uint16_t), 1, file);
    fwrite(&(diff_writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(diff_writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fwrite(sign_change_occ, sizeof(int32_t), binary_alphabet_size, file);
    fwrite(&sign_final_state, sizeof(uint32_t), 1, file);
    fwrite(&(sign_writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(sign_writing_stream.ms.stream, sizeof(uint8_t), sign_total_bytesize, file);
    fclose(file);

    free(occ);
    free(diff_writing_stream.ms.stream);
    free(sign_writing_stream.ms.stream);
    free(sign_change);
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














