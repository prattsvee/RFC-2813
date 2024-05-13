#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "base64_utils.h"

static const char base64_enc_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_dec_table[128] = {
    62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

static const int mod_table[] = {0, 2, 1};

void base64_encode(const unsigned char *input, size_t input_length, char *output) {
    size_t output_length = 4 * ((input_length + 2) / 3);
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)input[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)input[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)input[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        output[j++] = base64_enc_table[(triple >> 3 * 6) & 0x3F];
        output[j++] = base64_enc_table[(triple >> 2 * 6) & 0x3F];
        output[j++] = base64_enc_table[(triple >> 1 * 6) & 0x3F];
        output[j++] = base64_enc_table[(triple >> 0 * 6) & 0x3F];
    }
    for (int i = 0; i < mod_table[input_length % 3]; i++)
        output[output_length - 1 - i] = '=';
    output[output_length] = 0;
}

void base64_decode(const char *input, unsigned char *output, size_t *output_length) {
    if (input == NULL) { *output_length = 0; return; }
    size_t input_length = strlen(input);
    if (input_length % 4 != 0) { *output_length = 0; return; }

    *output_length = input_length / 4 * 3;
    if (input[input_length - 1] == '=') (*output_length)--;
    if (input[input_length - 2] == '=') (*output_length)--;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_dec_table[input[i++]];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_dec_table[input[i++]];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_dec_table[input[i++]];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_dec_table[input[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
            + (sextet_b << 2 * 6)
            + (sextet_c << 1 * 6)
            + (sextet_d << 0 * 6);

        if (j < *output_length) output[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) output[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) output[j++] = (triple >> 0 * 8) & 0xFF;
    }
}
