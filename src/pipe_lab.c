// Methasit Pittayanukoon
// 6880671


#include "pipe_lab.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * NOTE on fflush(stdout) before fork():
 * When stdout is a pipe or file (e.g. the test script captures output with
 * $(...)), stdio is fully buffered. fork() copies that buffer into the child,
 * so any not-yet-flushed text would be printed twice. Flushing before every
 * fork() prevents duplicated lines.
 *
 * NOTE on _exit() in the child:
 * The child must never `return` from these functions — it would fall back
 * into main() and run the remaining tasks as well. The child flushes its own
 * output and calls _exit().
 */

/*
 * Task A – one-way pipe
 *
 * The parent writes the string "ping" into a pipe.
 * The child reads it and prints exactly:
 *   child received: ping
 * The parent then waits for the child and prints exactly:
 *   parent: sent ping
 *
 * Required steps in the child:
 *   1. close fd[1] (unused write end)
 *   2. read the message into a buffer
 *   3. null-terminate the buffer and print "child received: <message>"
 *   4. close fd[0]
 *
 * Required steps in the parent:
 *   1. close fd[0] (unused read end)
 *   2. write the string "ping" into fd[1]
 *   3. close fd[1]
 *   4. waitpid() for the child
 *   5. print "parent: sent ping"
 */
int task_one_way(void) {
    int fd[2];
    if (pipe(fd) == -1) { perror("pipe"); return 1; }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* 1. child only reads: close the write end */
        close(fd[1]);

        /* 2. read the message */
        char buf[64];
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        if (n < 0) { perror("read"); close(fd[0]); _exit(1); }

        /* 3. null-terminate and print */
        buf[n] = '\0';
        printf("child received: %s\n", buf);
        fflush(stdout);

        /* 4. close the read end */
        close(fd[0]);
        _exit(0);
    }

    /* 1. parent only writes: close the read end */
    close(fd[0]);

    /* 2. send "ping" (without the '\0'; the child adds it) */
    const char *msg = "ping";
    if (write(fd[1], msg, strlen(msg)) == -1) {
        perror("write");
        close(fd[1]);
        waitpid(pid, NULL, 0);
        return 1;
    }

    /* 3. close the write end */
    close(fd[1]);

    /* 4. wait for the child */
    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); return 1; }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;

    /* 5. report */
    printf("parent: sent ping\n");
    return 0;
}

/*
 * Task B – two-way pipe
 *
 * Pipes are unidirectional, so bidirectional communication needs TWO pipes:
 *   p2c[2]: parent writes → child reads
 *   c2p[2]: child writes  → parent reads
 *
 * The parent sends the integer 21 through p2c.
 * The child reads it, doubles it to 42, and prints exactly:
 *   child computed: 42
 * The child sends the result back through c2p.
 * The parent reads the result and prints exactly:
 *   parent received: 42
 *
 * Required: close every pipe end that is not used in each process.
 * Hint: write/read a raw int with write(fd, &val, sizeof(val)).
 */
int task_two_way(void) {
    int p2c[2], c2p[2];
    if (pipe(p2c) == -1 || pipe(c2p) == -1) { perror("pipe"); return 1; }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* child reads from p2c and writes to c2p; close the other ends */
        close(p2c[1]);
        close(c2p[0]);

        int val;
        if (read(p2c[0], &val, sizeof(val)) != (ssize_t)sizeof(val)) {
            perror("child read");
            close(p2c[0]); close(c2p[1]);
            _exit(1);
        }
        close(p2c[0]);

        int result = val * 2;
        printf("child computed: %d\n", result);
        fflush(stdout); /* make sure this line appears before the parent's */

        if (write(c2p[1], &result, sizeof(result)) != (ssize_t)sizeof(result)) {
            perror("child write");
            close(c2p[1]);
            _exit(1);
        }
        close(c2p[1]);
        _exit(0);
    }

    /* parent writes to p2c and reads from c2p; close the other ends */
    close(p2c[0]);
    close(c2p[1]);

    int val = 21;
    if (write(p2c[1], &val, sizeof(val)) != (ssize_t)sizeof(val)) {
        perror("parent write");
        close(p2c[1]); close(c2p[0]);
        waitpid(pid, NULL, 0);
        return 1;
    }
    close(p2c[1]);

    int result;
    ssize_t n = read(c2p[0], &result, sizeof(result));
    close(c2p[0]);

    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); return 1; }
    if (n != (ssize_t)sizeof(result)) { fprintf(stderr, "parent read failed\n"); return 1; }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;

    printf("parent received: %d\n", result);
    return 0;
}

