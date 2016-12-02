#ifndef _HEADER_INIT_INIT
#define _HEADER_INIT_INIT

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include "process/coreprocessmgr.h"
#include "process/sysprocessmgr.h"
#include <aos/urpc/urpc.h>

struct aos_rpc core_rpc;
coreid_t my_core_id;
struct bootinfo *bi;
struct urpc_channel urpc_chan; // URPC thread holds reference to this

errval_t os_core_initialize(int argc, char** argv);
errval_t os_core_events_loop(void);

#define NUM_BOOTINFO_REGIONS 20

#endif
