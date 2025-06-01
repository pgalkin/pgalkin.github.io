#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void cat_file(FILE *file, int n, char *prefix)
{
    char buf[BUFSIZ] = {0};
    for (int i = 0; i < n; i++) {
        if (fgets(buf, BUFSIZ, file)) {
            printf("%s: %s", prefix, buf);
        }
    }
}

int main(int argc, char **argv)
{
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("fopen error");
        exit(1);
    }
    cat_file(file, 5, "parent");
    switch (fork()) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0:
            cat_file(file, 5, " child");
            exit(0);
        default:
            wait(0);
            cat_file(file, 5, "parent");
    }
    return 0;
}
