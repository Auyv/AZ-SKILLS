#!/usr/bin/env python3
"""
生成人工测试清单 HTML 页面
用法: python generate-test-review-html.py [项目路径] [功能编号]

示例:
  python generate-test-review-html.py /mnt/c/JPZ_Workspace/AI_Coding/nmrj/
  python generate-test-review-html.py /mnt/c/JPZ_Workspace/AI_Coding/nmrj/ 0001
"""

import json
import re
import sys
import os
from pathlib import Path
from datetime import datetime

PROJECT_PATH = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
SPEC_ID = sys.argv[2] if len(sys.argv) > 2 else None
OUTPUT_DIR = PROJECT_PATH / "az-sdd-test" / "references"
OUTPUT_FILE = OUTPUT_DIR / "test-review.html"

def get_specs():
    """获取所有规范及其测试用例"""
    specs = []
    
    # 从 SPEC.md 读取
    spec_md = PROJECT_PATH / "az-sdd" / "SPEC.md"
    if spec_md.exists():
        content = spec_md.read_text(encoding='utf-8')
        for line in content.split('\n'):
            if '|' in line and not line.startswith('|--') and not line.startswith('| -'):
                cells = [c.strip() for c in line.split('|') if c.strip()]
                if len(cells) >= 5 and (cells[0].isdigit() or re.match(r'^000\d+$', cells[0])):
                    status = cells[4] if len(cells) > 4 else ''
                    # 只获取需要测试的功能（状态为 acceptance/待验收）
                    if status in ['acceptance', '待验收', '测试中', 'testing', 'developing', '开发中']:
                        if SPEC_ID is None or cells[0] == SPEC_ID:
                            specs.append({
                                'id': cells[0],
                                'name': cells[1],
                                'module': cells[2] if len(cells) > 2 else '',
                                'status': status,
                                'specPath': str(PROJECT_PATH / "az-sdd" / "spec" / f"{cells[0]}-{cells[1]}.spec.md")
                            })
    
    # 备用：从 spec 目录扫描
    if not specs:
        spec_dir = PROJECT_PATH / "az-sdd" / "spec"
        if spec_dir.exists():
            for f in spec_dir.glob("*.spec.md"):
                m = re.match(r'^(\d+)-(.+)\.spec\.md$', f.name)
                if m:
                    content = f.read_text(encoding='utf-8')
                    status = '待验收'
                    if '| 状态 |' in content:
                        s = re.search(r'\|\s*状态\s*\|\s*([^|\n]+)\s*\|', content)
                        if s:
                            st = s.group(1).strip()
                            if st in ['待验收', '测试中', '已批准', '开发中']:
                                status = st
                    
                    if status in ['待验收', '测试中', '已批准', '开发中']:
                        if SPEC_ID is None or m.group(1) == SPEC_ID:
                            specs.append({
                                'id': m.group(1),
                                'name': m.group(2),
                                'module': '',
                                'status': status,
                                'specPath': str(f)
                            })
    
    return specs

def get_test_cases(spec_id, spec_name, spec_path):
    """从规范文件获取人工测试用例"""
    cases = []
    
    if not os.path.exists(spec_path):
        return cases
    
    content = Path(spec_path).read_text(encoding='utf-8')
    
    # 查找测试用例部分
    in_test_section = False
    current_group = None
    
    lines = content.split('\n')
    for i, line in enumerate(lines):
        # 检测测试用例章节
        if re.search(r'#{1,3}\s*(测试用例|人工测试|测试项)', line):
            in_test_section = True
            continue
        
        if in_test_section and line.startswith('##'):
            break
        
        # 解析测试用例行
        if in_test_section and '|' in line and not line.startswith('|--'):
            cells = [c.strip() for c in line.split('|') if c.strip()]
            # 匹配 MTxxx 格式
            if len(cells) >= 3:
                title_match = re.search(r'MT(\d+)\s*[-–]\s*(.+)', cells[1])
                if title_match:
                    case_id = f"MT{title_match.group(1).zfill(3)}"
                    cases.append({
                        'id': case_id,
                        'title': title_match.group(2).strip(),
                        'type': cells[2] if len(cells) > 2 else '人工',
                        'status': cells[3] if len(cells) > 3 else '待验证',
                        'specId': spec_id,
                        'specName': spec_name,
                        'specPath': spec_path
                    })
    
    # 如果没找到，生成默认测试用例（基于验收标准）
    if not cases:
        in_acceptance = False
        for line in content.split('\n'):
            if '## 验收标准' in line or '## 验收清单' in line:
                in_acceptance = True
                continue
            if in_acceptance and line.startswith('##'):
                break
            if in_acceptance and '- [ ]' in line:
                m = re.search(r'\[\s*(\d+)\s*\]\s*(.+)', line)
                if m:
                    cases.append({
                        'id': f"MT{m.group(1).zfill(3)}",
                        'title': m.group(2).strip(),
                        'type': '人工',
                        'status': '待验证',
                        'specId': spec_id,
                        'specName': spec_name,
                        'specPath': spec_path
                    })
    
    return cases

