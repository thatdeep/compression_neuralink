#include "heap.h"

static int key_compare(Heap *heap, int a, int b) {
    if (heap->type == MIN_HEAP) {
        return a < b;
    } else {
        return a > b;
    }
}

void swim(Heap *heap, int index) {
    int parent_index; 
    HeapEntry temp;
    while (index) {
        parent_index = (index - 1) / 2;
        if (key_compare(heap, heap->data[index].key, heap->data[parent_index].key)) {
            temp = heap->data[parent_index];
            heap->data[parent_index] = heap->data[index];
            heap->data[index] = temp;
            index = parent_index;
        } else {
            return;
        }
    }
    return;
}

void sink(Heap *heap, int index) {
    HeapEntry temp;
    while (1) {
        int left_child_index = 2 * index + 1, right_child_index = 2 * (index + 1);
        int selected_child_index = 0;
        if (left_child_index >= heap->size) return;
        selected_child_index = left_child_index;
        if (right_child_index < heap->size && key_compare(heap, heap->data[right_child_index].key, heap->data[left_child_index].key)) {
            selected_child_index = right_child_index;
        }
        if (key_compare(heap, heap->data[selected_child_index].key, heap->data[index].key)) {
            temp = heap->data[selected_child_index];
            heap->data[selected_child_index] = heap->data[index];
            heap->data[index] = temp;
            index = selected_child_index;
        } else {
            return;
        }
    }
    return;
}

void *pop(Heap *heap) {
    if (heap->size == 0) {
        return NULL;
    }
    void *data_returned = heap->data[0].data;
    heap->data[0] = heap->data[--heap->size];
    sink(heap, 0);
    return data_returned;
}

void push(Heap *heap, int key, void *data) {
    if (heap->size >= heap->capacity) {
        size_t new_capacity = (heap->size * 3) / 2;
        HeapEntry *new_data = (HeapEntry *)malloc(sizeof(HeapEntry) * new_capacity);
        memcpy(new_data, heap->data, sizeof(HeapEntry) * heap->size);
        free(heap->data);
        heap->data = new_data;
        heap->capacity = new_capacity;
    }
    heap->data[heap->size++] = (HeapEntry){.key = key, .data = data};
    swim(heap, heap->size - 1);
}

int find_extreme_key(Heap *heap) {
    if (heap->size == 0) {
        return heap->type == MIN_HEAP ? INT_MAX : INT_MIN;
    }
    return heap->data[0].key;
}

void print_heap(Heap *heap, void (*print_data_function)(void *)) {
     for (int i = 0; i < heap->size; ++i) {
          printf("[%d:", heap->data[i].key);
          print_data_function(heap->data[i].data);
          printf("] ");
     }
     printf("\n");
}