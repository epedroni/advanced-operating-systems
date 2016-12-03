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

    const char* files_and_contents[] = {
        "/home/pandabeer/hello.c", "#include <stdio.h>\n\nint main()\n{\n\tprintf(\"Hello World!\\n\");\n\treturn 0;\n}\n",
        "/home/pandabeer/lorem", "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit.\nSed ac risus rutrum,\nvolutpat ipsum vel,\nfinibus tortor.\n",
        "/home/pandabeer/logo",
            ",-.----.                                                                                                   \n"
            "\\    /  \\                                                             ,---,.                               \n"
            "|   :    \\                              ,---,                       ,'  .'  \\                              \n"
            "|   |  .\\ :                 ,---,     ,---.'|                     ,---.' .' |                      __  ,-. \n"
            ".   :  |: |             ,-+-. /  |    |   | :                     |   |  |: |                    ,' ,'/ /| \n"
            "|   |   \\ : ,--.--.    ,--.'|'   |    |   | |   ,--.--.           :   :  :  /   ,---.     ,---.  '  | |' | \n"
            "|   : .   //       \\  |   |  ,\"' |  ,--.__| |  /       \\          :   |    ;   /     \\   /     \\ |  |   ,' \n"
            ";   | |`-'.--.  .-. | |   | /  | | /   ,'   | .--.  .-. |         |   :     \\ /    /  | /    /  |'  :  /   \n"
            "|   | ;    \\__\\/: . . |   | |  | |.   '  /  |  \\__\\/: . .         |   |   . |.    ' / |.    ' / ||  | '    \n"
            ":   ' |    ,\" .--.; | |   | |  |/ '   ; |:  |  ,\" .--.; |         '   :  '; |'   ;   /|'   ;   /|;  : |    \n"
            ":   : :   /  /  ,.  | |   | |--'  |   | '/  ' /  /  ,.  |         |   |  | ; '   |  / |'   |  / ||  , ;    \n"
            "|   | :  ;  :   .'   \\|   |/      |   :    :|;  :   .'   \\        |   :   /  |   :    ||   :    | ---'     \n"
            "`---'.|  |  ,     .-./'---'        \\   \\  /  |  ,     .-./        |   | ,'    \\   \\  /  \\   \\  /           \n"
            "  `---`   `--`---'                  `----'    `--`---'            `----'       `----'    `----'            \n"
            "                                        ... Well actually we are simply TeamF. But we are still awesome ;)\n",
    };
    const int num_files = sizeof(files_and_contents) / sizeof(files_and_contents[0]);
    assert(num_files % 2 == 0);
    for (int i = 0; i < num_files/2; ++i)
    {
        FILE* f = fopen(files_and_contents[2*i], "w+");
        if (!f)
            continue;
        fprintf(f, files_and_contents[2*i + 1]);
        fclose(f);
    }
    return SYS_ERR_OK;
}

char* shell_read_absolute_path(const char* wd, const char* home, const char* in_path_mod)
{
    size_t path_mod_len = strlen(in_path_mod);

    // Home
    if (path_mod_len == 0)
        return strdup(home);

    char* path_mod = strdup(in_path_mod);
    // Absolute path
    if (path_mod[0] == FS_PATH_SEP)
        return path_mod;

    // Otherwise we need to go through the path_mod...
    char* path = strdup(wd);
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
            path = strdup(home);
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
        if (path[1] != 0) // path = '/'
            strcat(path, "/");
        strcat(path, segment);
    }
    free(path_mod);
    return path;
}

void shell_fs_match_files(char* const argv[], int argc, fs_callback_fn callback, void* data, int flags)
{
    struct shell_state* state = shell_get_state();

    struct fs_fileinfo fi;
    for (int i = 0; i < argc; ++i)
    {
        char* new_path = shell_read_absolute_path(state->wd, state->home, argv[i]);
        errval_t err = stat(new_path, &fi);
        if (err == FS_ERR_NOTFOUND)
            SHELL_STDERR("%s: No such file or directory\n", new_path);
        else
        {
            //SHELL_STDOUT("%s: type %d. Flags = 0x%x\n", new_path, (int)fi.type, (int) flags);
            assert(err_is_ok(err));
            if (fi.type == FS_FILE && !(flags & MATCH_FLAG_FILES))
                SHELL_STDERR("%s: Is not a directory!\n", new_path);
            if (fi.type == FS_DIRECTORY && !(flags & (MATCH_FLAG_DIRECTORIES | MATCH_FLAG_RECURSIVE)))
                SHELL_STDERR("%s: Is a directory!\n", new_path);

            if ((fi.type == FS_FILE && (flags & MATCH_FLAG_FILES)) ||
                (fi.type == FS_DIRECTORY && (flags & MATCH_FLAG_DIRECTORIES)))
                callback(data, argv[i], new_path);

            if (fi.type == FS_DIRECTORY && flags & MATCH_FLAG_RECURSIVE)
            {
                fs_dirhandle_t handle;
                err = opendir(new_path, &handle);
                assert(err_is_ok(err));
                char* name;
                while (err_is_ok(readdir(handle, &name)))
                {
                    char* rel_name = malloc(strlen(argv[i]) + 1 + strlen(name));
                    strcpy(rel_name, argv[i]);
                    if (rel_name[0] != '/' || rel_name[1] != 0)
                        strcat(rel_name, "/");
                    strcat(rel_name, name);
                    char* sub_argv[1] = {rel_name};
                    //SHELL_STDOUT("Recursive call: %s + %s => %s\n", argv[i], name, rel_name);
                    shell_fs_match_files(sub_argv, 1, callback, data, flags);
                    free(name);
                }
                closedir(handle);
            }
        }
        free(new_path);
    }
}
