---
title: A fork() leans on the implicit
layout: post
date: 2025-06-21T11:01:51Z
tags: [c]
uuid: 7bd8e322-4851-44c0-9df3-4ae9f7ae8f92
---
I came across an interesting paper called [A fork() in the road][fork-road] and
thought to refresh my knowledge of this system call and study it in more
detail. The paper is mostly an opinion piece, a critique of fork, written in a
style that might not be for everyone, although I find its arguments fair.

![Fork](/assets/small-fork-in-the-road.png){: style="display: block; margin: 0 auto" }

As I was reading the paper, the notion that fork was only simple on the surface
somehow struck me. If you think about it, a process is a fundamental concept
interacting with virtually every facet of the operating system. Creation of a
new process is ought to be complex, yet fork-exec take very few arguments. If
I were building a non-Unix operating system from scratch, I would've come up
with something along the lines of [CreateProcess][create-process] from Windows,
which looks like this:
```c
    bool ok = CreateProcess(0, // No module name (use command line)
            argv[1], // Command line
            0,       // Process handle not inheritable
            0,       // Thread handle not inheritable
            false,   // Set handle inheritance to false
            0,       // No creation flags
            0,       // Use parent's environment block
            0,       // Use parent's starting directory 
            &si,     // Pointer to STARTUPINFO structure
            &pi);    // Pointer to PROCESS_INFORMATION structure
    if (!ok) {
        // ...
    }
```
Because of course, if you want to create a new process, then just initialize
it and pass in everything it needs via an argument. What does a new process
need in order to start? Who knows, a bunch of stuff, but at the very least a
path to an executable and arguments. Add the rest of options as the system
evolves. This is a good start for an explicit interface.

Fork does it differently though. It says a new process will be mostly the same
as the current, and gives you a special transformation window before you call
exec. In that window you have way more power than any amount of procedure
parameters can provide, because you write arbitrary code. You inherit lots of
stuff and cancel out everything you don't need. There are restrictions though,
the code should be [async-signal-safe][signal-safety], but other than that you
are free to do whatever. And by the way, you don't even have to exec.

```c
    if ((pid = fork()) == 0) {
        // ...
        // arbitrary code
        // ...
        execvp("cmd", args);
```

This almost philosophical difference between the approaches made me want to
compare their merits and write a comprehensive overview, replete with examples
and gotchas, with use cases and war stories, historical lessons and convincing
benchmarks, all for different platforms too, and finish it all off with a
balanced and levelheaded summary.

And I actually started doing it, until I stumbled upon [commit
messages][musl-fork] from musl's implementation of fork. What a ghastly
rigmarole! I wanted to provide examples for *that*? Compare it with Windows? I
already know the verdit will be some kind of many handed fuzzy statement with
pointless meandering of "on the one hand fork encourages to overcommit, and on
the other..."

No, I don't want to do that. But I do want to show you one particularly
*wrinkled* example of the way fork interacts with printing stuff. If you look
at [its man page][fork] you'll notice there is a long list of nuance about what gets
inherited by the child process and what doesn't. So while it might be
interesting to look at *some* examples, looking at *all* is going to be rather
boring.

### Interleaved file descriptor offset

In particular I will demonstrate what this part means (partially):
> •  The child inherits copies of the parent's set of open file
     descriptors.  Each file descriptor in the child refers to the
     same open file description (see `open(2)`) as the corresponding
     file descriptor in the parent.  This means that the two file
     descriptors share open file status flags, file offset, and
     signal-driven I/O attributes (see the description of `F_SETOWN`
     and `F_SETSIG` in `fcntl(2)`).

