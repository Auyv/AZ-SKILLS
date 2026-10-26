"""Read-only lint for this skill's conservative Keil annotation subset.

This is not the Keil parser, a C preprocessor, or a business dependency check.
The numeric controls must immediately precede unconditional literal #defines.
"""
import argparse
from pathlib import Path
import re
import sys

START = "// <<< Use Configuration Wizard in Context Menu >>>"
END = "// <<< end of configuration section >>>"
NUMBER = r"(?:0[xX][0-9a-fA-F]+|0|[1-9][0-9]*)"


def number(text):
    value = text.strip()
    while value.startswith("(") and value.endswith(")"):
        value = value[1:-1].strip()
    if not re.fullmatch(NUMBER, value):
        raise ValueError("editable value must be a single decimal/hex literal")
    return int(value, 16 if value.lower().startswith("0x") else 10)


def check(path, encoding):
    raw = path.read_bytes()
    if raw.startswith((b"\xef\xbb\xbf", b"\xff\xfe", b"\xfe\xff")):
        raise ValueError("BOM detected; use the selected encoding without BOM")
    if encoding == "gbk" and any(byte > 127 for byte in raw):
        try:
            raw.decode("utf-8")
        except UnicodeDecodeError:
            pass
        else:
            raise ValueError("non-ASCII file looks like UTF-8; expected GBK for legacy Keil")
    content = raw.decode(encoding)
    lines = content.splitlines()
    if content.count(START) != 1 or content.count(END) != 1:
        raise ValueError("exactly one start marker and one explicit end marker required")
    start = lines.index(START)
    end = lines.index(END)
    if start >= 100 or end <= start:
        raise ValueError("start marker must be in first 100 lines and precede end marker")
    # Preserve line numbers while ignoring ordinary block comments.
    section = "\n".join(lines[start + 1:end])
    section = re.sub(r"/\*.*?\*/", lambda m: "\n" * m[0].count("\n"), section, flags=re.S)
    groups = []
    pending = []
    controls = []
    targets = set()
    for lineno, line in enumerate(section.splitlines(), start + 2):
        prefix = f"line {lineno}: "
        stripped = line.strip()
        if stripped.startswith("//"):
            annotation = stripped[2:].strip()
            if annotation.startswith("<"):
                tags = re.findall(r"<([^<>]+)>", annotation)
                remainder = re.sub(r"<[^<>]+>", "", annotation)
                if not tags or "<" in remainder or ">" in remainder:
                    raise ValueError(prefix + "malformed annotation or extra angle bracket")
                for tag in tags:
                    if tag == "h":
                        groups.append(lineno)
                    elif tag == "/h":
                        if not groups:
                            raise ValueError(prefix + "unmatched group end")
                        groups.pop()
                    elif tag == "i":
                        pass
                    elif re.fullmatch(r"q(?:\.[0-9]+)?|o", tag):
                        bit = int(tag.split(".")[1]) if "." in tag else None
                        if bit is not None and bit > 31:
                            raise ValueError(prefix + "bit index must be 0..31")
                        control = {"kind": tag[0], "bit": bit, "choices": [],
                                   "bounds": None, "line": lineno}
                        pending.append(control)
                        controls.append(control)
                    elif re.fullmatch(NUMBER + "=", tag):
                        if not pending or pending[-1]["kind"] != "o":
                            raise ValueError(prefix + "numeric choice requires preceding <o>")
                        pending[-1]["choices"].append(number(tag[:-1]))
                    elif re.fullmatch(NUMBER + "-" + NUMBER, tag):
                        if not pending or pending[-1]["kind"] != "o":
                            raise ValueError(prefix + "range requires preceding <o>")
                        low, high = (number(item) for item in tag.split("-"))
                        if low > high:
                            raise ValueError(prefix + "range endpoints reversed")
                        pending[-1]["bounds"] = (low, high)
                    else:
                        raise ValueError(prefix + f"unsupported legacy annotation <{tag}>")
        elif stripped and pending:
            definition = re.fullmatch(r"#\s*define\s+([A-Za-z_][A-Za-z_0-9]*)\s+(.+)",
                                      stripped.split("//", 1)[0].strip())
            if not definition:
                raise ValueError(prefix + "controls must directly precede a numeric #define")
            try:
                value = number(definition[2])
            except ValueError as error:
                raise ValueError(prefix + str(error)) from error
            for control in pending:
                origin = f"line {control['line']} -> {definition[1]}: "
                if control["kind"] == "q" and control["bit"] is None and value not in (0, 1):
                    raise ValueError(origin + "boolean must be 0 or 1")
                if control["bit"] is not None and value > 0xFFFFFFFF:
                    raise ValueError(origin + "bit mask exceeds 32 bits")
                if control["choices"] and value not in control["choices"]:
                    raise ValueError(origin + "current value is absent from dropdown choices")
                if control["bounds"] and not control["bounds"][0] <= value <= control["bounds"][1]:
                    raise ValueError(origin + "current value is outside range")
            targets.add(definition[1])
            pending.clear()
    if groups or pending or not controls:
        raise ValueError("unclosed group, unbound control, or no controls found")
    return len(controls), len(targets)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("header", type=Path)
    parser.add_argument("--encoding", choices=("gbk", "utf-8"), default="gbk")
    args = parser.parse_args()
    status = 0
    try:
        controls, targets = check(args.header, args.encoding)
        print(f"PASS: {controls} controls, {targets} macros, encoding={args.encoding}; GUI not tested")
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
