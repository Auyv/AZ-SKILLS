---
name: az-spc-interface-sync
description: |
  Trigger when the user asks to sync xrpc process definitions from process_define.yaml
  into an SPC workbook such as mcud.xlsx, especially the SPC-Interface worksheet.
  This skill preserves the SPC template layout and applies the project convention that
  every Function has a RETURN row with Int32 and ">=0 表示执行成功； <0 错误码".
version: 1.0.0
inputs:
  required:
    - name: workbook_path
      type: string
      description: Path to the SPC workbook, usually mcud.xlsx.
      validation: File exists and extension is .xlsx.
    - name: process_yaml_path
      type: string
      description: Path to process_define.yaml containing xrpc process definitions.
      validation: File exists and contains a top-level processes list.
  optional:
    - name: sheet_name
      type: string
      description: Target worksheet name.
      default: SPC-Interface
    - name: backup_dir
      type: string
      description: Directory for workbook backup before modification.
      default: ./spc_sync_tmp
outputs:
  - name: updated_workbook
    type: file
    description: The original workbook updated in place.
    path: "{workbook_path}"
  - name: backup_workbook
    type: file
    description: Backup copy created before modification.
    path: "{backup_dir}/<workbook>.before_spc_interface_sync.xlsx"
  - name: verification_summary
    type: text
    description: Function count, data row count, and RETURN rule validation result.
errors:
  - code: ERR_WORKBOOK_NOT_FOUND
    message: Workbook file does not exist.
    handling: Ask for the correct workbook_path and do not create a new workbook.
  - code: ERR_YAML_NOT_FOUND
    message: process_define.yaml does not exist.
    handling: Ask for the correct process_yaml_path.
  - code: ERR_TARGET_SHEET_MISSING
    message: SPC-Interface worksheet is missing.
    handling: Stop and ask whether to create the worksheet or use another sheet.
  - code: ERR_INVALID_TEMPLATE
    message: Target sheet does not have the expected seven-column SPC-Interface header.
    handling: Stop before editing and report the unexpected headers.
  - code: ERR_PARSE_PROCESS
    message: process_define.yaml cannot be parsed or contains no process definitions.
    handling: Report the parse issue and leave the workbook unchanged.
  - code: ERR_EXPORT_WORKBOOK
    message: Updated workbook cannot be exported or saved.
    handling: Keep the backup, report the original error, and do not claim success.
---

# SPC Interface Sync

## What It Does

This skill syncs xrpc process definitions from `process_define.yaml` into an SPC workbook's `SPC-Interface` worksheet.

It is designed for the FOTON-G11 `mcud.xlsx` workflow where the SPC workbook is fed into SpcBuilder. The skill keeps the SPC template format intact and rewrites only the interface data rows under the header.

## When To Use

Use this skill when the user asks any of the following:

- Sync functions from `process_define.yaml` to `mcud.xlsx`.
- Update the `SPC-Interface` sheet from xrpc process definitions.
- Regenerate SPC interface rows after adding or changing xrpc processes.
- Fix SPC workbook rows so every Function has a standard RETURN value.

Do not use this skill for unrelated Excel editing, general spreadsheet analysis, or generating C code directly from xrpc YAML.

## Required Mapping

The target worksheet must use these seven columns:

```text
*Function Name
Function Description
*Function Type
Parameter Name
Modifier
Parameter Type Name
Parameter Description
```

For every process in YAML:

- `process_name` -> `*Function Name`
- `brief` -> `Function Description`
- `*Function Type` -> always `Normal`
- First row for each Function is always the RETURN row:
  - `Parameter Name` -> blank
  - `Modifier` -> `RETURN`
  - `Parameter Type Name` -> `Int32`
  - `Parameter Description` -> `>=0 表示执行成功； <0 错误码`
- `input_params[]` rows:
  - `param_name` -> `Parameter Name`
  - `Modifier` -> `IN`
  - `param_type` -> `Parameter Type Name`
  - `param_brief` -> `Parameter Description`
- `output_params[]` rows:
  - `param_name` -> `Parameter Name`
  - `Modifier` -> `OUT`
  - `param_type` -> `Parameter Type Name`
  - `param_brief` -> `Parameter Description`

Type conversion:

- `String` with `param_len: N` becomes `String[N]`.
- `ByteArray` with `param_len: N` becomes `ByteArray[N]`.
- Other types are copied as-is.

## How To Do It

1. Read the workbook and inspect workbook sheets.
2. Confirm the target sheet exists and the first row matches the seven expected headers.
3. Render or inspect the existing `SPC-Interface` sheet to learn its formatting.
4. Read `process_define.yaml` as UTF-8. Do not trust terminal output if Chinese text looks garbled.
5. Parse all `processes` entries and preserve their order.
6. Back up the workbook before editing.
7. Clear old `SPC-Interface` data rows, keeping row 1 headers.
8. Rebuild rows with one standard RETURN row per Function, followed by all IN rows and OUT rows.
9. Copy existing template row styles and data validations into the new data area.
10. Save the workbook in place.
11. Verify:
    - Function count equals YAML process count.
    - Every Function row has `RETURN / Int32 / >=0 表示执行成功； <0 错误码`.
    - No formula errors are introduced.
    - A rendered preview is readable enough to spot obvious clipping.

## Implementation Notes

- Prefer the bundled spreadsheet runtime and `@oai/artifact-tool` for `.xlsx` edits.
- Do not use terminal-rendered YAML Chinese text as source truth; read the file with UTF-8 in script code.
- Keep edits scoped to `SPC-Interface` data rows unless the user explicitly asks for more.
- If the artifact tool emits temporary `.inspect.ndjson` files, clean them up after successful verification.
- Preserve the original workbook's format. Copy style from existing data rows instead of inventing new colors.
- If row heights clip long Chinese descriptions, enable wrapping on description columns and autofit data rows.

## File Structure

```text
az-spc-interface-sync/
|-- SKILL.md
|-- MANUAL.md
`-- evals/
    `-- evals.json
```
