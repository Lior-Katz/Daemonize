#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <sys/param.h>

#define TIME_LOG_FILE ("time.txt")

#define ERR(name) \
    do { \
        perror(# name); \
        exit(EXIT_FAILURE); \
    } while (false)

int max3(int n1, int n2, int n3) {
    return MAX(MAX(n1, n2), n3);
}

// forward declaration because headers do not contain it, but syscall wrapper does exist.
int close_range(unsigned int first, unsigned int last, unsigned int flags);

void time_log();
void become_orphan();
void detach_from_session();
void change_cwd();
void close_fds();

void deamonize() {
    umask(0);
    change_cwd();
    close_fds();
    become_orphan();
    detach_from_session();
}

int main() {
    deamonize();
    time_log();
    
    return 0;
}

void time_log() {
    while (1) {
        FILE *f = fopen(TIME_LOG_FILE, "w");  // overwrite file
        if (!f) {
            ERR(fopen);
        }

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        fprintf(f, "%02d:%02d:%02d\n",
                t->tm_hour,
                t->tm_min,
                t->tm_sec);
        fclose(f);

        sleep(1);
    }
}

void become_orphan() {
    pid_t pid = fork();
    switch (pid) {
        case -1:
            ERR(fork);
        case 0:
            // child
            return;
        default:
            // parent
            exit(EXIT_SUCCESS);
    }
}

void detach_from_session() {
    // After fork, the process is not a process group leader.
    pid_t sid = setsid();
    if (sid == -1) {
        ERR(setsid);
    }
    assert(sid == getpid());

    // Session leaders could acquire a controlling terminal.
    // Forking again causes the process to no longer be a session leader, meaning it can no longer
    // bind to a terminal.
    become_orphan();
    assert(getsid(getpid()) != getpid());
}

void change_cwd() {
    if (chdir("/") != 0) {
        ERR(cwd);
    }
}

void close_fds() {
    int devnull_fd = open("/dev/null", O_RDWR);
    if (devnull_fd == -1) {
        ERR(open);
    }

    int final_std_fd = max3(STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
    for (int fd = 0; fd <= final_std_fd; fd++) {
        if (dup2(devnull_fd, fd) == -1) {
            ERR(dup2);
        }
    }
    
    if (close_range(final_std_fd + 1, ~0U, 0) == -1) {
        ERR(close_range);
    }
}