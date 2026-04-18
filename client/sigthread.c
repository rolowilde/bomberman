#include <bits/types/sigset_t.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

static int sig_pipe[2];

void st_pipe_init(void) {
    int flags;

    pipe(sig_pipe);
    flags = fcntl(sig_pipe[0], F_GETFL, 0);
    fcntl(sig_pipe[0], F_SETFL, flags | O_NONBLOCK);
}

int st_pipe_read_fd(void) {
    return sig_pipe[0];
}

void *signal_thread(void *arg) {
    sigset_t *set = arg;
    int sig;
    
    while (1) {
        sigwait(set, &sig);
        write(sig_pipe[1], &sig, sizeof(sig));
    }
}
