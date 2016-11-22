#ifndef _HEADER_INIT_URPC
#define _HEADER_INIT_URPC

#include <aos/aos.h>

enum urpc_buffer_status
{
    URPC_NO_DATA,
    URPC_CLIENT_SENT_DATA,
    URPC_SERVER_REPLIED_DATA,
};

struct urpc_buffer_data
{
    volatile uint32_t status;
    size_t data_len;
    char data[];
};

#define URPC_BUF_HEADER_LENGTH (sizeof(size_t) + sizeof(volatile uint32_t))
#define URPC_MAX_DATA_SIZE(buf) (buf->buffer_len - URPC_BUF_HEADER_LENGTH)

struct urpc_buffer
{
    bool is_server;
    size_t buffer_len;
    struct urpc_buffer_data* buffer;
};

errval_t urpc_server_init(struct urpc_buffer* urpc, void* fullbuffer, size_t length);
errval_t urpc_client_init(struct urpc_buffer* urpc, void* fullbuffer, size_t length);
errval_t urpc_server_receive_block(struct urpc_buffer* urpc, void* buf, size_t len, size_t* datalen);
errval_t urpc_server_receive_try(struct urpc_buffer* urpc, void* buf, size_t len, size_t* datalen, bool* has_data);
errval_t urpc_client_send(struct urpc_buffer* urpc, void* data, size_t len, void** answer, size_t* answer_len);
errval_t urpc_server_answer(struct urpc_buffer* urpc, void* data, size_t len);

#endif
