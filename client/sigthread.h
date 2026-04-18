#ifndef BOMBER_CLIENT_SIGTHREAD_H
#define BOMBER_CLIENT_SIGTHREAD_H

void st_pipe_init(void);

int st_pipe_read_fd(void);

void *signal_thread(void *arg);

#endif
