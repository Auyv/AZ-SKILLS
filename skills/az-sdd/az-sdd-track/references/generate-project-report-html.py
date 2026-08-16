#!/usr/bin/env python3
"""
生成 AZ-SDD 项目汇报 HTML。

用法:
  python az-sdd-track/references/generate-project-report-html.py [项目路径]

输出:
  az-sdd/reports/project-status-report.html
"""

from __future__ import annotations

import html
import re
import sys
from datetime import datetime
from pathlib import Path


PROJECT_PATH = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
AZ_SDD_DIR = PROJECT_PATH / "az-sdd"
OUTPUT_DIR = AZ_SDD_DIR / "reports"
OUTPUT_FILE = OUTPUT_DIR / "project-status-report.html"

DONE_STATUSES = {"已完成", "已发布", "completed", "released"}
ACTIVE_STATUSES = {"开发中", "测试中", "待验收", "developing", "testing", "acceptance"}
OPEN_FEEDBACK_STATUSES = {
    "待澄清",
    "已计划",
    "处理中",
    "待验证",
    "待复现",
    "转需求",
    "clarifying",
    "planned",
    "processing",
    "verifying",
    "repro_pending",
    "transferred",
}

STATUS_ORDER = ["草稿", "审核中", "已批准", "开发中", "测试中", "待验收", "已完成", "已发布", "阻塞"]
STATUS_LABELS = {
    "draft": "草稿",
    "reviewing": "审核中",
    "approved": "已批准",
    "developing": "开发中",
    "testing": "测试中",
    "acceptance": "待验收",
    "completed": "已完成",
    "released": "已发布",
    "blocked": "阻塞",
}
STATUS_CLASS = {
    "草稿": "neutral",
    "审核中": "warning",
    "已批准": "ready",
    "开发中": "active",
    "测试中": "active",
    "待验收": "warning",
    "已完成": "done",
    "已发布": "done",
    "阻塞": "danger",
    "draft": "neutral",
    "reviewing": "warning",
    "approved": "ready",
    "developing": "active",
    "testing": "active",
    "acceptance": "warning",
    "completed": "done",
    "released": "done",
    "blocked": "danger",
}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def read_first(paths: list[Path]) -> str:
    for path in paths:
        if path.exists():
            return read_text(path)
    return ""


def esc(value: object) -> str:
    return html.escape(str(value or ""), quote=True)


def split_table_row(line: str) -> list[str]:
    if "|" not in line:
        return []
    cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
    if not cells or all(re.fullmatch(r":?-{2,}:?", cell or "") for cell in cells):
        return []
    return cells


def parse_key_value_table(content: str) -> dict[str, str]:
    data: dict[str, str] = {}
    for line in content.splitlines():
        cells = split_table_row(line)
        if len(cells) >= 2 and cells[0] not in {"字段", "指标", "项目"}:
            data[cells[0]] = cells[1]
    return data


def parse_status_board() -> dict[str, str]:
    return parse_key_value_table(read_text(AZ_SDD_DIR / "STATUS.md"))


def parse_spec_index() -> list[dict[str, str]]:
    content = read_text(AZ_SDD_DIR / "SPEC.md")
    specs: list[dict[str, str]] = []
    header: list[str] | None = None

    for line in content.splitlines():
        cells = split_table_row(line)
        if not cells:
            continue
        if "功能编号" in cells and "功能名称" in cells:
            header = cells
            continue
        if header and cells[0].isdigit():
            row = dict(zip(header, cells))
            specs.append(
                {
                    "id": row.get("功能编号", cells[0]),
                    "name": row.get("功能名称", cells[1] if len(cells) > 1 else ""),
                    "module": row.get("所属模块", cells[2] if len(cells) > 2 else ""),
                    "status": row.get("状态", cells[3] if len(cells) > 3 else "草稿"),
                    "updated": row.get("最后更新", cells[-1] if cells else ""),
                }
            )
        elif cells[0].isdigit() and len(cells) >= 4:
            specs.append(
                {
                    "id": cells[0],
                    "name": cells[1] if len(cells) > 1 else "",
                    "module": cells[2] if len(cells) > 2 else "",
                    "status": cells[3] if len(cells) > 3 else "草稿",
                    "updated": cells[5] if len(cells) > 5 else (cells[-1] if cells else ""),
                }
            )
    return specs


