#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "complete.h"

#define CASE_IMAGE \
    "    --image)\n"                                                 \
    "        COMPREPLY=( $(compgen -f -- \"$cur\") )\n"              \
    "        ;;\n"

#define CASE_CARD                                                    \
    "    --card)\n"                                                  \
    "        words=$(drminfo --complete-card)\n"                     \
    "        COMPREPLY=( $(compgen -W \"$words\" -- \"$cur\") )\n"   \
    "        ;;\n"

#define CASE_FBDEV                                                   \
    "    --fbdev)\n"                                                 \
    "        words=$(fbinfo --complete-fbdev)\n"                     \
    "        COMPREPLY=( $(compgen -W \"$words\" -- \"$cur\") )\n"   \
    "        ;;\n"

#define CASE_OUTPUT                                                    \
    "    --output)\n"                                                  \
    "        words=$(drminfo --complete-output)\n"                     \
    "        COMPREPLY=( $(compgen -W \"$words\" -- \"$cur\") )\n"     \
    "        ;;\n"

void complete_bash(const char *command, struct option *opts)
{
    bool have_image = false;
    bool have_card = false;
    bool have_fbdev = false;
    bool have_output = false;
    char opt_all[1024];
    char opt_arg[1024];
    int pos_all = 0;
    int pos_arg = 0;
    int i;

    for (i = 0; opts[i].name != NULL; i++) {
        /* hide completion options */
        if (strncmp(opts[i].name, "complete-", 9) == 0)
            continue;

        /* options with argument completion */
        if (strcmp(opts[i].name, "image") == 0) {
            have_image = true;
        } else if (strcmp(opts[i].name, "card") == 0) {
            have_card = true;
        } else if (strcmp(opts[i].name, "fbdev") == 0) {
            have_fbdev = true;
        } else if (strcmp(opts[i].name, "output") == 0) {
            have_output = true;

        } else if (opts[i].has_arg) {
            /* options without argument completion */
            pos_arg += snprintf(opt_arg+pos_arg, sizeof(opt_arg)-pos_arg,
                                "%s--%s",
                                pos_arg ? " | " : "",
                                opts[i].name);
        }

        /* (long) option completion */
        pos_all += snprintf(opt_all+pos_all, sizeof(opt_all)-pos_all,
                            "%s--%s",
                            pos_all ? " " : "",
                            opts[i].name);
    }
    if (!pos_arg)
        snprintf(opt_arg+pos_arg, sizeof(opt_arg)-pos_arg, "--dummy");

    printf("_%s_complete()\n"
           "{\n"
           "    local cur prev words\n"
           "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
           "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
           "    case \"$prev\" in\n"
           "%s%s%s%s"
           "    %s)\n"
           "        COMPREPLY=()\n"
           "        ;;\n"
           "    *)\n"
           "        COMPREPLY=( $(compgen -W \"%s\" -- \"$cur\") )\n"
           "        ;;\n"
           "    esac\n"
           "    return 0\n"
           "}\n"
           "complete -F _%s_complete %s\n"
           "\n",
           command,
           have_image  ? CASE_IMAGE  : "",
           have_card   ? CASE_CARD   : "",
           have_fbdev  ? CASE_FBDEV  : "",
           have_output ? CASE_OUTPUT : "",
           opt_arg, opt_all, command, command);
}

void complete_device_nr(const char *prefix)
{
    char filename[128];
    int nr;

    for (nr = 0;; nr++) {
        snprintf(filename, sizeof(filename), "%s%d", prefix, nr);
        if (access(filename, F_OK) < 0)
            return;
        printf("%d\n", nr);
    }
}
