#ifndef _HEADER_SERIALIZERS
#define _HEADER_SERIALIZERS

#include <aos/aos.h>

size_t serialize_size_string(char* string);
size_t serialize_string(void* buf, size_t buf_size, char* string);
size_t unserialize_string(void* buf, size_t buf_size, char** string);

size_t serialize_array_of_strings_size(char* const argv[], int argc);
size_t serialize_array_of_strings(void* buf, size_t buf_size, char* const argv[], int argc);
size_t unserialize_array_of_strings(void* buf, size_t buf_size, char*** argv, int* argc);

#endif
