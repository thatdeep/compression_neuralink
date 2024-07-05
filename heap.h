#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef int (*HeapKeyCompareFunc)(void *a, void *b);

typedef struct {
    void *key;
    void *data;
} HeapEntry;

typedef struct {
    size_t size;
    size_t capacity;
    HeapEntry *data;
    HeapKeyCompareFunc key_compare;
} Heap;

void swim_heap(Heap *heap, int index);

void sink_heap(Heap *heap, int index);

void *pop_heap(Heap *heap);

void push_heap(Heap *heap, void *key, void *data);

void *find_extreme_key_heap(Heap *heap);

void print_heap(Heap *heap, void (*print_key_function)(void *), void (*print_data_function)(void *));


#endif // HEAP_H