---
title: Copy-on-Write and the OOM killer
layout: post
date: 2025-06-22T09:42:51Z
tags: [linux]
uuid: 1fe85f23-1c87-4e8e-b021-aa4abead8fac
---

Copy-on-Write (CoW) is a common optimization technique used by filesystems
like btrfs and operating system memory managers. It reduces memory
consumption and improves performance by deferring page allocation and copying
until an actual page write occurs.

Consider a large Linux process (2 GiB) that forks, creating a child process.
Without CoW, this would allocate another 2 GiB and copy all data from the
parent - a high price to pay, especially if the child subsequently calls `exec`
and morphs into a much smaller process (1 MiB). CoW addresses this by
postponing allocation and copying until the child actually modifies memory,
akin to lazy evaluation.

You might think that forking a big process only to exec a small one doesn't
make much sense, and perhaps there's a better way to design a program. That is
correct in principle, but still, lots of stuff is happenning on the system, and
many benefit from CoW.

On Linux, fork is a fundamental way to create new processes. Actually, if you
use glibc, its fork wrapper uses the [clone][clone] syscall. It is conceptually
similar, and both rely on CoW, a mechanism that is built into the kernel.

CoW operates at page granularity. I've recently learned that Linux has
[Transparent Hugepage Support][ths] but for the sake of simplicity let's assume
that a page is 4 KiB, the minimum Linux can request from physical memory. So
even if you request to allocate a single byte, Linux will provide more. Exactly
how much more depends on the allocation procedure you used (e.g. malloc vs
mmap) and the inner turnings of kernel's memory management gears, but generally
speaking your request gets rounded up to the nearest page multiple. So you'll
get 4 KiB.

There are virtual pages and physical pages. What you ordinarily interact with
are virtual pages, while CoW works with physical pages. To allocate a virtual
page means to map it to a physical page, which itself must also be allocated.

In order to allocate a physical page it must first be accessed with a read or
write. This access makes Memory Management Unit (MMU, which is hardware) raise
an exception. The kernel maintains an array of functions called fault handlers.
An exception is used as an index into this array, invoking the appropriate
handler. An appropriate handler in this case is the page fault handler. If the
access is valid, the page can be mapped; if invalid a segmentation fault
occurs.

There's a difference between a page fault and a segmentation fault (segfault).
A page fault might result in a segfault, but it might also lead to a successful
page allocation. A segfault always indicates access violation.

The mapping from a virtual page to a physical page is another procedure that OS
implements, but the CPU is aware of it and even has a special Translation Lookaside
Buffer, to avoid going through it every time a page is accessed.

Each page has associated permissions, like read-only or read-write. CoW works
by marking a set of pages as read-only, and assigning a special page fault
handler that, when triggered, copies the page and makes it writable. This has
significant implications which are easier to see with an example.

```c
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
```
This is the output:
```
Initial values:
  global = 0 (0x55eb8940104c)
  stack = 0 (0x7ffcc4b14828)
  *heap = 0 (0x55eb8b05a2a0)
Child says:
  My pid: 3129
  global = 1 (0x55eb8940104c)
  stack = 1 (0x7ffcc4b14828)
  *heap = 1 (0x55eb8b05a2a0)
Parent says:
  My pid: 3128
  global = 0 (0x55eb8940104c)
  stack = 0 (0x7ffcc4b14828)
  *heap = 0 (0x55eb8b05a2a0)
Parent increments:
  global = 1 (0x55eb8940104c)
  stack = 1 (0x7ffcc4b14828)
  *heap = 1 (0x55eb8b05a2a0)
```
There are a few things to note here:
* CoW is in effect but we don't the address changes. This is expected, because
  these addresses are virtual. They might have changed for other reasons, but
  CoW copying happens for physical pages, and new copies are mapped to the same
  virtual addresses.
* Initially, the child uses the same memory as the parent. When it increments
  the values it gets its own copy of the pages that contain the values. Thus,
  its increments do not affect the parent's pages.
* After forking, the parent loses write access to its pages, which are marked
  read-only by the CoW mechanism. When the parent increments values, it either
  gets its own copies, or the pages already been unmarked as read-only and are
  now writable. In this case after the child writes to its pages, the parents
  pages are marked writable again, because parent doesn't share them with
  anyone anymore.
* The kernel keeps track of the original pages, somewhat reminiscent to a
  garbage collector.
* CoW doesn't care for semantics of the data. Our example shows different
  variable types (.bss, stack, and heap allocations). After the copy their
  semantics are preserved. This is entirely due to virtual memory, but still.