def parse_feedback_index() -> list[dict[str, str]]:
    content = read_text(AZ_SDD_DIR / "FEEDBACK.md")
    feedback: list[dict[str, str]] = []
    header: list[str] | None = None

    for line in content.splitlines():
        cells = split_table_row(line)
        if not cells:
            continue
        if "编号" in cells and "状态" in cells:
            header = cells
            continue
        if header and cells[0].startswith("FB-"):
            row = dict(zip(header, cells))
            feedback.append(
                {
                    "id": row.get("编号", cells[0]),
                    "type": row.get("类型", ""),
                    "summary": row.get("摘要", row.get("内容摘要", "")),
                    "severity": row.get("严重程度", ""),
                    "status": row.get("状态", ""),
                    "next_action": row.get("下一步动作", ""),
                    "artifact": row.get("关联产物", ""),
                    "updated": row.get("最后更新", row.get("提交日期", "")),
                }
            )
        elif cells[0].startswith("FB-") and len(cells) >= 5:
            feedback.append(
                {
                    "id": cells[0],
                    "type": cells[1] if len(cells) > 1 else "",
                    "summary": cells[2] if len(cells) > 2 else "",
                    "severity": cells[3] if len(cells) > 3 else "",
                    "status": cells[4] if len(cells) > 4 else "",
                    "next_action": cells[5] if len(cells) > 5 else "",
                    "artifact": cells[6] if len(cells) > 6 else "",
                    "updated": cells[8] if len(cells) > 8 else (cells[-1] if cells else ""),
                }
            )
    return feedback


def parse_recent_updates(content: str, limit: int = 6) -> list[tuple[str, str]]:
    updates: list[tuple[str, str]] = []
    for line in content.splitlines():
        cells = split_table_row(line)
        if len(cells) >= 2 and re.search(r"\d{4}-\d{2}-\d{2}|\d{2}-\d{2}", cells[0]):
            updates.append((cells[0], " | ".join(cells[1:])))
    return updates[-limit:]


def status_counts(specs: list[dict[str, str]]) -> dict[str, int]:
    counts = {status: 0 for status in STATUS_ORDER}
    for spec in specs:
        status = STATUS_LABELS.get(spec["status"], spec["status"])
        counts[status] = counts.get(status, 0) + 1
    return counts


def progress_percent(specs: list[dict[str, str]]) -> int:
    if not specs:
        return 0
    done = sum(1 for spec in specs if spec["status"] in DONE_STATUSES)
    return round(done / len(specs) * 100)


def project_summary(specs: list[dict[str, str]], feedback: list[dict[str, str]]) -> str:
    if not specs:
        return "项目尚未建立功能规范，建议先完成需求拆分和首批 spec。"
    open_feedback = [item for item in feedback if item["status"] in OPEN_FEEDBACK_STATUSES]
    active = [spec for spec in specs if spec["status"] in ACTIVE_STATUSES]
    done = [spec for spec in specs if spec["status"] in DONE_STATUSES]
    if open_feedback:
        return f"项目有 {len(open_feedback)} 条未闭环反馈，建议优先处理后再推进新功能。"
    if active:
        return f"项目正在推进 {len(active)} 个功能，当前重点是完成开发、测试和验收闭环。"
    if len(done) == len(specs):
        return "所有已登记功能均已完成或发布，可以进入阶段总结或规划下一轮需求。"
    return "项目处于规范到开发的推进阶段，建议优先处理已批准但未开发的功能。"


def render_status_bars(counts: dict[str, int], total: int) -> str:
    if total == 0:
        return '<p class="muted">暂无功能状态数据。</p>'
    rows = []
    for status in STATUS_ORDER:
        count = counts.get(status, 0)
        if count == 0:
            continue
        pct = round(count / total * 100)
        rows.append(
            f"""
            <div class="status-row">
              <div class="status-name"><span class="badge {STATUS_CLASS.get(status, 'neutral')}">{esc(status)}</span></div>
              <div class="bar"><span style="width:{pct}%"></span></div>
              <div class="status-count">{count}</div>
            </div>
            """
        )
    return "\n".join(rows)


def render_spec_table(specs: list[dict[str, str]]) -> str:
    if not specs:
        return '<p class="muted">暂无功能规范。</p>'
    rows = []
    for spec in specs:
        status = STATUS_LABELS.get(spec["status"], spec["status"])
        rows.append(
            f"""
            <tr>
              <td class="mono">{esc(spec['id'])}</td>
              <td>{esc(spec['name'])}</td>
              <td>{esc(spec['module'])}</td>
              <td><span class="badge {STATUS_CLASS.get(status, 'neutral')}">{esc(status)}</span></td>
              <td>{esc(spec['updated'])}</td>
            </tr>
            """
        )
    return f"""
    <table>
      <thead><tr><th>编号</th><th>功能</th><th>模块</th><th>状态</th><th>最后更新</th></tr></thead>
      <tbody>{''.join(rows)}</tbody>
    </table>
    """


