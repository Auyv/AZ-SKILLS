# -*- coding: utf-8 -*-
"""
构建脚本扫描引擎 - 递归追踪 Shell 构建脚本中的文件替换操作。

功能:
1. 解析 export 变量定义，建立变量引用链
2. 递归追踪 sh/source/. 调用的子脚本
3. 识别 cp/ln/mv/rm/tar/echo/sed/mkdir/touch 等文件操作
4. 过滤出目标路径引用了 target_variable 的操作
"""

import os
import re
import json
from collections import OrderedDict
from typing import Dict, List, Optional, Set, Tuple


class ScanContext:
    """扫描上下文 - 保存变量表和扫描状态"""

    def __init__(self, entry_script: str, target_variable: str,
                 output_path: Optional[str] = None, max_depth: int = 20):
        self.entry_script = os.path.abspath(entry_script)
        self.target_variable = target_variable
        self.max_depth = max_depth
        self.project_root = os.path.dirname(self.entry_script)
        # 找到项目根（包含 build/ 和 config/ 的最近父目录）
        probe = self.project_root
        for _ in range(5):
            up = os.path.dirname(probe)
            if up == probe:
                break
            if os.path.isdir(os.path.join(up, 'build')):
                self.project_root = up
            probe = up

        self.output_path = output_path or os.path.join(
            os.path.dirname(self.entry_script),
            f"{target_variable}_替换清单.xlsx"
        )
        self.output_path = os.path.abspath(self.output_path)

        # 变量表: var_name → raw_expression
        self.variables: Dict[str, str] = {}
        # 变量子变量映射: var_name → [child_var_names]
        self.var_children: Dict[str, List[str]] = OrderedDict()
        # 已扫描脚本
        self.scanned_scripts: Set[str] = set()
        # 扫描结果
        self.entries: List[dict] = []
        # 脚本深度
        self.script_depth: Dict[str, int] = {}
        # 统计
        self.scripts_count = 0


def validate_inputs(entry_script: str, target_variable: str,
                    output_path: Optional[str] = None,
                    max_depth: int = 20) -> Tuple[ScanContext, Optional[dict]]:
    """验证输入参数，返回 (ScanContext, error_dict)"""
    errors = []

    if not entry_script or not isinstance(entry_script, str):
        errors.append({
            "code": "ERR_ENTRY_NOT_FOUND",
            "message": "entry_script 为空或类型错误"
        })

    if entry_script and not os.path.isfile(entry_script):
        errors.append({
            "code": "ERR_ENTRY_NOT_FOUND",
            "message": f"入口脚本不存在: {entry_script}"
        })

    if entry_script and os.path.isfile(entry_script) and not entry_script.endswith('.sh'):
        errors.append({
            "code": "ERR_ENTRY_NOT_FOUND",
            "message": f"入口脚本不是 .sh 文件: {entry_script}"
        })

    if not target_variable or not isinstance(target_variable, str) or target_variable.strip() == '':
        errors.append({
            "code": "ERR_INVALID_VARIABLE",
            "message": "target_variable 为空或不是字符串"
        })

    if max_depth < 0 or max_depth > 100:
        errors.append({
            "code": "ERR_INVALID_VARIABLE",
            "message": f"max_depth 无效: {max_depth}（应 0-100）"
        })

    if errors:
        return None, {
            "status": "error",
            "errors": errors
        }

    ctx = ScanContext(entry_script, target_variable, output_path, max_depth)
    return ctx, None


# ==================== 变量解析 ====================

VAR_DEF_PATTERN = re.compile(
    r'^\s*(?:export\s+)?(\w+)\s*=\s*(.+?)(?:\s*#.*)?\s*$'
)
VAR_REF_PATTERN = re.compile(r'\$\{(\w+)\}|\$(\w+)')
SCRIPT_CALL_PATTERN = re.compile(
    r'(?:^|\s|[;&|])\s*(?:sh|bash|source|\.)\s+(\S+)'
)


def resolve_variable_refs(expr: str, variables: Dict[str, str],
                          target_var: str = None) -> str:
    """展开变量引用（一级展开），保留 ${VAR} 的原始形式"""
    def _replacer(match):
        var_name = match.group(1) or match.group(2)
        if var_name in variables:
            return variables[var_name]
        return match.group(0)
    return VAR_REF_PATTERN.sub(_replacer, expr)


