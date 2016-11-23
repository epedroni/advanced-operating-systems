#include "process/sysprocessmgr.h"

errval_t sysprocessmgr_init(struct sysprocessmgr_state* pm_state, struct urpc_channel* urpc_channel, coreid_t my_coreid)
{
    pm_state->running_count=0;
    pm_state->urpc_channel=urpc_channel;
    pm_state->my_core_id=my_coreid;

    pm_state->head=NULL;

    return SYS_ERR_OK;
}

errval_t sysprocessmgr_register_process(struct sysprocessmgr_state* pm_state, const char* name, coreid_t core_id, domainid_t* new_pid)
{
    struct sysprocessmgr_process *new_process = malloc(sizeof(struct sysprocessmgr_process));
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
    *new_pid = new_process->pid;

    return SYS_ERR_OK;
}

static struct sysprocessmgr_process* find_process_by_pid(struct sysprocessmgr_process* list, domainid_t pid)
{
    while (list && list->pid != pid)
        list = list->next;
    return list;
}

errval_t sysprocessmgr_deregister_process(struct sysprocessmgr_state* pm_state, domainid_t pid)
{
    struct sysprocessmgr_process *process = find_process_by_pid(pm_state->head, pid);
    if (!process)
        return SYS_PROCMGR_ERR_PROCESS_NOT_FOUND;

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

errval_t sysprocessmgr_get_process_name(struct sysprocessmgr_state* pm_state, domainid_t pid, char* name, size_t buffer_len)
{
    struct sysprocessmgr_process *process = find_process_by_pid(pm_state->head, pid);
    if (!process)
        return SYS_PROCMGR_ERR_PROCESS_NOT_FOUND;
    strncpy(name, process->name, buffer_len);
    return SYS_ERR_OK;
}

/**
 *  Returns the list of processes.
    - pids: preallocated array of size *number where to store PIDs.
    - number: Pointer to the initial size of the list
        Modified to the actual number of entries
*/
errval_t sysprocessmgr_list_pids(struct sysprocessmgr_state* pm_state, domainid_t* pids, size_t* number)
{
    struct sysprocessmgr_process* list = pm_state->head;
    int i = 0;
    int max_count = *number;
    while (list && i < max_count)
    {
        pids[i] = list->pid;
        list = list->next;
    }
    return SYS_ERR_OK;
}