def render_feedback_table(feedback: list[dict[str, str]]) -> str:
    open_items = [item for item in feedback if item["status"] in OPEN_FEEDBACK_STATUSES]
    if not open_items:
        return '<p class="muted">暂无未闭环反馈。</p>'
    rows = []
    for item in open_items:
        rows.append(
            f"""
            <tr>
              <td class="mono">{esc(item['id'])}</td>
              <td>{esc(item['summary'])}</td>
              <td><span class="badge warning">{esc(item['status'])}</span></td>
              <td>{esc(item['next_action'])}</td>
              <td>{esc(item['updated'])}</td>
            </tr>
            """
        )
    return f"""
    <table>
      <thead><tr><th>编号</th><th>摘要</th><th>状态</th><th>下一步</th><th>最后更新</th></tr></thead>
      <tbody>{''.join(rows)}</tbody>
    </table>
    """


def render_updates(updates: list[tuple[str, str]]) -> str:
    if not updates:
        return '<p class="muted">暂无近期更新记录。</p>'
    items = "".join(f"<li><strong>{esc(date)}</strong><span>{esc(text)}</span></li>" for date, text in updates)
    return f'<ol class="timeline">{items}</ol>'


def main() -> None:
    status_info = parse_status_board()
    init_info = parse_key_value_table(read_first([AZ_SDD_DIR / "INIT.md", AZ_SDD_DIR / "init.md"]))
    specs = parse_spec_index()
    feedback = parse_feedback_index()
    track_text = read_first([AZ_SDD_DIR / "TRACK.md", AZ_SDD_DIR / "track.md"])
    updates = parse_recent_updates(track_text)

    total = len(specs)
    done = sum(1 for spec in specs if spec["status"] in DONE_STATUSES)
    active = sum(1 for spec in specs if spec["status"] in ACTIVE_STATUSES)
    open_feedback = sum(1 for item in feedback if item["status"] in OPEN_FEEDBACK_STATUSES)
    progress = progress_percent(specs)
    counts = status_counts(specs)
    generated_at = datetime.now().strftime("%Y-%m-%d %H:%M")
    project_name = init_info.get("项目名称") or PROJECT_PATH.name
    project_type = init_info.get("项目类型", "-")
    tech_stack = init_info.get("技术栈摘要", init_info.get("技术栈", "-"))
    board_summary = status_info.get("下一步建议") or status_info.get("当前工作") or ""
    summary = board_summary or project_summary(specs, feedback)
    current_stage = status_info.get("当前阶段", "-")
    current_focus = status_info.get("当前工作", "-")

    html_text = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>项目状态汇报 - {esc(project_name)}</title>
  <style>
    * {{ box-sizing: border-box; }}
    body {{ margin: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Microsoft YaHei", sans-serif; color: #202124; background: #f7f8fa; line-height: 1.55; }}
    .page {{ max-width: 1180px; margin: 0 auto; padding: 28px; }}
    .topbar {{ display: flex; justify-content: space-between; gap: 16px; align-items: flex-start; margin-bottom: 20px; }}
    .title h1 {{ margin: 0 0 8px; font-size: 30px; }}
    .title p {{ margin: 0; color: #5f6368; }}
    .actions {{ display: flex; gap: 8px; }}
    button {{ border: 1px solid #d0d5dd; background: #fff; color: #344054; padding: 8px 12px; border-radius: 6px; cursor: pointer; }}
    .hero {{ background: #fff; border: 1px solid #e5e7eb; border-radius: 8px; padding: 24px; margin-bottom: 18px; }}
    .hero h2 {{ margin: 0 0 8px; font-size: 22px; }}
    .hero .summary {{ font-size: 17px; color: #344054; margin: 0 0 18px; }}
    .meta {{ display: flex; gap: 20px; flex-wrap: wrap; color: #5f6368; font-size: 13px; }}
    .kpis {{ display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; margin-bottom: 18px; }}
    .kpi {{ background: #fff; border: 1px solid #e5e7eb; border-radius: 8px; padding: 16px; }}
    .kpi .label {{ color: #667085; font-size: 13px; margin-bottom: 8px; }}
    .kpi .value {{ font-size: 30px; font-weight: 700; }}
    .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 18px; }}
    section {{ background: #fff; border: 1px solid #e5e7eb; border-radius: 8px; padding: 18px; margin-bottom: 18px; }}
    section h2 {{ margin: 0 0 14px; font-size: 18px; }}
    .progress {{ height: 12px; background: #eaecf0; border-radius: 999px; overflow: hidden; margin-top: 10px; }}
    .progress span {{ display: block; height: 100%; background: #16a34a; }}
    .status-row {{ display: grid; grid-template-columns: 92px 1fr 40px; gap: 10px; align-items: center; margin: 10px 0; }}
    .bar {{ height: 9px; background: #eef2f6; border-radius: 999px; overflow: hidden; }}
    .bar span {{ display: block; height: 100%; background: #475467; }}
    .status-count {{ text-align: right; color: #667085; }}
    table {{ width: 100%; border-collapse: collapse; font-size: 14px; }}
    th, td {{ border-bottom: 1px solid #edf0f2; padding: 10px 8px; text-align: left; vertical-align: top; }}
    th {{ color: #667085; font-weight: 600; background: #fafafa; }}
    .badge {{ display: inline-block; padding: 3px 8px; border-radius: 999px; font-size: 12px; font-weight: 600; white-space: nowrap; }}
    .neutral {{ background: #eef2f6; color: #475467; }}
    .warning {{ background: #fff7d6; color: #8a6100; }}
    .ready {{ background: #dcfce7; color: #166534; }}
    .active {{ background: #dbeafe; color: #1d4ed8; }}
    .done {{ background: #dcfce7; color: #166534; }}
    .danger {{ background: #fee2e2; color: #991b1b; }}
    .mono {{ font-family: Consolas, "SFMono-Regular", monospace; }}
    .muted {{ color: #667085; }}
    .timeline {{ margin: 0; padding-left: 22px; }}
    .timeline li {{ margin: 8px 0; }}
    .timeline span {{ margin-left: 8px; color: #344054; }}
    .full {{ grid-column: 1 / -1; }}
    @media (max-width: 860px) {{
      .page {{ padding: 16px; }}
      .topbar {{ flex-direction: column; }}
      .kpis, .grid {{ grid-template-columns: 1fr; }}
      table {{ font-size: 13px; }}
    }}
    @media print {{
      body {{ background: #fff; }}
      .page {{ padding: 0; max-width: none; }}
      .actions {{ display: none; }}
      section, .hero, .kpi {{ break-inside: avoid; }}
    }}
  </style>
</head>
<body>
  <main class="page">
    <div class="topbar">
      <div class="title">
        <h1>{esc(project_name)} 项目状态汇报</h1>
        <p>生成时间：{esc(generated_at)} · 项目路径：{esc(PROJECT_PATH)}</p>
      </div>
      <div class="actions">
        <button onclick="window.print()">打印 / 导出 PDF</button>
      </div>
    </div>

    <section class="hero">
      <h2>汇报结论</h2>
      <p class="summary">{esc(summary)}</p>
      <div class="meta">
        <span>项目类型：{esc(project_type)}</span>
        <span>技术栈：{esc(tech_stack)}</span>
        <span>当前阶段：{esc(current_stage)}</span>
      </div>
      <div class="progress"><span style="width:{progress}%"></span></div>
    </section>

    <div class="kpis">
      <div class="kpi"><div class="label">整体进度</div><div class="value">{progress}%</div></div>
      <div class="kpi"><div class="label">功能总数</div><div class="value">{total}</div></div>
      <div class="kpi"><div class="label">已完成/发布</div><div class="value">{done}</div></div>
      <div class="kpi"><div class="label">未闭环反馈</div><div class="value">{open_feedback}</div></div>
    </div>

    <div class="grid">
      <section>
        <h2>功能状态分布</h2>
        {render_status_bars(counts, total)}
      </section>
      <section>
        <h2>当前关注点</h2>
        <table>
          <tbody>
            <tr><th>进行中功能</th><td>{active}</td></tr>
            <tr><th>未闭环反馈</th><td>{open_feedback}</td></tr>
            <tr><th>当前工作</th><td>{esc(current_focus)}</td></tr>
            <tr><th>建议下一步</th><td>{esc(summary)}</td></tr>
          </tbody>
        </table>
      </section>
      <section class="full">
        <h2>未闭环反馈</h2>
        {render_feedback_table(feedback)}
      </section>
      <section class="full">
        <h2>功能清单</h2>
        {render_spec_table(specs)}
      </section>
      <section class="full">
        <h2>近期更新</h2>
        {render_updates(updates)}
      </section>
    </div>
  </main>
</body>
</html>
"""

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_FILE.write_text(html_text, encoding="utf-8")
    print(f"已生成: {OUTPUT_FILE}")
    print(f"功能: {total} 个，完成: {done} 个，未闭环反馈: {open_feedback} 条，整体进度: {progress}%")


if __name__ == "__main__":
    main()
