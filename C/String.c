// CONFIG
#define MIN_REALLOC_REQUEST_SIZE 32      // how big must be offset to request realloc
#define MIN_HEAP_ALLOC_REQUEST_SIZE 1024 // how big must be string to use heap
                                         // here count is 1024 - 1 kb
                                         // for linux max default stack size is 8 kb and for windows 1mb


// INCLUDES
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>
#include <math.h>
#include <float.h>
#include <wchar.h>

// UNSTABLE DEFINES (might be moved or deleted)
void __throw_RangeError(const char* msg);
void __throw_Error(const char* msg);

void dblog(const char* msg, ...);
void wdblog(const wchar_t* msg, ...);
void keepfn(void* p1, void* p2);
#ifdef _WIN32
    #include <Windows.h>
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif
#ifndef FORCE_UTF
#ifdef _WIN32
#define FORCE_UTF 16
#else
#define FORCE_UTF 8
#endif
#endif
// ENCODING-SPECIFIC DEFINES
#if FORCE_UTF == 16
    // we'd prefer to use utf16 encoding as it is standard for Windows
    #define UTF16 1
    #define STDCHAR_T wchar_t
    #define STDCHAR_T_STR "wchar_t"
    #define NULLTERM L'\0'
    #define _wsnprintf(buffer, size, format, ...) \
        swprintf(buffer, size, L##format, __VA_ARGS__)
    size_t char_conv(uint32_t codePoint, char* Str) {
        /*
            Converts codePoint to utf-16 encoding
            It is js standard but quite problematic.
        */

        // ref https://developer.mozilla.org/ru/docs/Web/JavaScript/Reference/Global_Objects/String/fromCodePoint#:~:text=if%20(codePoint%20%3C%3D,lowSurrogate)%3B%0A%20%20%20%20%20%20%20%20%7D
        if (codePoint <= 0xFFFF) {
            // BMP character
            // it consumes 2 bytes
            // so we're adding to offset other 2 bytes
            *Str++ = (codePoint >> 8) & 0xFF; // High byte
            *Str++ = codePoint & 0xFF;        // Low byte
            return 2; // 2 characters consumed
        } else { // codePoint <= 0x10FFFF
            // Supplementary character
            // it consumes 4 bytes
            codePoint -= 0x10000;
            uint16_t highSurrogate = (codePoint >> 10) + 0xD800;
            uint16_t lowSurrogate = (codePoint & 0x3FF) + 0xDC00;
            *Str++ = (highSurrogate >> 8) & 0xFF; // High surrogate high byte
            *Str++ = highSurrogate & 0xFF;        // High surrogate low byte
            *Str++ = (lowSurrogate >> 8) & 0xFF;  // Low surrogate high byte
            *Str++ = lowSurrogate & 0xFF;         // Low surrogate low byte
            return 4;  // 4 characters consumed
        }
    }
    EXPORT int uprint(const wchar_t* text) {
        // we use utf16 so print as wstring
        return wprintf(L"%ls\n", text);
    }
#else // FORCE_UTF == 8 or it is unix
    #define UTF16 0
    #define STDCHAR_T char
    #define STDCHAR_T_STR "char"
    #define NULLTERM '\0'
    #define _wsnprintf(buffer, size, format, ...) \
        snprintf(buffer, size, format, __VA_ARGS__)
    size_t char_conv(uint32_t codePoint, char* Str) {
        /*
            Converts the codePoint into utf-8 encoding
            It is not js standard, but may be less problematic compared to utf16
        */
        if (codePoint <= 0x7F) {
            // 1 byte
            *Str = (char) codePoint;
            return 1;
        } 
        else if (codePoint <= 0x7FF) {
            // 2 bytes
            *Str++ = (char) (0xC0 | (codePoint >> 6));
            *Str++ = (char) (0x80 | (codePoint & 0x3F));
            return 2;
        }
        else if (codePoint <= 0xFFFF) {
            // 3 bytes for BMP characters
            *Str++ = (char) (0xE0 | (codePoint >> 12));
            *Str++ = (char) (0x80 | ((codePoint >> 6) & 0x3F));
            *Str++ = (char) (0x80 | (codePoint & 0x3F));
            return 3;
        }
        else { // codePoint <= 0x10FFFF
            // 4 bytes for supplementary characters
            *Str++ = (char) (0xF0 | (codePoint >> 18));
            *Str++ = (char) (0x80 | ((codePoint >> 12) & 0x3F));
            *Str++ = (char) (0x80 | ((codePoint >> 6) & 0x3F));
            *Str++ = (char) (0x80 | (codePoint & 0x3F));
            return 4;
        }
    }
    EXPORT int uprint(const char* text) {
        // we use utf-8 so print as a regular string
        return printf("%s", text);
    }
#endif
// EXPORT SYMBOLS
EXPORT STDCHAR_T* _cfromCharCode(uint16_t* numN, size_t count);
EXPORT void* _cfromCodePoint(const double* numN, size_t count);
EXPORT void _cchangeLocale();
EXPORT int _chasFraction(double d);
EXPORT STDCHAR_T* _ctoString(double d);
// PRIVATE SYMBOLS
void* mmalloc(size_t size);
STDCHAR_T* fromCharCode_u8_stack(uint16_t* numN, size_t count);
STDCHAR_T* fromCharCode_u8_heap(uint16_t* numN, size_t count);
void* fromCodePoint_stack(const double* numN, size_t count, size_t len);
void* fromCodePoint_heap(const double* numN, size_t count, size_t len);
size_t fromCodePoint(const double* numN, size_t count, STDCHAR_T * Str);

// determine whether dynamic stack allocation can be supported
// if not it will be forcibly replaced by the heap allocation
#ifndef VLA
# if (                                                                    \
        defined(__clang__) ||                                             \
        defined(__GNUC__)  ||                                             \
        defined(_MSC_VER)  ||                                             \
        (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)        \
    )
