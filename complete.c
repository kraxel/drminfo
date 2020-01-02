#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include "complete.h"

#define CASE_IMAGE \
    "    --image)\n"                                                 \
    "        COMPREPLY=( $(compgen -f -- \"$cur\") )\n"              \
    "        ;;\n"

void complete_bash(const char *command, struct option *opts)
{
    bool have_image = false;
    char opt_all[1024];
    char opt_arg[1024];
    int pos_all = 0;
    int pos_arg = 0;
    int i;

    for (i = 0; opts[i].name != NULL; i++) {
        /* options with argument completion */
        if (strcmp(opts[i].name, "image") == 0) {
            have_image = true;

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
           "    local cur prev\n"
           "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
           "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
           "    case \"$prev\" in\n"
           "%s"
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
           have_image ? CASE_IMAGE : "",
           opt_arg, opt_all, command, command);
}
