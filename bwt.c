#include "bwt.h"

int compare_uint32_t(const void* a, const void* b) {
    uint32_t arg1 = *(const uint32_t*)a;
    uint32_t arg2 = *(const uint32_t*)b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int32_t *sort_cyclic_shifts(int32_t *s, int n, int alphabet_size) {
    int nmax = (alphabet_size < n) ? n : alphabet_size;
    int32_t *p = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *c = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *cnt = (int32_t *)malloc(sizeof(int32_t) * nmax);
    memset(cnt, 0, sizeof(int32_t) * nmax);

    for (int i = 0; i < n; ++i) {
        cnt[s[i]]++;
    }
    for (int i = 1; i < alphabet_size; ++i) {
        cnt[i] += cnt[i - 1];
    }
    for (int i = n - 1; i >= 0; --i) {
        p[--cnt[s[i]]] = i;
    }
    c[p[0]] = 0;
    int classes = 1;
    for (int i = 1; i < n; ++i) {
        if (s[p[i]] != s[p[i - 1]]) {
            ++classes;
        }
        c[p[i]] = classes - 1;
    }

    int32_t *pn = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *cn = (int32_t *)malloc(sizeof(int32_t) * n);
    for (int h = 0; (1 << h) < n; ++h) {
        for (int i = 0; i < n; ++i) {
            pn[i] = p[i] - (1 << h);
            if (pn[i] < 0) {
                pn[i] += n;
            }
        }
        memset(cnt, 0, sizeof(int32_t) * classes);
        for (int i = 0; i < n; ++i) {
            cnt[c[pn[i]]]++;
        }
        for (int i = 1; i < classes; ++i) {
            cnt[i] += cnt[i - 1];
        }
        for (int i = n - 1; i >= 0; --i) {
            p[--cnt[c[pn[i]]]] = pn[i];
        }
        cn[p[0]] = 0;
        classes = 1;
        for (int i = 1; i < n; ++i) {
            int cur1 = c[p[i]];
            int cur2 = c[(p[i] + (1 << h)) % n];
            int prev1 = c[p[i - 1]];
            int prev2 = c[(p[i - 1] + (1 << h)) % n];
            if (cur1 != prev1 || cur2 != prev2) {
                ++classes;
            }
            cn[p[i]] = classes - 1;
        }
        int32_t *temp = c;
        c = cn;
        cn = temp;
    }
    // for (int i = 0; i < n; ++i) {
    //     if (p[i] == 0) {
    //         *original_index = i;
    //         break;
    //     }
    // }
    free(c);
    free(cnt);
    free(pn);
    free(cn);
    return p;
}

uint32_t *bwt(uint32_t *s, int n, int alphabet_size, int *sentinel_index) {
    // thats how I see it: symbols 0 <= s[i], sentinel symbol will be -1 then for simplicity
    int sentinel_symbol = -1;
    int32_t *ss = (int32_t *)malloc(sizeof(int32_t) * (n + 1));
    // what about simply memsetting (assuming you are using s[i] with leading bit zero)
    for (int i = 0; i < n; ++i) {
        ss[i] = (int32_t)(s[i]) - sentinel_symbol;
    }
    printf("string in bwt:\n");
    for (int i = 0; i < n; ++i) {
        printf("[%x]", ss[i] + sentinel_symbol);
    }
    printf("\n");
    printf("as int32 numbers:\n");
    for (int i = 0; i < n; ++i) {
        printf("%3d ", ss[i] + sentinel_symbol);
    }
    printf("\n");
    ss[n] = 0;//sentinel_symbol - sentinel_symbol;
    int32_t *p = sort_cyclic_shifts(ss, n + 1, alphabet_size + 1);
    for (int i = 0; i < n + 1; ++i) {
        ss[i] += sentinel_symbol;
    }
    uint32_t *last_column = (uint32_t *)malloc(sizeof(uint32_t) * n);
    for (int i = 0; i < n + 1; ++i) {
        if (ss[(p[i] + n) % (n + 1)] == sentinel_symbol) {
            *sentinel_index = i;
            break;
        }
    }
    printf("sentinel_index in bwt=%d\n", *sentinel_index);
    printf("p array in bwt:\n");
    for (int i = 0; i < n + 1; ++i) {
        printf("%d ", p[i]);
    }
    printf("\n");
    printf("sent symbol??? %d\n", ss[n]);
    printf("sorted rotations in bwt:\n");
    for (int m = 0; m < n + 1; ++m) {
        printf("%3d: ", m);
        for (int i = 0; i < n + 1; ++i) {
            if (ss[(p[m] + i) % (n + 1)] == sentinel_symbol) {
                printf("[$$]");
            } else {
                printf("[%x]", ss[(p[m] + i) % (n + 1)]);
            }
        }
        printf("\n");
    }
    printf("\n");
    assert(ss[(p[*sentinel_index] + n) % (n + 1)] == sentinel_symbol);
    // we know explicit sentinel index, I dont want to mess with original symbols so I skip sentinel
    // printf("bwt string in bwt:\n");
    // for (int i = 0; i < *sentinel_index; ++i) {
    //     printf("%c", ss[(p[i] + n) % (n + 1)]);
    // }
    // printf("$");
    // for (int i = *sentinel_index + 1; i < n; ++i) {
    //     printf("%c", ss[(p[i] + n) % (n + 1)]);
    // }
    // printf("\n");
    for (int i = 0; i < *sentinel_index; ++i) {
        last_column[i] = ss[(p[i] + n) % (n + 1)];
    }
    for (int i = *sentinel_index + 1; i < n + 1; ++i) {
        last_column[i - 1] = ss[(p[i] + n) % (n + 1)];
    }
    free(ss);
    free(p);
    return last_column;
}

uint32_t *bwt_inverse(uint32_t *s, int n, int alphabet_size, int sentinel_index) {
    uint32_t *ss = (uint32_t *)malloc(sizeof(uint32_t) * n);
    //memcpy(ss, s, sizeof(uint32_t) * n);
    //qsort(ss, n, sizeof(uint32_t), compare_uint32_t); // sentinel would've sort itself to the beginning anyways, no need to worry
    int *alphabet_map = (int *)malloc(sizeof(int) * alphabet_size);
    int *alphabet_cnt = (int *)malloc(sizeof(int) * alphabet_size);
    int32_t *ranks = (int32_t *)malloc(sizeof(int32_t) * n);
    for (int i = 0; i < alphabet_size; ++i) {
        alphabet_cnt[i] = 0;
        alphabet_map[i] = -1;
    }
    for (int i = 0; i < n; ++i) {
        ranks[i] = alphabet_cnt[s[i]]++;
        //alphabet_cnt[s[i]]++;
    }
    int offset = 0;
    for (int i = 0; i < alphabet_size; ++i) {
        if (alphabet_cnt[i]) {
            alphabet_map[i] = offset;
            offset += alphabet_cnt[i];
        }
    }

    int row_index = 0;
    int l_index = 0;
    int rank = 0;
    // printf("bwt string:\n");
    // for (int i = 0; i < sentinel_index; ++i) {
    //     printf("%c", s[i]);
    // }
    // printf("$");
    // for (int i = sentinel_index; i < n; ++i) {
    //     printf("%c", s[i]);
    // }
    // printf("\n");
    // for (int i = 0; i < alphabet_size; ++i) {
    //     if (alphabet_cnt[i]) {
    //         printf("[%c:map=%d:cnt=%d]", i, alphabet_map[i], alphabet_cnt[i]);
    //     }
    // }
    for (int j = n - 1; j >= 0; --j) {
        ss[j] = s[l_index];
        rank = ranks[l_index];
        row_index = alphabet_map[ss[j]] + rank + 1;
        l_index = (row_index > sentinel_index) ? row_index - 1 : row_index;
    }
    // printf("row_index: %d, l_index: %d, sentinel_index: %d\n", row_index, l_index, sentinel_index);
    // assert(row_index == sentinel_index);
    // assert(l_index == sentinel_index);
    return ss;
}