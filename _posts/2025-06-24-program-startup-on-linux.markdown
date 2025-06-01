---
title: Program Startup on Linux, exit
layout: post
date: 2025-06-24T15:31:47Z
tags: [c]
uuid: a934e134-2222-4b5b-85f6-bda9d021d087
---

I've read the 7th chapter of the Advanced Programming in the UNIX
Environment book:

![apue-ch7](/assets/apue_ch_7.png){: style="width: 80%; display: block; margin: 0 auto" }

There is also an excellent [course][apue-course] by Jan Schaumann, with youtube
videos, examples, and exercises. I think it's a good book and a good course, so
I decided to go over it and maybe record some videos for youtube talking about
it. Even though it's interesting, it can be dry at times, and going over
examples provided by somebody else can be less engaging than coming up with
them on your own. However, I find that running them on different platforms
makes it more inherently interesting. **UPD:** *At least initially, after a while I
realize that it's a lot more work than I expected and lose interest. But not
for long. I guess it's a marathon, rather than a sprint.*

Imagine you have a code snippet that does something simple, the result of which
is obvious if you run it. However, can you run the same snippet on OpenBSD?
Does it require modification? Is the result any different there? What about
glibc vs musl? x86 vs ARM? Linux vs Windows? The last one can be especially
challenging, because it's a completely different OS.

The book uses C for examples, but there are other systems programming
languages. Of note, at least to me, are Odin and Go. I think I can learn a lot
by adapting the examples for these languages, because it can highlight some key
design decisions. At present, there are no books like Advanced Programming in
the UNIX Environment (Rust edition). There are good books on Rust and Odin, but
none that cover the core platform API.

![apue-odin](/assets/small_apue_odin_cover.png){: style="display: block; margin: 0 auto" }

For this blog post I've organized my notes as a series of questions and
answers. I think a question can be good even if it is phrased in a way that
reveals fundamental misunderstanding of something. For example, for a long time
I used to think that environment variables are implemented as a hashmap. I
didn't care to look inside and this mental model isn't terribly wrong, but
still it's good to learn and see how they actually work.

