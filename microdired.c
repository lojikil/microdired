#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#define nil NULL
#define nul '\0'

typedef struct _CMD {
    char cmd;
    int start;
    int stop;
    int glob_len;
    char glob[256];
    char arg[256];
} Command;

int parse(Command *, const char *, int);

int
main(int ac, char **al, char **el) {
    DIR *cwd = nil;
    struct dirent *fp = nil;
    int dot = 0, len = 0, tmp = 0, ret = 0;
    char prompt[32] = {'>', ' ', nul}, linebuf[512] = {0}, curdir[512] = {0};
    const char *editor = getenv("EDITOR"), *home = getenv("HOME");
    Command com;

    cwd = opendir(".");
    (void)getcwd(curdir, 512);

    if(cwd == nil) {
        printf("Cannot open current directory!\n");
        return 1;
    }

    while(1) {
        printf("%s", prompt);
        /* probably should have fdin, so that we can script this... */
        fgets(linebuf, 512, stdin);

        if(feof(stdin)) {
            break;
        }

        /* chop off the '\n' */
        len = strnlen(linebuf, 512);
        linebuf[len - 1] = nul;

        if(!strncmp(linebuf, "q", 512)) {
            break;
        } else if(!strncmp(linebuf, "~", 512)) {
            (void)closedir(cwd);
            cwd = opendir(home);
            strlcpy(curdir, home, 512);
        } else if(linebuf[0] == '/') {
            DIR *dtmp = opendir(linebuf);
            if(dtmp == nil) {
                printf("cannot open directory: %s\n", linebuf);
            } else {
                (void)closedir(cwd);
                cwd = dtmp;
                strlcpy(curdir, linebuf, 512);
            }
        } else if(!strncmp(linebuf, ".", 512)) {
            printf("%s\n", curdir);
        } else {
            ret = parse(&com, &linebuf[0], 512);
            printf("parse results: %d, %d, %c\n", com.start, com.stop, com.cmd);
            if(com.glob_len > 0) {
                printf("glob: %s\n", com.glob);
            }
            /* TODO neat idea:
             * - cache directory contents.
             * - create a "view" into the contents based on parsed command.
             * - call functions with this view.
             */
            switch(com.cmd) {
                case 'l':
                    if(cwd != nil) {
                        rewinddir(cwd);
                    }

                    for(tmp = 0; ; tmp ++) {
                        fp = readdir(cwd);
                        if(fp == nil) {
                            break;
                        }

                        if(com.start != -1) {
                            if(com.stop != -1) {
                                if(tmp >= com.start && tmp <= com.stop) {
                                    printf("%d\t%s\n", tmp, fp->d_name);
                                }
                            } else if(tmp == com.start) {
                                printf("%d\t%s\n", tmp, fp->d_name);
                            }
                        } else {
                            printf("%d\t%s\n", tmp, fp->d_name);
                        }
                    }
                    break;
                case 'L':
                    if(cwd != nil) {
                        rewinddir(cwd);
                    }

                    for(tmp = 0; ; tmp ++) {
                        fp = readdir(cwd);
                        if(fp == nil) {
                            break;
                        }

                        if(com.start != -1) {
                            if(com.stop != -1) {
                                if(tmp >= com.start && tmp <= com.stop) {
                                    printf("%s\n", fp->d_name);
                                }
                            } else if(tmp == com.start) {
                                printf("%s\n", fp->d_name);
                            }
                        } else {
                            printf("%s\n", fp->d_name);
                        }
                    }
                    break;
            }
        }

    }

    closedir(cwd);
    return 0;
}

int
parse(Command *com, const char *buffer, int len) {
    int start = -1, stop = -1, offset = 0, state = 0, goffset = 0;
    com->glob_len = 0;
    com->glob[0] = nul;
    com->arg[0] = nul;
    while(offset < len) {
        switch(state) {
            case 0:
                if(isalpha(buffer[offset])) {
                    com->start = start;
                    com->stop = stop;
                    com->cmd = buffer[offset];
                    strlcpy(com->arg, &buffer[offset + 1], 256);
                    return 0;
                } else if(buffer[offset] >= '0' && buffer[offset] <= '9') {
                    start = buffer[offset] - 48;
                    state = 1;
                } else if(buffer[offset] == '(') {
                    state = 3;
                } else {
                    return 1;
                }
                offset++;
                break;
            case 1:
                printf("here?\n");
                if(buffer[offset] >= '0' && buffer[offset] <= '9') {
                    start = (start * 10) + (buffer[offset] - 48);
                    offset++;
                } else if(buffer[offset] == ',') {
                    state = 2;
                    stop = 0;
                    offset++;
                } else if(buffer[offset] == '(') {
                    state = 3;
                    offset++;
                } else {
                    state = 0;
                }
                break;
            case 2:
                if(buffer[offset] >= '0' && buffer[offset] <= '9') {
                    stop = (stop * 10) + (buffer[offset] - 48);
                    offset++;
                } else if(isalpha(buffer[offset])) {
                    state = 0;
                } else if(buffer[offset] == '(') {
                    state = 3;
                    offset++;
                }
                break;
            case 3:
                printf("3here\n");
                if(buffer[offset] == ')') {
                    state = 0;
                    com->glob[goffset] = nul;
                    com->glob_len = goffset;
                } else if(buffer[offset] == '\\') {
                    // XXX not safe; could end a line with an escape
                    com->glob[goffset] = buffer[offset + 1];
                    offset += 1;
                } else {
                    com->glob[goffset] = buffer[offset];
                }
                goffset += 1;
                offset += 1;
                break;
        }
    }
    return 0;
}
