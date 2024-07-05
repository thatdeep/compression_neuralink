#include "vector.h"

void push_vector32(Vector32 *vec, int32_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        int32_t *new_data = (int32_t *)malloc(sizeof(int32_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(int32_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_uvector32(UVector32 *vec, uint32_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        uint32_t *new_data = (uint32_t *)malloc(sizeof(uint32_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(uint32_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_vector16(Vector16 *vec, int16_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        int16_t *new_data = (int16_t *)malloc(sizeof(int16_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(int16_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_uvector16(UVector16 *vec, uint16_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        uint16_t *new_data = (uint16_t *)malloc(sizeof(uint16_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(uint16_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_vector8(Vector8 *vec, int8_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        int8_t *new_data = (int8_t *)malloc(sizeof(int8_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(int8_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void push_uvector8(UVector8 *vec, uint8_t value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->size  * 3) / 2;
        uint8_t *new_data = (uint8_t *)malloc(sizeof(uint8_t) * new_capacity);
        memcpy(new_data, vec->data, sizeof(uint8_t) * vec->size);
        free(vec->data);
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

void free_vector32(Vector32 *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
}

void free_uvector32(UVector32 *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
}

void free_vector16(Vector16 *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
}

void free_uvector16(UVector16 *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
}

void free_vector8(Vector8 *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
}

void free_uvector8(UVector8 *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
}