def generate_html(specs, all_cases):
    """生成 HTML 页面"""
    
    # 统计
    total_specs = len(specs)
    total_cases = len(all_cases)
    pending_cases = sum(1 for c in all_cases if c['status'] in ['待验证', 'pending', '⏳'])
    passed_cases = sum(1 for c in all_cases if c['status'] in ['已通过', '通过', '✅', 'passed'])
    
    # 生成功能卡片
    spec_cards = []
    for spec in specs:
        spec_cases = [c for c in all_cases if c['specId'] == spec['id']]
        spec_pending = sum(1 for c in spec_cases if c['status'] in ['待验证', 'pending', '⏳'])
        
        file_url = f"vscode://file/{spec['specPath'].replace(chr(92), '/')}"
        
        spec_cards.append(f'''
        <div class="spec-card" data-spec-id="{spec['id']}">
            <div class="spec-header" onclick="toggleSpec('{spec['id']}')">
                <div class="spec-id">{spec['id']}</div>
                <div class="spec-info">
                    <h3>{spec['name']}</h3>
                    <div class="spec-meta">
                        <span class="badge badge-pending">{spec_pending} 项待验证</span>
                    </div>
                </div>
                <div class="toggle-icon" id="icon-{spec['id']}">▼</div>
            </div>
            <div class="spec-cases" id="cases-{spec['id']}">
                <div class="cases-list">
''')
        
        for case in spec_cases:
            is_checked = case['status'] in ['已通过', '通过', '✅', 'passed']
            case_file_url = f"vscode://file/{case['specPath'].replace(chr(92), '/')}"
            
            spec_cards.append(f'''
                    <div class="case-item {'checked' if is_checked else ''}" data-case-id="{case['id']}">
                        <div class="case-checkbox">
                            <input type="checkbox" {"checked" if is_checked else ""} 
                                   onchange="toggleCase('{spec['id']}', '{case['id']}', this.checked)">
                        </div>
                        <div class="case-content">
                            <div class="case-title">
                                <a href="{case_file_url}" onclick="event.stopPropagation()">
                                    {case['id']} - {case['title']}
                                </a>
                            </div>
                            <div class="case-type">{case['type']}</div>
                        </div>
                    </div>
''')
        
        spec_cards.append('''
                </div>
            </div>
        </div>
''')
    
    # 状态映射
    status_badge_map = '''
    const statusMap = {
        '待验证': 'badge-pending',
        'pending': 'badge-pending',
        '⏳': 'badge-pending',
        '已通过': 'badge-passed',
        '通过': 'badge-passed',
        '✅': 'badge-passed',
        'passed': 'badge-passed',
        '已失败': 'badge-failed',
        '失败': 'badge-failed',
        '❌': 'badge-failed',
        'failed': 'badge-failed'
    };
    '''
    
    html = f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>🧪 人工测试 - {PROJECT_PATH.name}</title>
  <style>
    * {{ margin: 0; padding: 0; box-sizing: border-box; }}
    body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f0f2f5; color: #333; line-height: 1.6; padding: 20px; }}
    .container {{ max-width: 1200px; margin: 0 auto; }}
    .header {{ background: linear-gradient(135deg, #2196F3 0%, #1976D2 100%); color: white; padding: 30px; border-radius: 12px; margin-bottom: 30px; }}
    .header h1 {{ font-size: 28px; margin-bottom: 10px; }}
    .header .subtitle {{ opacity: 0.9; font-size: 14px; word-break: break-all; }}
    .stats {{ display: flex; gap: 20px; margin-top: 20px; flex-wrap: wrap; }}
    .stat-item {{ background: rgba(255,255,255,0.2); padding: 10px 20px; border-radius: 8px; text-align: center; }}
    .stat-item .count {{ font-size: 24px; font-weight: bold; }}
    .stat-item .label {{ font-size: 12px; opacity: 0.9; }}
    .notice {{ background: #fff3cd; border: 1px solid #ffc107; border-radius: 8px; padding: 15px 20px; margin-bottom: 20px; font-size: 14px; color: #856404; }}
    .spec-card {{ background: white; border-radius: 12px; margin-bottom: 15px; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,0.05); }}
    .spec-header {{ display: grid; grid-template-columns: 80px 1fr auto; gap: 20px; align-items: center; padding: 20px; cursor: pointer; transition: background 0.2s; }}
    .spec-header:hover {{ background: #f5f5f5; }}
    .spec-id {{ font-size: 14px; font-weight: 600; color: #2196F3; text-align: center; padding: 8px; background: #E3F2FD; border-radius: 8px; }}
    .spec-info h3 {{ font-size: 18px; margin-bottom: 5px; }}
    .spec-meta {{ font-size: 13px; color: #666; }}
    .toggle-icon {{ font-size: 12px; color: #999; transition: transform 0.3s; }}
    .toggle-icon.expanded {{ transform: rotate(180deg); }}
    .spec-cases {{ display: none; border-top: 1px solid #eee; background: #fafafa; }}
    .spec-cases.expanded {{ display: block; }}
    .cases-list {{ padding: 15px 20px; }}
    .case-item {{ display: flex; align-items: flex-start; gap: 15px; padding: 12px 15px; background: white; border-radius: 8px; margin-bottom: 10px; border-left: 4px solid #2196F3; transition: all 0.2s; }}
    .case-item.checked {{ border-left-color: #4CAF50; background: #E8F5E9; }}
    .case-item.checked .case-title {{ text-decoration: line-through; color: #999; }}
    .case-checkbox {{ padding-top: 3px; }}
    .case-checkbox input {{ width: 18px; height: 18px; cursor: pointer; }}
    .case-content {{ flex: 1; }}
    .case-title {{ font-weight: 600; font-size: 15px; }}
    .case-title a {{ color: #333; text-decoration: none; }}
    .case-title a:hover {{ color: #2196F3; text-decoration: underline; }}
    .case-type {{ font-size: 12px; color: #666; margin-top: 4px; }}
    .badge {{ display: inline-block; padding: 4px 10px; border-radius: 12px; font-size: 12px; font-weight: 600; }}
    .badge-pending {{ background: #FFF3CD; color: #856404; }}
    .badge-passed {{ background: #d4edda; color: #155724; }}
    .badge-failed {{ background: #f8d7da; color: #721c24; }}
    .empty-state {{ text-align: center; padding: 60px 20px; color: #666; }}
    .empty-state .icon {{ font-size: 64px; margin-bottom: 20px; }}
    .footer-notice {{ background: #E3F2FD; border-radius: 8px; padding: 20px; margin-top: 20px; font-size: 14px; }}
    .footer-notice strong {{ color: #1565C0; }}
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🧪 人工测试清单</h1>
      <p class="subtitle">{PROJECT_PATH}</p>
      <div class="stats">
        <div class="stat-item"><div class="count">{total_specs}</div><div class="label">待测功能</div></div>
        <div class="stat-item"><div class="count">{total_cases}</div><div class="label">测试项</div></div>
        <div class="stat-item"><div class="count">{pending_cases}</div><div class="label">待验证</div></div>
        <div class="stat-item"><div class="count">{passed_cases}</div><div class="label">已通过</div></div>
      </div>
    </div>

    <div class="notice">
      💡 点击功能卡片展开测试项，点击测试项可直接打开规范文件。按测试要求完成后，勾选测试项。
    </div>

    <div class="spec-list">
      {''.join(spec_cards) if spec_cards else '<div class="empty-state"><div class="icon">📭</div><h3>暂无待测试功能</h3></div>'}
    </div>

    <div class="footer-notice">
      <strong>⚠️ 测试完成后：</strong>关闭此页面，回复 AI"测试已完成"，AI 将检查测试结果并更新规范状态。
    </div>
  </div>

  <script>
    {status_badge_map}
    
    let testResults = {{}};
    
    function toggleSpec(specId) {{
        const cases = document.getElementById('cases-' + specId);
        const icon = document.getElementById('icon-' + specId);
        
        if (cases.classList.contains('expanded')) {{
            cases.classList.remove('expanded');
            icon.classList.remove('expanded');
        }} else {{
            cases.classList.add('expanded');
            icon.classList.add('expanded');
        }}
    }}
    
    function toggleCase(specId, caseId, isChecked) {{
        const key = specId + '-' + caseId;
        testResults[key] = isChecked ? 'passed' : 'pending';
        
        const item = document.querySelector(`[data-case-id="${{caseId}}"]`);
        if (isChecked) {{
            item.classList.add('checked');
        }} else {{
            item.classList.remove('checked');
        }}
        
        updateStats();
        
        // 保存到 localStorage
        localStorage.setItem('testResults', JSON.stringify(testResults));
    }}
    
    function updateStats() {{
        const items = document.querySelectorAll('.case-item');
        let passed = 0;
        items.forEach(item => {{
            if (item.classList.contains('checked')) passed++;
        }});
        
        const pending = items.length - passed;
        document.querySelector('.stat-item:nth-child(3) .count').textContent = pending;
        document.querySelector('.stat-item:nth-child(4) .count').textContent = passed;
    }}
    
    // 页面加载时恢复状态
    window.addEventListener('DOMContentLoaded', () => {{
        const saved = localStorage.getItem('testResults');
        if (saved) {{
            testResults = JSON.parse(saved);
            Object.entries(testResults).forEach(([key, status]) => {{
                const [specId, caseId] = key.split('-');
                const item = document.querySelector(`[data-case-id="${{caseId}}"]`);
                if (item && status === 'passed') {{
                    item.classList.add('checked');
                    item.querySelector('input').checked = true;
                }}
            }});
            updateStats();
        }}
    }});
    
    // 离开页面提醒
    window.addEventListener('beforeunload', (e) => {{
        const unchecked = document.querySelectorAll('.case-item:not(.checked)');
        if (unchecked.length > 0) {{
            e.preventDefault();
            e.returnValue = '还有 ' + unchecked.length + ' 项测试未完成，确认要离开吗？';
            return e.returnValue;
        }}
    }});
  </script>
</body>
</html>'''
    
    return html

def main():
    print(f"项目路径: {PROJECT_PATH}")
    if SPEC_ID:
        print(f"筛选功能: {SPEC_ID}")
    print()
    
    specs = get_specs()
    print(f"发现 {len(specs)} 个待测试功能")
    
    all_cases = []
    for spec in specs:
        cases = get_test_cases(spec['id'], spec['name'], spec['specPath'])
        print(f"  [{spec['id']}] {spec['name']}: {len(cases)} 个测试项")
        all_cases.extend(cases)
    
    print()
    
    html = generate_html(specs, all_cases)
    
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_FILE.write_text(html, encoding='utf-8')
    print(f"已生成: {OUTPUT_FILE}")
    
    # 保存测试数据到 JSON
    data_file = OUTPUT_DIR / "test-data.json"
    data_file.write_text(json.dumps({
        'specs': specs,
        'cases': all_cases,
        'generatedAt': datetime.now().isoformat()
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f"测试数据: {data_file}")

if __name__ == "__main__":
    main()
