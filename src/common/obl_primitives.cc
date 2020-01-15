#include <iostream>
#include "obl_primitives.h"

/***************************************************************************************
 * Helper functions
 **************************************************************************************/

inline uint32_t greatest_power_of_two_less_than(uint32_t n) {
    uint32_t k = 1;
    while (k < n)
        k = k << 1;
    return k >> 1;
}

inline uint32_t log2_ceil(uint32_t n) {
    uint32_t k = 0;
    uint32_t _n = n;
    while (n > 1) {
        k++;
        n /= 2;
    }
    if ((1 << k) < _n)
        k++;
    return k;
}

/***************************************************************************************
 * Oblivious primitives 
 **************************************************************************************/

// Return x > y
template <typename T>
uint8_t ObliviousGreater(T x, T y) {
    uint8_t result;
    __asm__ volatile (
            "cmp %2, %1;"
            "setg %0;"
            : "=r" (result)
            : "r" (x), "r" (y)
            : "cc"
            );
    return result;
}
uint8_t ObliviousGreater(double x, double y) {
    uint8_t result;
    __asm__ volatile (
            "comisd %%xmm1, %%xmm0;"
            "seta %0;"
            : "=r" (result)
            :
            : "cc"
            );
    return result; 
}

// Return x >= y
template <typename T>
uint8_t ObliviousGreaterOrEqual(T x, T y) {
    uint8_t result;
    __asm__ volatile (
            "cmp %2, %1;"
            "setge %0;"
            : "=r" (result)
            : "r" (x), "r" (y)
            : "cc"
            );
    return result;
}
uint8_t ObliviousGreaterOrEqual(double x, double y) {
    uint8_t result;
    __asm__ volatile (
            "comisd %%xmm1, %%xmm0;"
            "setae %0;"
            : "=r" (result)
            :
            : "cc"
            );
    return result; 
}

// Return x == y
template<typename T>
uint8_t ObliviousEqual(T x, T y) {
    uint8_t result;
    __asm__ volatile (
            "cmp %2, %1;"
            "sete %0;"
            : "=r" (result)
            : "r" (x), "r" (y)
            : "cc"
            );
    return result;
}

// Return x < y
template<typename T>
uint8_t ObliviousLess(T x, T y) {
    uint8_t result;
    __asm__ volatile (
            "cmp %2, %1;"
            "setl %0;"
            : "=r" (result)
            : "r" (x), "r" (y)
            : "cc"
            );
    return result;
}
uint8_t ObliviousLess(double x, double y) {
    uint8_t result;
    __asm__ volatile (
            "comisd %%xmm1, %%xmm0;"
            "setb %0;"
            : "=r" (result)
            :
            : "cc"
            );
    return result; 
}

// Return x <= y
template <typename T>
uint8_t ObliviousLessOrEqual(T x, T y) {
    uint8_t result;
    __asm__ volatile (
            "cmp %2, %1;"
            "setle %0;"
            : "=r" (result)
            : "r" (x), "r" (y)
            : "cc"
            );
    return result;
}
uint8_t ObliviousLessOrEqual(double x, double y) {
    uint8_t result;
    __asm__ volatile (
            "comisd %%xmm1, %%xmm0;"
            "setbe %0;"
            : "=r" (result)
            :
            : "cc"
            );
    return result; 
}

// Fill `out` with (pred ? t_val : f_val). Supports 16, 32, and 64 bit types
template <typename T>
void ObliviousAssign(bool pred, const T& t_val, const T& f_val, T* out) {
    T result;
    __asm__ volatile (
            "mov %2, %0;"
            "test %1, %1;"
            "cmovz %3, %0;"
            : "=&r" (result)
            : "r" (pred), "r" (t_val), "r" (f_val), "m" (out)
            : "cc"
            );
    *out = result;
}

