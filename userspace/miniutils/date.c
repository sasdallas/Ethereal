#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    time_t t;
    char buffer[256];

    // Get the current time
    t = time(NULL);
    if (t == -1) {
        perror("time");
        return EXIT_FAILURE;
    }

    // Convert to local time
    struct tm *local_tm_info = localtime(&t);
    if (local_tm_info == NULL) {
        perror("localtime");
        return EXIT_FAILURE;
    }

    // Check if a format is provided
    if (argc > 1) {
        if (strftime(buffer, sizeof(buffer), argv[1], local_tm_info) == 0) {
            printf("date: strftime: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        // Default format
        if (strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Z %Y", local_tm_info) == 0) {
            printf("date: strftime: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    // Print the formatted date
    printf("%s\n", buffer);
    return EXIT_SUCCESS;
}