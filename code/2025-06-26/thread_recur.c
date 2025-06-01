#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/resource.h>

#define	print_limit(name) print_limit_(#name, name)

void func2(void);

void func(void)
{
	int fint;
    static int n = 1;
	printf("func (called %5d times): frame at : 0x%12lX\n", n, (unsigned long)&fint);
    n++;
	func2();
}

void func2(void)
{
	int fint;
	(void)printf("func2: frame at : 0x%12lX\n", (unsigned long)&fint);
	func();
}

static void *thread_func_a(void *arg)
{
    func(); // recur until crash
    return 0;
}


void print_limit_(char *name, int resource)
{
	struct rlimit		limit;
	unsigned long long	lim;

	if (getrlimit(resource, &limit) < 0) {
		fprintf(stderr, "getrlimit error for %s", name);
        exit(1);
    }
	printf("%-14s  ", name);
	if (limit.rlim_cur == RLIM_INFINITY) {
		printf("(infinite)  ");
	} else {
		lim = limit.rlim_cur;
		printf("%10lld  ", lim);
	}
	if (limit.rlim_max == RLIM_INFINITY) {
		printf("(infinite)");
	} else {
		lim = limit.rlim_max;
		printf("%10lld", lim);
	}
	putchar((int)'\n');
}

int main(int argc, char **argv)
{
    struct rlimit limit = { 0 };
	if (getrlimit(RLIMIT_STACK, &limit) < 0) {
		perror("getrlimit error for RLIMIT_STACK");
        exit(1);
    }
    limit.rlim_cur = 64; // KiB
    int err = setrlimit(RLIMIT_STACK, &limit);
    if (err == -1) {
        perror("setrlimit failed");
        exit(1);
    }

    print_limit(RLIMIT_STACK);
    print_limit(RLIMIT_AS);
    print_limit(RLIMIT_RSS);

    func();
    if (argc > 1) {
        pthread_t a;
        int err = pthread_create(&a, 0, thread_func_a, 0);
        if (err != 0) {
            fprintf(stderr, "pthread_create error: %d\n", err);
            exit(1);
        }
        pthread_join(a, 0);
        printf("Sentinel\n"); // not reached because thread causes SIGSEGV
    }
    return 0;
}
