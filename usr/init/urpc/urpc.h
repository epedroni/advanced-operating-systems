#ifndef _HEADER_INIT_URPC
#define _HEADER_INIT_URPC

#include <aos/aos.h>

enum urpc_buffer_status
{
    URPC_NO_DATA,
    URPC_CLIENT_SENT_DATA,
    URPC_SERVER_REPLIED_DATA,
    URPC_SERVER_REPLIED_ERROR,
};

struct urpc_buffer_data
{
    volatile uint32_t status;
    size_t data_len;
    uint32_t opcode;
    char data[0];
};

#define URPC_BUF_HEADER_LENGTH (sizeof(struct urpc_buffer_data))
#define URPC_MAX_DATA_SIZE(buf) (buf->buffer_len - URPC_BUF_HEADER_LENGTH)

struct urpc_buffer
{
    bool is_server;
    size_t buffer_len;
    struct urpc_buffer_data* buffer;
};

errval_t urpc_server_init(struct urpc_buffer* urpc, void* fullbuffer, size_t length);
errval_t urpc_client_init(struct urpc_buffer* urpc, void* fullbuffer, size_t length);
errval_t urpc_server_receive_block(struct urpc_buffer* urpc, void* buf, size_t len, size_t* datalen, uint32_t* opcode);
errval_t urpc_server_receive_try(struct urpc_buffer* urpc, void* buf, size_t len, size_t* datalen, uint32_t* opcode, bool* has_data);
errval_t urpc_client_send(struct urpc_buffer* urpc, uint32_t opcode, void* data, size_t len, void** answer, size_t* answer_len);
errval_t urpc_client_send_receive_fixed_size(struct urpc_buffer* urpc, uint32_t opcode, void* data, size_t len, void* answer, size_t answer_max_size, size_t* actual_answer_size);
errval_t urpc_server_answer(struct urpc_buffer* urpc, void* data, size_t len);
errval_t urpc_server_answer_error(struct urpc_buffer* urpc, errval_t error);
errval_t urpc_server_dummy_answer_if_need(struct urpc_buffer* urpc);

#endif
