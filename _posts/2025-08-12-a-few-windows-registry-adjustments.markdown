---
title: A Few Windows Registry Adjustments
layout: post
date: 2025-08-12T09:58:30Z
tags: [windows]
uuid: 4b999347-b71e-43de-a9fe-283261b1ea59
---

#### A Few Windows Registry Adjustments

Create a file `modify_registry.reg`:
``` 
Windows Registry Editor Version 5.00

; Interpret RTC time as UTC (dual-boot with Linux)
[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\TimeZoneInformation]
"RealTimeIsUniversal"=dword:00000001

; Disable 5x shift key / Sticky Keys popup
[HKEY_CURRENT_USER\Control Panel\Accessibility\StickyKeys]
"Flags"="510"

; Disable Telemetry
[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\DataCollection]
"AllowTelemetry"=dword:00000000
"MaxTelemetryAllowed"=dword:00000000

; Remap Caps Lock -> Ctrl
[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layout]
"Scancode Map"=hex:00,00,00,00,00,00,00,00,02,00,00,00,1d,00,3a,00,00,00,00,00
```
Then you import it to the Registry Edior. It being Windows, for the changes
to take effect you'll have to reboot. There's a few curious things about
these .reg files:
* They have to start with `Windows Registry Editor Version 5.00`, otherwise
  they won't be considered valid by the Registry Editor.
* When you cat it, you'll see ` ■Windows Registry Editor Version 5.00` at
  the top, even though you don't have the square character in the text
  files itself.
* It is considered a binary file by git, so you won't see any diffs after
  modifications.
