#!/usr/bin/env python3
"""
生成规范审查 HTML 页面
用法: python generate-review-html.py [项目路径]
"""

import re
import sys
from pathlib import Path

PROJECT_PATH = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
SPEC_DIR = PROJECT_PATH / "az-sdd" / "spec"
OUTPUT_FILE = PROJECT_PATH / "az-sdd" / "spec" / "references" / "spec-review.html"

STATUS_MAP = {
    '草稿': 'badge-draft',
    '待审核': 'badge-reviewing', 
    '审核中': 'badge-reviewing',
    '已审核': 'badge-approved',
    '已批准': 'badge-approved',
    '阻塞': 'badge-blocked',
}

def get_specs():
    specs = []
    
    # 从 SPEC.md 读取
    spec_md = PROJECT_PATH / "az-sdd" / "SPEC.md"
    if spec_md.exists():
        content = spec_md.read_text(encoding='utf-8')
        for line in content.split('\n'):
            if '|' in line and not line.startswith('|--') and not line.startswith('| -'):
                cells = [c.strip() for c in line.split('|') if c.strip()]
                if len(cells) >= 5 and (cells[0].isdigit() or re.match(r'^000\d+$', cells[0])):
                    status = cells[4] if len(cells) > 4 else '草稿'
                    # 映射状态
                    mapped = status
                    if status in ['草稿', '待审核', '审核中']:
                        mapped = '待审核'
                    elif status in ['已审核', '已批准']:
                        mapped = '已审核'
                    
                    specs.append({
                        'id': cells[0],
                        'name': cells[1],
                        'module': cells[2] if len(cells) > 2 else '',
                        'status': mapped,
                        'filePath': str(SPEC_DIR / f"{cells[0]}-{cells[1]}.spec.md")
                    })
    
    # 备用：从 spec 目录扫描
    if not specs and SPEC_DIR.exists():
        for f in SPEC_DIR.glob("*.spec.md"):
            m = re.match(r'^(\d+)-(.+)\.spec\.md$', f.name)
            if m:
                content = f.read_text(encoding='utf-8')
                status = '待审核'
                if '| 状态 |' in content:
                    s = re.search(r'\|\s*状态\s*\|\s*([^|\n]+)\s*\|', content)
                    if s:
                        st = s.group(1).strip()
                        if st in ['已审核', '已批准']:
                            status = '已审核'
                specs.append({
                    'id': m.group(1),
                    'name': m.group(2),
                    'module': '',
                    'status': status,
                    'filePath': str(f)
                })
    
    return specs

def main():
    specs = get_specs()
    
    total = len(specs)
    draft = sum(1 for s in specs if s['status'] == '待审核')
    approved = sum(1 for s in specs if s['status'] == '已审核')
    
    rows = []
    for s in specs:
        status_class = STATUS_MAP.get(s['status'], 'badge-draft')
        file_url = f"vscode://file/{s['filePath'].replace(chr(92), '/')}"
        
        rows.append(f'''
        <div class="spec-card">
            <div class="spec-id">{s['id']}</div>
            <div class="spec-info">
                <h3><a href="{file_url}" onclick="event.stopPropagation()">{s['name']}</a></h3>
                <div class="spec-meta">
                    <span>📁 {s.get('module', '-')}</span>
                </div>
            </div>
            <span class="badge {status_class}">{s['status']}</span>
        </div>''')
    
    html = f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>功能规范审查 - {PROJECT_PATH.name}</title>
  <style>
    * {{ margin: 0; padding: 0; box-sizing: border-box; }}
    body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f0f2f5; color: #333; line-height: 1.6; padding: 20px; }}
    .container {{ max-width: 1200px; margin: 0 auto; }}
    .header {{ background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 12px; margin-bottom: 30px; }}
    .header h1 {{ font-size: 28px; margin-bottom: 10px; }}
    .header .subtitle {{ opacity: 0.9; font-size: 14px; word-break: break-all; }}
    .stats {{ display: flex; gap: 20px; margin-top: 20px; flex-wrap: wrap; }}
    .stat-item {{ background: rgba(255,255,255,0.2); padding: 10px 20px; border-radius: 8px; text-align: center; }}
    .stat-item .count {{ font-size: 24px; font-weight: bold; }}
    .stat-item .label {{ font-size: 12px; opacity: 0.9; }}
    .spec-grid {{ display: grid; gap: 15px; }}
    .spec-card {{ background: white; border-radius: 12px; padding: 20px; display: grid; grid-template-columns: 80px 1fr auto; gap: 20px; align-items: center; box-shadow: 0 2px 8px rgba(0,0,0,0.05); transition: all 0.2s; cursor: pointer; }}
    .spec-card:hover {{ box-shadow: 0 4px 16px rgba(0,0,0,0.1); transform: translateY(-2px); background: #fafafa; }}
    .spec-id {{ font-size: 14px; font-weight: 600; color: #667eea; text-align: center; padding: 8px; background: #f0f2ff; border-radius: 8px; }}
    .spec-info h3 {{ font-size: 18px; margin-bottom: 8px; }}
    .spec-info h3 a {{ color: #333; text-decoration: none; }}
    .spec-info h3 a:hover {{ color: #667eea; }}
    .spec-meta {{ font-size: 13px; color: #666; }}
    .badge {{ display: inline-block; padding: 6px 14px; border-radius: 20px; font-size: 13px; font-weight: 600; }}
    .badge-draft {{ background: #e0e0e0; color: #666; }}
    .badge-reviewing {{ background: #fff3cd; color: #856404; }}
    .badge-approved {{ background: #d4edda; color: #155724; }}
    .badge-blocked {{ background: #f8d7da; color: #721c24; }}
    .notice {{ background: #fff3cd; border: 1px solid #ffc107; border-radius: 8px; padding: 15px 20px; margin-bottom: 20px; font-size: 14px; color: #856404; display: flex; align-items: center; gap: 10px; }}
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>📋 功能规范审查</h1>
      <p class="subtitle">{PROJECT_PATH}</p>
      <div class="stats">
        <div class="stat-item"><div class="count">{total}</div><div class="label">全部</div></div>
        <div class="stat-item"><div class="count">{draft}</div><div class="label">待审核</div></div>
        <div class="stat-item"><div class="count">{approved}</div><div class="label">已审核</div></div>
      </div>
    </div>

    <div class="notice" id="notice">
      ⚠️ 重要提示：审核完成后，请在 VSCode 中打开对应的 .spec.md 文件，将状态改为"已审核"并添加审核记录。
    </div>

    <div class="spec-grid">
      {''.join(rows) if rows else '<p style="text-align:center;padding:60px;color:#666;">暂无规范</p>'}
    </div>
  </div>

  <script>
    const pendingCount = {draft};
    
    window.addEventListener('beforeunload', function(e) {{
      if (pendingCount > 0) {{
        e.preventDefault();
        e.returnValue = '还有 ' + pendingCount + ' 个规范待审核，确认要离开吗？';
        return e.returnValue;
      }}
    }});
    
    document.querySelectorAll('.spec-card').forEach(card => {{
      card.addEventListener('click', function() {{
        const link = this.querySelector('a');
        if (link) link.click();
      }});
    }});
  </script>
</body>
</html>'''
    
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_FILE.write_text(html, encoding='utf-8')
    print(f"已生成: {OUTPUT_FILE}")
    print(f"总计: {total} 个规范，{draft} 个待审核，{approved} 个已审核")

if __name__ == "__main__":
    main()
