---
title: Native fork() on Windows
layout: post
date: 2025-06-23T19:42:01Z
tags: [c]
uuid: 3c8b7056-d7d8-4d34-9957-9006dc517735
---

In my post about [fork and stdio buffers](/blog/2025/06/21/) I said that I found
a way to make fork on Windows. In this post I'll show you an example of how it
might be done. Here is the [blog post where I learned it from][ref]. Sadly this
example has an semi-external dependency. Even though fork is native, it uses
the undocumented Windows API, and it turns out that not only is it
undocumented, but it's also missing the headers. These headers have been
reverese engineered some time ago, and you can get them from [phnt][phnt].
Anyways, here goes:
```c
#include <phnt_windows.h>
#include <phnt.h>
#include <stdio.h>
#include <stdbool.h>

int global = 0;

int wmain(void)
{
    int stack = 0;
    int *heap = calloc(1, sizeof(*heap)); // no free

    wprintf(L"Initial values:\n");
    wprintf(L"  global = %d; address = %p\n", global, &global);
    wprintf(L"  stack = %d; address = %p\n",  stack, &stack);
    wprintf(L"  *heap = %d; address = %p\n",  *heap, heap);

    RTL_USER_PROCESS_INFORMATION child_info;
    NTSTATUS status = RtlCloneUserProcess(
        RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES,
        0,
        0,
        0,
        &child_info
    );

    if (status == STATUS_PROCESS_CLONED) {
        FreeConsole();
        AttachConsole(ATTACH_PARENT_PROCESS); // for stdout

        global++;
        stack++;
        (*heap)++;

        wprintf(L"Child says:\n");
        wprintf(L"  My pid: %lu\n", GetCurrentProcessId());
        wprintf(L"  global = %d; address = %p\n", global, &global);
        wprintf(L"  stack = %d; address = %p\n",  stack, &stack);
        wprintf(L"  *heap = %d; address = %p\n",  *heap, heap);

        ExitProcess(0);
    } else {
        if (!NT_SUCCESS(status)) {
            wprintf(L"RtlCloneUserProcess error code 0x%x\n", status);
            return status;
        }

        WaitForSingleObject(child_info.ProcessHandle, INFINITE);

        wprintf(L"Parent says:\n");
        wprintf(L"  My pid: %lu\n", GetCurrentProcessId());
        wprintf(L"  global = %d; address = %p\n", global, &global);
        wprintf(L"  stack = %d; address = %p\n",  stack, &stack);
        wprintf(L"  *heap = %d; address = %p\n",  *heap, heap);

        wprintf(L"Increment...\n");
        global++;
        stack++;
        (*heap)++;

        wprintf(L"  global = %d; address = %p\n", global, &global);
        wprintf(L"  stack = %d; address = %p\n",  stack, &stack);
        wprintf(L"  *heap = %d; address = %p\n",  *heap, heap);
    }
    return 0;
}
```
You can compile and run it like this:
```
mkdir build
cl -std:c17 -nologo -W4 -wd4324 -wd4115 win_fork.c ntdll.lib -Fobuild/ -Febuild/win_fork -I"phnt"
.\build\win_fork
```
Note, that you should be in Developer Environment for VS 2022, either
Powershell or cmd will do fine. You can also compile with `clang-cl` if you
have it. Compilation on MinGW will fail. Pay attention to the `-I"phnt"`
option, this is the path to the headers from the phnt project. I cloned it
right next to the example source file.

This is the output:
```
Initial values:
  global = 0; address = 00007FF771BB1F10
  stack = 0; address = 0000000C0F3FF6B0
  *heap = 0; address = 0000014C67BFFAA0
Child says:
  My pid: 2332
  global = 1; address = 00007FF771BB1F10
  stack = 1; address = 0000000C0F3FF6B0
  *heap = 1; address = 0000014C67BFFAA0
Parent says:
  My pid: 2052
  global = 0; address = 00007FF771BB1F10
  stack = 0; address = 0000000C0F3FF6B0
  *heap = 0; address = 0000014C67BFFAA0
Increment...
  global = 1; address = 00007FF771BB1F10
  stack = 1; address = 0000000C0F3FF6B0
  *heap = 1; address = 0000014C67BFFAA0
```

As we can see insofar as private variables are concerned this is a real fork.
Virtual addresses stay the same, just like on Linux. This doesn't tell
anything about CoW, I'm not sure if it's implemented for this function.

The fork is not fully integrated with the rest of subsystems on Windows. It is
sufficient to print some text to the console, but a Win32 process will crash,
because the child doesn't inherit or initialize some necessary context. So you
can't really use it in any serious code as a fundamental building block. It's a
very niche function.

Just in case, I checked under busybox if redirection to a file causes output
duplication. It does not. That's not surprising, because who knows how Windows
port of busybox implements shell redirection. Since I started tinkering with
it, out of curiosity I made a quick modification to check if perhaps file
handles are inherited, and if they are, perhaps they are shared as well?
Here's the modified version:
```c
#define BUF_SIZE 64

int wmain(int argc, wchar_t **argv)
{

    unsigned long bytes_read, bytes_written;
    char buf[BUF_SIZE];

    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE in_handle = CreateFileW(argv[1], GENERIC_READ,
            0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (in_handle == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"CreateFileW error %d\n", GetLastError());
    }

    ReadFile(in_handle, buf, BUF_SIZE, &bytes_read, 0);
    WriteFile(stdout_handle, buf, bytes_read, &bytes_written, 0);

    RTL_USER_PROCESS_INFORMATION child_info;
    NTSTATUS status = RtlCloneUserProcess(
            RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES,
            0,
            0,
            0,
            &child_info
            );

    if (status == STATUS_PROCESS_CLONED) {
        FreeConsole();
        AttachConsole(ATTACH_PARENT_PROCESS); // for stdout

        bool ok = ReadFile(in_handle, buf, BUF_SIZE, &bytes_read, 0);
        if (!ok) {
            fwprintf(stderr, L"ReadFile error %d\n", GetLastError());
        }
        WriteFile(stdout_handle, buf, bytes_read, &bytes_written, 0);

        ExitProcess(0);
    } else {
        if (!NT_SUCCESS(status)) {
            wprintf(L"RtlCloneUserProcess error code 0x%x\n", status);
            return status;
        }

        WaitForSingleObject(child_info.ProcessHandle, INFINITE);

        ReadFile(in_handle, buf, BUF_SIZE, &bytes_read, 0);
        WriteFile(stdout_handle, buf, bytes_read, &bytes_written, 0);
    }
    return 0;
}
```
You give it as an argument the example program file name, it opens it, reads 64
bytes, writes to stdout, forks, tries to read from the inherited handle and
sadly, not succeeds. It prints the error (highlighted):
```diff
.\build\win_fork win_fork.c

 #include <phnt_windows.h>
 #include <phnt.h>
 #include <stdio.h>
+#ReadFile error 6
 include <stdbool.h>
 
 #define BUF_SIZE 64
 
 int wmain(int argc, wc
```
Well, despite the letdowns, I am glad to have looked at it.

[phnt]: https://github.com/winsiderss/phnt
[ref]: https://diversenok.github.io/2023/04/20/Process-Cloning.html