// Iteratively apply `ObliviousAssign` to fill generic types of size > 1 byte
template <typename T>
void ObliviousAssignEx(bool pred, T t_val, T f_val, size_t bytes, T* out) {
    char *res = (char*) out;
    char *t = (char*) &t_val;
    char *f = (char*) &f_val;

    // Obliviously assign 8 bytes at a time
    size_t num_8_iter = bytes / 8;
    #pragma omp simd
    for (int i = 0; i < num_8_iter; i++) {
        ObliviousAssign(pred, *((uint64_t*)t), *((uint64_t*)f), (uint64_t*)res);
        res += 8;
        t += 8;
        f += 8;
    }

    // Obliviously assign 4 bytes
    if ((bytes % 8) / 4) {
        ObliviousAssign(pred, *((uint32_t*)t), *((uint32_t*)f), (uint32_t*)res);
        res += 4;
        t += 4;
        f += 4;
    }

    // Obliviously assign 2 bytes
    if ((bytes % 4) / 2) {
        ObliviousAssign(pred, *((uint16_t*)t), *((uint16_t*)f), (uint16_t*)res);
        res += 2;
        t += 2;
        f += 2;
    }

    if ((bytes % 2)) {
        ObliviousAssign(pred, *((uint16_t*)t), *((uint16_t*)f), (uint16_t*)res);
    }
}

// Return (pred ? t_val : f_val). Supports types of size > 1 byte
template <typename T>
T ObliviousChoose(bool pred, const T& t_val, const T& f_val) {
    T result;
    ObliviousAssignEx(pred, t_val, f_val, sizeof result, &result);
    return result;
}

// Return arr[i]
template <typename T>
T ObliviousArrayAccess(T *arr, int i, size_t n) {
    T result = arr[0];
    int step = sizeof(T) < CACHE_LINE_SIZE ? CACHE_LINE_SIZE / sizeof(T) : 1;
    for (int j = 0; j < n; j += step) {
        bool cond = ObliviousEqual(j/step, i/step);
        int pos = ObliviousChoose(cond, i, j);
        result = ObliviousChoose(cond, arr[pos], result);
    }
    return result;
}

// Set arr[i] = val
template <typename T>
void ObliviousArrayAssign(T *arr, int i, size_t n, T val) {
    int step = sizeof(T) < CACHE_LINE_SIZE ? CACHE_LINE_SIZE / sizeof(T) : 1;
    for (int j = 0; j < n; j += step) {
        bool cond = ObliviousEqual(j/step, i/step);
        int pos = ObliviousChoose(cond, i, j);
        arr[pos] = ObliviousChoose(cond, val, arr[pos]);
    }
}

/***************************************************************************************
 * Oblivious bitonic sort (imperative version)
 **************************************************************************************/

// Imperative implementation of bitonic merge network
template <typename T>
inline void imperative_o_merge(T* arr, uint32_t low, uint32_t len, bool ascending) {
    uint32_t i, j, k;
    uint32_t l = log2_ceil(len);
    uint32_t n = 1 << l;
    for (i = 0; i < l; i++) {
        for (j = 0; j < n; j += n >> i) {
            for (k = 0; k < (n >> i) / 2; k++) {
                uint32_t i1 = low + k + j;
                uint32_t i2 = i1 + (n >> i) / 2;
                if (i2 >= low + len) 
                    break;
                bool pred = ObliviousGreater(arr[i1], arr[i2]);
                pred = ObliviousEqual(pred, ascending);
                // These array accesses are oblivious because the indices are deterministic
                T tmp = arr[i1];
                arr[i1] = ObliviousChoose(pred, arr[i2], arr[i1]);
                arr[i2] = ObliviousChoose(pred, tmp, arr[i2]);
            }
        }
    } 
}

// Imperative implementation of bitonic merge network for POD type
// Assumes T implements T::ogreater
template <typename T>
inline void imperative_o_merge_pod(T* arr, uint32_t low, uint32_t len, bool ascending) {
    uint32_t i, j, k;
    uint32_t l = log2_ceil(len);
    uint32_t n = 1 << l;
    for (i = 0; i < l; i++) {
        for (j = 0; j < n; j += n >> i) {
            for (k = 0; k < (n >> i) / 2; k++) {
                uint32_t i1 = low + k + j;
                uint32_t i2 = i1 + (n >> i) / 2;
                if (i2 >= low + len) 
                    break;
                bool pred = T::ogreater(arr[i1], arr[i2]);
                pred = ObliviousEqual(pred, ascending);
                // These array accesses are oblivious because the indices are deterministic
                T tmp = arr[i1];
                arr[i1] = ObliviousChoose(pred, arr[i2], arr[i1]);
                arr[i2] = ObliviousChoose(pred, tmp, arr[i2]);
            }
        }
    } 
}

