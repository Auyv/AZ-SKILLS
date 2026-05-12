"""
关键词检测脚本
用于检测用户输入中的满意/不满意关键词
"""

import json
import re
from pathlib import Path
from typing import List, Tuple, Optional

def load_config(config_path: str) -> dict:
    """加载配置文件"""
    with open(config_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def detect_keywords(user_input: str, config_path: Optional[str] = None) -> Tuple[bool, List[str], bool, List[str]]:
    """
    检测用户输入中的关键词
    
    Args:
        user_input: 用户输入文本
        config_path: 配置文件路径，None时使用默认路径
    
    Returns:
        Tuple[bool, List[str], bool, List[str]]
        - has_positive: 是否有正向关键词
        - positive_found: 匹配到的正向关键词列表
        - has_negative: 是否有负向关键词
        - negative_found: 匹配到的负向关键词列表
    """
    if config_path is None:
        config_path = str(Path(__file__).parent.parent / "config" / "skill-creator-config.json")
    
    config = load_config(config_path)
    
    positive_keywords = config.get("positive_keywords", [])
    negative_keywords = config.get("negative_keywords", [])
    
    user_input_lower = user_input.lower()
    
    positive_found = []
    for keyword in positive_keywords:
        if keyword.lower() in user_input_lower:
            positive_found.append(keyword)
    
    negative_found = []
    for keyword in negative_keywords:
        if keyword.lower() in user_input_lower:
            negative_found.append(keyword)
    
    return (
        len(positive_found) > 0,
        positive_found,
        len(negative_found) > 0,
        negative_found
    )

def update_keywords(config_path: str, positive_to_add: List[str] = None, 
                    negative_to_add: List[str] = None,
                    positive_to_remove: List[str] = None,
                    negative_to_remove: List[str] = None) -> dict:
    """
    更新关键词配置
    
    Args:
        config_path: 配置文件路径
        positive_to_add: 要添加的正向关键词列表
        negative_to_add: 要添加的负向关键词列表
        positive_to_remove: 要移除的正向关键词列表
        negative_to_remove: 要移除的负向关键词列表
    
    Returns:
        更新后的配置字典
    """
    config = load_config(config_path)
    
    if positive_to_add:
        for kw in positive_to_add:
            if kw not in config["positive_keywords"]:
                config["positive_keywords"].append(kw)
    
    if negative_to_add:
        for kw in negative_to_add:
            if kw not in config["negative_keywords"]:
                config["negative_keywords"].append(kw)
    
    if positive_to_remove:
        config["positive_keywords"] = [kw for kw in config["positive_keywords"] 
                                        if kw not in positive_to_remove]
    
    if negative_to_remove:
        config["negative_keywords"] = [kw for kw in config["negative_keywords"] 
                                       if kw not in negative_to_remove]
    
    with open(config_path, 'w', encoding='utf-8') as f:
        json.dump(config, f, ensure_ascii=False, indent=2)
    
    return config

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        test_input = " ".join(sys.argv[1:])
    else:
        test_input = "搞定了，很好！"
    
    has_pos, pos_found, has_neg, neg_found = detect_keywords(test_input)
    
    print(f"输入: {test_input}")
    print(f"检测结果:")
    print(f"  - 正向关键词: {pos_found if pos_found else '无'}")
    print(f"  - 负向关键词: {neg_found if neg_found else '无'}")
