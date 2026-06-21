#include "libk.h"

void *kmemset(void *destination, int value, size_t count) {
    uint8_t *bytes = (uint8_t *)destination;
    while (count--) *bytes++ = (uint8_t)value;
    return destination;
}

void *kmemcpy(void *destination, const void *source, size_t count) {
    uint8_t *out = (uint8_t *)destination;
    const uint8_t *in = (const uint8_t *)source;
    while (count--) *out++ = *in++;
    return destination;
}

size_t kstrlen(const char *text) {
    size_t length = 0;
    while (text && text[length]) length++;
    return length;
}

int kstrcmp(const char *left, const char *right) {
    while (*left && *left == *right) { left++; right++; }
    return (uint8_t)*left - (uint8_t)*right;
}

void kstrcpy(char *destination, const char *source, size_t capacity) {
    if (capacity == 0) return;
    size_t index = 0;
    while (source[index] && index + 1 < capacity) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

void kitoa(uint32_t value, char *buffer, uint32_t base) {
    static const char digits[] = "0123456789ABCDEF";
    char reverse[33];
    uint32_t length = 0;
    if (base < 2 || base > 16) { buffer[0] = '\0'; return; }
    if (value == 0) { buffer[0] = '0'; buffer[1] = '\0'; return; }
    while (value) { reverse[length++] = digits[value % base]; value /= base; }
    for (uint32_t index = 0; index < length; index++) buffer[index] = reverse[length - index - 1];
    buffer[length] = '\0';
}
