---
title: Memory Limits
layout: post
date: 2025-06-26T11:54:57Z
tags: [c]
uuid: 0dc59f8f-d7e2-4100-a1d5-0e4dc35ac2d4
---

#### Stack size

Here's a program that prints the memory limits and then recurs until it runs
out of stack space:
```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

#define print_limit(name) print_limit_(#name, name)

void func2(void);

void func(void)
{
    int fint;
    static int n = 1;
    printf("func (called %5d times): frame at : 0x%12lX\n",
      n, (unsigned long)&fint);
    n++;
    func2();
}

void func2(void)
{
    int fint;
    printf("func2: frame at : 0x%12lX\n", (unsigned long)&fint);
    func();
}

void print_limit_(char *name, int resource)
{
    struct rlimit limit;
    unsigned long long lim;

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
    print_limit(RLIMIT_STACK);
    print_limit(RLIMIT_AS);
    print_limit(RLIMIT_RSS);

    if (argc > 1) {
        func(); // recur until crash
    }
    return 0;
}
```
The output on Alpine Linux with default limits, when executed like `./a.out 1`:
```
RLIMIT_STACK       8388608  (infinite)
RLIMIT_AS       (infinite)  (infinite)
RLIMIT_RSS      (infinite)  (infinite)
func (called     1 times): frame at : 0x7FFCCBC725E4
func2: frame at : 0x7FFCCBC725C4
func (called     2 times): frame at : 0x7FFCCBC725A4
func2: frame at : 0x7FFCCBC72584
...
func (called 131015 times): frame at : 0x7FFCCB473464
func2: frame at : 0x
'./a.out 1 > out' terminated by signal SIGSEGV
   (Address boundary error)

~> ulimit -s
8192
```
`ulimit` is a shell built-in, it allows setting a stack limit for all subsequent
program calls from the current shell instance. Memory values are in kilobytes.
```
~> ulimit -s 64
~> a.out 1 > out
~> cat out
RLIMIT_STACK         65536       65536
RLIMIT_AS       (infinite)  (infinite)
RLIMIT_RSS      (infinite)  (infinite)
func (called     1 times): frame at : 0x7FFC036BC1A4
func2: frame at : 0x7FFC036BC184
func (called     2 times): frame at : 0x7FFC036BC164
...
func (called   947 times): frame at : 0x7FFC036AD524
func2: frame at : 0x7FFC036AD504
func (called 
'./a.out 1 > out' terminated by signal SIGSEGV
  (Address boundary error)
```
You cannot increase the limit back:
```
~> ulimit -s 8192
ulimit: Permission denied when changing resource of type
  'Maximum stack size'
```
Opening a new shell instance (not the child though) has the old limit intact. I
suppose if you want a wrapper, you can use something like
```sh
bash -c 'ulimit -s 2 && echo foo
```
There is a notion of a soft limit and a hard limit. Only the soft limit is
enforced by the kernel, the hard limit acts as an upper boundary for the soft
limit. A process can decrease and increase the soft limit, as long as it is
smaller than the hard limit.
```
~> ulimit -Ss 64
~> ulimit -s
64
~> ulimit -Ss 8192
~> ulimit -s
8192
```

It's also possible to change the limit within the program itself:
```c
int main(int argc, char **argv)
{
    struct rlimit limit = { 0 };
    if (getrlimit(RLIMIT_STACK, &limit) < 0) {
        perror("getrlimit error for RLIMIT_STACK");
        exit(1);
    }
    limit.rlim_cur = 64; // KiB, soft limit
    int err = setrlimit(RLIMIT_STACK, &limit);
    if (err == -1) {
        perror("setrlimit failed");
        exit(1);
    }

    print_limit(RLIMIT_STACK);
    print_limit(RLIMIT_AS);
    print_limit(RLIMIT_RSS);

    if (argc > 1) {
        func(); // recur until crash
    }
    return 0;
}
```
`setrlimit` fails if the `rlimit` structure contains `rlim_cur > rlim_max`. So
it's necessary to fill both values, and to avoid guessing I call `getrlimit` to
initially fill it in.

These limits hold true for all child processes and all threads within. Each
thread has its own stack. And this stack size and limit is defined by the limit
that the main thread sets. So if a program starts, limits its stack size to 64
KiB, spawns N threads, then each thread will also have 64 KiB stack size and
limit.

Here's the amended version that spawns 1 thread, which crashes the program:
```c
static void *thread_func_a(void *arg)
{
    func(); // recur until crash
    return 0;
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
        printf("Sentinel\n"); // not reached
    }
    return 0;
}
```

Curiously, these segfaults cannot be intercepted by a signal handler. Ordinary
SIGSEGV, such as those caused by a null pointer dereference, can be
intercepted.

Note:
* The limit is effective immediately. If the process already in violation of
  the limit, it doesn't crash until it tries to allocate more stack space.
* You have no control over the range of stack space in virtual memory. So you
  cannot assume anything about numeric values of stack addresses.
* Just because a program is approaching its memory limit, doesn't mean the
  system is, so OOM will not wake up and kernel will not start swaping space.

#### Observe current limits

Other than the mentioned `getrlimit` there is a file `/proc/<pid>/limits`, on
Linux, which contains current resource limits of the process.
```
~> cat /proc/self/limits
Limit                     Soft Limit           Hard Limit           Units
Max cpu time              unlimited            unlimited            seconds
Max file size             unlimited            unlimited            bytes
Max data size             unlimited            unlimited            bytes
Max stack size            8388608              unlimited            bytes
Max core file size        0                    unlimited            bytes
Max resident set          unlimited            unlimited            bytes
Max processes             127691               127691               processes
Max open files            1024                 4096                 files
Max locked memory         8388608              8388608              bytes
Max address space         unlimited            unlimited            bytes
Max file locks            unlimited            unlimited            locks
Max pending signals       127691               127691               signals
Max msgqueue size         819200               819200               bytes
Max nice priority         0                    0
Max realtime priority     0                    0
Max realtime timeout      unlimited            unlimited            us
```
This is for cat, which in my case is part of busybox. Here's what `ulimit`
itself reports:
```
~> ulimit -a
Maximum size of core files created                              (kB, -c) 0
Maximum size of a process’s data segment                        (kB, -d) unlimited
Control of maximum nice priority                                    (-e) 0
Maximum size of files created by the shell                      (kB, -f) unlimited
Maximum number of pending signals                                   (-i) 127691
Maximum size that may be locked into memory                     (kB, -l) 8192
Maximum resident set size                                       (kB, -m) unlimited
Maximum number of open file descriptors                             (-n) 1024
Maximum bytes in POSIX message queues                           (kB, -q) 800
Maximum realtime scheduling priority                                (-r) 0
Maximum stack size                                              (kB, -s) 8192
Maximum amount of CPU time in seconds                      (seconds, -t) unlimited
Maximum number of processes available to current user               (-u) 127691
Maximum amount of virtual memory available to each process      (kB, -v) unlimited
Maximum contiguous realtime CPU time                                (-y) unlimited
```

#### Why use

I think mostly to decrease unexpected resource consumption. Can also be useful
during testing, as it allows simulating various types of memory and resource
pressure. Ever wanted to make malloc fail? And this is not only about memory
limits, there are many other knobs to turn.