#  define VLA 1               // we can use VLA
# else
#  define VLA 0               // we always rely on heap
# endif
#endif

#if VLA
# undef VLA
# ifdef _MSC_VER        
#  define VLA 2         // in MSVC we must use _alloca
#  define stalloc(name, len) STDCHAR_T* name = _alloca(len)
# else
#  define VLA 1         // we can follow C99 standard (for better compatibility) and use char[size] construct
#  define stalloc(name, len) STDCHAR_T name[len]
# endif
#else
#  define stalloc(name, len) STDCHAR_T* name;
#endif
// change locale to output chars correctly (both utf8 and utf16)
void _cchangeLocale() {
    setlocale(LC_CTYPE, "en_US.UTF-8");
}
int _chasFraction(double d) {
    return floor(d) != d;
}
// define if not define in ts
void* mmalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        Error("bad alloc");
    }
    return ptr;
};
STDCHAR_T* _ctoString(double d) {
    STDCHAR_T buf[40]; 
    int len;

    if (_chasFraction(d)) {
        // Significant fractional part
        len = _wsnprintf(buf, sizeof(buf), "%.9f", d);
        // Remove trailing zeros
        STDCHAR_T *end = buf + len - 1;
        while (*end == '0' && end > buf)
            end--;
        if (*end == '.')
            end--;
        end++;
        *end = NULLTERM;
        len = end - buf;
    } else {
        // No significant fractional part
        len = _wsnprintf(buf, sizeof(buf), "%lld", (long long) d);
    }

    // Allocate exact memory size, plus room for the null terminator
    STDCHAR_T *str = (STDCHAR_T*) mmalloc((len + 1) * sizeof(STDCHAR_T));

    // Copy the content and null-terminate
    memcpy(str, buf, len * sizeof(STDCHAR_T));
    str[len] = NULLTERM;

    return str;
}
STDCHAR_T* _cfromCharCode(uint16_t* numN, size_t count) {
    #if UTF16
        // Allocate space for UTF-16 string
        STDCHAR_T* str = (STDCHAR_T*) mmalloc((count + 1) * sizeof(STDCHAR_T));
        if (!str) return NULL;

        // Copy UTF-16 code units exactly as in JavaScript
        memcpy(str, numN, count * sizeof(uint16_t));

        str[count] = NULLTERM; // Null-terminate
        return str;
    #else
        // Convert UTF-16 to UTF-8
        size_t len = (count + 1) * 4; // Max UTF-8 size
        #if VLA
            if (len <= MIN_HEAP_ALLOC_REQUEST_SIZE)
                return fromCharCode_u8_stack(numN, len);
        #endif
        return fromCharCode_u8_heap(numN, len);
    #endif
}


