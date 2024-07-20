#include "../bwt.h"
#include <stdio.h>

void test_bwt(const char* test_name, uint32_t *input, int n, int alphabet_size) {
    int sentinel_index;
    uint32_t *bwt_result = bwt(input, n, alphabet_size, &sentinel_index);
    uint32_t *inverse_result = bwt_inverse(bwt_result, n, alphabet_size, sentinel_index);

    printf("%s: ", test_name);
    int failed = 0;

    for (int i = 0; i < n; ++i) {
        printf("i=%d, input[%d]=%u, decod[%d]=%u\n", i, i, input[i], i, inverse_result[i]);
        if (input[i] != inverse_result[i]) {
            failed = 1;
        }
    }

    free(bwt_result);
    free(inverse_result);
    if (failed) {
        printf("Failed\n");    
    } else {
        printf("Passed\n");
    }
    return;
}

int main() {
    uint32_t input1[] = { 'm', 'i', 's', 's', 'i', 's', 's', 'i', 'p', 'p', 'i' };
    int n1 = 11;
    int alphabet_size1 = 256;  // Assuming ASCII input

    test_bwt("Test 1", input1, n1, alphabet_size1);

    uint32_t input2[] = { 'b', 'a', 'n', 'a', 'n', 'a' };
    int n2 = 6;
    int alphabet_size2 = 256;

    test_bwt("Test 2", input2, n2, alphabet_size2);

    // Add more tests for different strings

    return 0;
}