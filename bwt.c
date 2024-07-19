#include "bwt.h"

int32_t *sort_cyclic_shifts(int32_t *s, int n, int alphabet_size, int *original_index) {
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
    for (int i = 0; i < n; ++i) {
        if (p[i] == 0) {
            *original_index = i;
            break;
        }
    }
    free(c);
    free(cnt);
    free(pn);
    free(cn);
    return p;
}

int32_t *bwt(int32_t *s, int n, int alphabet_size, int *original_index) {
    int32_t *p = sort_cyclic_shifts(s, n, alphabet_size, original_index);
    int32_t *last_column = (int32_t *)malloc(sizeof(int32_t) * n);
    for (int i = 0; i < n; ++i) {
        last_column[i] = s[(p[i] + n - 1) % n];
    }
    return last_column;
}