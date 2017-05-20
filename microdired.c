#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>

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

/* a simple directory buffer;
 * since I am mainly using strings
 * right now, just keep this as a simple
 * list of strings and integers. The 
 * integer is the offset of the original;
 * if it is nil, then the offset == 
 * the index in the buffer. Why do this?
 * because I want to reuse the same buffer
 * interface for all transactions, which means
 * sometimes we have to have that information
 * available. For example, if we enter the 
 * command:
 * > 7,13([A-Z]?.txt)l
 * we want to know _which_ lines matched
 * the glob, not just that there *are* 
 * lines that match the glob.
 */
typedef struct _DIRBUF {
    size_t count;
    int *offsets;
    char **buffer;
} DirectoryBuffer;

int parse(Command *, const char *, int);
DirectoryBuffer *cachedirectory(char *);
DirectoryBuffer *filtercache(Command *, DirectoryBuffer *);
void cleancache(DirectoryBuffer *);

int
main(int ac, char **al, char **el) {
    DirectoryBuffer *curbuf = nil, *view = nil;
    int dot = 0, len = 0, tmp = 0, ret = 0;
    char prompt[32] = {'>', ' ', nul}, linebuf[512] = {0}, curdir[512] = {0};
    const char *editor = getenv("EDITOR"), *home = getenv("HOME");
    Command com;

    (void)getcwd(curdir, 512);
    curbuf = cachedirectory(curdir);

    if(curbuf == nil) {
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
            strlcpy(curdir, home, 512);
            cleancache(curbuf);
            curbuf = cachedirectory(curdir);
        } else if(linebuf[0] == '/') {
            DirectoryBuffer *dtmp = cachedirectory(linebuf);

            if(dtmp == nil) {
                printf("cannot open directory: %s\n", linebuf);
            } else {
                cleancache(curbuf);
                curbuf = dtmp;
                strlcpy(curdir, linebuf, 512);
            }
        } else if(!strncmp(linebuf, ".", 512)) {
            printf("%s\n", curdir);
        } else {
            ret = parse(&com, &linebuf[0], 512);
            view = filtercache(&com, curbuf);

            switch(com.cmd) {
                case 'l':

                    for(tmp = 0; tmp < view->count ; tmp++) {
                        if(view->offsets != nil) {
                            printf("%d\t%s\n", view->offsets[tmp], view->buffer[tmp]);
                        } else {
                            printf("%d\t%s\n", tmp, view->buffer[tmp]);
                        }
                    }
                    break;

                case 'L':

                    for(tmp = 0; tmp < view->count ; tmp++) {
                        printf("%s\n", view->buffer[tmp]);
                    }
                    break;
            }
        }

    }
    
    cleancache(curbuf);
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
                } else if(buffer[offset] == '$') {
                    start = INT_MAX;
                    state = 1;
                } else if(buffer[offset] == '(') {
                    state = 3;
                } else {
                    return 1;
                }
                offset++;
                break;
            case 1:
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
                } else if(buffer[offset] == '$') {
                    stop = INT_MAX;
                    state = 0;
                    offset++;
                } else if(isalpha(buffer[offset])) {
                    state = 0;
                } else if(buffer[offset] == '(') {
                    state = 3;
                    offset++;
                }
                break;
            case 3:
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

DirectoryBuffer *
cachedirectory(char *directory) {
    DIR *dtmp = opendir(directory);
    DirectoryBuffer *ret = nil;
    char **newstack = nil;
    struct dirent *fp = nil;
    size_t count = 0;
    
    if(dtmp == nil) {
        return nil;
    }

    /* this is a *really* funny way 
     * of avoiding complex allocations
     * buuuuuuut... we can avoid or 
     * elide doing a ton of allocations
     * for some sort of stack by reading
     * the directory *twice*. Probably
     * should look at the impact of that
     */
    while(1) {
        fp = readdir(dtmp);

        if(fp == nil) {
            break;
        }

        count += 1;
    }

    rewinddir(dtmp);
    newstack = (char **)malloc(sizeof(char *) * count);

    for(int idx = 0; idx < count; idx++) {
        fp = readdir(dtmp);

        /* technically, something else could
         * have removed a file or the like
         * from this directory; avoid a 
         * TOCTOU bug here by not assuming
         * that the count above == count now.
         * We, of course, run ths risk that
         * we miss a new file now. That's the
         * idea with recaching ever N operations
         * as well too.
         */
        if(fp == nil) {
            break;
        }

        newstack[idx] = strdup(fp->d_name);
    }
    closedir(dtmp);
    ret = (DirectoryBuffer *)malloc(sizeof(DirectoryBuffer));
    ret->count = count;
    ret->offsets = nil;
    ret->buffer = newstack;
    return ret;
}

DirectoryBuffer *
filtercache(Command *cmd, DirectoryBuffer* dirb) {
    return dirb;
}

void
cleancache(DirectoryBuffer *dirb) {
    if(dirb->offsets != nil) {
        free(dirb->offsets);
    }

    for(size_t idx = 0; idx < dirb->count; idx++) {
        free(dirb->buffer[idx]);
    }

    free(dirb->buffer);
}
