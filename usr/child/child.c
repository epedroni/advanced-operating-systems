
#include <stdio.h>
#include <aos/aos.h>
#include <aos/urpc/udp.h>
#include <aos/urpc/server.h>
#include <netutil/htons.h>
#include <time.h>

struct aos_rpc *init_rpc;

struct udp_state udp_state;

int inet_aton(const char *cp, uint32_t* address);
int inet_aton(const char *cp, uint32_t* address)
{
    int dots = 0;
    register u_long acc = 0, addr = 0;

    do {
    register char cc = *cp;

    switch (cc) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        acc = acc * 10 + (cc - '0');
        break;

    case '.':
        if (++dots > 3) {
        return 0;
        }
        /* Fall through */

    case '\0':
        if (acc > 255) {
        return 0;
        }
        addr = addr << 8 | acc;
        acc = 0;
        break;

    default:
        return 0;
    }
    } while (*cp++) ;

    /* Normalize the address */
    if (dots < 3) {
    addr <<= 8 * (3 - dots) ;
    }

    /* Store it if requested */
    if (address) {
        *address = htonl(addr);
    }

    return 1;
}

/* NTP DATA, TODO: move to separate file */
struct __attribute__((packed)) ntp_packet{
    uint8_t flags;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t reference_identifier;

    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.
};

void server_connection_established(struct udp_socket socket);
void server_connection_established(struct udp_socket socket){
    debug_printf("Connection with server established\n");

    struct ntp_packet ntp_request;
    memset(&ntp_request, 0, sizeof(struct ntp_packet));
    ntp_request.flags=0xe3; // Copied from wireshark
    ntp_request.stratum=0x0;
    ntp_request.poll=0x3;
    ntp_request.precision=0xfa;
    ntp_request.root_delay=0x100;
    ntp_request.root_dispersion=0x100;
    ntp_request.reference_identifier=0x0;

    udp_send_data(&socket, &ntp_request, sizeof(ntp_request));
}

struct __attribute__((packed)) packed_time{
    uint32_t low;
    uint32_t high;
};

static uint64_t NTP_TIMESTAMP_DELTA = 2208988800ull;

void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len);
void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len){
    debug_printf("###### Remote porrt: %lu local port: %lu\n", data->source_port, data->dest_port);

    struct ntp_packet* packet=(struct ntp_packet* )data->data;

    packet->txTm_s = ntohl( packet->txTm_s ); // Time-stamp seconds.
    packet->txTm_f = ntohl( packet->txTm_f ); // Time-stamp fraction of a second.
    time_t txTm = ( time_t ) ( packet->txTm_s - NTP_TIMESTAMP_DELTA );
    printf( "Time: %s", ctime( ( const time_t* ) &txTm ) );

//    char buf[30];
//    strftime(buf, 30, "%a, %d %b %YYYY %HH:%MM:%SS", ntp_response->transmit_timestamp);
//    debug_printf("Time is: %s\n", buf);

//    struct packed_time* t=(struct packed_time*)&ntp_response->transmit_timestamp;
//    uint32_t time=(t->high);
//    printf("UTC seconds %lu\n",time);
//    printf("%s", asctime(gmtime((time_t*) &time)));
//    uint16_t data_length=lwip_ntohs(data->length)-sizeof(struct udp_packet);
//
//    debug_printf("Received UDP packet of length: %lu and data lenght: %lu\n", len, data_length);
//    debug_printf("Received UDP text: %s\n", data->data);

//    udp_send_data(&socket, data->data, data_length);
}

int main(int argc, char *argv[])
{
	debug_printf("Received %d arguments \n",argc);
	for(int i=0;i<argc;++i){
		debug_printf("Printing argument: %d %s\n",i, argv[i]);
	}
	errval_t err;

    init_rpc = get_init_rpc();
	debug_printf("init rpc: 0x%x\n", init_rpc);
	err = aos_rpc_send_number(get_init_rpc(), (uintptr_t)42);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not send number");
	}

//	ERR_CHECK("Create udp server", udp_create_server(&udp_state, 12345, handle_udp_packet));
	char ntp_server_address[]="81.94.123.17";
	uint32_t ntp_address=0;
	inet_aton(ntp_server_address, &ntp_address);
	ERR_CHECK("Create udp client", udp_connect_to_server(&udp_state, ntp_address, htons(123), handle_udp_packet, server_connection_established));

	return 0;
}
