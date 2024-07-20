#include "../bwt.h"
#include <stdio.h>

void test_bwt(const char* test_name, uint32_t *input, int n, int alphabet_size) {
    int sentinel_index;
    uint32_t *bwt_result = bwt(input, n, alphabet_size, &sentinel_index);
    uint32_t *inverse_result = bwt_inverse(bwt_result, n, alphabet_size, sentinel_index);

    printf("%s: ", test_name);
    int failed = 0;

    for (int i = 0; i < n; ++i) {
        //printf("i=%d, input[%d]=[%x], decod[%d]=[%x]\n", i, i, input[i], i, inverse_result[i]);
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

    const int n3 = 5000;
    uint32_t input3[n3] = {0};
    int alphabet_size3 = 256;

    srand(13);

    for (int i = 0; i < n3; i++) {
        input3[i] = ((rand() % alphabet_size3) + alphabet_size3) % alphabet_size3;
    }
    test_bwt("Test 3", input3, (int)n3, alphabet_size3);

    const int n4 = 200000;
    uint32_t input4[n4] = {0};
    int alphabet_size4 = 65536;

    srand(666);

    for (int i = 0; i < n4; i++) {
        input4[i] = ((rand() % alphabet_size4) + alphabet_size4) % alphabet_size4;
    }
    test_bwt("Test 4", input4, (int)n4, alphabet_size4);

    // Add more tests for different strings

    return 0;
}