// Imperative implementation of bitonic sorting network -- works only for powers of 2
template <typename T>
inline void imperative_o_sort(T* arr, size_t n, bool ascending) {
    uint32_t i, j, k;
    for (k = 2; k <= n; k = 2 * k) {
        for (j = k >> 1; j > 0; j = j >> 1) {
            for (i = 0; i < n; i++) {
                uint32_t ij = i ^ j;
                if (ij > i) {
                    if ((i & k) == 0) {
                        bool pred = ObliviousGreater(arr[i], arr[ij]);
                        pred = ObliviousEqual(pred, ascending);
                        // These array accesses are oblivious because the indices are deterministic
                        T tmp = arr[i];
                        arr[i] = ObliviousChoose(pred, arr[ij], arr[i]);
                        arr[ij] = ObliviousChoose(pred, tmp, arr[ij]);
                    } else {
                        bool pred = ObliviousGreater(arr[ij], arr[i]);
                        pred = ObliviousEqual(pred, ascending);
                        // These array accesses are oblivious because the indices are deterministic
                        T tmp = arr[i];
                        arr[i] = ObliviousChoose(pred, arr[ij], arr[i]);
                        arr[ij] = ObliviousChoose(pred, tmp, arr[ij]);
                    }
                }
            }
        }
    }
}

// Imperative implementation of bitonic sorting network for POD type -- works only for powers of 2
// Assumes T implements T::ogreater
template <typename T>
inline void imperative_o_sort_pod(T* arr, size_t n, bool ascending) {
    uint32_t i, j, k;
    for (k = 2; k <= n; k = 2 * k) {
        for (j = k >> 1; j > 0; j = j >> 1) {
            for (i = 0; i < n; i++) {
                uint32_t ij = i ^ j;
                if (ij > i) {
                    if ((i & k) == 0) {
                        bool pred = T::ogreater(arr[i], arr[ij]);
                        pred = ObliviousEqual(pred, ascending);
                        // These array accesses are oblivious because the indices are deterministic
                        T tmp = arr[i];
                        arr[i] = ObliviousChoose(pred, arr[ij], arr[i]);
                        arr[ij] = ObliviousChoose(pred, tmp, arr[ij]);
                    } else {
                        bool pred = T::ogreater(arr[ij], arr[i]);
                        pred = ObliviousEqual(pred, ascending);
                        // These array accesses are oblivious because the indices are deterministic
                        T tmp = arr[i];
                        arr[i] = ObliviousChoose(pred, arr[ij], arr[i]);
                        arr[ij] = ObliviousChoose(pred, tmp, arr[ij]);
                    }
                }
            }
        }
    }
}

// Sort <len> elements in arr -- starting from index arr[low]
template <typename T>
void o_sort(T* arr, uint32_t low, uint32_t len, bool ascending) {
    if (len > 1) {
        uint32_t m = greatest_power_of_two_less_than(len);
        if (m * 2 == len) {
            imperative_o_sort(arr + low, len, ascending);
        } else {
            imperative_o_sort(arr + low, m, !ascending);
            o_sort(arr, low + m, len - m, ascending);
            imperative_o_merge(arr, low, len, ascending);
        }
    }
}

// Sort <len> elements in arr of POD type -- starting from index arr[low]
// Assumes T implements T::ogreater
template <typename T>
void o_sort_pod(T* arr, uint32_t low, uint32_t len, bool ascending) {
    if (len > 1) {
        uint32_t m = greatest_power_of_two_less_than(len);
        if (m * 2 == len) {
            imperative_o_sort_pod(arr + low, len, ascending);
        } else {
            imperative_o_sort_pod(arr + low, m, !ascending);
            o_sort_pod(arr, low + m, len - m, ascending);
            imperative_o_merge_pod(arr, low, len, ascending);
        }
    }
}

