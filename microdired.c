#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>

#define nil NULL
#define nul '\0'

#define debugprnt printf("here? %d\n", __LINE__)

/* next step would be to
 * make the cmd into a
 * char *, so that we can
 * have longer command names.
 */
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
    struct stat **stats;
} DirectoryBuffer;

int parse(Command *, const char *, int);
DirectoryBuffer *cachedirectory(char *);
DirectoryBuffer *filtercache(Command *, DirectoryBuffer *);
void cleancache(DirectoryBuffer *);

int
main(int ac, char **al, char **el) {
    DirectoryBuffer *curbuf = nil, *view = nil, *dtmp = nil;
    int dot = 0, len = 0, tmp = 0, ret = 0;
    char prompt[32] = {'>', ' ', nul}, linebuf[512] = {0}, curdir[512] = {0};
    const char *editor = getenv("EDITOR"), *home = getenv("HOME");
    FILE *fp = nil;
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
            dtmp = cachedirectory(linebuf);

            if(dtmp == nil) {
                printf("cannot open directory: %s\n", linebuf);
            } else {
                cleancache(curbuf);
                curbuf = dtmp;
                strlcpy(curdir, linebuf, 512);
            }
        } else if(!strncmp(linebuf, ".", 512)) {
            printf("%s\n", curdir);
        } else if(!strncmp(linebuf, "..", 512)) {
            char *tmp = strrchr(curdir, '/');
            if(tmp != nil) {
                tmp[0] = nul;
                dtmp = cachedirectory(curdir);
                if(dtmp == nil) {
                    printf("cannot open directory %s\n", curdir);
                } else {
                    cleancache(curbuf);
                    curbuf = dtmp;
                }
            }
        } else {
            ret = parse(&com, &linebuf[0], 512);
            view = filtercache(&com, curbuf);

            switch(com.cmd) {
                case 'l':
                    // list directory with numbers
                    for(tmp = 0; tmp < view->count ; tmp++) {
                        if(view->offsets != nil) {
                            printf("%d\t%s\n", view->offsets[tmp], view->buffer[tmp]);
                        } else {
                            printf("%d\t%s\n", tmp, view->buffer[tmp]);
                        }
                    }
                    break;

                case 'L':
                    // list directory sans numbers
                    for(tmp = 0; tmp < view->count ; tmp++) {
                        printf("%s\n", view->buffer[tmp]);
                    }
                    break;

                case 'p':
                    // pretty print
                    for(tmp = 0; tmp < view->count; tmp++) {
                        if(view->offsets != nil) {
                            printf("%d\t%s\n", view->offsets[tmp], view->buffer[tmp]);
                        } else {
                            printf("%d\t%s\n", tmp, view->buffer[tmp]);
                        }
                    } 
                    break;

                case 'P':
                    // pretty print sans numbers
                    for(tmp = 0; tmp < view->count ; tmp++) {
                        printf("%s\n", view->buffer[tmp]);
                    }
                    break;
                
                case 'c': // create a file

                    fp = fopen(com.arg, "w");
                    if(fp == nil) {
                        printf("could not create file \"%s\"\n", com.arg);
                    } else {
                        fclose(fp);
                        fp = nil;
                        if(view != curbuf) {
                            cleancache(view);
                        }
                        dtmp = cachedirectory(curdir);
                        cleancache(curbuf);
                        curbuf = dtmp;
                        view = dtmp;
                    }
                    break;

                case 'C': // create a directory

                    tmp = mkdir(com.arg, 0700);
                    if(tmp == -1) {
                        printf("could not create directory \"%s\"\n", com.arg);
                    } else {
                        if(view != curbuf) {
                            cleancache(view);
                        }
                        dtmp = cachedirectory(curdir);
                        cleancache(curbuf);
                        curbuf = dtmp;
                        view = dtmp;
                    }
                    break;

                case 'f': // only print files
                    break;

                case 'F': // pretty print only files
                    break;

                case 'd': // only print directories
                    break;

                case 'D': // pretty print only directories
                    break;

                case '!': // execute a shell command w/ args
                    break;

                case 'e': // invoke $EDITOR on arguments
                    break;

                case 'E': // ignore $EDITOR, invoke built-in ed(1)
                    break;

                case 't': // test/[-like interface
                    break;

                case 'm': // more, like the pager
                    break;

                case 'M': // mode, like chmod
                    break;

                default:
                    printf("?\n");
                    break;
            }

            if(view != curbuf) {
                cleancache(view);
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
                    // +2, so as to skip the ' '
                    strlcpy(com->arg, &buffer[offset + 2], 256);
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
    struct stat **stats = nil;
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
    stats = (struct stat **)malloc(sizeof(struct stat *) * count);

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
filtercache(Command *com, DirectoryBuffer* dirb) {
    int len = 0, end = 0, count = 0, start = 0;
    DirectoryBuffer *ret = nil;

    if(com->start == -1 && com->stop == -1) {
        return dirb;
    } else if(com->start != -1 && com->stop == -1) {

        if(com->start > dirb->count) {
            return nil;
        }

        ret = (DirectoryBuffer *)malloc(sizeof(DirectoryBuffer));
        ret->count = 1;
        ret->offsets = (int *)malloc(sizeof(int) * 1);
        ret->offsets[0] = com->start;
        ret->buffer = (char **)malloc(sizeof(char *));
        ret->buffer[0] = strdup(dirb->buffer[com->start]);
    } else {
        ret = (DirectoryBuffer *)malloc(sizeof(DirectoryBuffer));
        end = com->stop > dirb->count ? dirb->count : com->stop;
        start = com->start;
        ret->count = end - start;
        count = ret->count;
        ret->offsets = (int *)malloc(sizeof(int) * count);
        ret->buffer = (char **)malloc(sizeof(char *) * count);

        for(int idx = 0; idx < count; idx++) {
            ret->offsets[idx] = idx + start;
            ret->buffer[idx] = strdup(dirb->buffer[idx + start]);
        }
    }

    return ret;
}

void
cleancache(DirectoryBuffer *dirb) {

    if(dirb->offsets != nil) {
        free(dirb->offsets);
    }

    for(size_t idx = 0; idx < dirb->count; idx++) {
        if(dirb->buffer[idx] != nil) {
            free(dirb->buffer[idx]);
        }
    }

    free(dirb->buffer);

    /* I wonder if we should **actually** 
     * do this; could refactor and reuse these
     * objects more frequently... that
     * actually might make more sense.
     */
    free(dirb);
}
