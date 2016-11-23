#ifndef _HEADER_INIT_INIT
#define _HEADER_INIT_INIT

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include "urpc/urpc.h"
#include "process/coreprocessmgr.h"
#include "process/sysprocessmgr.h"
#include "urpc/urpc.h"

struct aos_rpc rpc;
coreid_t my_core_id;
struct bootinfo *bi;
struct urpc_channel urpc_chan; // URPC thread holds reference to this

errval_t os_core_initialize(int argc, char** argv);
errval_t os_core_events_loop(void);

#endif
