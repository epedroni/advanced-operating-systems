#include "processmgr.h"

errval_t processmgr_init(struct processmgr_state* pm_state, struct urpc_channel* urpc_channel, coreid_t my_coreid){
    pm_state->running_count=0;
    pm_state->urpc_channel=urpc_channel;
    pm_state->my_core_id=my_coreid;

    pm_state->head=NULL;

    return SYS_ERR_OK;
}

errval_t processmgr_register_process(struct processmgr_state* pm_state, const char* name, coreid_t core_id, domainid_t* new_pid){
    struct processmgr_process *new_process = malloc(sizeof(struct processmgr_process));
    size_t namelen = strlen(name);
    namelen++;

    new_process->core_id=core_id;
    new_process->pid=pm_state->next_pid++;

    new_process->name=malloc(namelen);
    strncpy(new_process->name, name, namelen);
    new_process->next=pm_state->head;
    new_process->prev=NULL;
    if(pm_state->head)
        pm_state->head->prev=new_process;
    pm_state->head=new_process;
    pm_state->running_count++;

    return SYS_ERR_OK;
}

errval_t processmgr_deregister_process(struct processmgr_state* pm_state, domainid_t pid){
    struct processmgr_process *process=pm_state->head;
    while(process){
        if(process->pid==pid)
            break;
        process=process->next;
    }

    if(!process)
        return SYS_ERR_ILLEGAL_INVOCATION;  //TODO: Create propper error messages

    if(process->next){
        process->next->prev=process->prev;
    }
    if(process->prev){
        process->prev->next=process->next;
    }
    if(process==pm_state->head){
        pm_state->head=process->next;
    }
    pm_state->running_count--;

    free(process->name);
    free(process);

    return SYS_ERR_OK;
}

errval_t processmgr_get_process_name(struct processmgr_state* pm_state, domainid_t pid, char** name, size_t buffer_len){


    return SYS_ERR_OK;
}

errval_t processmgr_get_proces_ids(struct processmgr_state* pm_state, const char* name, domainid_t* pids, size_t* number){

    return SYS_ERR_OK;
}


#if 0
#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/aos_rpc.h>
#include "processmgr.h"

// ProcessMgr data
//struct running_process
//{
//    struct running_process *next, *prev;
//    domainid_t pid;
//    char *name;
//    struct lmp_endpoint *endpoint;
//};
//
//static struct running_process *running_procs = NULL;
//static uint32_t running_count = 0;
//static domainid_t next_pid = 0;

errval_t processmgr_allocate_process_id(struct processmgr_state* pm_state, const char* process_name, coreid_t core_id,
        domainid_t* new_id){
    errval_t err;

    struct running_process *rp = malloc(sizeof(struct running_process));
    rp->prev = NULL;
    rp->next = pm_state->running_procs;
    if (rp->next) {
        rp->next->prev = rp;
    }
    rp->pid = pm_state->next_pid++;
    rp->name = process_name;
    rp->endpoint = sess->lc.endpoint;

    running_count++;

    debug_printf("Spawned process with endpoint 0x%x\n", rp->endpoint);

    running_procs = rp;

    *ret_pid = rp->pid;

    return SYS_ERR_OK;
}

// ProcessMgr functions
errval_t spawn_process(char* process_name, struct aos_rpc* rpc, coreid_t core_id, domainid_t *ret_pid){
    errval_t err;
    struct aos_rpc_session* sess = NULL;
    ERROR_RET1(aos_server_add_client(rpc, &sess));

    struct spawninfo* process_info = malloc(sizeof(struct spawninfo));
    process_info->core_id=core_id;
    err = spawn_load_by_name(process_name,
        process_info,
        &sess->lc);
    free(process_info);
    if(err_is_fail(err)) {
        *ret_pid = 0;
        return err;
    }

    ERROR_RET1(aos_server_register_client(rpc, sess));

    // add to running processes list
    struct running_process *rp = malloc(sizeof(struct running_process));
    rp->prev = NULL;
    rp->next = running_procs;
    if (rp->next) {
        rp->next->prev = rp;
    }
    rp->pid = next_pid++;
    rp->name = process_name;
    rp->endpoint = sess->lc.endpoint;

    running_count++;

    debug_printf("Spawned process with endpoint 0x%x\n", rp->endpoint);

    running_procs = rp;

    *ret_pid = rp->pid;
    return SYS_ERR_OK;
}

