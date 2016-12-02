#include "shell.h"
#include <fs/fs.h>
#include <fs/dirent.h>


errval_t shell_setup_file_system(void)
{
    // Setup file system
    ERROR_RET1(filesystem_init());

    const char* init_directories[] = {
        "/home",
        "/home/pandabeer",
        "/home/pandabeer/Documents",
        "/home/pandabeer/Downloads",
        "/home/pandabeer/Music",
        "/home/pandabeer/Pictures",
        "/home/pandabeer/Public",
        "/home/pandabeer/Videos",
        "/root",
        "/usr",
        "/sbin",
        "/bin",
        "/lib",
        "/lib64",
        "/etc",
    };
    for (int i = 0; i < sizeof(init_directories) / sizeof(init_directories[0]); ++i)
        ERROR_RET1(mkdir(init_directories[i]));
    return SYS_ERR_OK;
/*
    fs_dirhandle_t handle;
    err = opendir("/", &handle);
    DEBUG_ERR(err, "opendir /");
    char* name;
    err = readdir(handle, &name);
    DEBUG_ERR(err, "readdir");
    debug_printf("Found %s\n", name);
    closedir(handle);
*/
}

char* shell_read_absolute_path(struct shell_state* state, char* path_mod)
{
    size_t path_mod_len = strlen(path_mod);

    // Home
    if (path_mod_len == 0)
        return strdup(state->home);

    // Absolute path
    if (path_mod[0] == FS_PATH_SEP)
        return strdup(path_mod);

    // Otherwise we need to go through the path_mod...
    char* path = strdup(state->wd);
    size_t path_mod_pos = 0;
    while (path_mod_pos < path_mod_len)
    {
        assert(path[0] == FS_PATH_SEP); // Invariant
        size_t segment_begin = path_mod_pos;
        // Find the next "/"
        while (path_mod[path_mod_pos] && path_mod[path_mod_pos] != FS_PATH_SEP)
            ++path_mod_pos;
        if (path_mod[path_mod_pos] == FS_PATH_SEP)
        {
            path_mod[path_mod_pos] = 0;
            ++path_mod_pos;
        }
        char* segment = &path_mod[segment_begin];
        // Special cases...
        if (!strcmp(segment, "")) // foo//bar/...
            continue;
        if (!strcmp(segment, "."))
            continue;
        if (!strcmp(segment, "~"))
        {
            free(path);
            path = strdup(state->home);
            continue;
        }
        if (!strcmp(segment, ".."))
        {
            size_t path_len = strlen(path);
            if (path_len == 1) // We are at "/"
                continue;
            // Find the last "/"
            int i = path_len;
            while (i > 0 && path[i] != FS_PATH_SEP)
                --i;
            if (i == 0)
            {
                free(path);
                path = strdup("/");
                continue;
            }
            // Cut the path
            path[i] = 0;
            continue;
        }
        // Just going to a folder:
        // path = path + '/' + segment
        path = realloc(path, strlen(path) + 1 + strlen(segment) + 1);
        strcat(path, "/");
        strcat(path, segment);
    }
    return path;
}
