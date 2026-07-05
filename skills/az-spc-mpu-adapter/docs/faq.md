# FAQ

## Build script cannot access Docker

Symptom:

```text
open //./pipe/docker_engine: Access is denied
```

Action:

- Preserve the raw output.
- Retry only through the same user-provided build script if authorization is available.
- If authorization is blocked, ask the user to run the same script locally and paste the compiler output.

## `SCInit` or `SCFini` is implicitly declared

Cause: the wrapper uses SpanCall lifecycle APIs without the SpanCall declaration header.

Fix:

```c
#include "SpanCall.h"
```

## `TSfTask` or task APIs are unknown

Cause: the wrapper uses SFC task APIs without the task header.

Fix:

```c
#include "sfc/sfc_task.h"
```

## Duplicate `main` while building

Cause: generated SPC examples `main_client.c` or `main_server.c` were copied into the app build.

Fix:

- Keep generated `main_client.c` and `main_server.c` in documentation or generated output only.
- Do not copy them into `common/view/<service>/c`.

## Generated SPC handler returns uninitialized data

Cause: generated `*_srv.c` contains TODO blocks with local output variables declared but never assigned.

Fix:

- Bridge the handler to existing business APIs.
- Initialize input and output structs.
- Only write response fields after a successful or non-negative business call.

## The client test app has a duplicate or missing entry point

Cause: generated example `main_client.c` was copied, or no project-specific test entry was created.

Fix:

- Do not compile generated `main_client.c`.
- Create `common/<project>_test.c` as the only entry point.
- Let that entry call `SCInit`, generated client init/open, the test command runner, generated close/done, and `SCFini`.

## A generated client API is hard to test interactively

Cause: the method has several typed fields or output values and the generated sample does not provide a reusable command shell.

Fix:

- Add a small command handler for the method.
- Prompt for each input with a typed helper such as `read_uint` or `read_string`.
- Print the raw return value and print output fields only when the return value is success or non-negative.
