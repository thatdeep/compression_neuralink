#include "bwt.h"

int compare_uint32_t(const void* a, const void* b) {
    uint32_t arg1 = *(const int*)a;
    uint32_t arg2 = *(const int*)b;

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
    int32_t ss = (int32_t *)malloc(sizeof(int32_t) * (n + 1));
    // what about simply memsetting (assuming you are using s[i] with leading bit zero)
    for (int i = 0; i < n; ++i) {
        ss[i] = (int32_t)(s[i]);
    }
    ss[n] = sentinel_symbol;
    int32_t *p = sort_cyclic_shifts(ss, n, alphabet_size);
    uint32_t *last_column = (int32_t *)malloc(sizeof(int32_t) * n);
    for (int i = 0; i < n + 1; ++i) {
        if (ss[(p[i] + n - 1) % (n + 1)] == sentinel_symbol) {
            *sentinel_index = i;
            break;
        }
    }
    assert(ss[(p[i] + n - 1) % (n + 1)] == sentinel_symbol);
    // we know explicit sentinel index, I dont want to mess with original symbols so I skip sentinel
    for (int i = 0; i < sentinel_index; ++i) {
        last_column[i] = ss[(p[i] + n - 1) % (n + 1)];
    }
    for (int i = sentinel_index + 1; i < n + 1; ++i) {
        last_column[i - 1] = ss[(p[i] + n - 1) % (n + 1)];
    }
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
        alphabet_cnt = 0;
        alphabet_map = -1;
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
    // THIS. Think about that.
    // sentinel_index: 5
    // symbols_sorted: [$]imps
    // alphabet_map: i:0, m:4, p:5, s:7, *:-1
    // cnt: i:4, m:1, p:2, s:4
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //        | c=i, r=0, am[c]=0, row_index = 0 + 1 = 1 ($ omitted in f), l_index = 1, ss=i
    //        v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //             | c=p, r=0, am[p]=5, row_index = 5 + 1 = 6, l_index = 5, ss=pi
    //             v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                               | c=p, r=1, am[c]=5, row_index = 5 + 1 + 1 = 7, l_index = 6, ss = ppi
    //                               v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                                   | c=i, r=1, am[c]=0, row_index = 0 + 1 + 1 = 2, l_index = 2, ss = ippi
    //                                   v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                 | c=s, r=0, am[c]=7, row_index = 7 + 0 + 1 = 8, l_index = 7, ss = sippi
    //                 v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                                       | c=s, r=2, am[c]=7, row_index = 7 + 2 + 1 = 10, l_index = 9, ss = ssippi
    //                                       v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                                               | c=i, r=2, am[c]=0, row_index = 0 + 2 + 1 = 3, l_index = 3, ss = issippi
    //                                               v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                     | c=s, r=1, am[c]=7, row_index = 7 + 1 + 1 = 9, l_index = 8, ss = sissippi
    //                     v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                                           | c=s, r=3, am[c]=7, row_index = 7 + 3 + 1 = 11, l_index = 10, ss = ssissippi
    //                                           v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                                                   | c=i, r=3, am[c]=0, row_index = 0 + 3 + 1 = 4, l_index = 4, ss = ississippi
    //                                                   v
    // s:     0:i 1:p 2:s 3:s 4:m[$]5:p 6:i 7:s 8:s 9:i A:i ($ is omitted)
    // rk:    0:0 1:0 2:0 3:1 4:0   5:1 6:1 7:2 8:3 9:2 A:3
    // f:  [$]0:i 1:i 2:i 3:i 4:m   5:p 6:p 7:s 8:s 9:s A:s ($ is omitted)
    //                         | c=m, r=0, am[c]=4, row_index = 4 + 0 + 1 = 5, l_index = 5, ss = mississippi
    //                         v
    // DONE. at the end row_index = l_index = sentinel_index
    int row_index = 0;
    int l_index = 0;
    int rank = 0;
    for (j = n - 1; j >= 0; --j) {
        ss[j] = s[l_index];
        rank = ranks[l_index];
        row_index = alphabet_map[ss[j]] + rank + 1;
        l_index = (row_index > sentinel_index) ? row_index - 1 : row_index;
    }

    
    // algo: 1) sentinel last, corresponding first symbol is ss[sentinel_index - 1] (-1 because [$] should be 0-th symbol)
    // for (int i = 0; i < n; ++i) {
    //     uint32_t symbol = s[i];
    //     int k = alphabet_cnt[symbol];
    //     t[i] = alphabet_map[symbol];
    //     alphabet_map[symbol]++;
    //     alphabet_cnt[symbol]++;
    // }


}