#include <slip_parser.h>

errval_t slip_init(struct slip_state* slip_state, system_raw_write write_handler){
    slip_state->my_ip_address=htonl(MY_IP_ADDRESS);
    slip_state->current_position=0;
    slip_state->struct_initialized=SLIP_STATE_MAGIC_NUMBER;
    slip_state->current_state=SLIP_PARSE_STATE_READY;
    slip_state->is_escape=false;
    slip_state->active_handler=NULL;
    slip_state->write_handler=write_handler;
    thread_mutex_init(&slip_state->serial_lock);
    memset(slip_state->available_protocol_handlers, 0, sizeof(slip_state->available_protocol_handlers));

    return SLIP_ERR_OK;
}

static
uint8_t slip_unescape(uint8_t escaped){
    switch(escaped){
        case SLIP_ESC_END:
            return SLIP_END;
        case SLIP_ESC_ESC:
            return SLIP_ESC;
        case SLIP_ESC_NUL:
        default:
            return 0x00;
    }
}

static
uint16_t calculate_checksum(uint16_t* header_buffer, size_t length){
    uint32_t sum=0x0;
    for(size_t i=0; i<length; ++i){
        sum+=header_buffer[i];
    }

    while (sum>>16)
        sum = (sum & 0xFFFF)+(sum >> 16);

    uint16_t calculated_checksum=~sum;

    return calculated_checksum;
}

static
bool slip_correct_ip_header_checksum(struct ip_header* header){
    uint32_t ihl= GET_IHL(header->version);
    if(ihl>IP_MAX_IHL)
        return false;   //If IHL is larger then maximum value, something is wrong

    uint16_t header_checksum=header->header_checksum;
    header->header_checksum=0x0;
    size_t half_word_count=ihl*2;
    uint16_t* header_buffer=(uint16_t*)header;
    uint16_t calculated_checksum=calculate_checksum(header_buffer, half_word_count);

    return (calculated_checksum==header_checksum);
}

void slip_dump_ip_header(struct ip_header* ip_header){
    debug_printf("\n--- Dumping ip header ---\n");
    debug_printf("Version: %lu Header length: %lu\n", GET_IP_VERSION(ip_header->version), GET_IHL(ip_header->version));
    debug_printf("Total length: %lu identification: 0x%04x\n", lwip_ntohs(ip_header->total_length), lwip_ntohs(ip_header->identification));
    debug_printf("Flags and offset: 0x%04x\n", lwip_ntohs(ip_header->fragmentation_info));
    debug_printf("TTL: %lu Protocol: 0x%02x\n", ip_header->ttl, ip_header->protocol);
    debug_printf("Header checksum: 0x%04x\n", (ip_header->header_checksum));

    debug_printf("\n\nPrinting out raw bytes!\n");
    size_t packet_length=GET_IHL(ip_header->version)*4;
    uint8_t* ip_buffer=(uint8_t*)ip_header;
    for(int i=0;i<packet_length;++i){
        printf("%02x ", ip_buffer[i]);
    }
    printf("\n\n\n");
}

static
errval_t slip_parse_ip_data(struct slip_state* slip_state, uint8_t byte){
    assert(slip_state->active_handler->data_handler!=NULL);
    assert(slip_state->active_handler->buffer_capacity>slip_state->active_handler->data_length);

    if(!slip_state->remaining_data_bytes){
        debug_printf("Invalid state! Too much bytes for processing\n");
        slip_state->current_state=SLIP_PARSE_STATE_INVALID;
        return SLIP_ERR_OK;
    }

    slip_state->active_handler->buffer[slip_state->active_handler->data_length++]=byte;
    if (--slip_state->remaining_data_bytes==0){
        debug_printf("Finished parsing ip data\n");
        slip_state->active_handler->data_handler(slip_state->current_ip_header->source_ip,
                slip_state->current_ip_header->destination_ip,
                slip_state->active_handler->buffer,
                slip_state->active_handler->data_length,
                slip_state->active_handler->context);
    }

    return SLIP_ERR_OK;
}