#### Table of Contents
- [`main` parameters](#main-parameters)
- [Return type of `main`](#return-type-of-main)
- [Implicit return](#implicit-return)
- [exit vs return](#exit-vs-return)
- [exit status codes](#exit-status-codes)
- [Observe the exit status code](#observe-the-exit-status-code)
- [`exit` vs `_exit` vs `_Exit` vs `abort`](#exit-vs-_exit-vs-_exit-vs-abort)
- [`atexit` vs `on_exit`](#atexit-vs-on_exit)
- [More startup and exit callbacks](#more-startup-and-exit-callbacks)
- [What gets called before main?](#what-gets-called-before-main)
- [argv null termination](#argv-null-termination)
- [Does `main(void)` still receive the arguments?](#does-mainvoid-still-receive-the-arguments)
- [env null termination](#env-null-termination)
- [Is env a hashmap?](#is-env-a-hashmap)
- [How to manipulate env?](#how-to-manipulate-env)
- [setenv vs putenv](#setenv-vs-putenv)
- [Memory layout difference between platforms](#memory-layout-difference-between-platforms)
- [Size of binary sections on different platforms](#size-of-binary-sections-on-different-platforms)
- [Get extra information about a binary](#get-extra-information-about-a-binary)
- [Get program arguments outside of main](#get-program-arguments-outside-of-main)
- [C23 main](#c23-main)
- [`&argv` vs `&argv[0]`](#argv-vs-argv0)
- [Are `argc` and `argv` on the stack or in the registers?](#are-argc-and-argv-on-the-stack-or-in-the-registers)
- [How to read the `.bss` and `.data` sections?](#how-to-read-the-bss-and-data-sections)
- [How to define a custom entry point?](#how-to-define-a-custom-entry-point)
- [What happens if a program doesn't `exit`?](#what-happens-if-a-program-doesnt-exit)

#### main parameters

> In C we start our programs with main. It can be declared like:
  ```c
  int main(void)
  int main(int argc, char **argv)
  int main(int argc, char **argv, char **env)
  ```
  So does this mean that it's variadic? Is this function overloading?

No, it's declared as `main()` in libc. When you omit parameters,
the C compiler omits type checking for them. So this is legal:
```c
int add()
{
    return 0;
}

int main(void)
{
    int res = add(1, 3);
    return res;
}
```
The arguments are still passed to `add`, as can be seen from the disassembly:
```diff
 ┌ 44: int main (int argc, char **argv, char **envp);
 │ afv: vars(2:sp[0xc..0x10])
 │           push rbp
 │           mov rbp, rsp
 │           sub rsp, 0x10
 │           mov dword [var_4h], 0
+│           mov edi, 1
+│           mov esi, 3
 │           mov al, 0
 │           call sym.add
 │           mov dword [var_8h], eax
 │           mov eax, dword [var_8h]
 │           add rsp, 0x10
 │           pop rbp
 └           ret
```

---

#### Return type of main

> We can `int main`, `void main`, and even just `main`. Why do the last two
  even work?

Simply `main` without a type doesn't work for newer standards of C, but
compiles with `--std=c89`. The reason why is because in the past, any procedure
without a return type defaulted to `int`. This behavior is still present with
implicitly defined procedures from libc, when you forget to include the header.

`void main` works because we don't have to `return` from `main`. I'm not sure
how it passes the type check though, but somehow it does. It is not standard.

---

#### Implicit return

> Does C have implicit return? For example:
  ```c
  int main(void)
  {
      printf("implicit\n");
  }
  ```

Only for `main`, and it is 0, according to the standard. It used to be
undefined behavior and in practice returned the value of the last statement,
but not anymore.

---

#### exit vs return

> Within `main`, is `exit(0)` somehow different from `return 0`?

Not really, because [libc does][musl-calls-main] something to the effect of
```c
exit(return(argc, argv, envp))
```

By the way check out that source code browser, huh! Especially for big
codebases like Linux, it beat browing via github.

---

#### exit status codes

> Is there any standard for the meaning of exit codes? For example there is
  `EXIT_SUCCESS` and `EXIT_FAILURE`.

BSD standard defines some status codes, you can read about them with [man
sysexits.h][sysexits]. But really the most important thing is that 0 is success
and everything else is some kind of failure. This is true even for Windows.
Also, even though `exit` expects an `int` as an argument, only the last byte
counts:
```c
status & 0xFF
```
This happens within the system call itself, not the wrapper or POSIX exit.

Randomly I remember the author of Crafting Interpreters
[mentioning][crafting-interpreters-mention] that BSD's sysexit was the closest
thing to a standard he could find. So he exits with 64 status code when the
command line program prints the usage message.

---

#### Observe the exit status code

> How to observe the exit status code?

In case it's not automatically displayed by your shell prompt or GUI (think
debugger), you can call:
```sh
~> ./prog
~> echo $?
42
```
In case you use the fish shell, call `echo $status`. On Windows in
Powershell call `echo $LASTEXITCODE`. In cmd call `echo
%errorlevel%`.

By the way, did you know that Powershell is cross-platform? Imagine replacing
bash with it on Linux. What? Never say never! Maybe it's time for you to change
your shell?

---

#### `exit` vs `_exit` vs `_Exit` vs `abort`

> What's the difference between all these?

`exit` is defined by libc and performs libc-aware cleanup before terminating
the process. For example, it flushes stdio buffers and executes exit-hooks
registered with `atexit`. It then calls `_exit`, which is the actual system
call. Keep in mind that system calls have wrappers in libc, so technically
`_exit` procedure is also "defined by libc".

`_exit` and `_Exit` are the same, it's that they're coming from different
standardization efforts. `_exit` is defined by POSIX, while `_Exit` by the C
standard.

Here's a quick example to demonstrate the difference:
```c
void hook_1(void)
{
    printf("%s\n", __FUNCTION__); // __FUNCTION__ would be "hook_1"
}

void hook_2(void)
{
    printf("%s\n", __FUNCTION__);
}

int main(void)
{
    atexit(hook_1);
    atexit(hook_2);
    exit(0); // try _exit(0) to compare
}

~> ./a.out
hook_2
hook_1
```

`abort` is an abnormal way to exit a program. It can be triggered by calling
`abort()`, but there is also SIGABRT. It is the same signal mechanism
underneath. You can send it manually with `kill -6 <pid>`. List other signals
with `kill -l`. Aborting doesn't execute the termination hooks, but there are
signal handlers that can be registered, and they might execute if present.

---

#### `atexit` vs `on_exit`

> Any difference between `atexit` and `on_exit`?

`on_exit` registers a callback that takes an argument:
```c
void cleanup(int status, void *data)
{
    printf("Exit status %d, data: %s\n", status, (char*)data);
}

int main(void)
{
    on_exit(cleanup, "custom data");  // glibc-only
    return 0;
}
```
It is also available only on glibc (as far as I know). `atexit` is
standard.

---

#### More startup and exit callbacks

> If there is `atexit`, is there something like `init` in Go?

Yes, in fact it turns out a C program is quite an onion, with several layers of
callbacks. I've learned about it from [the article][startup-linux] that was
linked as additional reading on the course page. As I say, the course is really
great! One note about the article though, it's for x86 (32-bit) and glibc, so
it's pretty old. Here's an example with all callbacks:
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void preinit(int argc, char **argv, char **envp)
{
    printf("%s\n", __FUNCTION__);
}

void init(int argc, char **argv, char **envp)
{
    printf("%s\n", __FUNCTION__);
}

void fini(void)
{
    printf("%s\n", __FUNCTION__);
}

__attribute((section(".preinit_array"))) typeof(preinit) *__preinit = preinit;
__attribute((section(".init_array"))) typeof(init) *__init = init;
__attribute((section(".fini_array"))) typeof(fini) *__fini = fini;

void __attribute((constructor)) constructor(void)
{
    printf("%s\n", __FUNCTION__);
}

void __attribute((destructor)) destructor(void)
{
    printf("%s\n", __FUNCTION__);
}

void my_atexit(void)
{
    printf("%s\n", __FUNCTION__);
}

void my_atexit2(void)
{
    printf("%s\n", __FUNCTION__);
}

int main(void)
{
    atexit(my_atexit);
    atexit(my_atexit2);
    exit(0);
}
```
If linked with glibc it prints:
```
preinit
init
constructor
my_atexit2
my_atexit
destructor
fini
```
If linked with musl, the preinit hook is skipped, it doesn't execute. There
is no error, it's just skipped.

---

#### What gets called before main?

libc startup code, which is written partly in C, partly in assembly, and is
platform dependent. It also matters whether you compile into a dynamic or
static binary.

The way to find out is to run an executable in a debugger and print a
backtrace. For example with gdb on Alpine I get this:
```
gef➤  bt -past-main on
#0  0x00005555555552b4 in main ()
#1  0x00007ffff7f9a496 in libc_start_main_stage2 (main=0x5555555552b0 <main>, argc=0x1, argv=0x7fffffffe7c8)
    at src/env/__libc_start_main.c:95
#2  0x0000555555555076 in _start ()
```

Another way is to run `readelf -h <binary>` and look at the "Entry point
address" and put a breakpoint there like this: `b *0x401050`. However, this is
only for position-dependent binaries (`-no-pie` flag).

By convention `_start` is the entry point. In fact, if you want to write some
code in assembly you'd start your program like this:
```nasm
section .text
    global _start

_start:
    mov rax, 60
    xor rdi, rdi
    syscall        ; exit(0)
```
With musl it's easy enough to find the two source files that construct the
`_start` call for x86-64: [crt/crt1.c][musl-crt1] and
[arch/x86\_64/crt\_arch.h][musl-x86\_64-arch]. Then it executes the init
callbacks, calls main, and exits, executing the rest of the registered
callbacks.

---

#### argv null termination

> If we have argc, we don't need null-terminated argv. Why is it
  null-terminated?

argc is derived from argv. argv is self-contained and you can loop over every
argument like this:
```c
for (i = 0; argv[i] != 0; i++)
```

`argv[argc]` is always a null pointer. It's possible that `argv[0]` might not
necessarily be the program name. For example:
```c
// asdf.c
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char *argv[] = { "something-else", 0 };
    execve("./print_args", argv, 0);
    exit(0);
}

// print_args.c
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
}

~> ./asdf
argc = 1
argv[0] = something-else
```
So it says "something-else" is the program name, even though the binary's name
is `print_args`. It can also be omitted:
```diff
int main(void)
{
-   char *argv[] = { "something-else", 0 };
+   char *argv[] = { 0 };
    execve("./print_args", argv, 0);
    exit(0);
}

./asdf
argc = 1
argv[0] =
```
You could even do `execve("./print_args", 0, 0)`, although I suspect that's
undefined behavior.

---

#### Does `main(void)` still receive the arguments?

Yes, they're passed unconditionally. Just because there is no declaration in
the signature, doesn't mean they're not passed it. You can access them directly
via the registers, `$rbx` is argc and `$rbp` is argv. So you can print them
from `main(void)`, it's just that you'll need to use inline assembly and pass
them to the printing functions according to the [calling
convention][calling-convention] while making sure that the functions are linked
in. It's a bit too finicky for a quick example.


---

#### env null termination

> Is the third argument `char **env` also a null-terminated array of strings?
  What about `extern char **environ`?

Yes, it is. `environ` is the same as `env` initially, but it's managed by libc
and it is undefined behavior to increment the pointer itself. So this is
illegal and can crash your program on subsequent `setenv`:
```c
while (environ != 0) {
    printf("*environ = %s\n", *environ);
    environ++;
}
```
Here's [an example][memory-layout8.c] to demonstrate their behavior. The
important notes are:
* `env` is on the stack, a local variable, while `environ` is a global and can
  be on the heap or in the .bss section, depending on the platform.
* `env[i]` and `environ[i]` initially point at the same strings.
* As you call `setenv` and `unsetenv`, they (`env` and `environ`, and their
  contents respectively) can diverge. Initially, both `env` and `environ`
  have a certain size as arrays of strings. After `setenv`, `environ`
  changes its size, while `env` stays the same.
* The underlying strings are shared, they are never duplicated.
* Some strings can be located in the .data area, some on the heap.

---

#### Is env a hashmap?

No, it's iterated over every time you call `getenv`, so access is O(n). It
contains strings like `"LANG=C.UTF-8"`, they are not separated into key and
value. So there is also a search for `'='` to extract only the value part of the
string. This search is actually pretty clever as it utilizes [SIMD within a
register][swar], as can be seen in musl: [getenv.c][musl-getenv.c] and
[strchrnul.c][musl-strchrnul.c].

It all dependes on libc implementation of course. But in any case, env is not
meant to be big, so access is not efficient, it is not a hashmap.

---

#### How to manipulate env?

Here's several equivalent methods:
```
FOO=asdf BAR=qwerty ./prog
env FOO=asdf BAR=qwerty ./prog
rarun2 program=prog setenv=FOO=asdf setenv=BAR=qwerty
```
env is usually available by default, while rarun2 is a tool from
[radare2][radare2]. You can also pass modified env to the [exec][exec] family
of functions.

---

#### setenv vs putenv

The obvious signature difference:
```c
putenv("FOO=bar");
setenv("FOO", "bar", true); // true means overwrite
```
Note that putenv will not make a copy of `"FOO=bar"`, while setenv will copy
both `"FOO"` and `"bar"`. This means that if `"FOO=bar"` is on the stack and
the function returns, the pointer within `environ` (say `environ[33]`) will
become invalid.

---

#### Memory layout difference between platforms

A C program compiles to a binary. There are a few binary formats, for example
ELF on Unix-like systems, PE on Windows, and Mach-O on macOS. These files have
different sections. When you execute a program it loads into memory with
approximately the same sections, just filled in with data (addresses are
resolved, variables are allocated, etc.).

When you declare variables in C, some of them might be allocated in different
sections of memory. A stack, a heap, a .bss, a .data section. Here's a nice
[example][memory-layout.c] that demonstrates the variables and their addresses.
I've run it on multiple platforms, here's the results:
- [Debian with glibc `x86_64`][glibc-x86-64]
- [Debian with glibc ARM64][glibc-arm64]
- [Alpine with musl `x86_64`][musl-x86-64]

You can diff them, although all addresses are different. Nevertheless, it's
possible to see where each variable is located by how big or small the address
is.

The biggest difference is of course between x86-64 and ARM64, because the
programs are loaded at different addresses ( x86-64 at `0x564D1C12xxxx` vs
ARM64 at `0xAAAADC9Exxxx`). Another difference is where `environ` is located
before and after `setenv`.

---

#### Size of binary sections on different platforms

There's the size utility that prints the size of some sections of a binary.
Given [this][memory-layout.c] I run it on multiple platforms:
```
# x86-64 glibc
   text	   data	    bss	    dec	    hex	filename
   5858	    696	  40048	  46602	   b60a	memory-layout
# x86-64 musl
   text	   data	    bss	    dec	    hex	filename
   6287	    672	  40112	  47071	   b7df	memory-layout
# ARM64 glibc
   text	   data	    bss	    dec	    hex	filename
   6268	    736	  40040	  47044	   b7c4	memory-layout
```
Not that interesting, because it depends on compilation options and the
compiler itself. This is clang everywhere.

---

#### Get extra information about a binary

There are many tools to do it, I prefer [rabin2 from radare2][rabin2], but if
you don't have anything installed and have a binary that's been linked with
glibc, chances are that this might work:
```
~> env LD_SHOW_AUXV=1 ./memory-layout
AT_SYSINFO_EHDR:      0xffffa58aa000
AT_MINSIGSTKSZ:       4720
AT_HWCAP:             119fff
AT_PAGESZ:            4096
AT_CLKTCK:            100
AT_PHDR:              0xaaaacae00040
AT_PHENT:             56
AT_PHNUM:             9
AT_BASE:              0xffffa586d000
AT_FLAGS:             0x0
AT_ENTRY:             0xaaaacae00a00
AT_UID:               1000
AT_EUID:              1000
AT_GID:               1000
AT_EGID:              1000
AT_SECURE:            0
AT_RANDOM:            0xffffdbe0a928
AT_HWCAP2:            0x0
AT_EXECFN:            ./memory-layout
AT_PLATFORM:          aarch64
```

I think that might be very convenient for a sanity check when you're debugging
something.

---

#### Get program arguments outside of main

> Is there any way for a function that is called by `main` to examine the
  command-line arguments without (a) passing `argc` and `argv` as arguments
  from `main` to the function or (b) having `main` copy `argc` and `argv`
  into global variables?

On Linux it's possible to parse the file `/proc/self/cmdline`, it contains
null-terminated string arguments back-to-back:
```
> hexdump -C /proc/self/cmdline
00000000  68 65 78 64 75 6d 70 00  2d 43 00 2f 70 72 6f 63  |hexdump.-C./proc|
00000010  2f 73 65 6c 66 2f 63 6d  64 6c 69 6e 65 00        |/self/cmdline.|
0000001e
```
Curiously for hexdump alias hd we get duplication (on busybox):
```
> hd -C /proc/self/cmdline
00000000  68 64 00 2d 43 00 2f 70  72 6f 63 2f 73 65 6c 66  |hd.-C./proc/self|
00000000  68 64 00 2d 43 00 2f 70  72 6f 63 2f 73 65 6c 66  |hd.-C./proc/self|
00000010  2f 63 6d 64 6c 69 6e 65  00                       |/cmdline.|
00000010  2f 63 6d 64 6c 69 6e 65  00                       |/cmdline.|
0000001
```

In place of self can be pid of any running process.

---

#### C23 main

C23 introduced many features, relatively speaking, so main can look like this:
```c
int main(int argc, char *argv[argc+1])
```
This means that array `argv` contains `argc+1` elements, number of arguments
plus the null terminator. You can also mark the parameters as unused:
```c
int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[argc+1])
```
There is also this form:
```c
int main(int argc, char *argv[static 1])
```
Which means that `argv` cannot be `NULL`.

---

#### `&argv` vs `&argv[0]`

> Are these the same? If not, how are they different?

These are different because `&` evaluates to a new pointer. `&argv[0]` would
be same as just `argv` though, without the "address-of". So it doesn't
literally create a new pointer. These calls all evaluate to the same address:
```c
&argv[0]; // 0x7ffc69bb0530
&argv[0]; // 0x7ffc69bb0530
&argv[0]; // 0x7ffc69bb0530
```

---

#### Are `argc` and `argv` on the stack or in the registers?

On x86-64 Linux they are in the registers, according to the [System V AMD64
ABI][calling-convention], but might spill to the stack if you `&argv` for
example.

---

#### How to read the `.bss` and `.data` sections?

Here's how to do it with some tools:
```sh
readelf -S ./a.out         # to show the section numbers
readelf -x 19 ./a.out      # read .bss section (assuming it's 19)
objdump -D -j .bss ./a.out # disassemble .bss
r2 -A ./a.out
  iS
  px @ .data
```

---

#### How to define a custom entry point?

With the `-e` compiler flag:
```sh
clang -e custom_entry src.c
```
Although libc still expects you to define `main`. If you're doing something
like this, likely you want to avoid linking to libc, so you might actually do
something like this:
```c
// clang -nostdlib -fno-buildin src.c
__attribute((force_align_arg_pointer))
void _start(void)
{
   // ...
```
Or if you don't like `_start`:
```c
// clang -e custom_entry -nostdlib -fno-buildin src.c
__attribute((force_align_arg_pointer))
void custom_entry(void)
{
   // ...
```
But this will lead to some difficulties, like hallucinated libc function
calls, missing stack protector, etc.

---

#### What happens if a program doesn't `exit`?

It will crash because `rip` will just keep going, and stumble upon illegal
instruction.

---

I think that's it for today. The chapter is big, and there are a lot of details
in the example outputs. I mentioned Odin and Go, but there's no way I'm
translating these snippets anytime soon (the ones from the book and the
course). I guess, after you see the structure of a C program, it's not that
interesting to inspect the structure of an Odin program, because it's
approximately the same. Still, I think there might be a lot of value in a book
like `apue` but for Odin.

[sysexits]: https://www.man7.org/linux/man-pages/man3/sysexits.h.3head.html
[apue-course]: https://stevens.netmeister.org/631/
[startup-linux]: http://dbp-consulting.com/tutorials/debugging/linuxProgramStartup.html
[musl-crt1]: https://git.musl-libc.org/cgit/musl/tree/crt/crt1.c
[musl-x86\_64-arch]: https://git.musl-libc.org/cgit/musl/tree/arch/x86_64/crt_arch.h
[calling-convention]: https://en.wikipedia.org/wiki/X86_calling_conventions#System_V_AMD64_ABI
[memory-layout8.c]: https://stevens.netmeister.org/631/apue-code/06/memory-layout8.c
[swar]: https://en.wikipedia.org/wiki/SWAR
[musl-getenv.c]: https://git.musl-libc.org/cgit/musl/tree/src/env/getenv.c
[musl-strchrnul.c]: https://git.musl-libc.org/cgit/musl/tree/src/string/strchrnul.c
[radare2]: https://book.rada.re/tools/rarun2/intro.html
[exec]: https://www.man7.org/linux/man-pages/man3/exec.3.html
[memory-layout.c]: /assets/memory-layout.c
[glibc-x86-64]: /assets/memory_layout_glibc_x86_64.txt
[glibc-arm64]: /assets/memory_layout_glibc_arm64.txt
[musl-x86-64]: /assets/memory_layout_musl_x86_64.txt
[musl-calls-main]: https://elixir.bootlin.com/musl/v1.2.5/source/src/env/__libc_start_main.c#L95
[crafting-interpreters-mention]: https://craftinginterpreters.com/scanning.html#the-interpreter-framework
[rabin2]: https://book.rada.re/tools/rabin2/intro.html
