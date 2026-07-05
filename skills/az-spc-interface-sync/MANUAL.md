# az-spc-interface-sync Manual

## Purpose

Use this manual to sync xrpc `process_define.yaml` definitions into an SPC workbook's `SPC-Interface` worksheet.

The target convention is:

```text
Function row: Function Name / Function Description / Normal / blank / RETURN / Int32 / >=0 表示执行成功； <0 错误码
Input rows:   blank / blank / blank / param_name / IN  / type / param_brief
Output rows:  blank / blank / blank / param_name / OUT / type / param_brief
```

## Inputs

Typical project paths:

```text
C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\app\mcud\doc\mcud.xlsx
C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\app\mcud\business\xrpc\process_define.yaml
```

## Manual Steps

1. Open `mcud.xlsx` and confirm the target sheet is named `SPC-Interface`.
2. Confirm row 1 has these headers:

```text
*Function Name
Function Description
*Function Type
Parameter Name
Modifier
Parameter Type Name
Parameter Description
```

3. Back up the workbook before editing.
4. Read `process_define.yaml` as UTF-8.
5. For each `processes` item, write one Function RETURN row:

```text
A: process_name
B: brief
C: Normal
D: blank
E: RETURN
F: Int32
G: >=0 表示执行成功； <0 错误码
```

6. Under that Function row, write `input_params` in YAML order:

```text
D: param_name
E: IN
F: param_type, with String + param_len converted to String[n]
G: param_brief
```

7. Then write `output_params` in YAML order:

```text
D: param_name
E: OUT
F: param_type, with String + param_len converted to String[n]
G: param_brief
```

8. Preserve existing SPC template formatting:
   - Copy formatting and dropdown validation from existing data rows.
   - Keep row 1 unchanged.
   - Wrap description columns if long Chinese text is clipped.

9. Verify the result:
   - Number of Function rows equals YAML process count.
   - Every Function row contains `RETURN`, `Int32`, and `>=0 表示执行成功； <0 错误码`.
   - Workbook saves successfully.
   - The final `SPC-Interface` preview is readable.

## Common Issues

### Chinese text looks garbled in terminal

Do not copy from the terminal output. Read `process_define.yaml` as UTF-8 in the script or editor.

### Missing RETURN rows

Every Function must have one RETURN row even if the YAML has no input or output parameters.

### Wrong return type

Do not use `Bool`. In this project convention, Function RETURN type is fixed to `Int32`.

### String length is lost

If YAML contains:

```yaml
param_type: String
param_len: 64
```

write `String[64]` to `Parameter Type Name`.

### Temporary inspect files appear

Some spreadsheet tooling may generate `*.inspect.ndjson`. Remove those after verification if they are not part of the requested output.
