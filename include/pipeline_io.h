#ifndef SIGNALS_PIPES_PIPELINE_IO_H
#define SIGNALS_PIPES_PIPELINE_IO_H

void write_int(int fd, int value);
int read_int(int fd, int *value);
void close_pipe_pair(int pipefd[2]);

#endif /* SIGNALS_PIPES_PIPELINE_IO_H */
