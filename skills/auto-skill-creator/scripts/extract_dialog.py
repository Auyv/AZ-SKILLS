"""
对话提取脚本
用于从对话历史中提取正向和负向流程
"""

import json
import re
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from datetime import datetime

def extract_dialog_segment(messages: List[Dict], target_message_index: int, 
                           window_size: int = 10) -> Dict:
    """
    提取以目标消息为中心的对话片段
    
    Args:
        messages: 消息列表，格式为 [{"role": "user/assistant", "content": "..."}, ...]
        target_message_index: 目标消息的索引（检测到关键词的消息）
        window_size: 向前追溯的消息数量
    
    Returns:
        包含正向和负向流程的字典
    """
    start_index = max(0, target_message_index - window_size)
    end_index = min(len(messages), target_message_index + 1)
    
    segment = messages[start_index:end_index]
    
    positive_dialog = []
    negative_dialog = []
    
    positive_keywords = ["good", "搞定", "可以了", "没问题", "解决了", "完美", "👍", "感谢", 
                        "不错", "很好", "棒", "ok", "okay", "done", "finished", "success", "solved"]
    negative_keywords = ["不行", "失败了", "报错", "不对", "没用", "垃圾", "烂", "错误",
                        "bug", "issue", "problem", "failed"]
    
    for i, msg in enumerate(segment):
        content = msg.get("content", "").lower()
        is_positive = any(kw.lower() in content for kw in positive_keywords)
        is_negative = any(kw.lower() in content for kw in negative_keywords)
        
        if msg.get("role") == "user":
            if is_positive:
                positive_dialog.append({"role": msg.get("role"), "content": msg.get("content")})
            elif is_negative:
                negative_dialog.append({"role": msg.get("role"), "content": msg.get("content")})
    
    # 提取从问题到解决的全过程
    full_flow = segment.copy()
    
    return {
        "dialog_segment": segment,
        "positive_flow": positive_dialog,
        "negative_flow": negative_dialog,
        "full_flow": full_flow,
        "time_range": {
            "start": start_index,
            "end": end_index,
            "target": target_message_index
        }
    }

def format_dialog_for_skill(dialog_data: Dict) -> str:
    """
    将对话数据格式化为技能文档可用的格式
    
    Args:
        dialog_data: extract_dialog_segment 返回的数据
    
    Returns:
        格式化的对话文本
    """
    lines = []
    lines.append("## 对话示例\n")
    
    for msg in dialog_data.get("full_flow", []):
        role = msg.get("role", "unknown")
        content = msg.get("content", "")
        lines.append(f"**{role}**: {content}\n")
    
    return "\n".join(lines)

def format_negative_for_faq(dialog_data: Dict, skill_name: str = "") -> str:
    """
    将负向流程格式化为 FAQ 格式
    
    Args:
        dialog_data: extract_dialog_segment 返回的数据
        skill_name: 关联的技能名称
    
    Returns:
        FAQ 格式的文本
    """
    lines = []
    lines.append("# 异常处理 FAQ\n")
    lines.append(f"关联技能: {skill_name}\n")
    lines.append(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
    lines.append("---\n\n")
    
    negative_flow = dialog_data.get("negative_flow", [])
    
    if not negative_flow:
        lines.append("暂无负向反馈记录。\n")
        return "\n".join(lines)
    
    for i, msg in enumerate(negative_flow, 1):
        content = msg.get("content", "")
        lines.append(f"## Q{i}: {content}\n")
        lines.append(f"**A**: 请参考上述对话内容，联系上下文进行排查。\n\n")
    
    return "\n".join(lines)

def save_faq(faq_content: str, skill_name: str, base_path: str) -> str:
    """
    保存 FAQ 到文件
    
    Args:
        faq_content: FAQ 内容
        skill_name: 技能名称（用于文件名）
        base_path: 基础路径
    
    Returns:
        保存的文件路径
    """
    faq_dir = Path(base_path) / "docs" / "faq"
    faq_dir.mkdir(parents=True, exist_ok=True)
    
    filename = f"faq-{skill_name.lower().replace(' ', '-')}.md"
    filepath = faq_dir / filename
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(faq_content)
    
    return str(filepath)

if __name__ == "__main__":
    # 测试代码
    test_messages = [
        {"role": "user", "content": "我想要一个自动总结的技能"},
        {"role": "assistant", "content": "好的，我来帮您创建这个技能..."},
        {"role": "user", "content": "需要检测用户的满意信号"},
        {"role": "assistant", "content": "可以使用关键词检测..."},
        {"role": "user", "content": "搞定！很好用！"}
    ]
    
    result = extract_dialog_segment(test_messages, 4)
    
    print("提取结果:")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    
    print("\n格式化对话:")
    print(format_dialog_for_skill(result))