Here's a function in C that reads a file and prints `n` lines prefixed by a
string, to distinguish the process:
```c
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
```
Don't mind the absence of error handling or repeated strlen call, it's just an
example. Here's the main program to drive it:
```c
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
```
The man page says that the two file descriptors share the offset, so output should
be interleaved, because the child continues where the parent left off, and then
the parent continues where the child left off. That is indeed the case:
```
~> ./a.out fd_interleaved.c

parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
 child: #include <sys/wait.h>
 child:
 child: void cat_file(int fd, int n, char *prefix)
 child: {
 child:     char buf[BUFSIZ] = {0};
parent:     size_t len = 0; // how many bytes in buf currently
parent:     int line = 0;
parent:     while (line < n && read(fd, buf + len, 1) > 0) {
parent:         len++;
parent:         if (buf[len-1] == '\n') {
```
If `wait(0)` wasn't there, then it would be up to the process scheduler to
decide when and how long the processes run. You might get something like this:
```
~> ./a.out fd_interleaved.c

parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
parent child: : ludeit.
#inc <sys/wah>
parent: voiatle(int fd, int n, char *prefix)
 child: d c_fi{
 child: buf[BUFSIZ] = {0};
 child:   size_t len = 0; // how many bytes in buf currently
parent:     char       int line = 0;
 child:     while (line < n && read(fd, buf + len, 1) > 0) {
parent:         len++;
parent:         if (buf[len-1] == '\n') {
```
Alright, that's fine, that's to be expected, don't access the same resource
from different processes without coordination. Where does the "wrinkle" come
in? It comes in if you use file streams from the standard library.

### Two buffers, two offsets

We're printing the file line-by-line, so let's replace our `cat_file`
implementation with the one that uses [fgets][fgets] to read the file
line-by-line. Like this:
```c
void cat_file(FILE *file, int n, char *prefix)
{
    char buf[BUFSIZ] = {0};
    for (int i = 0; i < n; i++) {
        if (fgets(buf, BUFSIZ, file) > 0) {
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
```
A `FILE` is a concept from the C standard library and on Unix-like operating
systems it is implemented on top of file descriptors. So intuitively the output
should be the same. With `fgets` we read an entire line now, instead of
collecting it by character (recall `read(fd, buf + len, 1)`), but the offset
has to be updated anyway. And the offset is shared, as we have seen. Yet the
output comes out like this:
```
~> ./a.out fd_interleaved.c

parent: #include <stdio.h>
parent: #include <stdlib.h>
parent: #include <unistd.h>
parent: #include <sys/wait.h>
parent:
 child: void cat_file(FILE *file, int n, char *prefix)
 child: {
 child:     char buf[BUFSIZ] = {0};
 child:     for (int i = 0; i < n; i++) {
 child:         if (fgets(buf, BUFSIZ, file) > 0) {
parent: void cat_file(FILE *file, int n, char *prefix)
parent: {
parent:     char buf[BUFSIZ] = {0};
parent:     for (int i = 0; i < n; i++) {
parent:         if (fgets(buf, BUFSIZ, file) > 0) {
```
Note that input file `fd_interleaved.c` is same as our first example. There's
some duplication going on. Even more baffling if we redirect:
```
~> ./a.out fd_interleaved.c > out; cat out

parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
 child: #include <sys/wait.h>
 child:
 child: void cat_file(int fd, int n, char *prefix)
 child: {
 child:     char buf[BUFSIZ] = {0};
parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
parent: #include <sys/wait.h>
parent:
parent: void cat_file(int fd, int n, char *prefix)
parent: {
parent:     char buf[BUFSIZ] = {0};
```
We get more lines! The reason why this happens is built-in stdio buffering.
First of all, file descriptor offset is indeed shared, but it doesn't play a
role here because the very first `fgets` reads the whole file into its internal
buffer (not our `buf`). This can be verified with strace:
```
~> strace ./a.out fd_interleaved.c
...
read(3, "#include <stdio.h>\n#include <uni"..., 4096) = 1164
...

~> wc -c fd_interleaved.c
1164 fd_interleaved.c
```
The entire file is 1164 bytes and it fits in stdio's 4096-byte buffer. Our
`buf` also happens to be 4096-bytes big (because of `BUFSIZ`), but this read is
not into it. The way our `buf` gets the data is by copying it from this buffer
until the newline.

So the parent reads the whole file, prints 5 lines, then forks. What does the
child process inherit? The offset --- yes, which is at the end of the file now
--- but also the buffer with all its contents and *position*. The position here
is significant, because even though it is inherited, it is not shared.

So the child continues reading its copy of the buffer from where the parent
stopped, but this reading is not reflected in the parent's buffer, because it
doesn't use the shared offset, it uses the position variable from libc. It
prints the lines as they come in, and exits.

The parent resumes where it stopped --- not where the child stopped. And it
essentially does the same as the child just did. Thus, the duplication.

