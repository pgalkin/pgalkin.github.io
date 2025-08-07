---
title: "Permission denied: Symlinks on Windows"
layout: post
date: 2025-08-06T11:00:00Z
tags: [troubleshooting]
uuid: 1d033dcb-bbec-4469-8a5c-6d76a048398f
---

#### I want to create a symlink

From busybox, ported to Windows, I run:
```
~/code/dotfiles $ ln -sf $(pwd)/git/.gitconfig ~/.gitconfig
ln: C:/Users/betelgeuse/.gitconfig: Permission denied
```

This is because Windows [doesn't allow][win-symlinks] creating symlinks
without a setting being toggled. Me using `ln` and busybox doesn't matter,
the underlying mechanism is the same even if I used powershell or cmd.

There are several ways to toggle the setting. Consult your local AI on how to
do it. I chose to explicity allow my user to create symlinks via this thing:

![allow-symlink-creation](/assets/symlinks_edit.png){ :style="display: block; margin: 0 auto" }

[win-symlinks]: https://learn.microsoft.com/en-us/previous-versions/windows/it-pro/windows-10/security/threat-protection/security-policy-settings/create-symbolic-links
