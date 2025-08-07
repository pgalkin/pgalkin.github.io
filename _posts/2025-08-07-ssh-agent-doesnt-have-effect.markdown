---
title: ssh-agent doesn't have effect on Windows
layout: post
date: 2025-08-07T18:33:22Z
tags: [troubleshooting]
uuid: 1d033dcb-bbec-4469-8a5c-6d76a048398f
---

#### OpenSSH for Windows

By default Windows 11 includes OpenSSH but it's usually an older version. It
comes as an additional feature and as such requires a special uninstallation
procedure by first disabling it in the list of additional features for Windows
and then rebooting. I did that. Then I installed the newer version with scoop.

I added my ssh key to the agent and made sure the agent is always running as a
background service. So if I list I see this:
```
$ ssh-add -l
256 SHA256:ZaaUwbBNf4WfTs8Zajbt+htrv+fPvaExJpIPpEUcw2M betelgeuse@DESKTOP-6JIFKPI (ED25519)
```

Yet, it doesn't "take". I'm always being promted for the passphrase. What
gives? `git` uses ssh that doesn't match the server, which is storing my ssh
key. To fix I simply needed to add this to my `.gitconfig`:
```
[core]
  ... skipped ...
  sshCommand = C:/Users/betelgeuse/scoop/apps/openssh/current/ssh.exe
```

If you're using the one that comes with Windows, then you'll probably need to
specify `C:/Windows/System32/OpenSSH/ssh.exe`, if you have the same issue.
Although in that case it's better to use the client that comes with Windows as
well, rather than installing with scoop.