template <typename T>
inline void ObliviousMerge(T* arr, uint32_t low, uint32_t len, bool ascending) {
    imperative_o_merge(arr, low, len, ascending);
}

template <typename T>
inline void ObliviousMergePOD(T* arr, uint32_t low, uint32_t len, bool ascending) {
    imperative_o_merge_pod(arr, low, len, ascending);
}

template <typename T>
inline void ObliviousSort(T* arr, uint32_t low, uint32_t len, bool ascending) {
    o_sort(arr, low, len, ascending);
}

template <typename T>
inline void ObliviousSortPOD(T* arr, uint32_t low, uint32_t len, bool ascending) {
    o_sort_pod(arr, low, len, ascending);
}

/***************************************************************************************
 * Testing
 **************************************************************************************/
struct Generic {
    double x;
    short y;
    double z;

    Generic() = default;

    Generic(double x, short y, double z)
        : x(x), y(y), z(z) {}

    inline bool operator<(const Generic &b) const {
        return (x < b.x);
    }
    inline bool operator<=(const Generic &b) const {
        return (x <= b.x);
    }
    
    static inline bool ogreater(Generic a, Generic b) {
        return ObliviousGreater(a.x, b.x);
    }
};

struct Generic_16B {
    double x;
    uint64_t y;

    Generic_16B() = default;

    Generic_16B(double x, uint64_t y)
        : x(x), y(y) {}
};