static
errval_t slip_parse_ip_header(struct slip_state* slip_state, uint8_t byte){
    assert(slip_state->current_state==SLIP_PARSE_STATE_IPV4_HEADER);
    assert(slip_state->current_position<slip_state->ip_header_size);

    slip_state->rcv_buffer[slip_state->current_position++]=byte;

    if(slip_state->current_position>=slip_state->ip_header_size){
        debug_printf("Received datagram of type: [0x%02x]\n", (int)slip_state->current_ip_header->protocol);

        uint16_t total_datagram_size=lwip_ntohs(slip_state->current_ip_header->total_length);
        slip_state->remaining_data_bytes=total_datagram_size-slip_state->ip_header_size;
        if(slip_state->available_protocol_handlers[slip_state->current_ip_header->protocol].data_handler!=NULL){
            struct slip_protocol_handler* protocol_handler=&slip_state->available_protocol_handlers[slip_state->current_ip_header->protocol];

            debug_printf("Fragmenting bytes: 0x%04x\n", slip_state->current_ip_header->fragmentation_info);

            if(slip_correct_ip_header_checksum(slip_state->current_ip_header)){
                if(protocol_handler->buffer_capacity<slip_state->remaining_data_bytes){
                    debug_printf("Protocol buffer capacity: %lu total datagram size: %lu data bytes: %lu\n",protocol_handler->buffer_capacity, total_datagram_size, slip_state->remaining_data_bytes);
                    debug_printf("Data to be received is larger then provided buffer! Dropping packet\n");
                    slip_state->current_state=SLIP_PARSE_STATE_INVALID;
                    return SLIP_ERR_OK;
                }
                if(slip_state->current_ip_header->destination_ip!=slip_state->my_ip_address){
                    debug_printf("Destination IP address is not valid, dropping packet!\n");
                    slip_state->current_state=SLIP_PARSE_STATE_INVALID;
                    return SLIP_ERR_OK;
                }
                debug_printf("IP headers seems to be OK, proceeding to user data parsing of length: %lu\n", slip_state->remaining_data_bytes);
                slip_state->active_handler=protocol_handler;
                protocol_handler->data_length=0;
                slip_state->current_state=SLIP_PARSE_STATE_IP_USER_DATA;
                return SLIP_ERR_OK;
            }else{
                debug_printf("Received IP packet doesn't have correct checksum, dropping it\n");
                slip_state->current_state=SLIP_PARSE_STATE_INVALID;
                return SLIP_ERR_OK;
            }
        }else{
            debug_printf("Unsupported protocol!\n");
            slip_state->current_state=SLIP_PARSE_STATE_INVALID;
        }
    }

    return SLIP_ERR_OK;
}

static
errval_t slip_verify_datagram_ip_version(struct slip_state* slip_state, uint8_t byte){
    uint8_t ihl=GET_IHL(byte);
    if(GET_IP_VERSION(byte)==IP_VERSION_V4 && (ihl<=IP_MAX_IHL) && (ihl>=IP_MIN_IHL)){
        debug_printf("Recognized IPV4 version!\n");
        slip_state->rcv_buffer[0]=byte;
        slip_state->current_position=1;
        slip_state->ip_header_size=ihl*IP_WORD_SIZE;
        slip_state->current_state=SLIP_PARSE_STATE_IPV4_HEADER;
    }else{
        debug_printf("Unsupported IP version!\n");
        slip_state->current_state=SLIP_PARSE_STATE_INVALID;
    }

    return SLIP_ERR_OK;
}

static
errval_t slip_datagram_process_byte(struct slip_state* slip_state, uint8_t byte){
    //TODO: Check if we are in header or user data process data
    switch(slip_state->current_state){
    case SLIP_PARSE_STATE_READY:
        return slip_verify_datagram_ip_version(slip_state, byte);
    case SLIP_PARSE_STATE_IPV4_HEADER:
        if(slip_state->current_position==MAX_IP_BUFF_SIZE){
            debug_printf("IP header size exceeded, returning\n");
            return SLIP_ERR_OK;
        }
        return slip_parse_ip_header(slip_state, byte);
    case SLIP_PARSE_STATE_IP_USER_DATA:
        return slip_parse_ip_data(slip_state, byte);
    case SLIP_PARSE_STATE_INVALID:
        return SLIP_ERR_OK;
    }

    return SLIP_ERR_OK;
}

static
errval_t slip_datagram_finished(struct slip_state* slip_state){
    if(slip_state->current_state==SLIP_PARSE_STATE_IP_USER_DATA){
        if(slip_state->remaining_data_bytes){
            debug_printf("Received end of frame, but there was still %lu bytes to be received\n", slip_state->remaining_data_bytes);
        }
    }

    slip_state->current_position=0;
    slip_state->is_escape=false;
    slip_state->current_state=SLIP_PARSE_STATE_READY;
    return SLIP_ERR_OK;
}

