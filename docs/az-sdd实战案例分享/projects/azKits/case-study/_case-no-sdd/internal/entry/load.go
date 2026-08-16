package entry

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	yaml "go.yaml.in/yaml/v4"
)

const MaxYAMLBytes = 8 << 20

// LoadFile reads, parses and validates a YAML entry file.
//
// Relative script paths are resolved against the entry file directory. The
// returned entry always has fully populated Issues; callers must inspect
// e.Blocking() before using the entry for listing or running.
func LoadFile(path string) (*Entry, error) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("读取入口文件失败: %w", err)
	}
	if len(raw) > MaxYAMLBytes {
		return nil, fmt.Errorf("入口文件过大（超过 %d 字节）", MaxYAMLBytes)
	}
	e, err := Parse(raw, path)
	if err != nil {
		return nil, err
	}
	return e, nil
}

// Parse parses and validates entry YAML bytes. The filePath is used to resolve
// relative script paths.
func Parse(raw []byte, filePath string) (*Entry, error) {
	raw = bytes.TrimPrefix(raw, []byte{0xEF, 0xBB, 0xBF})

	var e Entry
	if err := yaml.Load(raw, &e, yaml.WithKnownFields()); err != nil {
		return nil, fmt.Errorf("YAML 解析失败: %w", err)
	}

	e.FilePath = filePath
	if filePath != "" {
		e.BaseDir = filepath.Dir(filePath)
	}

	validate(&e)
	return &e, nil
}

// validate fills e.Issues with validation results.
func validate(e *Entry) {
	e.Issues = nil
	scriptLabel := func(idx int) string {
		if idx < 0 || idx >= len(e.Scripts) {
			return ""
		}
		s := e.Scripts[idx]
		if s.Alias != "" {
			return fmt.Sprintf("[%s]", s.Alias)
		}
		if s.ID != "" {
			return fmt.Sprintf("(id=%s)", s.ID)
		}
		return fmt.Sprintf("(scripts[%d])", idx)
	}
	add := func(sev Severity, code, msg string, idx int) {
		e.Issues = append(e.Issues, Issue{Severity: sev, Code: code, Msg: msg, Script: idx})
	}

	if e.Version != 1 {
		add(SeverityBlocking, "bad-version", fmt.Sprintf("version 必须是 1，当前为 %d", e.Version), -1)
	}
	if strings.TrimSpace(e.Name) == "" {
		add(SeverityInfo, "no-name", "入口未设置 name", -1)
	}
	if len(e.Scripts) == 0 {
		add(SeverityInfo, "no-scripts", "入口没有声明任何脚本", -1)
	}

	seenIDs := map[string]int{}
	seenAliases := map[string]int{}
	for i := range e.Scripts {
		s := &e.Scripts[i]
		if strings.TrimSpace(s.ID) == "" {
			add(SeverityBlocking, "missing-id", "脚本缺少 id", i)
		} else if prev, dup := seenIDs[s.ID]; dup {
			add(SeverityBlocking, "dup-id", fmt.Sprintf("id %q 与脚本 %s 重复", s.ID, scriptLabel(prev)), i)
		} else {
			seenIDs[s.ID] = i
		}

		if strings.TrimSpace(s.Alias) == "" {
			add(SeverityBlocking, "missing-alias", "脚本缺少 alias", i)
		} else {
			key := strings.ToLower(s.Alias)
			if prev, dup := seenAliases[key]; dup {
				add(SeverityBlocking, "dup-alias", fmt.Sprintf("alias %q 与脚本 %s 重复（忽略大小写）", s.Alias, scriptLabel(prev)), i)
			} else {
				seenAliases[key] = i
			}
		}

		if s.Type == "" {
			s.Type = TypeAuto
		}
		if !SupportedTypes[s.Type] {
			add(SeverityBlocking, "bad-type", fmt.Sprintf("type %q 不受支持，支持: auto, bat, cmd, powershell, python, shell, exe", s.Type), i)
		}

		if s.Encoding == "" {
			s.Encoding = defaultEncoding(s.Type)
		}
		if !SupportedEncodings[s.Encoding] {
			add(SeverityBlocking, "bad-encoding", fmt.Sprintf("encoding %q 不受支持，支持: utf-8, utf-8-sig, gbk, gb18030", s.Encoding), i)
		}

		for _, a := range s.Args {
			if strings.ContainsRune(a, '\x00') {
				add(SeverityBlocking, "bad-arg", "args 中不允许包含 NUL 字符", i)
				break
			}
		}

		if !s.IsEnabled() {
			add(SeverityInfo, "disabled", fmt.Sprintf("脚本 %s 已禁用", scriptLabel(i)), i)
			continue
		}

		for field, v := range map[string]string{
			"name":        s.Name,
			"description": s.Description,
			"path":        s.Path,
		} {
			if strings.TrimSpace(v) == "" {
				add(SeverityBlocking, "missing-"+field, fmt.Sprintf("启用脚本 %s 的 %s 不能为空", scriptLabel(i), field), i)
			}
		}

		if s.Path == "" {
			continue
		}
		typ := s.Type
		if typ == TypeAuto {
			typ = InferType(s.Path)
			if typ == "" {
				add(SeverityBlocking, "bad-auto-type", fmt.Sprintf("无法从路径 %q 推断脚本 %s 的 type，请显式指定", s.Path, scriptLabel(i)), i)
			}
		}
		abs := e.ResolvePath(s)
		if _, err := os.Stat(abs); err != nil {
			if os.IsNotExist(err) {
				add(SeverityWarning, "missing-file", fmt.Sprintf("脚本 %s 的文件不存在: %s", scriptLabel(i), abs), i)
			} else {
				add(SeverityWarning, "file-error", fmt.Sprintf("无法访问脚本 %s 的文件: %v", scriptLabel(i), err), i)
			}
			continue
		}
	}

	sort.SliceStable(e.Issues, func(a, b int) bool {
		if e.Issues[a].Script != e.Issues[b].Script {
			return e.Issues[a].Script < e.Issues[b].Script
		}
		return e.Issues[a].Severity > e.Issues[b].Severity
	})
}

// defaultEncoding returns the default output encoding for a script type.
func defaultEncoding(t Type) Encoding {
	return EncodingUTF8
}

// InferType maps a file extension to a script type, or "" when unknown.
func InferType(path string) Type {
	ext := strings.ToLower(filepath.Ext(path))
	switch ext {
	case ".bat", ".cmd":
		return TypeCmd
	case ".ps1":
		return TypePowerShell
	case ".py":
		return TypePython
	case ".sh":
		return TypeShell
	case ".exe", ".com":
		return TypeExe
	default:
		return ""
	}
}
