#include <slip_parser.h>
#include <netutil/htons.h>

errval_t slip_init(struct slip_state* slip_state, system_raw_write write_handler){
    slip_state->current_position=0;
    slip_state->struct_initialized=SLIP_STATE_MAGIC_NUMBER;
    slip_state->current_state=SLIP_PARSE_STATE_READY;
    slip_state->is_escape=false;
    slip_state->active_handler=NULL;
    memset(slip_state->available_protocol_handlers,0,sizeof(slip_state->available_protocol_handlers));

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
uint16_t ip_sum_calc(struct ip_header* hdr, uint16_t len_ip_header)
{
    uint16_t* buff=(uint16_t*)hdr;
    uint16_t word16;
    uint32_t sum=0;
    uint16_t i;

    // make 16 bit words out of every two adjacent 8 bit words in the packet
    // and add them up
    for (i=0;i<len_ip_header;i=i+2){
        word16 =((buff[i]<<8)&0xFF00)+(buff[i+1]&0xFF);
        sum = sum + (uint32_t) word16;
    }

    // take only 16 bits out of the 32 bit sum and add up the carries
    while (sum>>16)
      sum = (sum & 0xFFFF)+(sum >> 16);

    // one's complement the result
    sum = ~sum;

    return ((uint16_t) sum);
}

static
uint16_t cksum(struct ip_header *ip, size_t len){
  uint32_t sum = 0;  /* assume 32 bit long, 16 bit short */

  while(len > 1){
    sum += (*((uint16_t*) ip))++;
    if(sum & 0x80000000)   /* if high order bit set, fold */
      sum = (sum & 0xFFFF) + (sum >> 16);
    len -= 2;
  }

  if(len)       /* take care of left over byte */
    sum += (uint16_t) *(uint8_t*)ip;

  while(sum>>16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return ~sum;
}

static
errval_t slip_parse_ip_data(struct slip_state* slip_state, uint8_t byte){
    assert(slip_state->active_handler->data_handler!=NULL);
    assert(slip_state->active_handler->buffer_capacity>slip_state->active_handler->data_length);

    if(!slip_state->remaining_data_bytes)
        return SLIP_ERR_OK;

    slip_state->active_handler->buffer[slip_state->active_handler->data_length++]=byte;
    if (--slip_state->remaining_data_bytes==0){
        slip_state->active_handler->data_handler(slip_state->current_ip_header->source_ip,
                slip_state->current_ip_header->destination_ip,
                slip_state->active_handler->buffer,
                slip_state->active_handler->data_length);
        slip_state->current_state=SLIP_PARSE_STATE_READY;
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

            uint16_t checksum=slip_state->current_ip_header->header_checksum;
            slip_state->current_ip_header->header_checksum=0x0000;
            if(ip_sum_calc(slip_state->current_ip_header, slip_state->ip_header_size)==checksum){
                debug_printf("EQUAL OHHHOHO\n");
            }else{
                if(cksum(slip_state->current_ip_header, slip_state->ip_header_size) == ip_sum_calc(slip_state->current_ip_header, slip_state->ip_header_size)){
                    debug_printf("BOTH ARE EQUAL!\n");
                }

                debug_printf("Received checksum: [0x%04x] calculated: [0x%04x]\n", checksum, ip_sum_calc(slip_state->current_ip_header, slip_state->ip_header_size));
            }

            if(protocol_handler->buffer_capacity<slip_state->remaining_data_bytes){
                debug_printf("Data to be received is larger then provided buffer! Dropping packet\n");
                slip_state->current_state=SLIP_PARSE_STATE_INVALID;
            }
            slip_state->active_handler=protocol_handler;
            protocol_handler->data_length=0;
            slip_state->current_state=SLIP_PARSE_STATE_IP_USER_DATA;
        }else{
            debug_printf("Unsupported protocol!\n");
            slip_state->current_state=SLIP_PARSE_STATE_INVALID;
        }
    }

    return SLIP_ERR_OK;
}

static
errval_t slip_verify_datagram_ip_version(struct slip_state* slip_state, uint8_t byte){
    if(GET_IP_VERSION(byte)==IP_VERSION_V4 && GET_IHL(byte)<=IP_MAX_IHL){
        debug_printf("Recognized IPV4 version!\n");
        slip_state->rcv_buffer[0]=byte;
        slip_state->current_position=1;
        slip_state->ip_header_size=GET_IHL(byte)*IP_WORD_SIZE;
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
    debug_printf("Datagram end detected\n");

    slip_state->current_position=0;
    slip_state->is_escape=false;
    slip_state->current_state=SLIP_PARSE_STATE_READY;
    return SLIP_ERR_OK;
}

static
errval_t slip_consume_byte(struct slip_state* slip_state, uint8_t byte){
    //Check if we have to escape character
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
        uint8_t* buffer, size_t buff_size, slip_data_received data_handler){
    SLIP_STATE_INITIALIZED(slip_state);

    slip_state->available_protocol_handlers[protocol_id].buffer=buffer;
    slip_state->available_protocol_handlers[protocol_id].buffer_capacity=buff_size;
    slip_state->available_protocol_handlers[protocol_id].data_length=0;
    slip_state->available_protocol_handlers[protocol_id].data_handler=data_handler;

    return SLIP_ERR_OK;
}

errval_t slip_send_datagram(struct slip_state* slip_state, uint32_t to, uint32_t from,
        uint8_t *buf, size_t len){
    SLIP_STATE_INITIALIZED(slip_state);



    return SLIP_ERR_OK;
}
