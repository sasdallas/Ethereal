#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void handle_sigcont(int sig) {
    (void)sig; // Suppress unused parameter warning
    printf("Received SIGCONT, resuming execution...\n");
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigcont;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    printf("Process PID: %d\n", getpid());
    
    pid_t cpid = fork();
    if (!cpid) {
        if (sigaction(SIGCONT, &sa, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        for (;;) {
        }
    } else {
        kill(cpid, SIGSTOP);
        printf("Stopped process %d\n", cpid);
        sleep(1);
        printf("Resuming process %d\n", cpid);
        kill(cpid, SIGCONT);

        sleep(1);
    }

    return 0;
}