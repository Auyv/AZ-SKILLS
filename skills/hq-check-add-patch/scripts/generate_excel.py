# -*- coding: utf-8 -*-
"""
Excel 生成器 - 将 scan_build.py 的扫描结果输出为 9 列 Excel (.xlsx)。

用法:
    python generate_excel.py <scan_result.json> <output.xlsx>
    或作为模块: from generate_excel import generate_excel
"""

import json
import os
import sys
from typing import Dict, List, Optional


def generate_excel_from_data(entries: List[dict], output_path: str,
                             target_variable: str = "G_SDK_PATH",
                             summary: Optional[dict] = None) -> str:
    """
    从扫描结果数据生成 Excel 文件。

    参数:
        entries: 扫描条目列表
        output_path: 输出 .xlsx 路径
        target_variable: 目标变量名
        summary: 可选的汇总统计

    返回:
        生成的 Excel 文件路径

    异常:
        ImportError: 缺少 openpyxl
        PermissionError: 输出路径无写权限
    """
    try:
        import openpyxl
    except ImportError:
        # 回退到 CSV
        return _generate_csv(entries, output_path, target_variable)

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = f"{target_variable}替换清单"

    # ==================== 表头 ====================
    headers = ["序号", "执行脚本", "行号", "操作类型", "源文件路径",
               "目标文件路径", "原始内容(被替换前)", "影响的SDK子目录", "备注"]

    from openpyxl.styles import Font, Alignment, PatternFill, Border, Side
    from openpyxl.utils import get_column_letter

    header_font = Font(name="微软雅黑", bold=True, size=11, color="FFFFFF")
    header_fill = PatternFill(start_color="4472C4", end_color="4472C4",
                              fill_type="solid")
    header_align = Alignment(horizontal="center", vertical="center",
                             wrap_text=True)
    thin_border = Border(
        left=Side(style="thin"), right=Side(style="thin"),
        top=Side(style="thin"), bottom=Side(style="thin"),
    )

    for col_idx, h in enumerate(headers, 1):
        cell = ws.cell(row=1, column=col_idx, value=h)
        cell.font = header_font
        cell.fill = header_fill
        cell.alignment = header_align
        cell.border = thin_border

    # ==================== 数据行 ====================
    data_font = Font(name="Consolas", size=10)
    data_align = Alignment(vertical="top", wrap_text=True)
    alt_fill = PatternFill(start_color="D9E2F3", end_color="D9E2F3",
                           fill_type="solid")

    for i, entry in enumerate(entries):
        row = i + 2
        vals = [
            i + 1,                          # 序号
            entry.get("script", ""),         # 执行脚本
            str(entry.get("line", "")),      # 行号
            entry.get("op", ""),             # 操作类型
            entry.get("src", ""),            # 源文件路径
            entry.get("dst", ""),            # 目标文件路径
            entry.get("original", entry.get("dst", "")),  # 原始内容
            entry.get("subdir", ""),         # 影响的SDK子目录
            entry.get("desc", ""),           # 备注
        ]
        for col_idx, val in enumerate(vals, 1):
            cell = ws.cell(row=row, column=col_idx, value=val)
            cell.font = data_font
            cell.alignment = data_align
            cell.border = thin_border
            if i % 2 == 0:
                cell.fill = alt_fill

    # ==================== 格式设置 ====================
    col_widths = [6, 55, 10, 18, 60, 60, 60, 20, 50]
    for col_idx, w in enumerate(col_widths, 1):
        ws.column_dimensions[get_column_letter(col_idx)].width = w

    ws.freeze_panes = "A2"
    ws.auto_filter.ref = f"A1:I{len(entries) + 1}"

    # ==================== 保存 ====================
    output_dir = os.path.dirname(output_path)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    wb.save(output_path)

    if not os.path.exists(output_path):
        raise PermissionError(f"无法写入文件: {output_path}")

    return output_path


def _generate_csv(entries: List[dict], output_path: str,
                  target_variable: str) -> str:
    """CSV 回退方案"""
    import csv

    csv_path = output_path.replace('.xlsx', '.csv')
    headers = ["序号", "执行脚本", "行号", "操作类型", "源文件路径",
               "目标文件路径", "原始内容(被替换前)", "影响的SDK子目录", "备注"]

    with open(csv_path, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        for i, entry in enumerate(entries):
            writer.writerow([
                i + 1,
                entry.get("script", ""),
                entry.get("line", ""),
                entry.get("op", ""),
                entry.get("src", ""),
                entry.get("dst", ""),
                entry.get("original", entry.get("dst", "")),
                entry.get("subdir", ""),
                entry.get("desc", ""),
            ])

    print(f"openpyxl 不可用，已回退生成 CSV: {csv_path}")
    print("提示: 运行 pip install openpyxl 安装后支持 .xlsx 格式")
    return csv_path


def generate_excel(scan_result_path: str, output_path: str) -> str:
    """
    从 scan_build.py 输出的 JSON 生成 Excel。

    参数:
        scan_result_path: scan_build.py 输出的 JSON 文件路径
        output_path: Excel 输出路径

    返回:
        生成的 Excel 文件路径
    """
    if not os.path.isfile(scan_result_path):
        raise FileNotFoundError(f"扫描结果文件不存在: {scan_result_path}")

    with open(scan_result_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if data.get("status") == "error":
        raise RuntimeError(f"扫描出错: {json.dumps(data.get('errors', []), ensure_ascii=False)}")

    entries = data.get("data", {}).get("entries", [])
    if not entries:
        print("警告: 扫描结果中没有条目")

    return generate_excel_from_data(entries, output_path)


# ==================== CLI ====================
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python generate_excel.py <scan_result.json> <output.xlsx>")
        sys.exit(1)

    result_json = sys.argv[1]
    out_xlsx = sys.argv[2]

    try:
        path = generate_excel(result_json, out_xlsx)
        print(f"OK: {path}")
    except FileNotFoundError as e:
        print(f"错误: {e}", file=sys.stderr)
        sys.exit(2)
    except RuntimeError as e:
        print(f"错误: {e}", file=sys.stderr)
        sys.exit(3)
    except PermissionError as e:
        print(f"权限不足: {e}", file=sys.stderr)
        sys.exit(4)
