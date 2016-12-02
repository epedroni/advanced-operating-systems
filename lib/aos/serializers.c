#include <aos/serializers.h>

size_t serialize_size_string(char* string)
{
    return sizeof(size_t) + strlen(string) * sizeof(char);
}

size_t serialize_string(void* buf, size_t buf_size, char* string)
{
    size_t arg_len = strlen(string);
    if (sizeof(size_t) + arg_len * sizeof(char) > buf_size)
        return 0;
    memcpy(buf, &arg_len, sizeof(size_t));
    memcpy(buf + sizeof(size_t), string, sizeof(char) * arg_len);
    return sizeof(size_t) + sizeof(char) * arg_len;
}

size_t unserialize_string(void* buf, size_t buf_size, char** string)
{
    if (buf_size < sizeof(size_t))
        return 0;
    size_t arg_len;
    memcpy(&arg_len, buf, sizeof(size_t));
    if (buf_size < sizeof(size_t) + sizeof(char) * arg_len)
        return 0;
    *string = malloc(sizeof(char) * arg_len);
    memcpy(*string, buf + sizeof(size_t), arg_len * sizeof(char));
    (*string)[arg_len] = 0;
    return sizeof(size_t) + sizeof(char) * arg_len;
}


size_t serialize_array_of_strings_size(char* const argv[], int argc)
{
    size_t size = sizeof(size_t);
    for (int i = 0; i < argc; ++i)
        size += serialize_size_string(argv[i]);
    return size;
}

size_t serialize_array_of_strings(void* buf, size_t buf_size, char* const argv[], int argc)
{
    if (buf_size < sizeof(int))
        return 0;

    memcpy(buf, &argc, sizeof(int));
    size_t buf_pos = sizeof(int);

    for (int i = 0; i < argc; ++i)
    {
        size_t this_str_size = serialize_string(buf + buf_pos, buf_size - buf_pos, argv[i]);
        if (!this_str_size)
            return 0;
        buf_pos += this_str_size;
    }
    return buf_pos;
}

size_t unserialize_array_of_strings(void* buf, size_t buf_size, char*** _argv, int* _argc)
{
    if (buf_size < sizeof(int))
        return 0;
    int argc;
    memcpy(&argc, buf, sizeof(int));
    char** argv = malloc(sizeof(char*) * argc);
    size_t buf_pos = sizeof(int);

    for (int i = 0; i < argc; ++i)
    {
        size_t this_str_size = unserialize_string(buf + buf_pos, buf_size - buf_pos, &argv[i]);
        if (!this_str_size)
            return 0;
        buf_pos += this_str_size;
    }
    *_argv = argv;
    *_argc = argc;
    return buf_pos;
}
