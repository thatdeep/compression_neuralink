#include "stream.h"

void fill_buffer_bitstream(bitStream *bs) {
    size_t total_bytesize = (bs->total_bitsize % 8 == 0) ? (bs->total_bitsize / 8) : (bs->total_bitsize / 8 + 1);
    while ((bs->bits_in_buffer <= 56) && (bs->current_bytesize < total_bytesize)) {
        uint8_t next_byte = fgetc(bs->stream);
        bs->bit_buffer |= ((uint64_t)next_byte << bs->bits_in_buffer);
        bs->bits_in_buffer += 8;
        bs->current_bytesize++;
    }
}

uint32_t read_bits_bitstream(bitStream *bs, int n) {
    if (bs->bits_in_buffer < n) fill_buffer_bitstream(bs);
    uint32_t result = (uint32_t)(bs->bit_buffer & (uint64_t)((1 << n) - 1));
    bs->bit_buffer >>= n;
    bs->bits_in_buffer -= n;
    return result;
}

void flush_buffer_memstream(memStream *ms) {
    while (ms->bits_in_buffer >= 8) {
        ms->stream[ms->current_bytesize++] = (uint8_t)(ms->bit_buffer & 0xFF);
        ms->bit_buffer >>= 8;
        ms->bits_in_buffer -= 8;
    }
}

// bits input:  last bits -> [{zero_padding}{higher}...{lower}] <- first bits
void write_bits_memstream(memStream *ms, uint32_t bits, int n) {
    if (ms->bits_in_buffer + n > 64) {
        flush_buffer_memstream(ms);
    }
    bits = (bits & (((uint32_t)1 << n) - 1));
    ms->bit_buffer |= ((uint64_t)bits << ms->bits_in_buffer);
    ms->bits_in_buffer += n;
}

void finalize_memstream(memStream *ms) {
    flush_buffer_memstream(ms);
    if (ms->bits_in_buffer > 0) {
        ms->stream[ms->current_bytesize++] = (uint8_t)(ms->bit_buffer & ((1 << ms->bits_in_buffer) - 1));
    }
}

void fill_buffer_memstream(memStream *ms) {
    size_t total_bytesize = (ms->total_bitsize % 8 == 0) ? (ms->total_bitsize / 8) : (ms->total_bitsize / 8 + 1);
    while ((ms->bits_in_buffer <= 56) && (ms->current_bytesize < total_bytesize)) {
        uint8_t next_byte = ms->stream[ms->current_bytesize++];
        ms->bit_buffer |= ((uint64_t)next_byte << ms->bits_in_buffer);
        ms->bits_in_buffer += 8;
    }
}

uint32_t read_bits_memstream(memStream *ms, int n) {
    if (ms->bits_in_buffer < n) fill_buffer_memstream(ms);
    uint32_t result = (uint32_t)(ms->bit_buffer & (uint64_t)((1 << n) - 1));
    ms->bit_buffer >>= n;
    ms->bits_in_buffer -= n;
    return result;
}

void flush_buffer_vecstream(vecStream *vs) {
    while (vs->ms.bits_in_buffer >= 8) {
        if (vs->ms.current_bytesize + 1 >= vs->stream_capacity) {
            vs->stream_capacity = (vs->stream_capacity * 3) / 2;
            uint8_t *new_stream = (uint8_t *)malloc(sizeof(uint8_t) * vs->stream_capacity);
            memcpy(new_stream, vs->ms.stream, vs->ms.current_bytesize);
            free(vs->ms.stream);
            vs->ms.stream = new_stream;
        }
        vs->ms.stream[vs->ms.current_bytesize++] = (uint8_t)(vs->ms.bit_buffer & 0xFF);
        vs->ms.bit_buffer >>= 8;
        vs->ms.bits_in_buffer -= 8;
    }
}

// bits input:  last bits -> [{zero_padding}{higher}...{lower}] <- first bits
void write_bits_vecstream(vecStream *vs, uint32_t bits, int n) {
    if (vs->ms.bits_in_buffer + n > 64) {
        flush_buffer_vecstream(vs);
    }
    bits = (bits & (((uint32_t)1 << n) - 1));
    vs->ms.bit_buffer |= ((uint64_t)bits << vs->ms.bits_in_buffer);
    vs->ms.bits_in_buffer += n;
}

void finalize_vecstream(vecStream *vs) {
    flush_buffer_vecstream(vs); // specifically guarantees that last byte will be allocated
    if (vs->ms.bits_in_buffer > 0) {
        vs->ms.stream[vs->ms.current_bytesize++] = (uint8_t)(vs->ms.bit_buffer & ((1 << vs->ms.bits_in_buffer) - 1));
    }
}


void reverse_bits_memstream(memStream *ms) {
    // have         : [0:7|0:6|0:5|0:4|0:3|0:2|0:1|0:0][1:7|1:6|1:5|1:4|1:3|1:2|1:1|1:0]...[n:v|n:u|n:z|n:y|n:x|n:2|n:1|n:0]
    // revert bytes : [n:v|n:u|n:z|n:y|n:x|n:2|n:1|n:0]...[1:7|1:6|1:5|1:4|1:3|1:2|1:1|1:0][0:7|0:6|0:5|0:4|0:3|0:2|0:1|0:0]
    // revert bits  : [n:0|n:1|n:2|n:x|n:y|n:z|n:u|n:v]...[1:0|1:1|1:2|1:3|1:4|1:5|1:6|1:7][0:0|0:1|0:2|0:3|0:4|0:5|0:6|0:7]
    uint8_t temp;
    for (size_t i = 0; i < ms->current_bytesize / 2; ++i) {
        temp = ms->stream[i];
        ms->stream[i] = ms->stream[ms->current_bytesize - i - 1];
        ms->stream[ms->current_bytesize - i - 1] = temp;
    }
    for (size_t i = 0; i < ms->current_bytesize; ++i) {
        temp = ms->stream[i];
        temp = (temp & 0xF0) >> 4 | (temp & 0x0F) << 4;
        temp = (temp & 0xCC) >> 2 | (temp & 0x33) << 2;
        temp = (temp & 0xAA) >> 1 | (temp & 0x55) << 1;
        ms->stream[i] = temp;
    }
    return;
}