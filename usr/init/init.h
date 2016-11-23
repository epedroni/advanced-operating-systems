#ifndef _HEADER_INIT_INIT
#define _HEADER_INIT_INIT

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include "urpc/urpc.h"
#include "core_processmgr.h"

struct aos_rpc rpc;
struct core_processmgr_state core_pm_state;
coreid_t my_core_id;
struct bootinfo *bi;

errval_t os_core_initialize(int argc, char** argv);
errval_t os_core_events_loop(void);

#endif