* CoW applies only to private memory like stack, heap, and global variables.
  Shared memory (shmget, mmap with `MAP_SHARE`) doesn't use CoW. File-backed
  memory maps might also opt in for CoW with `MAP_PRIVATE` flag.

![CoW1](/assets/cow1.png)
![CoW2](/assets/cow2.png)

Some time ago I read a blog post about Ruby's garbage collector. It had a flaw
in which it triggered CoW copying for the pages it was about to reclaim. It
happened because it was writing some accounting data to these pages. It was
fixed with a bit vector that is stored separately. I guess it goes to show
that with CoW you should be careful, lest you trigger the copying.

### Overcommit

In the example above I didn't use exec, but fork is often paired with it. In
any case, the kernel needs a policy of allowing fork to succeed when memory is
limited. Remember that available memory includes both RAM and swap. So
realistically, the kernel must be vigilant well before reaching the actual
limits.

Let's come back to my example with a big process that forks. Suppose there's
not enough memory for a second copy. What the little kernel gonna do? We
potentially ask for more than it has. Does it fail the fork? No.

The kernel's strategy will typically allow fork to succeed through overcommit.
This behavior is configurable via `/proc/sys/vm/overcommit_memory`, and even if
you've never modified it, the value might be different from default. You can
read more about it in this small [document][overcommit].

Overcommit strategy is orthogonal to another mechanism involving a page-out
daemon (kswapd). kswapd wakes up when memory runs low and swaps the less
frequently used pages to disk. If memory pressure continues, it shifts to
foreground mode and begins direct reclaim. In this mode, allocations block and
wait until the daemon is done freeing memory. It drops caches first, and if
that's insufficient, it wakes the [Out-Of-Memmory Killer][oom] (also known as
`oom_reaper`), which terminates the biggest process that is not critical to the
kernel.

Parallel to this, a page compaction mechanism reorganizes fragmented pages into
contiguous chunks, potentially freeing memory.

### How to take a look

By the way, you can manually drop caches with:
```
echo 3 > /proc/sys/vm/drop_caches
```

Important processes can be protected from the OOM Killer via
`/proc/<pid>/oom_*` files. Normally these contain 0 inside:
```
~> ll /proc/433/oom_*
-rw-r--r-- 1 root root 0 Jun  4 02:35 /proc/433/oom_adj
-r--r--r-- 1 root root 0 Jun  4 02:35 /proc/433/oom_score
-rw-r--r-- 1 root root 0 Jun  4 02:35 /proc/433/oom_score_adj
~> cat /proc/433/oom_*
0
0
0
```
But sometimes they may contain a negative or positive number. I think it's good
to know about these.

When the OOM Killer terminates a process and it's memory is reclamed, it logs a
message in `dmesg`:
```
[11686.244316] Out of memory: Killed process 1234 (unicorn) total-vm:2460020kB, anon-rss:1428900kB, file-rss:420kB, shmem-rss:0kB, UID:1000 pgtables:4732kB oom_score_adj:200
[11686.244416] oom_reaper: reaped process 1234 (unicorn), now anon-rss:0kB, file-rss:0kB, shmem-rss:0kB
```
* `total-vm` Total virtual memory used
* `anon-rss` Resident set size for stack/heap
* `file-rss` Memory-mapped files/libraries
* `shmem-rss` Shared memory usage
* `pgtables` Kernel accounting overhead
* `oom_score_adj` OOM priority adjustment (-1000 to 1000). This value is added
  to `oom_score`, which might be bigger than 1000.
* `oom_score` is not displayed here, but this is the actual value OOM Killer
  uses to determine the victim. The higher the value, the more likely the
  process will be terminated. It is managed by the kernel, and it's value is
  derived from process's memory usage and other heuristics, plus
  `oom_score_adj`.

You might want to grep like this:
```
dmesg | grep -i "out of memory\|killed process"
```

In `htop`, you can view processes sorted by OOM Score. To do it select
```
F2 -> Screen -> Available Columns -> OOM
```
then sort with `F6 -> OOM`.

![htop](/assets/htop.png)

[mmap]: https://man.openbsd.org/mmap
[overcommit]: https://www.kernel.org/doc/Documentation/vm/overcommit-accounting
[oom]: https://elixir.bootlin.com/linux/v6.15/source/mm/oom_kill.c
[ths]: https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html
[clone]: https://www.man7.org/linux/man-pages/man2/clone.2.html
