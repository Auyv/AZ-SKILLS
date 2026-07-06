---
name: az-spc-mpu-adapter
description: Use when adapting SPC Builder generated C code from an `*-spc_out/<app>/c` directory into a Hopechart MPU application, especially when matching an existing `common/view/spc_*` style, wiring SpanCall service lifecycle, bridging SPC handlers to xrpc business APIs, running service upkeep in an SFC task, creating a sibling `spc_<app>_clt_test` client test project, updating CMake source paths, and fixing build warnings from missing SDK headers or thread placement.
version: 1.0.0
inputs:
  required:
    - name: generated_spc_c_dir
      type: string
      description: Path to the generated SPC C output directory, such as `doc/mcud-spc_out/mcud/c`.
      validation: Must exist and contain generated `*_app.c`, `*_srv.c`, and related headers.
    - name: mpu_app_dir
      type: string
      description: Target MPU app directory containing `common/`, `build/`, and optional `business/xrpc`.
      validation: Must exist.
  optional:
    - name: reference_spc_dir
      type: string
      description: Existing adapted SPC service directory to mirror, such as `common/view/spc_hq_fota_app`.
      default: null
    - name: service_name
      type: string
      description: Target wrapper name, for example `spc_mcud`.
      default: derived from generated app name.
    - name: build_script
      type: string
      description: User-provided build entry to validate the change.
      default: project build script.
    - name: reference_client_test_dir
      type: string
      description: Existing SPC client test app to mirror, such as `app/spc_hq_fota_app_clt_test`.
      default: null
outputs:
  - name: adapted_spc_dir
    type: directory
    description: New target directory under `common/view/<service_name>`.
  - name: wrapper_files
    type: files
    description: Service lifecycle wrapper, normally `<service_name>_srv.c/.h`.
  - name: build_updates
    type: file edits
    description: CMake include/source additions and main-loop lifecycle changes.
  - name: client_test_project
    type: directory
    description: Optional sibling `spc_<app>_clt_test` project with generated client files, test commands, and build scripts.
errors:
  - code: ERR_GENERATED_DIR_MISSING
    message: Generated SPC C directory does not exist or lacks expected files.
    handling: Stop and ask for the correct `*-spc_out` path.
  - code: ERR_REFERENCE_UNREADABLE
    message: Reference SPC adapter cannot be read.
    handling: Continue with the standard wrapper pattern and note the fallback.
  - code: ERR_BUILD_ENTRY_BLOCKED
    message: User-provided build script cannot run because Docker or environment access is blocked.
    handling: Preserve raw output and ask user to run or authorize the same script.
  - code: ERR_MISSING_SDK_HEADER
    message: Build reports implicit declarations for SDK functions.
    handling: Add the specific SDK header, such as `SpanCall.h` or `sfc/sfc_task.h`.
  - code: ERR_XRPC_API_MISMATCH
    message: Generated SPC methods do not map cleanly to existing business wrapper APIs.
    handling: Inspect the closest local xrpc/process headers, implement only confirmed bridges, and leave explicit TODOs or ask for mapping when semantics are ambiguous.
  - code: ERR_CLIENT_TEST_REFERENCE_MISSING
    message: Requested client test project style cannot be read.
    handling: Create the standard minimal client test project with generated client/conn files, test framework, CMake, and build scripts.
---

# SPC MPU Adapter

## What This Skill Does

Adapt SPC Builder generated C code into an MPU application using the local `common/view/spc_*` style.

The usual target shape is:

```text
common/view/<service_name>/
  <service_name>_srv.c
  <service_name>_srv.h
  c/
    <app>_app.c
    <app>_app.h
    <app>_conn.c
    <app>_conn.h
    <app>_srv.c
    <app>_srv.h
```

Do not copy generated `main_client.c` or `main_server.c` into the real app build, because the MPU app already has its own `common/main.c`.

## When To Use

Use this skill when the user asks to:

- Adapt `doc/*-spc_out/<app>/c` into an MPU project.
- Follow another app's `common/view/spc_*` adaptation style.
- Expose generated SPC service methods from an MPU app.
- Bridge generated SPC server TODO handlers to existing xrpc process wrappers.
- Move `Spc*RunOnce()` to an SFC task thread.
- Create a sibling `spc_<app>_clt_test` project from generated SPC client code.
- Fix build warnings such as implicit `SCInit`, `SCFini`, or `TSfTask*` declarations.

