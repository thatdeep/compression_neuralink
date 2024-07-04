#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef enum {
	MIN_HEAP,
	MAX_HEAP
} HeapType;

typedef struct {
     int key;
     void *data;
} HeapEntry;

typedef struct {
     size_t size;
     size_t capacity;
     HeapEntry *data;
     HeapType type;
} Heap;

static int key_compare(Heap *heap, int a, int b);

void swim(Heap *heap, int index);

void sink(Heap *heap, int index);

void *pop(Heap *heap);

void push(Heap *heap, int key, void *data);

int find_extreme_key(Heap *heap);

void print_heap(Heap *heap, void (*print_data_function)(void *));


#endif // HEAP_H