#ifndef BASE64_UTILS_H
#define BASE64_UTILS_H

#include <stddef.h>

void base64_encode(const unsigned char *input, size_t input_length, char *output);
void base64_decode(const char *input, unsigned char *output, size_t *output_length);

#endif // BASE64_UTILS_H