errval_t slip_consume_byte(struct slip_state* slip_state, uint8_t byte){
    SLIP_STATE_INITIALIZED(slip_state);

    if(!slip_state->is_escape && byte==SLIP_ESC){
        slip_state->is_escape=true;
        return SLIP_ERR_OK;
    }else if(slip_state->is_escape){
        byte=slip_unescape(byte);
        slip_state->is_escape=false;
    }else if(!slip_state->is_escape && byte==SLIP_END){

        return slip_datagram_finished(slip_state);
    }
    return slip_datagram_process_byte(slip_state, byte);
}

errval_t slip_raw_rcv(struct slip_state* slip_state, uint8_t *buf, size_t len){
    SLIP_STATE_INITIALIZED(slip_state);

    for(size_t i=0;i<len;++i){
        ERROR_RET1(slip_consume_byte(slip_state, buf[i]));
    }

    return SLIP_ERR_OK;
}

errval_t slip_register_protocol_handler(struct slip_state* slip_state, uint8_t protocol_id,
        uint8_t* buffer, size_t buff_size, slip_data_received data_handler, void* context){
    SLIP_STATE_INITIALIZED(slip_state);

    slip_state->available_protocol_handlers[protocol_id].buffer=buffer;
    slip_state->available_protocol_handlers[protocol_id].buffer_capacity=buff_size;
    slip_state->available_protocol_handlers[protocol_id].data_length=0;
    slip_state->available_protocol_handlers[protocol_id].data_handler=data_handler;
    slip_state->available_protocol_handlers[protocol_id].context=context;

    return SLIP_ERR_OK;
}

static
errval_t slip_write_raw_data(struct slip_state* slip_state, uint8_t *buf, size_t len, bool finished_datagram){
    static uint8_t ESCAPED_ESC_SEQ[2]={SLIP_ESC, SLIP_ESC_ESC};
    static uint8_t ESCAPED_END_SEQ[2]={SLIP_ESC, SLIP_ESC_END};
    static uint8_t ESCAPED_NUL_SEQ[2]={SLIP_ESC, SLIP_ESC_NUL};
    static uint8_t END_SEQ[1]={SLIP_END};

    for(size_t i=0;i<len;++i){
        switch(buf[i]){
        case SLIP_END:
            slip_state->write_handler(ESCAPED_END_SEQ, 2);
            break;
        case SLIP_ESC:
            slip_state->write_handler(ESCAPED_ESC_SEQ, 2);
            break;
        case 0x0:
            slip_state->write_handler(ESCAPED_NUL_SEQ, 2);
            break;
        default:
            slip_state->write_handler(buf+i, 1);
        }
    }

    if(finished_datagram){
        slip_state->write_handler(END_SEQ, 1);
        debug_printf("Writing end sequence\n");
    }

    return SLIP_ERR_OK;
}

errval_t slip_send_datagram(struct slip_state* slip_state, uint32_t to, uint32_t from,
        uint8_t protocol, uint8_t *buf, size_t len){
    SLIP_STATE_INITIALIZED(slip_state);

    static const uint8_t HEADER_WORDS=5;   //TODO: Check if fixed size satisfies everything
    static const uint16_t HEADER_SIZE=HEADER_WORDS*4;  //4 bytes per word
    static uint16_t last_used_identifier=0xABCD;

    static struct ip_header ip_header;
    ip_header.version=IP_VERSION_V4<<4 | HEADER_WORDS;
    ip_header.reserved_1=0x0;
    ip_header.total_length=lwip_htons(HEADER_SIZE+len);
    ip_header.identification=last_used_identifier++;
    ip_header.fragmentation_info=lwip_htons(0x4000);
    ip_header.ttl=64;
    ip_header.protocol=protocol;
    ip_header.source_ip=from;
    ip_header.destination_ip=to;
    ip_header.header_checksum=0;
    ip_header.header_checksum=inet_checksum((uint8_t*)&ip_header, HEADER_SIZE);

    ERR_CHECK("Sending header data", slip_write_raw_data(slip_state, (uint8_t*)&ip_header, HEADER_SIZE, false));
    ERR_CHECK("Sending data", slip_write_raw_data(slip_state, buf, len, true));

    return SLIP_ERR_OK;
}
