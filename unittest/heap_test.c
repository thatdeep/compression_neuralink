#include "../heap.h"

// Function to print integer data
void print_int(void *data) {
    printf("%d", *(int *)data);
}

// Comparison function for min-heap
int compare_min(void *a, void *b) {
    return (*(int *)a > *(int *)b) - (*(int *)a < *(int *)b);
}

// Comparison function for max-heap
int compare_max(void *a, void *b) {
    return (*(int *)b > *(int *)a) - (*(int *)b < *(int *)a);
}

// Function to initialize a heap
void init_heap(Heap *heap, size_t capacity, HeapKeyCompareFunc compare_func) {
    heap->size = 0;
    heap->capacity = capacity;
    heap->data = malloc(capacity * sizeof(HeapEntry));
    heap->key_compare = compare_func;
}

// Test function for min-heap
void test_min_heap() {
    printf("Testing Min-Heap:\n");
    Heap min_heap;
    init_heap(&min_heap, 10, compare_min);

    int values[] = {5, 3, 8, 1, 2};
    for (int i = 0; i < 5; ++i) {
        push_heap(&min_heap, &values[i], &values[i]);
    }

    // Print the min-heap
    print_heap(&min_heap, print_int, print_int);

    // Test find_extreme_key_heap
    assert(*(int *)find_extreme_key_heap(&min_heap) == 1);

    // Test pop_heap
    assert(*(int *)pop_heap(&min_heap) == 1);
    assert(*(int *)find_extreme_key_heap(&min_heap) == 2);

    assert(*(int *)pop_heap(&min_heap) == 2);
    assert(*(int *)find_extreme_key_heap(&min_heap) == 3);

    assert(*(int *)pop_heap(&min_heap) == 3);
    assert(*(int *)find_extreme_key_heap(&min_heap) == 5);

    assert(*(int *)pop_heap(&min_heap) == 5);
    assert(*(int *)find_extreme_key_heap(&min_heap) == 8);

    assert(*(int *)pop_heap(&min_heap) == 8);
    assert(min_heap.size == 0);

    free(min_heap.data);
    printf("Min-Heap test passed.\n");
}

// Test function for max-heap
void test_max_heap() {
    printf("Testing Max-Heap:\n");
    Heap max_heap;
    init_heap(&max_heap, 10, compare_max);

    int values[] = {5, 3, 8, 1, 2};
    for (int i = 0; i < 5; ++i) {
        push_heap(&max_heap, &values[i], &values[i]);
    }

    // Print the max-heap
    print_heap(&max_heap, print_int, print_int);

    // Test find_extreme_key_heap
    assert(*(int *)find_extreme_key_heap(&max_heap) == 8);

    // Test pop_heap
    assert(*(int *)pop_heap(&max_heap) == 8);
    assert(*(int *)find_extreme_key_heap(&max_heap) == 5);

    assert(*(int *)pop_heap(&max_heap) == 5);
    assert(*(int *)find_extreme_key_heap(&max_heap) == 3);

    assert(*(int *)pop_heap(&max_heap) == 3);
    assert(*(int *)find_extreme_key_heap(&max_heap) == 2);

    assert(*(int *)pop_heap(&max_heap) == 2);
    assert(*(int *)find_extreme_key_heap(&max_heap) == 1);

    assert(*(int *)pop_heap(&max_heap) == 1);
    assert(max_heap.size == 0);

    free(max_heap.data);
    printf("Max-Heap test passed.\n");
}


typedef struct {
    int first;
    int second;
} IntPair;

int compare_int_pair(void *a, void *b) {
    IntPair *pair_a = (IntPair *)a;
    IntPair *pair_b = (IntPair *)b;

    if (pair_a->first < pair_b->first) return -1;
    if (pair_a->first > pair_b->first) return 1;
    // If the first elements are equal, compare the second elements
    if (pair_a->second < pair_b->second) return -1;
    if (pair_a->second > pair_b->second) return 1;

    return 0;
}

void print_int_pair(void *data) {
    IntPair *pair = (IntPair *)data;
    printf("(%d, %d)", pair->first, pair->second);
}

// Function to initialize a heap for IntPair
void init_pair_heap(Heap *heap, size_t capacity, HeapKeyCompareFunc compare_func) {
    heap->size = 0;
    heap->capacity = capacity;
    heap->data = malloc(capacity * sizeof(HeapEntry));
    heap->key_compare = compare_func;
}

// Test function for heap with IntPair keys
void test_pair_heap() {
    printf("Testing Pair-Heap:\n");
    Heap pair_heap;
    init_pair_heap(&pair_heap, 10, compare_int_pair);

    IntPair pairs[] = {{5, 2}, {3, 8}, {8, 1}, {1, 5}, {2, 7}};
    for (int i = 0; i < 5; ++i) {
        push_heap(&pair_heap, &pairs[i], &pairs[i]);
    }

    // Print the pair-heap
    print_heap(&pair_heap, print_int_pair, print_int_pair);

    // Test find_extreme_key_heap
    IntPair *extreme_key = find_extreme_key_heap(&pair_heap);
    assert(extreme_key->first == 1 && extreme_key->second == 5);

    // Test pop_heap
    IntPair *popped_pair = pop_heap(&pair_heap);
    assert(popped_pair->first == 1 && popped_pair->second == 5);

    extreme_key = find_extreme_key_heap(&pair_heap);
    assert(extreme_key->first == 2 && extreme_key->second == 7);

    popped_pair = pop_heap(&pair_heap);
    assert(popped_pair->first == 2 && popped_pair->second == 7);

    extreme_key = find_extreme_key_heap(&pair_heap);
    assert(extreme_key->first == 3 && extreme_key->second == 8);

    popped_pair = pop_heap(&pair_heap);
    assert(popped_pair->first == 3 && popped_pair->second == 8);

    extreme_key = find_extreme_key_heap(&pair_heap);
    assert(extreme_key->first == 5 && extreme_key->second == 2);

    popped_pair = pop_heap(&pair_heap);
    assert(popped_pair->first == 5 && popped_pair->second == 2);

    extreme_key = find_extreme_key_heap(&pair_heap);
    assert(extreme_key->first == 8 && extreme_key->second == 1);

    popped_pair = pop_heap(&pair_heap);
    assert(popped_pair->first == 8 && popped_pair->second == 1);

    assert(pair_heap.size == 0);

    free(pair_heap.data);
    printf("Pair-Heap test passed.\n");
}

int main() {
    test_min_heap();
    test_max_heap();
    test_pair_heap();
    printf("All tests passed.\n");
    return 0;
}