---
title: udhcpc failed to get a DHCP lease
layout: post
date: 2025-08-05T10:00:00Z
tags: [troubleshooting]
uuid: 7a7d13c6-eaed-4755-84ea-4c74d2007db6
---

#### No internet

Picture this: you're dual booting Windows and Linux, each on their own SSD,
both with their bootloaders. You do something in Windows, then reboot, press
F2, enter the motherboard firmware interface, change the boot order, save and
restart, hear the fans speed up, wait for GRUB to timeout, boot into Alpine
Linux, as it's the only option, and then notice:
```
udhcpc: started, v1.37.0
udhcpc: broadcasting discover
udhcpc: broadcasting discover
udhcpc: broadcasting discover
udhcpc: broadcasting discover
udhcpc: broadcasting discover
udhcpc: failed to get a DHCP lease
udhcpc: no lease, forking to background
```
No internet. The router is on, the Wi-Fi is up, your phone is connected and can
browse anything. Yet, your PC cannot acquire the lease, even though it's
connected to the router with an ethernet cable.

#### How to fix

Shutdown your PC and turn off the power unit. Wait 3 seconds, then boot.

#### Why

I don't know and at present lack the capacity to investigate. All I can say is
that Windows does something to the hardware and it lingers even after you
choose to boot into Linux. Complete shutdown seems to fix it in my case.

#### More details

Windows 11 with latest updates as of 2025-08-05, x86-64. I was running a VPN
service in the background.

On the Linux side, the network tries to connect automatically on boot, but you
can trigger the procedure manually with
```
doas /etc/init.d/networking restart
```

You can also experiment with `udhcpc` command. It comes from Busybox.
```
/sbin/udhcpc --help
/sbin/udhcpc -R -p /var/run/udhcpc.eth0.pid -i eth0 -r 192.168.1.5
```

You can visit your router's web UI through 192.168.1.1, just be sure to disable
VPN and look up user and password on the side of the router if you haven't
changed them.

The router is a cheap Chinese model HG6143D. This didn't use to happen with the
other router I had.