## Workflow

1. Inspect the generated SPC C directory and the reference adapter.
2. Create `common/view/<service_name>/c`.
3. Copy only generated runtime files:
   - `*_app.c/.h`
   - `*_conn.c/.h`
   - `*_srv.c/.h`
4. Add wrapper `<service_name>_srv.c/.h`.
5. Wire lifecycle in app `main.c`:
   - Call wrapper init after lower transport dependencies are ready.
   - Call wrapper done before shared dependencies are destroyed.
6. Update CMake:
   - Add include dirs for `common/view/<service_name>` and `common/view/<service_name>/c`.
   - Add source dirs for the same paths.
7. If generated `*_srv.c` has TODO handlers, implement a thin business bridge:
   - Read SPC request params with `SCRespParamAsInt` / `SCRespParamAsStr`.
   - Fill the local business input struct.
   - Call existing `XrpcProcess*` APIs when available.
   - Fill response fields only when the call result is non-negative.
   - Return the `Int32` result through `SCRespRetByInt`.
8. If periodic service upkeep is required, run it in `TSfTask`, not in a busy main loop, when requested or when the project style requires it.
9. If a client test app is requested, create a sibling `spc_<app>_clt_test` project:
   - Copy generated `*_client.c/.h` and `*_conn.c/.h` into `common/spc_<app>/c`.
   - Reuse the reference `test_framework.c/.h` and build/debug scripts when available.
   - Add a `common/<project>_test.c` entry that calls `SCInit`, generated client init/open, the interactive command runner, generated close/done, and `SCFini`.
   - Add a command table that covers connection commands and every generated `Hc<App>_*` client API with typed input prompts.
   - Add a `build/common/CMakeLists.txt` target linked with the same SDK libraries used by the reference client test.
10. Validate with the user-provided build script first.

## Wrapper Pattern

Include these headers as needed:

```c
#include "SpanCall.h"
#include "sfc/sfc_api.h"
#include "sfc/sfc_task.h"
```

Use `SpanCall.h` for `SCInit()` and `SCFini()`. Use `sfc/sfc_task.h` for `TSfTaskCreate`, `TSfTaskStop`, and `TSfTaskWaitFor`.

Task pattern:

```c
static TSfTask g_spc_task = {0};
static Bool g_spc_task_created = False;

static void L_SpcTaskExecute(TOfObject *sender)
{
    TSfTask *self = (TSfTask *)sender;

    while (!TSfTaskTerminated(self))
    {
        SpcServiceRunOnce();
        SfSleep(100);
    }
}
```

Start the task in init after opening the SPC app. Stop and wait in done before closing the SPC app.

## Build Handling

When the user provides a script, run that exact script as the build entry. Do not replace it with manually decomposed Docker commands.

If Docker access fails, preserve the raw error and ask the user to authorize or run the same script. When the user returns compiler output, patch the exact issue and ask them to rerun.

## Common Fixes

- `implicit declaration of function 'SCInit'`: include `SpanCall.h`.
- `implicit declaration of function 'SCFini'`: include `SpanCall.h`.
- `unknown type name 'TSfTask'`: include `sfc/sfc_task.h`.
- Duplicate `main`: do not compile generated `main_client.c` / `main_server.c`.
- SPC handler has uninitialized output variables: bridge it to real business APIs or return a defined error.

## Client Test Pattern

Use this sibling layout:

```text
app/spc_<app>_clt_test/
  .vs/launch.vs.json
  build/build.sh
  build/debug.bat
  build/common/CMakeLists.txt
  common/<project>_test.c
  common/<app>_test_cmd.c
  common/test_framework.c
  common/test_framework.h
  common/spc_<app>/c/
    <app>_client.c
    <app>_client.h
    <app>_conn.c
    <app>_conn.h
```

The test app should be useful immediately from an interactive shell. Include command names that match the generated client methods, print the SPC return value, and print output fields only after the call returns success.

## Related Docs

- FAQ: [docs/faq.md](docs/faq.md)
- Manual workflow: [MANUAL.md](MANUAL.md)
