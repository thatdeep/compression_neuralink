// TODO REWRITE THESE FUNCTIONS INTO CORRESPONDING UNIT TESTS
// MOVED HERE TO NOT LOSE

void test_write_read_streams() {
    int bitsizes[15] = {1, 3, 7, 15, 2, 30, 8, 2, 3, 5, 29, 15, 4, 3, 2};
    uint32_t bits[15] = {0xF, 0x2, 0x4, 0xA, 0x1, 0xFFA, 0x6, 0x1, 0x3, 0x5, 0x1C, 0xB, 0x2, 0x1, 0x3};
    uint32_t rbits[15] = {0};
    for (int i = 0; i < 15; ++i) {
        bits[i] &= ((1 << bitsizes[i]) - 1);
    }
    size_t streamsize = 15;
    char filename[] = "bitstream_testfile.bitstream";
    FILE *file = fopen(filename, "wb");
    size_t total_bytesize = 0, total_bitsize = 0;
    for (int i = 0; i < 15; ++i) {
        total_bitsize += bitsizes[i];
    }
    fwrite(&total_bitsize, sizeof(size_t), 1, file);
    total_bytesize = (total_bitsize % 8 == 0) ? (total_bitsize / 8) : (total_bitsize / 8 + 1);
    vecStream writing_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = total_bitsize,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * 10)
        },
        .stream_capacity = 10
    };
    for (int i = 0; i < 15; ++i) {
        write_bits_vecstream(&writing_stream, bits[i], bitsizes[i]);
    }
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.ms.current_bytesize, total_bytesize);
    finalize_vecstream(&writing_stream);
    printf("w| current_bytesize: %lu, total_bytesize: %lu\n", writing_stream.ms.current_bytesize, total_bytesize);
    printf("printing writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(writing_stream.ms.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");
    reverse_bits_memstream(&(writing_stream.ms));
    printf("printing reversed writing_stream byte-by-byte:\n");
    for (int i = 0; i < total_bytesize; ++i) {
        printf("[");
        print_uint8_t(writing_stream.ms.stream[i], 8);
        printf("]");
        if (i && (i % 10 == 0)) printf("\n");
    }
    printf("\n");

    fwrite(writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);

    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    bitStream reading_stream = (bitStream){
        .bit_buffer = 0,
        .bits_in_buffer = 0,
        .current_bytesize = 0,
        .total_bitsize = read_total_bitsize,
        .stream=file
    };

    int i = 0;
    size_t garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        //print_uint64_t(reading_stream.bit_buffer);
        printf("reading %lu garbage bits:\n", garbage_bitsize);
        read_bits_bitstream(&reading_stream, garbage_bitsize);
        //print_uint64_t(reading_stream.bit_buffer);
    }
    while (read_total_bitsize) {
        //print_uint64_t(reading_stream.bit_buffer);
        rbits[i] = read_bits_bitstream(&reading_stream, bitsizes[15 - i - 1]);
        printf("reading %d bits at reversed index %d\n", bitsizes[15 - i - 1], i);
        read_total_bitsize -= bitsizes[15 - i - 1];
        i++;
        //print_uint64_t(reading_stream.bit_buffer);
    }

    printf("r| current_bytesize: %lu, total_bytesize: %lu\n", reading_stream.current_bytesize, read_total_bytesize);
    for (int i = 0; i < 15; ++i) {
        printf("i = %d, b:\n", i);
        print_uint32_t(bits[i], bitsizes[i]);
        printf("\nrrb:\n");
        print_uint32_t(rbits[15 - i - 1], bitsizes[i]);
        //printf("b:%u --- rrb:%u\n", bits[i], rbits[15 - i - 1]);
        printf("\n");
    }
    fclose(file);
    free(writing_stream.ms.stream);
    return;
}


void test_tans_algorithm() {
    uint8_t alphabet[ASIZE] = {0};
    for (int i = 0; i < ASIZE; ++i) {
        alphabet[i] = (uint8_t)i;
    }
    for (int i = 0; i < ASIZE; ++i) {
        occ[i] = (ASIZE - i);
    }
    quantize_occurences();
    uint8_t text[20] = {1, 2, 5, 10, 0, 0, 2, 2, 5, 5, 10, 10, 128, 3, 4, 7, 4, 4, 4, 2};
    size_t dsize = 20;
    vecStream writing_stream = (vecStream){
        .ms = (memStream){
            .bit_buffer = 0,
            .bits_in_buffer = 0,
            .current_bytesize = 0,
            .total_bitsize = 0,
            .stream=(uint8_t *)malloc(sizeof(uint8_t) * 10)
        },
        .stream_capacity = 10
    };
    uint32_t final_state = encode(text, dsize, &writing_stream);
    printf("final state: %u\n", final_state);
    finalize_vecstream(&writing_stream);
    printf("memstream finalized.\n");
    reverse_bits_memstream(&(writing_stream.ms));
    size_t total_bytesize = (writing_stream.ms.total_bitsize % 8 == 0) ? (writing_stream.ms.total_bitsize / 8) : (writing_stream.ms.total_bitsize / 8 + 1);
    char filename[] = "bitstream_testfile.bitstream";
    FILE *file = fopen(filename, "wb");
    fwrite(&dsize, sizeof(size_t), 1, file);
    fwrite(&(writing_stream.ms.total_bitsize), sizeof(size_t), 1, file);
    fwrite(&final_state, sizeof(uint32_t), 1, file);
    fwrite(writing_stream.ms.stream, sizeof(uint8_t), total_bytesize, file);
    fclose(file);

    size_t recovered_text_size = 0;
    file = fopen(filename, "rb");
    size_t read_total_bytesize, read_total_bitsize;
    uint32_t decode_initial_state;
    fread(&recovered_text_size, sizeof(size_t), 1, file);
    fread(&read_total_bitsize, sizeof(size_t), 1, file);
    fread(&decode_initial_state, sizeof(uint32_t), 1, file);
    bitStream reading_stream = (bitStream){
        .bit_buffer = 0,
        .bits_in_buffer = 0,
        .current_bytesize = 0,
        .total_bitsize = read_total_bitsize,
        .stream = file
    };
    read_total_bytesize = (read_total_bitsize % 8 == 0) ? (read_total_bitsize / 8) : (read_total_bitsize / 8 + 1);
    size_t garbage_bitsize = (read_total_bitsize % 8 == 0) ? (0) : (8 - (read_total_bitsize % 8));
    if (garbage_bitsize) {
        printf("read %lu bits of garbage:\n", garbage_bitsize);
        read_bits_bitstream(&reading_stream, garbage_bitsize);
    }
    uint8_t *recovered_text = decode(recovered_text_size, read_total_bitsize, decode_initial_state, (memStream *)(&reading_stream));
    for (int i = 0; i < recovered_text_size; ++i) {
        printf("%d --- %d\n", text[i], recovered_text[recovered_text_size - i - 1]);
    }
    free(recovered_text);
    free(writing_stream.ms.stream);
}