In case you want to take a quick look at this buffer and position, here the
[link][musl-iofile]. It's from musl, but glibc is similar (got to be). Look for
`rpos` that's the pointer that moves. I verified it on both libraries, as well
as on OpenBSD and Cygwin. This is standard behavior.

### Output redirection and buffering

What about the redirection? The output looks different then. That happens
because of stream orientationa, also called [buffering mode][setbuf]. It
controls when the buffer gets flushed. Ordinarily, stdout attached to a
terminal is line buffered, so a flush happens on every `'\n'`. But when it's
redirected to a text file it becomes block buffered, also called fully
buffered. There, a flush happens only when the buffer is full or when some
function calls `fflush` or `fclose`. In our example, `exit(0)` causes a flush.

The whole procedure goes like this. The parent reads the entire file into
internal buffer in one big gulp, because of `gets`. Then it starts copying
lines from this buffer into `buf` and calls `printf`. But `printf` doesn't
flush, because the buffering mode is set to `_IOFBF` - fully buffered.

Then it forks, nothing's been printed yet. The child inherits whatever parent
has collected, and continues reading in the same manned. Doesn't print anything
until `exit(0)` causes a flush. Then it comes out:
```
parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
 child: #include <sys/wait.h>
 child:
 child: void cat_file(int fd, int n, char *prefix)
 child: {
 child:     char buf[BUFSIZ] = {0};
```
The parent haven't printed anything, it's all child.

The parent resumes, and reads its part. Then it reaches `return 0;`, which
leads to `exit(0)` call in whatever called the `main` function from libc (it
was `_start`). And so it goes:
```
parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
parent: #include <sys/wait.h>
parent:
parent: void cat_file(int fd, int n, char *prefix)
parent: {
parent:     char buf[BUFSIZ] = {0};
```

If we replace `exit(0)` with `_exit(0)`, the form that doesn't cause a flush we
don't even hear from the child:
```diff
         case 0:
             cat_file(file, 5, " child");
-            exit(0);
+           _exit(0);
```
```
~> ./a.out fd_interleaved.c > out; cat out

parent: #include <stdio.h>
parent: #include <unistd.h>
parent: #include <string.h>
parent: #include <fcntl.h>
parent: #include <stdlib.h>
parent: #include <sys/wait.h>
parent:
parent: void cat_file(int fd, int n, char *prefix)
parent: {
parent:     char buf[BUFSIZ] = {0};
```
This isn't the only subtlety you can face when dealing with buffered I/O. There
are rules of thumb to avoid these kind of issues, and sometimes, especially in
CTFs, buffering is simply disabled:
```
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin,  NULL, _IONBF, 0);
    // ...
```
I don't know these rules of thumb, so I'm not going to tell you. I would
imagine they involve being Very Careful when you share stuff and calling flush
every so often. By the way, this flush thing is quite a hammer, and sometimes
you have to swing it twice, when just one doesn't do the job. Don't believe me?
Read this [blog post][files-are-hard] about file system reliability, keeping in
mind that fsync is a flush. I found it funny and a bit depressing at the same
time.

### How does Windows handle it?

Windows good.

Windows doesn't have file descriptors but does have file handles, and buffered
streams use these handles. It also has fork. It's been mentioned in the paper,
and I managed to [unearth the hidden ways of invoking the damned
thing](/blog/2025/06/23/).

With `CreateProcess` it is more difficult to get into a situation like this. At
least, I can't immediately think of a similar example, and I also don't want to
boot into it. My Windows setup is good, there's almost no ads now, it's just,
you know... Anyways, that's it for today.

[fork-road]: https://www.microsoft.com/en-us/research/wp-content/uploads/2019/04/fork-hotos19.pdf
[signal-safety]: https://man7.org/linux/man-pages/man7/signal-safety.7.html
[musl-fork]: https://git.musl-libc.org/cgit/musl/log/src/process/fork.c?showmsg=1
[fork]: https://man7.org/linux/man-pages/man2/fork.2.html
[musl-iofile]: https://git.musl-libc.org/cgit/musl/tree/src/internal/stdio_impl.h#n23
[setbuf]: https://man7.org/linux/man-pages/man3/setbuf.3.html
[fgets]: https://man7.org/linux/man-pages/man3/fgets.3p.html
[files-are-hard]: https://danluu.com/file-consistency/
[create-process]: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
