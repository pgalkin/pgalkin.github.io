#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>

void cat_file(int fd, int n, char *prefix)
{
    char buf[BUFSIZ] = {0};
    size_t len = 0; // how many bytes in buf currently
    int line = 0;
    while (line < n && read(fd, buf + len, 1) > 0) {
        len++;
        if (buf[len-1] == '\n') {
            write(STDOUT_FILENO, prefix, strlen(prefix));
            write(STDOUT_FILENO, ": ", 2);
            write(STDOUT_FILENO, buf, len);
            len = 0;
            line++;
        }
    }
    if (len > 0) { // in case last line doesn't end with \n
        write(STDOUT_FILENO, buf, len);
        write(STDOUT_FILENO, "\n", 1);
    }
}

int main(int argc, char **argv)
{
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        exit(1);
    }
    cat_file(fd, 5, "parent");
    switch (fork()) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0:
            cat_file(fd, 5, " child");
            exit(0);
        default:
            wait(0);
            cat_file(fd, 5, "parent");
    }
    return 0;
}