def extract_variable_defs(script_path: str, ctx: ScanContext) -> int:
    """
    从脚本中提取 export 变量定义，更新 ctx.variables。
    返回提取到的变量数量。
    """
    if not os.path.isfile(script_path):
        return 0

    count = 0
    try:
        with open(script_path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.rstrip('\n\r')
                match = VAR_DEF_PATTERN.match(line)
                if not match:
                    continue
                var_name, raw_value = match.group(1), match.group(2).strip()
                # 跳过函数名、保留字
                if var_name in ('function', 'if', 'for', 'while', 'case',
                                'then', 'else', 'elif', 'fi', 'do', 'done',
                                'in', 'esac', 'local'):
                    continue
                # 跳过纯数字值赋值
                if var_name in ctx.variables and not raw_value.startswith('$'):
                    continue  # 不覆盖已有定义（通常环境变量在更上层定义）
                ctx.variables[var_name] = raw_value
                count += 1
    except Exception:
        pass
    return count


def build_var_children(ctx: ScanContext):
    """构建变量子变量映射: 找出哪些变量引用了 parent var"""
    target = ctx.target_variable
    ctx.var_children[target] = []
    for var_name, raw_val in ctx.variables.items():
        if var_name == target:
            continue
        # 检查值是否引用了 target_variable
        if target in raw_val:
            ctx.var_children.setdefault(target, []).append(var_name)


def get_all_target_vars(ctx: ScanContext) -> Set[str]:
    """
    获取所有涉及 target_variable 的变量名集合（传递闭包）。
    例: G_SDK_PATH → G_SDK_ROOTFS_PATH → G_SDK_ROOTFS_PATH/bin
    """
    result = {ctx.target_variable}
    changed = True

    # 迭代扩展，直到不再变化
    while changed:
        changed = False
        for var_name, raw_val in ctx.variables.items():
            if var_name in result:
                continue
            # 检查 raw_val 中是否引用了 result 中的任一变量
            found = VAR_REF_PATTERN.findall(raw_val)
            for g1, g2 in found:
                ref_var = g1 or g2
                if ref_var in result:
                    result.add(var_name)
                    changed = True
                    break

    return result


# ==================== 操作检测 ====================

# 文件操作模式: (操作类型, 正则)
OPERATION_PATTERNS = [
    # ${FAKEROOT_TOOL} cp -rf SRC DST
    (r'\$\{\w+\}\s+cp\s+-rf?\s+(\S+)\s+(\S+)', 'cp -rf (fakeroot)'),
    # cp -rf SRC DST
    (r'\bcp\s+-rf?\s+(\S+)\s+(\S+)', 'cp -rf'),
    # cp SRC DST (不带 -r)
    (r'(?<!\S)cp\s+(\S+)\s+(\S+)', 'cp'),
    # ln -sf TARGET LINK_NAME
    (r'\bln\s+-s[fv]*\s+(\S+)\s+(\S+)', 'ln -sf'),
    # mv SRC DST
    (r'\bmv\s+(\S+)\s+(\S+)', 'mv'),
    # rm -rf PATH
    (r'\brm\s+-rf?\s+(\S+)', 'rm -rf'),
    # tar -zxvf SRC -C DST
    (r'\btar\s+-[a-z]*z[a-z]*x[a-z]*v?f?\s+(\S+).*?-C\s+(\S+)', 'tar -zxvf'),
    # echo TEXT >> FILE
    (r'\becho\s+.*?>>\s+(\S+)', 'echo >>'),
    # sed -i EXPR FILE
    (r'\bsed\s+-i\s+.*?(\S+)\s*$', 'sed -i'),
    # mkdir PATH
    (r'\bmkdir\s+(?:-p\s+)?(\S+)', 'mkdir'),
    # touch FILE
    (r'\btouch\s+(\S+)', 'touch'),
]


def path_contains_target_var(path: str, target_vars: Set[str]) -> bool:
    """检查 path 表达式是否包含任意目标变量"""
    found = VAR_REF_PATTERN.findall(path)
    for g1, g2 in found:
        var_name = g1 or g2
        if var_name in target_vars:
            return True
    return False


def _resolve_var_chain(var_name: str, ctx: ScanContext) -> str:
    """
    递归解析变量链，直到找到包含 target_variable 的值。
    例: G_SDK_VENDOR_HQ_LIB_PATH → ... → ${G_SDK_PATH}/oemapp/app/hq/lib
    返回解析后的值，或原变量名如果无法解析。
    """
    seen = set()
    current = var_name
    while current in ctx.variables and current not in seen:
        seen.add(current)
        raw = ctx.variables[current]
        if ctx.target_variable in raw:
            return raw
        # 尝试下一级
        found = VAR_REF_PATTERN.findall(raw)
        for g1, g2 in found:
            next_var = g1 or g2
            if next_var in ctx.variables and next_var not in seen:
                current = next_var
                break
        else:
            break  # 无更多引用
    return ctx.variables.get(var_name, '')


def extract_subdir(target_path: str, ctx: ScanContext) -> str:
    """
    计算目标路径在 target_variable 下的一级子目录。
    递归解析变量链。
    """
    target = ctx.target_variable

    # 1. 直接查找: target_path 中是否包含 ${target_variable}
    direct_idx = target_path.find('${' + target + '}')
    if direct_idx >= 0:
        prefix = target_path[direct_idx + len('${' + target + '}'):]
        if prefix.startswith('}/'):
            prefix = prefix[2:]
        elif prefix.startswith('}'):
            prefix = prefix[1:]
        parts = prefix.strip('/').split('/')
        if parts and parts[0]:
            return parts[0] + '/'
        return '(根目录)'

    # 2. 通过变量链: 解析 target_path 中的每个变量引用
    found_vars = VAR_REF_PATTERN.findall(target_path)
    for g1, g2 in found_vars:
        var_name = g1 or g2
        if var_name == target:
            continue
        resolved = _resolve_var_chain(var_name, ctx)
        if resolved:
            # 找到 target_variable 在 resolved 中的位置
            idx = resolved.find('${' + target + '}')
            if idx >= 0:
                after_target = resolved[idx + len('${' + target + '}'):]
                if after_target.startswith('}/'):
                    after_target = after_target[2:]
                elif after_target.startswith('}'):
                    after_target = after_target[1:]
                parts = after_target.strip('/').split('/')
                if parts and parts[0]:
                    return parts[0] + '/'

    return '(根目录)'


def scan_script_operations(script_path: str, ctx: ScanContext) -> int:
    """
    扫描单个脚本中的文件替换操作。
    返回发现的操作数。
    """
    if not os.path.isfile(script_path):
        return 0

    target_vars = get_all_target_vars(ctx)
    count = 0

    try:
        with open(script_path, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
    except Exception:
        return 0

    for line_no, line in enumerate(lines, 1):
        line_stripped = line.strip()
        if not line_stripped or line_stripped.startswith('#'):
            continue

        for pattern, op_type in OPERATION_PATTERNS:
            match = re.search(pattern, line_stripped)
            if not match:
                continue

            groups = match.groups()
            src = groups[0] if len(groups) >= 1 else ''
            dst = groups[1] if len(groups) >= 2 else groups[0]

            # 检查目标路径是否引用了 target_variable
            dst_has_target = path_contains_target_var(dst, target_vars)
            src_has_target = path_contains_target_var(src, target_vars)

            # 只保留 WRITE TO / DELETE FROM G_SDK_PATH 的操作
            # (排除从 G_SDK_PATH 读取后写到别处的操作)
            if op_type in ('rm -rf', 'sed -i', 'mkdir', 'touch'):
                # 删除/修改类: 操作对象必须在 G_SDK_PATH 内
                if not dst_has_target and not src_has_target:
                    continue
                if dst_has_target:
                    entry_src, entry_dst = src, dst
                else:
                    entry_src, entry_dst = dst, src
            elif op_type in ('tar -zxvf', 'echo >>'):
                # tar -C DST / echo >> DST: DST 必须在 G_SDK_PATH 内
                if not dst_has_target:
                    continue
                entry_src, entry_dst = src, dst
            else:
                # cp/cp -rf/mv/ln -sf: 目标必须在 G_SDK_PATH 内
                if not dst_has_target:
                    continue
                entry_src, entry_dst = src, dst

            # 计算子目录
            subdir = extract_subdir(entry_dst, ctx)

            # 相对路径
            try:
                rel_script = os.path.relpath(script_path, ctx.project_root)
            except ValueError:
                rel_script = script_path

            ctx.entries.append({
                "script": rel_script.replace('\\', '/'),
                "line": str(line_no),
                "op": op_type,
                "src": entry_src,
                "dst": entry_dst,
                "dst_for_subdir": entry_dst,
                "subdir": subdir,
                "desc": "",
            })
            count += 1

    return count


# ==================== 递归追踪 ====================

def find_script_calls(script_path: str, ctx: ScanContext) -> List[str]:
    """在脚本中找出所有 sh/source/. 调用的子脚本路径"""
    if not os.path.isfile(script_path):
        return []

    script_dir = os.path.dirname(script_path)
    calls = []

    try:
        with open(script_path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue

                # 匹配: sh PATH、source PATH、. PATH、bash PATH
                for prefix in ['sh ', 'bash ', 'source ', '. ']:
                    if prefix in line:
                        # 提取 sh/source 后面的路径
                        idx = line.find(prefix)
                        after = line[idx + len(prefix):].strip()
                        # 处理可能的变量引用
                        # 尝试找到第一个空格或 ; & | 之前的内容作为路径
                        end_chars = ' ;&|#'
                        end_idx = len(after)
                        for ch in end_chars:
                            p = after.find(ch)
                            if p > 0 and p < end_idx:
                                end_idx = p
                        path_candidate = after[:end_idx].strip().strip('"\'')
                        if path_candidate and path_candidate.endswith('.sh'):
                            # 解析相对路径
                            resolved = resolve_variable_refs(path_candidate, ctx.variables)
                            # 展开变量引用
                            if not os.path.isabs(resolved):
                                resolved = os.path.normpath(
                                    os.path.join(script_dir, resolved))
                            calls.append(resolved)

                # 也匹配 source <path> 形式
                m = re.search(r'(?:^|\s)(?:sh|bash|source|\.)\s+(\S+\.sh)', line)
                if m:
                    path_candidate = m.group(1).strip('"\'')
                    resolved = resolve_variable_refs(path_candidate, ctx.variables)
                    if not os.path.isabs(resolved):
                        resolved = os.path.normpath(
                            os.path.join(script_dir, resolved))
                    if resolved not in calls:
                        calls.append(resolved)

    except Exception:
        pass

    return calls


def scan_recursive(script_path: str, ctx: ScanContext, depth: int = 0):
    """
    递归扫描脚本及其调用的子脚本。
    depth: 当前递归深度
    """
    script_path = os.path.abspath(script_path)

    if depth > ctx.max_depth:
        return
    if script_path in ctx.scanned_scripts:
        return
    if not os.path.isfile(script_path):
        return

    ctx.scanned_scripts.add(script_path)
    ctx.script_depth[script_path] = depth
    ctx.scripts_count += 1

    # 提取变量定义
    extract_variable_defs(script_path, ctx)

    # 扫描操作
    scan_script_operations(script_path, ctx)

    # 找到子脚本调用
    calls = find_script_calls(script_path, ctx)

    for call_path in calls:
        if call_path not in ctx.scanned_scripts:
            scan_recursive(call_path, ctx, depth + 1)


# ==================== 主入口 ====================

def collect_build_scripts(entry_script: str) -> List[str]:
    """
    收集构建目录树中所有 .sh 和 .env 文件。
    扫描范围: build/ 和 config/ 目录。
    """
    scripts = []
    entry_dir = os.path.dirname(os.path.abspath(entry_script))

    # 找项目根（包含 build/ 或 config/ 的目录）
    project_root = entry_dir
    for _ in range(5):
        if os.path.isdir(os.path.join(project_root, 'build')) or \
           os.path.isdir(os.path.join(project_root, 'config')):
            break
        parent = os.path.dirname(project_root)
        if parent == project_root:
            break
        project_root = parent

    # 扫描 build/ 和 config/ 目录
    scan_roots = []
    for d in ['build', 'config']:
        path = os.path.join(project_root, d)
        if os.path.isdir(path):
            scan_roots.append(path)

    # 如果 build/config 不在 project_root，反向查找
    if not scan_roots:
        prob = entry_dir
        for _ in range(5):
            d = os.path.join(prob, 'build')
            if os.path.isdir(d):
                scan_roots.append(d)
            d = os.path.join(prob, 'config')
            if os.path.isdir(d):
                scan_roots.append(d)
            parent = os.path.dirname(prob)
            if parent == prob:
                break
            prob = parent

    for scan_root in scan_roots:
        for root, dirs, files in os.walk(scan_root):
            dirs[:] = [d for d in dirs
                       if not d.startswith('.') and d != '__pycache__']
            for fname in files:
                if fname.endswith('.sh') or fname.endswith('.env'):
                    scripts.append(os.path.join(root, fname))

    return scripts


def pre_scan_all_scripts(entry_script: str, ctx: ScanContext):
    """
    批量预扫描构建目录中所有 .sh 和 .env 文件，提取变量定义。
    解决变量链引用问题（如 G_SDK_ROOTFS_PATH 引用 G_SDK_PATH）。
    """
    all_scripts = collect_build_scripts(entry_script)
    for fpath in all_scripts:
        extract_variable_defs(fpath, ctx)


def run_scan(entry_script: str, target_variable: str,
             output_path: Optional[str] = None,
             max_depth: int = 20) -> dict:
    """
    执行完整扫描流程。
    返回标准输出结构。
    """
    # 1. 验证输入
    ctx, error = validate_inputs(entry_script, target_variable,
                                 output_path, max_depth)
    if error:
        return error

    # 2. 预扫描：加载构建目录中所有脚本的变量定义
    pre_scan_all_scripts(ctx.entry_script, ctx)

    # 3. 批量扫描：扫描构建目录中所有脚本的文件操作
    all_scripts = collect_build_scripts(ctx.entry_script)
    # 确保入口脚本也在列表中
    if ctx.entry_script not in all_scripts:
        all_scripts.insert(0, ctx.entry_script)

    try:
        for script_path in all_scripts:
            scan_script_operations(script_path, ctx)
        ctx.scripts_count = len(set(
            e["script"] for e in ctx.entries
        ))
    except Exception as e:
        return {
            "status": "error",
            "errors": [{
                "code": "ERR_UNKNOWN",
                "message": f"扫描异常: {str(e)}"
            }]
        }

    # 3. 构建子变量映射
    build_var_children(ctx)

    # 4. 重新解析子目录（现在变量表完整了）
    for entry in ctx.entries:
        entry["subdir"] = extract_subdir(entry["dst_for_subdir"], ctx)

    # 5. 统计
    subdir_stats = OrderedDict()
    op_stats = OrderedDict()
    for entry in ctx.entries:
        sd = entry["subdir"]
        subdir_stats[sd] = subdir_stats.get(sd, 0) + 1
        op = entry["op"]
        op_stats[op] = op_stats.get(op, 0) + 1

    # 6. 清理内部字段
    clean_entries = []
    for i, entry in enumerate(ctx.entries, 1):
        clean_entries.append({
            "index": i,
            "script": entry["script"],
            "line": entry["line"],
            "op": entry["op"],
            "src": entry["src"],
            "dst": entry["dst"],
            "original": entry["dst"],  # 原内容 = 目标路径
            "subdir": entry["subdir"],
            "desc": entry["desc"],
        })

    if not clean_entries:
        return {
            "status": "warning",
            "code": "ERR_NO_OPERATIONS",
            "message": (f"扫描了 {ctx.scripts_count} 个脚本，"
                        f"未发现任何对 {target_variable} 的文件替换操作"),
            "data": {
                "excel_path": None,
                "total_entries": 0,
                "scripts_scanned": ctx.scripts_count,
                "entries": []
            }
        }

    return {
        "status": "success",
        "data": {
            "excel_path": ctx.output_path,
            "total_entries": len(clean_entries),
            "scripts_scanned": ctx.scripts_count,
            "subdir_stats": dict(subdir_stats),
            "op_stats": dict(op_stats),
            "entries": clean_entries,
        }
    }


# ==================== CLI 入口 ====================
if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print("Usage: python scan_build.py <entry_script> <target_variable> [output_path] [max_depth]")
        sys.exit(1)

    entry = sys.argv[1]
    target = sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else None
    depth = int(sys.argv[4]) if len(sys.argv) > 4 else 20

    result = run_scan(entry, target, out, depth)
    print(json.dumps(result, ensure_ascii=False, indent=2))