/*
 * Task C – EOF limitation
 *
 * When every write end of a pipe is closed, the next read() returns 0.
 * This is how a reader detects end-of-stream (EOF).
 *
 * Sequence:
 *   parent: write "hello" (5 bytes) into the pipe, then CLOSE the write end
 *   child:  read in a loop until read() returns 0; track total bytes
 *           print exactly: child: got EOF after 5 bytes
 *
 * The loop must check the return value of read() on every iteration.
 * The child should NOT assume a single read delivers all the bytes.
 */
int task_eof(void) {
    int fd[2];
    if (pipe(fd) == -1) { perror("pipe"); return 1; }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* Must close our copy of the write end, otherwise read() would never
         * return 0 — a write end would still be open (in this very process). */
        close(fd[1]);

        char buf[16];
        size_t total = 0;
        for (;;) {
            ssize_t n = read(fd[0], buf, sizeof(buf));
            if (n > 0) {
                total += (size_t)n;          /* got some data, keep going */
            } else if (n == 0) {
                break;                        /* EOF: all writers closed */
            } else if (errno == EINTR) {
                continue;                     /* interrupted by a signal, retry */
            } else {
                perror("read");
                close(fd[0]);
                _exit(1);
            }
        }

        printf("child: got EOF after %zu bytes\n", total);
        fflush(stdout);
        close(fd[0]);
        _exit(0);
    }

    close(fd[0]);

    const char *msg = "hello";
    if (write(fd[1], msg, strlen(msg)) == -1) {
        perror("write");
        close(fd[1]);
        waitpid(pid, NULL, 0);
        return 1;
    }
    close(fd[1]); /* this is what makes the child's read() return 0 */

    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); return 1; }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;
    return 0;
}

/*
 * Task D – SIGPIPE / EPIPE limitation
 *
 * Writing to a pipe with no readers delivers SIGPIPE to the writer,
 * killing it unless the signal is ignored.  With SIGPIPE ignored,
 * write() returns -1 and sets errno to EPIPE.
 *
 * A synchronisation pipe (sync_fd) is used so the parent only writes
 * after the child has already closed the read end of data_fd.
 *
 * Required sequence:
 *   child:  close data_fd[0] and data_fd[1]
 *           write one byte to sync_fd[1] (ack), close sync_fd[1], exit
 *   parent: close sync_fd[1]
 *           read one byte from sync_fd[0] (blocks until child sends ack)
 *           close sync_fd[0] and data_fd[0]
 *           call signal(SIGPIPE, SIG_IGN)
 *           write one byte to data_fd[1]; check if errno == EPIPE
 *           print exactly: parent: broken pipe detected
 *           close data_fd[1], waitpid
 */
int task_broken_pipe(void) {
    int data_fd[2];
    int sync_fd[2];
    if (pipe(data_fd) == -1 || pipe(sync_fd) == -1) { perror("pipe"); return 1; }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* child doesn't use data_fd at all, and only writes to sync_fd */
        close(data_fd[0]);
        close(data_fd[1]);
        close(sync_fd[0]);

        char ack = 'x';
        if (write(sync_fd[1], &ack, 1) != 1) { perror("child write"); }
        close(sync_fd[1]);
        _exit(0);
    }

    close(sync_fd[1]);

    /* block until the child confirms it has closed its data_fd ends */
    char ack;
    if (read(sync_fd[0], &ack, 1) != 1) {
        fprintf(stderr, "sync read failed\n");
        close(sync_fd[0]); close(data_fd[0]); close(data_fd[1]);
        waitpid(pid, NULL, 0);
        return 1;
    }
    close(sync_fd[0]);

    /* close the LAST read end -> the pipe now has no readers */
    close(data_fd[0]);

    /* ignore SIGPIPE so write() fails with EPIPE instead of killing us */
    void (*old_handler)(int) = signal(SIGPIPE, SIG_IGN);

    char byte = 'z';
    ssize_t n = write(data_fd[1], &byte, 1);
    int saved_errno = errno;

    signal(SIGPIPE, old_handler); /* restore previous behaviour */
    close(data_fd[1]);

    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); return 1; }

    if (n == -1 && saved_errno == EPIPE) {
        printf("parent: broken pipe detected\n");
        return 0;
    }

    fprintf(stderr, "expected EPIPE, got n=%zd errno=%d\n", n, saved_errno);
    return 1;
}

/* ------------------------------------------------------------------ */
int main(void) {
    if (task_one_way() != 0)     { fprintf(stderr, "task_one_way failed\n");     return 1; }
    if (task_two_way() != 0)     { fprintf(stderr, "task_two_way failed\n");     return 1; }
    if (task_eof() != 0)         { fprintf(stderr, "task_eof failed\n");         return 1; }
    if (task_broken_pipe() != 0) { fprintf(stderr, "task_broken_pipe failed\n"); return 1; }
    printf("all tasks done\n");
    return 0;
}
