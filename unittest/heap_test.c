#include "../heap.h"

void print_int(void *data) {
    printf("%d", *(int *)data);
}

void test_min_heap() {
    printf("Testing Min-Heap:\n");
    Heap min_heap = {.size = 0, .capacity = 10, .data = malloc(10 * sizeof(HeapEntry)), .type = MIN_HEAP};

    int values[] = {5, 3, 8, 1, 2};
    for (int i = 0; i < 5; ++i) {
        push(&min_heap, values[i], &values[i]);
    }

    // Print the min-heap
    print_heap(&min_heap, print_int);

    // Test find_extreme_key
    assert(find_extreme_key(&min_heap) == 1);

    // Test pop
    assert(*(int *)pop(&min_heap) == 1);
    assert(find_extreme_key(&min_heap) == 2);

    assert(*(int *)pop(&min_heap) == 2);
    assert(find_extreme_key(&min_heap) == 3);

    assert(*(int *)pop(&min_heap) == 3);
    assert(find_extreme_key(&min_heap) == 5);

    assert(*(int *)pop(&min_heap) == 5);
    assert(find_extreme_key(&min_heap) == 8);

    assert(*(int *)pop(&min_heap) == 8);
    assert(min_heap.size == 0);

    free(min_heap.data);
    printf("Min-Heap test passed.\n");
}

void test_max_heap() {
    printf("Testing Max-Heap:\n");
    Heap max_heap = {.size = 0, .capacity = 10, .data = malloc(10 * sizeof(HeapEntry)), .type = MAX_HEAP};

    int values[] = {5, 3, 8, 1, 2};
    for (int i = 0; i < 5; ++i) {
        push(&max_heap, values[i], &values[i]);
    }

    // Print the max-heap
    print_heap(&max_heap, print_int);

    // Test find_extreme_key
    assert(find_extreme_key(&max_heap) == 8);

    // Test pop
    assert(*(int *)pop(&max_heap) == 8);
    assert(find_extreme_key(&max_heap) == 5);

    assert(*(int *)pop(&max_heap) == 5);
    assert(find_extreme_key(&max_heap) == 3);

    assert(*(int *)pop(&max_heap) == 3);
    assert(find_extreme_key(&max_heap) == 2);

    assert(*(int *)pop(&max_heap) == 2);
    assert(find_extreme_key(&max_heap) == 1);

    assert(*(int *)pop(&max_heap) == 1);
    assert(max_heap.size == 0);

    free(max_heap.data);
    printf("Max-Heap test passed.\n");
}

int main() {
    test_min_heap();
    test_max_heap();
    printf("All tests passed.\n");
    return 0;
}