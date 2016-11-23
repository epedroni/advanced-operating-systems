#include "process/coreprocessmgr.h"

// ProcessMgr functions
errval_t coreprocessmgr_spawn_process(struct coreprocessmgr_state* pm_state, char* process_name,
        struct aos_rpc* rpc, coreid_t core_id, domainid_t *ret_pid){
    errval_t err;

    //TODO: Ask main PM for new pid for process

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
        //TODO: Notify PM to remove newly created pid
        return err;
    }

    ERROR_RET1(aos_server_register_client(rpc, sess));

    // add to running processes list
    struct running_process *rp = malloc(sizeof(struct running_process));
    rp->prev = NULL;
    rp->next = pm_state->running_procs;
    if (rp->next) {
        rp->next->prev = rp;
    }
    rp->pid = pm_state->next_pid++;
    rp->name = process_name;
    rp->endpoint = sess->lc.endpoint;

    pm_state->running_count++;

    debug_printf("Spawned process with endpoint 0x%x\n", rp->endpoint);

    pm_state->running_procs = rp;

    *ret_pid = rp->pid;
    return SYS_ERR_OK;
}

// TODO these handlers need to go somewhere else
static
errval_t handle_get_name(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);
    assert(context && "Context to core process mgr must be set");
    struct coreprocessmgr_state* pm_state=(struct coreprocessmgr_state*)context;

    //TODO: Ask PM for process name

    struct running_process *rp = pm_state->running_procs;
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
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);
    assert(context && "Context to core process mgr must be set");
    struct coreprocessmgr_state* pm_state=(struct coreprocessmgr_state*)context;

    // should the running processes be kept in an array instead of a linked list?
    domainid_t pids[pm_state->running_count];
    struct running_process *rp = pm_state->running_procs;
    for (int i = 0; i < pm_state->running_count && rp; i++) {
        pids[i] = rp->pid;
        rp = rp->next;
    }
    domainid_t *pidptr = &pids[0];

    if (pm_state->running_count * sizeof(domainid_t) > sess->shared_buffer_size)
        return RPC_ERR_BUF_TOO_SMALL;

    memcpy(sess->shared_buffer, pidptr, pm_state->running_count * sizeof(domainid_t));

    ERROR_RET1(lmp_chan_send2(&sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            MAKE_RPC_MSG_HEADER(RPC_GET_PID, RPC_FLAG_ACK),
            pm_state->running_count));

    return SYS_ERR_OK;
}

static
errval_t handle_spawn(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);
    assert(context && "Context to core process mgr must be set");
    struct coreprocessmgr_state* pm_state=(struct coreprocessmgr_state*)context;

    if (!sess->shared_buffer_size)
        return RPC_ERR_SHARED_BUF_EMPTY;

    size_t string_size = msg->words[1];
    coreid_t core_id = msg->words[2];
    ASSERT_PROTOCOL(string_size <= sess->shared_buffer_size);

    char* process_name = malloc(string_size + 1);
    memcpy(process_name, sess->shared_buffer, string_size);
    process_name[string_size] = 0;

    domainid_t ret_pid;
    errval_t err = coreprocessmgr_spawn_process(pm_state, process_name, sess->rpc, core_id, &ret_pid);
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
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);
    assert(context && "Context to core process mgr must be set");
    struct coreprocessmgr_state* pm_state=(struct coreprocessmgr_state*)context;

    debug_printf("Received exit message from endpoint 0x%x\n", sess->lc.endpoint);
    struct running_process *rp = pm_state->running_procs;

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
        if (rp == pm_state->running_procs) {
            pm_state->running_procs = rp->next;
        }
        pm_state->running_count--;

        free(rp->name);
        free(rp);
    }

    return SYS_ERR_OK;
}

static
void register_rpc_handlers(struct coreprocessmgr_state* pm_state, struct aos_rpc* rpc)
{
    aos_rpc_register_handler_with_context(rpc, RPC_GET_NAME, handle_get_name, false, pm_state);
    aos_rpc_register_handler_with_context(rpc, RPC_GET_PID, handle_get_pid, false, pm_state);
    aos_rpc_register_handler_with_context(rpc, RPC_SPAWN, handle_spawn, false, pm_state);
    aos_rpc_register_handler_with_context(rpc, RPC_EXIT, handle_exit, false, pm_state);
}

errval_t coreprocessmgr_init(struct coreprocessmgr_state* pm_state, coreid_t core_id,
        struct aos_rpc* rpc, const char* init_name)
{
    //TODO: Ask process manager for id for init process

    pm_state->core_id=core_id;
    pm_state->running_count=0;

    size_t namelen = strlen(init_name);

    struct running_process *init_rp = malloc(sizeof(struct running_process));
    init_rp->prev = NULL;
    init_rp->next = NULL;
    init_rp->pid = core_id;
    init_rp->name = malloc(namelen + 1);
    memcpy(init_rp->name, init_name, namelen+1);

    init_rp->endpoint = NULL;
    pm_state->running_procs=init_rp;
    register_rpc_handlers(pm_state, rpc);
    return SYS_ERR_OK;
}
