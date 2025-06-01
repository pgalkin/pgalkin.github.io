#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int global = 0;

int main(void)
{
    int stack = 0;
    int *heap = calloc(1, sizeof(*heap)); // no free

    printf("Initial values:\n");
    printf("  global = %d (%p)\n", global, &global);
    printf("  stack = %d (%p)\n",  stack, &stack);
    printf("  *heap = %d (%p)\n",  *heap, heap);

    switch (fork()) {
        case -1: {
            perror("fork() failed");
            exit(EXIT_FAILURE);
        }
        case 0: {
            global++;
            stack++;
            (*heap)++;

            printf("Child says:\n");
            printf("  My pid: %d\n",     getpid());
            printf("  global = %d (%p)\n", global, &global);
            printf("  stack = %d (%p)\n",  stack, &stack);
            printf("  *heap = %d (%p)\n",  *heap, heap);

            exit(EXIT_SUCCESS);
        }
        default: {
            wait(0);

            printf("Parent says:\n");
            printf("  My pid: %d\n",  getpid());
            printf("  global = %d (%p)\n", global, &global);
            printf("  stack = %d (%p)\n",  stack, &stack);
            printf("  *heap = %d (%p)\n",  *heap, heap);

            printf("Parent increments:\n");

            global++;
            stack++;
            (*heap)++;

            printf("  global = %d (%p)\n", global, &global);
            printf("  stack = %d (%p)\n",  stack, &stack);
            printf("  *heap = %d (%p)\n",  *heap, heap);
        }
    }
    exit(EXIT_SUCCESS);
}
