# MiniOS App Library

This folder is the default MiniPkg library.

MiniOS downloads package metadata from GitHub raw:

```text
https://raw.githubusercontent.com/dev-stanislav/OS/main/apps/
```

Examples inside MiniOS:

```text
minipkg list
minipkg info calc
minipkg install calc
run calc 12 * 7
```

## Add a package manifest

Create a folder:

```text
apps/myapp/
```

Add:

```text
apps/myapp/manifest.txt
```

Manifest format:

```text
id=myapp
name=My App
version=1.0
command=run myapp
description=Short app description.
```

Then add one line to `apps/index.txt`.

## Add a runnable app

MiniPkg v1 downloads manifests only. The actual app code still has to be compiled into the kernel.

To add a runnable app:

1. Add the app source under `kernel/src/apps/`.
2. Register its start/key/tick functions in `kernel/src/apps/app_manager.c`.
3. Add a `PACKAGE(...)` entry to `apps/catalog.def`.
4. Add `apps/myapp/manifest.txt`.
5. Add the app to `apps/index.txt`.
6. Build and push to GitHub.

After that MiniOS can install it with:

```text
minipkg install myapp
```

