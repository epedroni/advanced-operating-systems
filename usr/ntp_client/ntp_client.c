
#include <stdio.h>
#include <aos/aos.h>
#include <aos/deferred.h>
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

static uint64_t NTP_TIMESTAMP_DELTA = 2208988800ull;

void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len);
void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len){
    struct ntp_packet* packet=(struct ntp_packet* )data->data;

    packet->txTm_s = ntohl( packet->txTm_s ); // Time-stamp seconds.
    packet->txTm_f = ntohl( packet->txTm_f ); // Time-stamp fraction of a second.
    time_t txTm = ( time_t ) ( packet->txTm_s - NTP_TIMESTAMP_DELTA );
    printf("Current UTC time is: %s", ctime( ( const time_t* ) &txTm ) );
}

int main(int argc, char *argv[])
{
    char ntp_server_address[]="81.94.123.17";
    uint32_t ntp_address=0;
    inet_aton(ntp_server_address, &ntp_address);

    struct udp_socket connection_socket;
    ERR_CHECK("Create UDP client", udp_connect_to_server(&udp_state, ntp_address, htons(123), &connection_socket));
    ERR_CHECK("Starting to listen", udp_listen(&connection_socket, handle_udp_packet, true));

    struct ntp_packet ntp_request;
    memset(&ntp_request, 0, sizeof(struct ntp_packet));
    ntp_request.flags=0xe3; // Copied from wireshark
    ntp_request.stratum=0x0;
    ntp_request.poll=0x3;
    ntp_request.precision=0xfa;
    ntp_request.root_delay=0x100;
    ntp_request.root_dispersion=0x100;
    ntp_request.reference_identifier=0x0;

    for(int i=0; i<=5; ++i){
        udp_send_data(&connection_socket, &ntp_request, sizeof(ntp_request));
        barrelfish_usleep(1000000);
    }

    aos_rpc_accept(get_nameserver_rpc());

    return 0;
}