void* _cfromCodePoint(const double* numN, size_t count) {
    size_t len = count * 4 + sizeof(STDCHAR_T);
#if VLA
    if (len <= MIN_HEAP_ALLOC_REQUEST_SIZE)
        return fromCodePoint_stack(numN, count, len);
#endif
    return fromCodePoint_heap(numN, count, len);
}

/*
        ### private members ###
*/
#if !UTF16

#if VLA
// Corrected function signature for fromCharCode_u8_stack
STDCHAR_T* fromCharCode_u8_stack(uint16_t* numN, size_t len) {
    stalloc(stack, len);
    len = 0;
    for (size_t i = 0; i < len; i++) {
        len += char_conv(numN[i], &stack[len]);
    }
    STDCHAR_T* str = mmalloc(len + sizeof(STDCHAR_T));
    memcpy(str, stack, len);
    str[len] = NULLTERM;
    return str;
}
#endif
STDCHAR_T* fromCharCode_u8_heap(uint16_t* numN, size_t len) {
    STDCHAR_T* str = mmalloc(len);
    size_t newlen = 0;
    for (size_t i = 0; i < len; i++) {
        newlen += char_conv(numN[i], &str[newlen]);
    }
    size_t offset = len - newlen;
    if (offset >= MIN_REALLOC_REQUEST_SIZE) {
        // realloc is needed
        STDCHAR_T* p = (STDCHAR_T*) realloc(str, len + sizeof(STDCHAR_T));
        if (p)
            str = p;
    }
    str[newlen] = NULLTERM;
    return str;
}
#endif
#if VLA
// does not exist when !VLA and therefore call will lead to compile-time err
// use more efficient stack pre-allocation than heap
void* fromCodePoint_stack(const double* numN, size_t count, size_t len) {
    stalloc(stack, len);
    len = fromCodePoint(numN, count, stack);
    len -= fromCodePoint(numN, count, stack);
    STDCHAR_T* heap = (STDCHAR_T*) mmalloc(len); // alloc heap memory with an exact size
    memcpy(heap, stack, len);       // copy
    // null terminate the string
    heap[len] = NULLTERM;
    return heap;

}
#endif
// use heap pre-allocation
// not so fast as stack allocation, but not slow in common execution practise
void* fromCodePoint_heap(const double* numN, size_t count, size_t len) {
    dblog("fromCodePoint: %s* heap = mmalloc(%zu)\n", STDCHAR_T_STR, len);
    STDCHAR_T* heap = (STDCHAR_T*) mmalloc(len); // pre-allocate max amount of bytes onto heap
    size_t offset = fromCodePoint(numN, count, heap); // manage characters and get offset
    len -= offset; // now len represents the actual string length
    if (offset >= MIN_REALLOC_REQUEST_SIZE) {
        // reduce the pre-allocated size if it is worth to
        STDCHAR_T* new_heap = (STDCHAR_T*) realloc(heap, len);
        if (new_heap)
            heap = new_heap;
    }
    // null terminate the string
    heap[len] = NULLTERM;
    return heap;
}
size_t fromCodePoint(const double* numN, size_t count, STDCHAR_T* Str) {
    size_t offset = 0;
    dblog("fromCodePoint: numN: %p;count: %zu;Str:%p\n", numN, count, Str);
    for (size_t i = 0; i < count; ++i) {
        double codePoint = numN[i];
        if ( // ref https://developer.mozilla.org/ru/docs/Web/JavaScript/Reference/Global_Objects/String/fromCodePoint#:~:text=if%20(%0A%20%20%20%20%20%20%20%20%20%20!isFinite(codePoint)%20%7C%7C%20//%20%60NaN%60%2C%20%60%2BInfinity%60%20%D0%B8%D0%BB%D0%B8%20%60%2DInfinity%60%0A%20%20%20%20%20%20%20%20%20%20codePoint%20%3C%200%20%7C%7C%20//%20%D0%BD%D0%B5%D0%B2%D0%B5%D1%80%D0%BD%D0%B0%D1%8F%20%D0%BA%D0%BE%D0%B4%D0%BE%D0%B2%D0%B0%D1%8F%20%D1%82%D0%BE%D1%87%D0%BA%D0%B0%20%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B0%0A%20%20%20%20%20%20%20%20%20%20codePoint%20%3E%200x10ffff%20%7C%7C%20//%20%D0%BD%D0%B5%D0%B2%D0%B5%D1%80%D0%BD%D0%B0%D1%8F%20%D0%BA%D0%BE%D0%B4%D0%BE%D0%B2%D0%B0%D1%8F%20%D1%82%D0%BE%D1%87%D0%BA%D0%B0%20%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B0%0A%20%20%20%20%20%20%20%20%20%20floor(codePoint)%20!%3D%20codePoint%20//%20%D0%BD%D0%B5%20%D1%86%D0%B5%D0%BB%D0%BE%D0%B5%20%D1%87%D0%B8%D1%81%D0%BB%D0%BE%0A%20%20%20%20%20%20%20%20)
            !isfinite(codePoint)            ||
            codePoint < 0                   ||
            codePoint > 0x10ffff            ||
            _chasFraction(codePoint) // floor(codePoint) != codePoint
        ) {
            // natively javascript would include floating point only when it does exist
            // so we'd use the same approach i think

            // ref https://developer.mozilla.org/ru/docs/Web/JavaScript/Reference/Global_Objects/String/fromCodePoint#:~:text=throw%20RangeError(%22Invalid%20code%20point%3A%20%22%20%2B%20codePoint)%3B
            if (_chasFraction(codePoint))
                RangeError("Invalid code point %e", codePoint); // using exponental output
            else
                RangeError("Invalid code point %lld", (long long) codePoint);
        }
        // apply convertion for char according with encoding and get offset
        size_t consumed = char_conv((int32_t) codePoint, (char*) Str);
        offset += 4 - consumed;

        Str += consumed;
    }
    return offset;
}
// throw range error (with printf features)
// Throws an exception message to the console and calls exit()
// you may adjust if the throw function works differently
void RangeError(const char* msg, ...) {
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "Range Error: ");
    vfprintf(stderr, msg, args);
    fputc('\n', stderr);

    va_end(args);
    exit(EXIT_FAILURE);
}
void Error(const char* msg, ...) {
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "Error: ");
    vfprintf(stderr, msg, args);
    fputc('\n', stderr);

    va_end(args);
    exit(EXIT_FAILURE);
}
// debug log with printf format when DEBUG defined
void dblog(const char* msg, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
#endif
}
// debug log with wprintf format, when DEBUG defined
void wdblog(const wchar_t* msg, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, msg);
    vwprintf(msg, args);
    va_end(args);
#endif
}
/*
    It is used to prevent unused functions from being removed by the linker.
    Though this can be removed and done in the linker's command arguments
*/
void keepfn(void* p1, void* p2) {
    printf("%s: %d: keepfn is a keep function\n", __FILE__, __LINE__);
    exit(1);
    p1 = malloc(1);
    p2 = realloc(p1, 1);
    free(p2);
    memcpy(p1, p2, 0);
    wprintf(L"\n");
    printf("\n");
}