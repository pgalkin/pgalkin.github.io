---
title: main, WinMain, and mainCRTStartup
layout: post
date: 2025-06-25T06:31:47Z
tags: [c]
uuid: e934e134-3afe-4b5b-85f6-bda9d021d087
---

When I first saw that the way to write the `main` function on Windows was like
this, I couldn't stop laughing:
```c
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    return MessageBoxW(NULL, L"hello, world", L"caption", MB_OK | MB_ICONINFORMATION);
}
```

That was after about a week of me struggling to find my whereabouts with the
shell and C setup, coming from Linux. Now, I'm feeling comfortable on Windows,
but I still remember how comically bad everything felt and how seemingly
everything I was used to on Linux was somehow broken on Windows. You can make a
promising setup relatively quickly, like for example install terminal version
of neovim, and start feeling the inspiration to actually do something, only to
discover that it doesn't integrate well with Powershell or cmd, there is no job
control, and your favorite plugins don't work.

Attrition. Friction. That was some time ago, and it now mostly subsided. I've
gotten used to Windows. I have to be fair though, looking even further into the
past I remember how frustrating it was getting used to Linux, coming from
Windows. The concept of editing a configuration file instead of clicking
through a settings menu in a GUI was foreign to me. But those were the times,
huh!

Anyways, I think everyone would agree that Windows has somewhat lackadaisical
attitute towards C. There is this paradigm of Microsoft that can be described
as "C/C++". When you're new to Windows and want to learn C, you will be
confused. For example, in Visual Studio there is no way to create a C project.
You can only create a C++ project. It is unclear whether you need Win32 or not,
or whether you should use main or WinMain, or what's the difference between
Cygwin and MinGW, and why you might want to use them. This is all part of
Windows landscape for C programming and you have to get used to it.

I think Windows API is well documented. There are good books as well. So I'm
not going to teach anything in this blog post. This will be just my commentary
on some of the things that are related to C program startup on Windows. Not
exhaustive. How deeply are you interested in the minutia and how much patience
do you have for it? Personally, I'm interested but not that much. I just want
to take a look. I know where some rabbit holes lead and don't care to reach
the bottom.

### WinMain

First of all it is a bit unusual that this is even a thing. On Windows, if you
want to define a Win32 GUI application you have to start it with WinMain. Sure,
there are ways to do it differently but this is more than a convention. It is
built-in to linkers and C runtime initialization is different for Win32 GUI
application and a console application. On Linux for example, if we want to
define a GUI application we don't start it with XMain or WaylandMain. We start
with main and treat GUI as any other library.

Win32 applications are not attached to any console by default and thus there is
no stdin or stdout. On Linux even GUI apps might use these default file
descriptors for logging. If you want the same kind of transient log on Windows
you can use [OutputDebugStringA][output-debug-string]. This doesn't spawn a
console, but instead writes to a separate stream that is visible in the
debugger.

On Windows you are destined to suffer through the Unicode-ASCII dichotomy. It
has many shapes and forms, most visible is the `w` suffix, most frustrating is
UTF-16. Overall I agree with this statement from [The UTF-8 Everywhere
Manifesto][utf8everywhere]:

> I believe Microsoft made a wrong design choice in the text domain, because
  they did it earlier than others.


I think it's good to follow the advice from the manifesto, and `#define
UNICODE` which means you might as well use wWinMain or wmain. The `WINAPI`
specifies the calling convention. Often can be omitted, but sometimes it might
matter if you want to support 32-bit or older versions. This is a [good
page][calling-conventions-ms] because it contains all the relevant links.

Windows has custom primitive types. It's possible to not use them, and even
define your custom [miniwin.h header][miniwin]. It makes the code look much
better, although it's not always feasible to do so and if it is, it's better to
postpone until the set of Windows API functions is more or less settled.
[This][base-win-types] page might be very helpful.

Lets look at WinMain's signature:
```c
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpCmdLine, int nCmdShow)
```

I think it's not a bad idea to rename the parameters in your code, but I'm
going to use them as is. `hPrevInstance` has been legacy even in the 2000s, so
it's always 0 nowadays. `nCmdShow` is used to determine how your Window will
appear: maximized, minimized, or regular. This can be configured in the
executable properties menu:

![window-props](/assets/window_props.png){: style="display: block; margin: 0 auto;" }

`lpCmdLine` might be puzzling at first, because you're starting a GUI
application on Windows, but you can pass the arguments via the console:
```
./hello.exe my-arg1 my-arg2
```
Or by setting them in shortcut properties:

![shortcut-arg](/assets/shortcut-arg.png)

If you don't link to libc then the convention is to use WinMainCRTStartup as
the entry point. You can see the LLD linker logic [here][lld-entry].

### main

There are multiple signatures of main as I documented in my [previous][prev]
blog post.

Unlike main on Linux, main on Windows doesn't have an `env` parameter. You can
still access the environment variables with `GetEnvironmentStrings` and
`getenv`. An interesting thing to note here though, is that if you use wWinMain
then you'll have the wide version of the environment variable strings, which
you are supposed to access with `GetEnvironmentStringsW` or `_wgetenv`. However
if you call `getenv` for example, it will copy the environment block, and
then translate it to the narrow version. Then you'll have two environment
blocks and they will be kept in sync by triggering a call to `_putenv` when you
call `_wputenv` and vice versa, although they still can desync due to encoding
differences.

`extern char **environ` might be available, but using it directly is tricky. I
think it's better to stick to `GetEnvironmentVariableW` function.

If you don't link to libc then use `mainCRTStartup`.

### exit

You exit the program with ExitProcess or TerminateProcess. The difference is
that ExitProcess triggers additional detachment logic. If you use mutliple
threads and DLLs, then under certain circumstances ExitProcess might trigger a
deadlock, so in that sense TerminateProcess might be a more reliable way to
exit.

There is also `exit` from libc. You can register callbacks with `atexit` that
are triggered by `exit` call. These callbacks won't be triggered by
ExitProcess. They also won't be triggered by `_Exit` and `_exit`, which are yet
another way to exit from a program.

return at the end of main results in ExitProcess.

I was wondering if there is any difference in exit code on Windows and Linux.
They appear to be mostly the same, in part because there isn't any standard or
convention beyond 0 being success and everything else an error. 128-255 is
reserved though because it can overlap with internal NTSTATUS, which indicate
system level errors.

### Some external utilities

You can see the exit code like this:
* In busybox with `echo $?`
* In Powershell with `echo $LASTEXITCODE`
* In cmd with `echo %errorlevel%`

You can modify the environment with `env FOO=BAR ./your_prog` in busybox. Or
with rarun2 from radare2. For example:
```
rarun2 program=your_program setenv=FOO=BAR
rarun2 program=your_program setenv=FOO=@filename1
rarun2 program=your_program envfile=filename2
rarun2 program=your_program envfile=filename2 clearenv=true
```

[utf8everywhere]: https://utf8everywhere.org/
[calling-conventions-ms]: https://learn.microsoft.com/en-us/cpp/cpp/argument-passing-and-naming-conventions?view=msvc-170
[miniwin]: https://nullprogram.com/blog/2023/05/31/
[base-win-types]: https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types
[output-debug-string]: https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-outputdebugstringa
[lld-entry]: https://github.com/llvm/llvm-project/blob/029f8892a500594bd044507352503249fd641e6c/lld/COFF/SymbolTable.cpp#L1076-L1094
[prev]: /blog/2025/06/24/
