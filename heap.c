#include "heap.h"

void swim_heap(Heap *heap, int index) {
    int parent_index; 
    HeapEntry temp;
    while (index) {
        parent_index = (index - 1) / 2;
        if (heap->key_compare(heap->data[index].key, heap->data[parent_index].key) < 0) {
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

void sink_heap(Heap *heap, int index) {
    HeapEntry temp;
    while (1) {
        int left_child_index = 2 * index + 1, right_child_index = 2 * (index + 1);
        int selected_child_index = 0;
        if (left_child_index >= heap->size) return;
        selected_child_index = left_child_index;
        if (right_child_index < heap->size && heap->key_compare(heap->data[right_child_index].key, heap->data[left_child_index].key) < 0) {
            selected_child_index = right_child_index;
        }
        if (heap->key_compare(heap->data[selected_child_index].key, heap->data[index].key) < 0) {
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

void *pop_heap(Heap *heap) {
    if (heap->size == 0) {
        return NULL;
    }
    void *data_returned = heap->data[0].data;
    heap->data[0] = heap->data[--heap->size];
    sink_heap(heap, 0);
    return data_returned;
}

void push_heap(Heap *heap, void *key, void *data) {
    if (heap->size >= heap->capacity) {
        size_t new_capacity = (heap->size * 3) / 2;
        HeapEntry *new_data = (HeapEntry *)malloc(sizeof(HeapEntry) * new_capacity);
        memcpy(new_data, heap->data, sizeof(HeapEntry) * heap->size);
        free(heap->data);
        heap->data = new_data;
        heap->capacity = new_capacity;
    }
    heap->data[heap->size++] = (HeapEntry){.key = key, .data = data};
    swim_heap(heap, heap->size - 1);
}

void *find_extreme_key_heap(Heap *heap) {
    assert(heap->size > 0);
    return heap->data[0].key;
}

void print_heap(Heap *heap, void (*print_key_function)(void *), void (*print_data_function)(void *)) {
    for (int i = 0; i < heap->size; ++i) {
        printf("[");
        print_key_function(heap->data[i].key);
        printf(":");
        print_data_function(heap->data[i].data);
        printf("] ");
    }
    printf("\n");
}