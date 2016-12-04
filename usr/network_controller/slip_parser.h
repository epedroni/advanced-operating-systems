#ifndef _SLIP_PARSER_
#define _SLIP_PARSER_

#include <aos/aos.h>

//SLIP specific defines
#define SLIP_END        0xC0
#define SLIP_ESC        0xDB
#define SLIP_ESC_END    0xDC
#define SLIP_ESC_ESC    0xDD
#define SLIP_ESC_NUL    0xDE

#define IP_WORD_SIZE            4
#define MAX_IP_BUFF_SIZE        60

#define SLIP_STATE_MAGIC_NUMBER    607495481
#define SLIP_STATE_INITIALIZED(slp) assert(slp->struct_initialized==SLIP_STATE_MAGIC_NUMBER && "Slip state not initialized, or corrupted")
#define GET_IP_VERSION(byte) (byte>>4)
#define GET_IHL(byte) (byte & 0x0F)

#define IP_VERSION_V4   4
#define IP_MAX_IHL      15

typedef void (*system_raw_write)(uint8_t *buf, size_t len);
typedef void (*slip_data_received)(uint32_t from, uint32_t to, uint8_t *buf, size_t len);

struct slip_protocol_handler{
    slip_data_received data_handler;
    size_t buffer_capacity;
    size_t data_length;
    uint8_t* buffer;
};

enum slip_parsing_state{
    SLIP_PARSE_STATE_INVALID,
    SLIP_PARSE_STATE_READY,
    SLIP_PARSE_STATE_IPV4_HEADER,
    SLIP_PARSE_STATE_IP_USER_DATA
};

struct __attribute__((packed)) ip_header{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t total_length;
    uint16_t identification;
    uint16_t fragmentation_info;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t source_ip;
    uint32_t destination_ip;
    uint32_t options[4];
};

struct slip_state{
    uint32_t struct_initialized;
    enum slip_parsing_state current_state;
    uint32_t current_position;
    bool is_escape;
    system_raw_write write_handler;
    uint8_t ip_header_size;

    struct slip_protocol_handler available_protocol_handlers[20]; //TODO: Dynamically sized?
    struct slip_protocol_handler* active_handler;
    uint16_t remaining_data_bytes;

    struct ip_header current_ip_header[0];
    uint8_t rcv_buffer[MAX_IP_BUFF_SIZE];
};

/**
 * \brief Initializes slip state
 *
 * This function initializes slip state that is being used for tracking current state of
 *
 * \param slip_state Pointer to previously allocated structure
 * \param write_handler Handler function that will be invoked..
 *
 */
errval_t slip_init(struct slip_state* slip_state, system_raw_write write_handler);

errval_t slip_raw_rcv(struct slip_state* slip_state, uint8_t *buf, size_t len);

errval_t slip_register_protocol_handler(struct slip_state* slip_state, uint8_t protocol_id,
        uint8_t* buffer, size_t buff_size, slip_data_received data_handler);

errval_t slip_send_datagram(struct slip_state* slip_state, uint32_t to, uint32_t from,
        uint8_t *buf, size_t len);

#endif //_SLIP_PARSER_
