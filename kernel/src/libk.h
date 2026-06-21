#ifndef LIBK_H
#define LIBK_H

#include <stddef.h>
#include <stdint.h>

void *kmemset(void *destination, int value, size_t count);
void *kmemcpy(void *destination, const void *source, size_t count);
size_t kstrlen(const char *text);
int kstrcmp(const char *left, const char *right);
void kstrcpy(char *destination, const char *source, size_t capacity);
void kitoa(uint32_t value, char *buffer, uint32_t base);

#endif
