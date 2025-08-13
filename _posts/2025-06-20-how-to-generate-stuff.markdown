---
title: How to generate stuff
layout: post
date: 2025-06-20T12:00:00Z
tags: [cmdline]
uuid: 6d3236dd-9f46-4de7-87e3-91919f6a65b6
---

Sometimes I need to generate something on the command line and I often don't
remember how. I search on the internet or ask AI about it and usually it helps.
Still want to write it down though.

#### Generate UUID

There are many versions UUID, here's two methods I prefer:
```
~> uuidgen
6d3236dd-9f46-4de7-87e3-91919f6a65b6
~> dbus-uuidgen
a5cf5ce38ed6ba670029c082685d347d
```
These require installing packages uuidgen and dbus respectively. On Windows
it is possible to do with Python:
```
python -c "import uuid; print(uuid.uuid4())"
```

#### A timestamp

Easy enough:
```
~> date +"%Y-%m-%dT%H:%M:%SZ"
2025-06-20T12:03:56Z
```
date is available in busybox and GNU coreutils.