void test(const char* name, bool cond) {
    printf("%s : ", name);
    if (cond)
        printf("pass\n");
    else
        printf("fail\n");
}
void test_ObliviousGreater() {
    // Test generic cases
    test("4 > 5", ObliviousGreater(4,5) == 4 > 5);
    test("5 > 4", ObliviousGreater(5,4) == 5 > 4);
    test("4 > 4", ObliviousGreater(4,4) == 4 > 4);

    // Test negative cases
    test("-4 > 4", ObliviousGreater(-4,4) == -4 > 4);
    test("4 > -4", ObliviousGreater(4, -4) == 4 > -4);
    test("-4 > -5", ObliviousGreater(-4, -5) == -4 > -5);
    test("-5 > -4", ObliviousGreater(-5, -4) == -5 > -4);

    // Test floating point
    test("-4. > -3.", ObliviousGreater(-4., -3.) == -4. > -3.);
    test("-4.1 > -4.2", ObliviousGreater(-4.1, -4.2) == -4.1 > -4.2);
    test("-4.2 > -4.1", ObliviousGreater(-4.2, -4.1) == -4.2 > -4.1);
    test("-4. > -4.", ObliviousGreater(-4., -4.) == -4. > -4.);
    test(".4 > .3", ObliviousGreater(.4, .3) == .4 > .3);
    test(".4 > .5", ObliviousGreater(.4, .5) == .4 > .5);

    // Test integer overflow
    test("(int32_t) 2147483648 > 42", !ObliviousGreater((int32_t)2147483648, 42));
    test("2147483648 > 42", ObliviousGreater(2147483648, (int64_t) 42));
}
void test_ObliviousLess() {
    // Test generic cases
    test("4 < 5", ObliviousLess(4,5) == 4 < 5);
    test("5 < 4", ObliviousLess(5,4) == 5 < 4);
    test("4 < 4", ObliviousLess(4,4) == 4 < 4);

    // Test negative cases
    test("-4 < 4", ObliviousLess(-4,4) == -4 < 4);
    test("4 < -4", ObliviousLess(4, -4) == 4 < -4);
    test("-4 < -5", ObliviousLess(-4, -5) == -4 < -5);
    test("-5 < -4", ObliviousLess(-5, -4) == -5 < -4);

    // Test floating point
    test("-4. < -3.", ObliviousLess(-4., -3.) == -4. < -3.);
    test("-4.1 < -4.2", ObliviousLess(-4.1, -4.2) == -4.1 < -4.2);
    test("-4.2 < -4.1", ObliviousLess(-4.2, -4.1) == -4.2 < -4.1);
    test("-4. < -4.", ObliviousLess(-4., -4.) == -4. < -4.);
    test(".4 < .3", ObliviousLess(.4, .3) == .4 < .3);
    test(".4 < .5", ObliviousLess(.4, .5) == .4 < .5);

    // Test integer overflow
    test("(int32_t) 2147483648 < 42", ObliviousLess((int32_t)2147483648, 42));
    test("2147483648 < 42", !ObliviousLess(2147483648, (int64_t) 42));
}
void test_ObliviousEqual() {
    // Test generic cases
    test("4 == 5", ObliviousEqual(4,5) == (4==5));
    test("5 == 4", ObliviousEqual(5,4) == (5==4));
    test("4 == 4", ObliviousEqual(4,4) == (4==4));

    // Test negative cases
    test("-4 == 4", ObliviousEqual(-4,4) == (-4==4));
    test("4 == -4", ObliviousEqual(4, -4) == (4==-4));
    test("-4 == -5", ObliviousEqual(-4, -5) == (-4==-5));
    test("-5 == -4", ObliviousEqual(-5, -4) == (-5==-4));
    test("-4 == -4", ObliviousEqual(-4,-4) == (-4==-4));

    // Test floating point
    test("-4. == -3.", ObliviousEqual(-4., -3.) == (-4.==-3.));
    test("-4.1 == -4.2", ObliviousEqual(-4.1, -4.2) == (-4.1==-4.2));
    test("-4.2 == -4.1", ObliviousEqual(-4.2, -4.1) == (-4.2==-4.1));
    test(".4 == .3", ObliviousEqual(.4, .3) == (.4==.3));
    test(".4 == .5", ObliviousEqual(.4, .5) == (.4==.5));
    test(".4 == .400001", ObliviousEqual(.4, .400001) == (.4==.4000001));
    test("-4. == -4.", ObliviousEqual(-4., -4.) == (-4.==-4.));
    test("4. == 4.", ObliviousEqual(4., 4.) == (4.==4.));
}
void test_ObliviousAssign() {
    test(" (true, 4, 5) ", ObliviousChoose(true, 4, 5) == 4);
    test(" (false, 4, 5)", ObliviousChoose(false, 4, 5) == 5);
    test(" (true, -4, 5) ", ObliviousChoose(true, -4, 5) == -4);
    test(" (false, 4, -5)", ObliviousChoose(false, 4, -5) == -5);
    test(" (true, -4.2, 5.) ", ObliviousChoose(true, -4.2, 5.4) == -4.2);
    test(" (false, 4.23, 5.34)", ObliviousChoose(false, 4.23, 5.34) == 5.34);
    test(" (false, -4.23, -5.34)", ObliviousChoose(false, -4.23, -5.34) == -5.34);
    test(" (false, 4.23, -5.34)", ObliviousChoose(false, 4.23, -5.34) == -5.34);
    test(" (true, 4.23, -5.34)", ObliviousChoose(true, 4.23, -5.34) == 4.23);

    Generic g_a = Generic(-1.35, 2, 3.21);
    Generic g_b = Generic(4.123, 5, 6.432);
    Generic g_c = ObliviousChoose(true, g_a, g_b);
    test(" (true, (-1.35, 2, 3.21), (4.123, 5, 6.432)) ",
        (g_c.x == -1.35 && g_c.y == 2 && g_c.z == 3.21));
    g_c = ObliviousChoose(false, g_a, g_b);
    test(" (false, (-1.35, 2, 3.21), (4.123, 5, 6.432)) ",
        (g_c.x == 4.123 && g_c.y == 5 && g_c.z == 6.432));
}
void test_ObliviousSort() {
    double d_arr[5] = {2.123456789, 3.123456789, 1.123456789, -2.123456789, -1.123456789};
    bool pass = true;
    ObliviousSort(d_arr, 0, 5, true);

    for (int i = 0; i < 5; i++) {
        printf("%f ", d_arr[i]);
        if (i < 4) pass = (pass && (d_arr[i] <= d_arr[i+1]));
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");

    int int_arr[5] = {2, 3, 1, -2, -1};
    ObliviousSort(int_arr, 0, 5, true);
    pass = true;
    for (int i = 0; i < 5; i++) {
        printf("%d ", int_arr[i]);
        if (i < 4) pass = (pass && (d_arr[i] <= d_arr[i+1]));
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");
    
    Generic g_arr[5] = {Generic(-1.35, 2, 3.21), Generic(4.123, 5, 6.432), Generic(-5.123, 3, 7.432), Generic(6.123, 1, 1.432), Generic(-3.123, 4, 0.432)};
    ObliviousSortPOD(g_arr, 0, 5, true);
    pass = true;
    for (int i = 0; i < 5; i++) {
        printf("%f,%d,%f -- ", g_arr[i].x, g_arr[i].y, g_arr[i].z);
        if (i < 4) pass = (pass && (g_arr[i] <= g_arr[i+1]));
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");
}
void test_ObliviousArrayAccess() {
    double d_arr[100]; 
    for (int i = 0; i < 100; i++) {
        d_arr[i] = i + 0.5;
    }
    bool pass = true;
    for (int i = 0; i < 100; i++) {
        double val = ObliviousArrayAccess(d_arr, i, 100);
        if (i % 10 == 0)
            printf("%f ", val);
        pass = pass && (val == d_arr[i]);
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");

    int i_arr[100]; 
    for (int i = 0; i < 100; i++) {
        i_arr[i] = i;
    }
    pass = true;
    for (int i = 0; i < 100; i++) {
        int val = ObliviousArrayAccess(i_arr, i, 100);
        if (i % 10 == 0)
            printf("%d ", val);
        pass = pass && (val == i_arr[i]);
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");

    Generic_16B g_arr[100]; 
    for (int i = 0; i < 100; i++) {
        g_arr[i] = Generic_16B(i+0.5, i);
    }
    pass = true;
    for (int i = 0; i < 100; i++) {
        Generic_16B val = ObliviousArrayAccess(g_arr, i, 100);
        if (i % 10 == 0)
            printf("%f,%llu ", val.x, val.y);
        pass = pass && (val.x == g_arr[i].x) && (val.y == g_arr[i].y);
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");
}

void test_ObliviousArrayAssign() {
    bool pass = true;
    for (int i = 0; i < 100; i++) {
        double d_arr[100]; 
        for (int i = 0; i < 100; i++) {
            d_arr[i] = i + 0.5;
        }
        ObliviousArrayAssign(d_arr, i, 100, 999.0);
        if (i % 10 == 0)
            printf("%f ", d_arr[i]);
        for (int j = 0; j < 100; j++) {
            if (i == j)
                pass = pass && (d_arr[j] == 999);
            else
                pass = pass && (d_arr[j] == j + 0.5);
        }
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");

    pass = true;
    for (int i = 0; i < 100; i++) {
        Generic_16B g_arr[100]; 
        for (int i = 0; i < 100; i++) {
            g_arr[i] = Generic_16B(i+0.5, i);
        }
        ObliviousArrayAssign(g_arr, i, 100, Generic_16B(999.0, 999));
        if (i % 10 == 0)
            printf("%f,%llu ", g_arr[i].x, g_arr[i].y);
        for (int j = 0; j < 100; j++) {
            if (i == j)
                pass = pass && (g_arr[j].x == 999.0) && (g_arr[j].y == 999);
            else
                pass = pass && (g_arr[j].x == j + 0.5) && (g_arr[j].y == j);
        }
    }
    if (pass) 
        printf(" : pass");
    else
        printf(" : fail");
    printf("\n");
}

/***************************************************************************************
 * Main
 **************************************************************************************/

int main() {
    test_ObliviousGreater();
    test_ObliviousLess();
    test_ObliviousEqual();
    test_ObliviousAssign();
    test_ObliviousSort();
    test_ObliviousArrayAccess();
    test_ObliviousArrayAssign();
}