// TODO these handlers need to go somewhere else
static
errval_t handle_get_name(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);

    struct running_process *rp = running_procs;
    domainid_t requested_pid = msg->words[1];
    while (rp && rp->pid != requested_pid) {
        rp = rp->next;
    }

    size_t size = 0;
    if (rp) {
        size = strlen(rp->name);
        if (size+1 > sess->shared_buffer_size)
            return RPC_ERR_BUF_TOO_SMALL;
        memcpy(sess->shared_buffer, rp->name, size + 1);
    }

    ERROR_RET1(lmp_chan_send2(&sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            MAKE_RPC_MSG_HEADER(RPC_GET_NAME, rp ? RPC_FLAG_ACK : RPC_FLAG_ERROR),
            size));

    return SYS_ERR_OK;
}

static
errval_t handle_get_pid(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);

    // should the running processes be kept in an array instead of a linked list?
    domainid_t pids[running_count];
    struct running_process *rp = running_procs;
    for (int i = 0; i < running_count && rp; i++) {
        pids[i] = rp->pid;
        rp = rp->next;
    }
    domainid_t *pidptr = &pids[0];

    if (running_count * sizeof(domainid_t) > sess->shared_buffer_size)
        return RPC_ERR_BUF_TOO_SMALL;

    memcpy(sess->shared_buffer, pidptr, running_count * sizeof(domainid_t));

    ERROR_RET1(lmp_chan_send2(&sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            MAKE_RPC_MSG_HEADER(RPC_GET_PID, RPC_FLAG_ACK),
            running_count));

    return SYS_ERR_OK;
}

static
errval_t handle_spawn(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    if (!sess->shared_buffer_size)
        return RPC_ERR_SHARED_BUF_EMPTY;

    size_t string_size = msg->words[1];
    coreid_t core_id = msg->words[2];
    ASSERT_PROTOCOL(string_size <= sess->shared_buffer_size);

    char* process_name = malloc(string_size + 1);
    memcpy(process_name, sess->shared_buffer, string_size);
    process_name[string_size] = 0;

    domainid_t ret_pid;
    errval_t err = spawn_process(process_name, sess->rpc, core_id, &ret_pid);
    if (err_is_fail(err))
    {
        // don't need to free otherwise because it is assigned to running_proc
        free(process_name);
        ret_pid = 0;
    }

    ERROR_RET1(lmp_chan_send2(&sess->lc,
                LMP_FLAG_SYNC,
                NULL_CAP,
                MAKE_RPC_MSG_HEADER(RPC_SPAWN, (err_is_fail(err) ? RPC_FLAG_ERROR : RPC_FLAG_ACK)),
                ret_pid));

    return SYS_ERR_OK;
}

static
errval_t handle_exit(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    debug_printf("Received exit message from endpoint 0x%x\n", sess->lc.endpoint);
    struct running_process *rp = running_procs;

    while (rp && rp->endpoint != sess->lc.endpoint) {
        rp = rp->next;
    }

    if (rp) {
        if (rp->next) {
            rp->next->prev = rp->prev;
        }
        if (rp->prev) {
            rp->prev->next = rp->next;
        }
        if (rp == running_procs) {
            running_procs = rp->next;
        }
        running_count--;

        free(rp->name);
        free(rp);
    }

    return SYS_ERR_OK;
}

errval_t processmgr_init(struct aos_rpc* rpc, const char* init_name)
{
    size_t namelen = strlen(init_name);

    struct running_process *init_rp = malloc(sizeof(struct running_process));
    init_rp->prev = NULL;
    init_rp->next = NULL;
    init_rp->pid = next_pid++;
    init_rp->name = malloc(namelen + 1);
    memcpy(init_rp->name, init_name, namelen+1);

    init_rp->endpoint = NULL;
    running_count = 1;
    running_procs = init_rp;
    processmgr_register_rpc_handlers(rpc);
    return SYS_ERR_OK;
}

void processmgr_register_rpc_handlers(struct aos_rpc* rpc)
{
    aos_rpc_register_handler(rpc, RPC_GET_NAME, handle_get_name, false);
    aos_rpc_register_handler(rpc, RPC_GET_PID, handle_get_pid, false);
    aos_rpc_register_handler(rpc, RPC_SPAWN, handle_spawn, false);
    aos_rpc_register_handler(rpc, RPC_EXIT, handle_exit, false);
}
#endif
