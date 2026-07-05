# az-spc-mpu-adapter Manual

## Purpose

Use this guide to manually adapt SPC Builder generated C files into an MPU app.

## Steps

1. Locate the generated C directory.

Example:

```text
doc/mcud-spc_out/mcud/c
```

2. Locate the target MPU app.

Example:

```text
app/mcud
```

3. Create the target view directory.

```text
common/view/spc_<app>/c
```

4. Copy only the generated runtime files.

Copy:

```text
<app>_app.c
<app>_app.h
<app>_conn.c
<app>_conn.h
<app>_srv.c
<app>_srv.h
```

Do not copy:

```text
main_client.c
main_server.c
```

5. Add wrapper files.

Create:

```text
common/view/spc_<app>/spc_<app>_srv.c
common/view/spc_<app>/spc_<app>_srv.h
```

The wrapper should:

- Call `SCInit()`.
- Initialize and open the generated app object.
- Start a `TSfTask` if `RunOnce` needs its own thread.
- Stop the task in done.
- Close and finalize the generated app.
- Call `SCFini()`.

6. Update CMake.

Add include directories:

```cmake
include_directories(${SRC_PATH}/common/view/spc_<app>)
include_directories(${SRC_PATH}/common/view/spc_<app>/c)
```

Add source directories:

```cmake
aux_source_directory(${SRC_PATH}/common/view/spc_<app> SRC_FILES)
aux_source_directory(${SRC_PATH}/common/view/spc_<app>/c SRC_FILES)
```

7. Update `common/main.c`.

Include wrapper header:

```c
#include "spc_<app>_srv.h"
```

Initialize after transport or xrpc dependencies are ready:

```c
SpcAppSrvInit();
```

Shutdown before shared dependencies are destroyed:

```c
SpcAppSrvDone();
```

8. Bridge generated handlers.

Generated `*_srv.c` often contains `TODO` blocks and uninitialized output fields. Replace each handler with a real business call, commonly an `XrpcProcess*` wrapper.

9. Build with the project script.

When a script is provided, use it as-is.

Example:

```bat
C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\az_build_linux.bat
```

10. Fix compiler warnings iteratively.

Common fixes:

- Add `#include "SpanCall.h"` for `SCInit()` and `SCFini()`.
- Add `#include "sfc/sfc_task.h"` for `TSfTask*`.
- Remove any generated main files from the build.

## Optional Client Test Project

Create the test project when the user asks for a sibling `spc_<app>_clt_test` app or needs a quick interactive SPC client.

1. Create the sibling directory beside the target app.

Example:

```text
app/spc_mcud_clt_test
```

2. Copy generated client runtime files only.

Copy from `doc/*-spc_out/<app>/c`:

```text
<app>_client.c
<app>_client.h
<app>_conn.c
<app>_conn.h
```

Place them under:

```text
common/spc_<app>/c
```

3. Reuse the reference client test app style.

Copy or mirror:

```text
.vs/launch.vs.json
build/build.sh
build/debug.bat
common/test_framework.c
common/test_framework.h
```

4. Add the test entry file.

The entry should:

- Call `SCInit()`.
- Call generated client `Init` and `Open`.
- Run one named command from argv or launch the interactive menu.
- Call generated client `Close` and `Done`.
- Call `SCFini()`.

5. Add command handlers.

Create `common/<app>_test_cmd.c` with a command table that covers:

- Connection and smoke-test commands.
- Every generated `Hc<App>_*` client method.
- Typed prompts for integer and string parameters.
- Return-value printing and output-field printing after successful calls.

6. Add `build/common/CMakeLists.txt`.

The target should include:

```cmake
common
common/spc_<app>/c
```

and link the same SDK libraries used by the reference client test, commonly:

```cmake
alog srt logclient smqueue spancall ffc rfc sfc mfc ofc pthread
```

7. Validate through the user-provided build script or the new test project's own `build/build.sh` when that is the requested entry.
