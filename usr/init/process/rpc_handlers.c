
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

#define MAX_PID 100

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

    // should the running processes be kept in an array instead of a linked list?
    domainid_t pids[MAX_PID];
    size_t numpid = MAX_PID;
    ERROR_RET1(processmgr_list_pids(&pids, &numpid))

    if (numpid * sizeof(domainid_t) > sess->shared_buffer_size)
        return RPC_ERR_BUF_TOO_SMALL;

    memcpy(sess->shared_buffer, pidptr, numpid * sizeof(domainid_t));

    ERROR_RET1(lmp_chan_send2(&sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            MAKE_RPC_MSG_HEADER(RPC_GET_PID, RPC_FLAG_ACK),
            numpid));

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
    errval_t err = processmgr_spawn_process(process_name, core_id, &ret_pid);
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

    debug_printf("Received exit message from endpoint 0x%x\n", sess->lc.endpoint);
    return processmgr_process_exited(sess->lc.endpoint);
}

void processmgr_register_rpc_handlers(struct aos_rpc* rpc)
{
    aos_rpc_register_handler(rpc, RPC_GET_NAME, handle_get_name, false);
    aos_rpc_register_handler(rpc, RPC_GET_PID, handle_get_pid, false);
    aos_rpc_register_handler(rpc, RPC_SPAWN, handle_spawn, false);
    aos_rpc_register_handler(rpc, RPC_EXIT, handle_exit, false);
}
