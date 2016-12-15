#include "nameserver.h"
#include "init.h"

#define DEBUG_NS(s, ...) debug_printf("[NS] " s "\n", ##__VA_ARGS__)

errval_t finish_nameserver(void) {
    errval_t err;

    struct capability selfep, nsep;
    debug_cap_identify(cap_nameserverep, &nsep);
    debug_cap_identify(cap_selfep, &selfep);

    DEBUG_NS("********* Listener: 0x%x", nsep.u.endpoint.listener);
    DEBUG_NS("********* EPOffset: 0x%x", nsep.u.endpoint.epoffset);

    while (nsep.u.endpoint.listener == selfep.u.endpoint.listener
            && nsep.u.endpoint.epoffset == selfep.u.endpoint.epoffset) {
        // wait for nameserver cap
        err = event_dispatch(core_rpc.ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }

        debug_cap_identify(cap_nameserverep, &nsep);
        DEBUG_NS("********* Listener: 0x%x", nsep.u.endpoint.listener);
        DEBUG_NS("********* EPOffset: 0x%x", nsep.u.endpoint.epoffset);
    }

    DEBUG_NS("Received EP from nameserver, moving on...");

    DEBUG_NS("Attempting handshake with nameserver");
    err = aos_rpc_init(&ns_rpc, cap_nameserverep, true);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_MORECORE_INIT); // TODO find a better error
    }

    DEBUG_NS("Testing RPC to nameserver");
    aos_rpc_send_string(&ns_rpc, "Sup buddy");

    return SYS_ERR_OK;
}
