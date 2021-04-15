#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>

#define debug 1

void log_printf(const char *,...);
void debug_print_data(long, int);

#define LOG_ERROR(args)               \
{                                     \
    fprintf(stderr, "ERROR      : "); \
    log_printf args;                  \
}

#define LOG_DEBUG(args)                   \
{                                         \
    if (debug) {                          \
        fprintf(stderr, "DEBUG      : "); \
        log_printf args;                  \
    }                                     \
}

#define LOG_INFO(args)                \
{                                     \
    fprintf(stderr, "INFO       : "); \
    log_printf args;                  \
}

static int iter = 0;

static void reset_iter() {
    iter = 0;
}

long long sum(int data[], int start, int end) {
    long sum = 0L;
    for (int i = start; i <= end; i++) {
        sum += data[i];
        iter++;
    }
    return sum;
}

void debug_printf(long data[], int len) {
    if (!debug) return;

    for (int i = 0; i < len; i++) {
        printf("%ld ", data[i]);
    }
    printf("\n");
}

long *runSum(int data[], int start, int end) {
    int size = abs(end - start) + 1;
    long *sum = (long *)malloc(size * sizeof(long));
    if (sum == NULL) {
        LOG_ERROR(("Unable to allocate memory for running sum"));
        return NULL;
    }
    int j = 0;
    if (start < end) {
        sum[j] = data[start];
        for (int i = start + 1; i <= end; i++) {
            j++;
            sum[j] = data[i] + sum[j-1];
        }        

        return sum;
    }
    j = size - 1;
    sum[j] = data[start];
    for (int i = start - 1; i >= end; i--) {
        j--;
        sum[j] = data[i] + sum[j+1];
        iter++;
    }

    return sum;
}

void log_printf(const char *format,...) {
    va_list argptr;

    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}

int equilibrium_index_runsum(int data[], int len) {
    iter = len * 2;

    long *prefixSum = runSum(data, 0, len-1);
    long *suffixSum = runSum(data, len-1, 0);

    int idx = -1;

    for (int i = 0; i < len - 1; i++) {
        if (i > 0) {
            if (prefixSum[i - 1] == suffixSum[i + 1]) {
                idx = i;
                break;
            }
        }
        if (prefixSum[0] == suffixSum[1]) {
            idx = 0;
            break;
        }
        iter++;
    }

    free(prefixSum);
    free(suffixSum);

    return idx;
}

int equilibrium_index_bsearch(int data[], int len, int start, int end) {
    int candidate = (start + end) >> 1;
    long long prefixSum = sum(data, 0, candidate - 1);
    long long suffixSum = sum(data, candidate + 1, len - 1);

    if (prefixSum < suffixSum) {
        return equilibrium_index_bsearch(data, len, candidate, end);
    } else if (prefixSum > suffixSum) {
        return equilibrium_index_bsearch(data, len, start, candidate);
    }

    return candidate;
}

int equilibrium_index_search(int data[], int len) {
    return equilibrium_index_bsearch(data, len, 0, len);
}

typedef int (*equi_func)(int data[], int len);

void test_equi(int data[], int len, int *expected, equi_func test_func, const char *func_name) {
    LOG_INFO(("Testing function: %s\n", func_name));

    int idx = test_func(data, len);

    LOG_INFO(("Equilibrium index found at index: %d\n", idx));
    LOG_INFO(("Number of data points: %d, max iterations: %d\n", len, (len * len)));
    LOG_INFO(("Number of iterations to find index: %d, score: %g\n", iter, (float)iter / (len * len)));

    int found = 0;
    char msg[255] = "\0";
    int off = snprintf(msg, 255, "%d", expected[0]);
    for (int i = 0; i < sizeof(expected)/sizeof(int); i++) {
        if (idx == expected[i]) {
            found = 1;
            break;
        }
        if (i > 0) {
            snprintf(msg + off, 255 - off - 1, ",%d", expected[i]);
        }
    }
    if (!found) {
        LOG_ERROR(("Test failed, expected: %s got: %d\n", msg, idx));
    }
    reset_iter();
}

typedef void (*test_case)(void);

void test1 (void) {
    int data[] = {
      -1,
       3,
      -4,
       5,
       1,
      -6,
       2,
       1
    };
    int answers[] = { 
        1, 
        3 
    };
    int len = sizeof(data) / sizeof(int);

    test_equi(data, len, answers, equilibrium_index_runsum, "pre/suffix sum");
    test_equi(data, len, answers, equilibrium_index_search, "binary search");
}

void test2 (void) {
    int test2[] = {
       1,
       2,
       3,
       0,
       3,
       2,
       1
    };
    int test2idx[] = { 3 };
    int len2 = sizeof(test2) / sizeof(int);

    test_equi(test2, len2, test2idx, equilibrium_index_runsum, "pre/suffix sum");
    test_equi(test2, len2, test2idx, equilibrium_index_search, "binary search");
}

int main (int argc, char **argv) {
    int num_tests = 2;
    test_case tests[] = {
        test1,
        test2
    };
    for (int i = 0; i < num_tests; i++) {
        tests[i]();
    }
    exit(0);
}
