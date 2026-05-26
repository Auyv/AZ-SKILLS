from __future__ import annotations

import sys
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import messagebox, ttk

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tools.skill_feedback.daily_evolution import FEEDBACK_ROOT, INBOX_DIR, discover_skills, safe_name
else:
    from .daily_evolution import FEEDBACK_ROOT, INBOX_DIR, discover_skills, safe_name


def timestamp() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def iso_now() -> str:
    return datetime.now().isoformat(timespec="seconds")


def write_feedback(skill: str, pain: str, scenario: str, expected: str) -> Path:
    INBOX_DIR.mkdir(parents=True, exist_ok=True)
    FEEDBACK_ROOT.mkdir(parents=True, exist_ok=True)
    file_name = f"feedback_{timestamp()}_{safe_name(skill)}.md"
    path = INBOX_DIR / file_name
    content = f"""---
created_at: {iso_now()}
status: pending
skill: {skill}
---

# Skill 用户反馈单

## 被反馈的 skill 技能名称

{skill}

## 用户痛点描述

{pain}

## 痛点场景举例

{scenario or "未填写"}

## 期望效果

{expected or "未填写"}
"""
    path.write_text(content, encoding="utf-8")
    return path


class FeedbackForm(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Skill 用户反馈单")
        self.geometry("760x640")
        self.minsize(680, 560)

        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        frame = ttk.Frame(self, padding=18)
        frame.grid(row=0, column=0, sticky="nsew")
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(3, weight=1)
        frame.rowconfigure(5, weight=1)
        frame.rowconfigure(7, weight=1)

        ttk.Label(frame, text="被反馈的 skill 技能名称").grid(row=0, column=0, sticky="w")
        self.skill_var = tk.StringVar()
        self.skill_box = ttk.Combobox(frame, textvariable=self.skill_var, values=discover_skills())
        self.skill_box.grid(row=1, column=0, sticky="ew", pady=(6, 14))

        ttk.Label(frame, text="用户痛点描述（必填）").grid(row=2, column=0, sticky="w")
        self.pain_text = tk.Text(frame, height=8, wrap="word")
        self.pain_text.grid(row=3, column=0, sticky="nsew", pady=(6, 14))

        ttk.Label(frame, text="痛点场景举例（可选）").grid(row=4, column=0, sticky="w")
        self.scenario_text = tk.Text(frame, height=6, wrap="word")
        self.scenario_text.grid(row=5, column=0, sticky="nsew", pady=(6, 14))

        ttk.Label(frame, text="期望效果（可选）").grid(row=6, column=0, sticky="w")
        self.expected_text = tk.Text(frame, height=6, wrap="word")
        self.expected_text.grid(row=7, column=0, sticky="nsew", pady=(6, 14))

        actions = ttk.Frame(frame)
        actions.grid(row=8, column=0, sticky="e")
        ttk.Button(actions, text="提交反馈", command=self.submit).grid(row=0, column=0, padx=(0, 8))
        ttk.Button(actions, text="退出", command=self.destroy).grid(row=0, column=1)

    def submit(self) -> None:
        skill = self.skill_var.get().strip()
        pain = self.pain_text.get("1.0", "end").strip()
        scenario = self.scenario_text.get("1.0", "end").strip()
        expected = self.expected_text.get("1.0", "end").strip()

        if not skill:
            messagebox.showwarning("缺少信息", "请填写被反馈的 skill 技能名称。")
            return
        if not pain:
            messagebox.showwarning("缺少信息", "请填写用户痛点描述。")
            return

        path = write_feedback(skill, pain, scenario, expected)
        messagebox.showinfo("已保存", f"反馈单已保存：\n{path}")
        self.destroy()


def main() -> int:
    app = FeedbackForm()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
