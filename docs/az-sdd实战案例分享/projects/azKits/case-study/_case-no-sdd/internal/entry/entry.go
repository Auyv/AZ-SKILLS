package entry

import (
	"fmt"
	"path/filepath"
	"strings"

	yaml "go.yaml.in/yaml/v4"
)

// StringArray is a []string that rejects non-string YAML scalars during
// decoding. This enforces the "args 只能是字符串数组" constraint strictly.
type StringArray []string

// UnmarshalYAML implements yaml.Unmarshaler and only accepts sequences whose
// elements are all plain or quoted strings.
func (a *StringArray) UnmarshalYAML(node *yaml.Node) error {
	if node.Kind != yaml.SequenceNode {
		return fmt.Errorf("args 必须是字符串数组")
	}
	out := make([]string, 0, len(node.Content))
	for _, item := range node.Content {
		if item.Kind != yaml.ScalarNode {
			return fmt.Errorf("args 中的元素必须是字符串")
		}
		if item.Tag != "!!str" {
			return fmt.Errorf("args 中的元素 %q 必须是字符串，不能是数字或布尔值", item.Value)
		}
		out = append(out, item.Value)
	}
	*a = out
	return nil
}

// Encoding defines the output encoding of a script's console output.
type Encoding string

const (
	EncodingUTF8    Encoding = "utf-8"
	EncodingUTF8Sig Encoding = "utf-8-sig"
	EncodingGBK     Encoding = "gbk"
	EncodingGB18030 Encoding = "gb18030"
)

// Type defines how a script is launched.
type Type string

const (
	TypeAuto       Type = "auto"
	TypeBat        Type = "bat"
	TypeCmd        Type = "cmd"
	TypePowerShell Type = "powershell"
	TypePython     Type = "python"
	TypeShell      Type = "shell"
	TypeExe        Type = "exe"
)

// SupportedEncodings is the set of accepted encoding values.
var SupportedEncodings = map[Encoding]bool{
	EncodingUTF8:    true,
	EncodingUTF8Sig: true,
	EncodingGBK:     true,
	EncodingGB18030: true,
}

// SupportedTypes is the set of accepted type values.
var SupportedTypes = map[Type]bool{
	TypeAuto:       true,
	TypeBat:        true,
	TypeCmd:        true,
	TypePowerShell: true,
	TypePython:     true,
	TypeShell:      true,
	TypeExe:        true,
}

// Script is a single runnable script declared in the YAML entry.
type Script struct {
	ID          string   `yaml:"id"`
	Alias       string   `yaml:"alias"`
	Name        string   `yaml:"name"`
	Description string   `yaml:"description"`
	Path        string   `yaml:"path"`
	Type        Type     `yaml:"type"`
	Encoding    Encoding `yaml:"encoding"`
	Interactive bool     `yaml:"interactive"`
	Enabled     *bool       `yaml:"enabled"`
	Args        StringArray `yaml:"args"`
}

// IsEnabled reports whether the script is enabled. The YAML field defaults to
// true when absent.
func (s Script) IsEnabled() bool {
	if s.Enabled == nil {
		return true
	}
	return *s.Enabled
}

// Entry is a parsed YAML toolkit entry.
type Entry struct {
	Version     int      `yaml:"version"`
	Name        string   `yaml:"name"`
	Description string   `yaml:"description"`
	Scripts     []Script `yaml:"scripts"`

	FilePath string `yaml:"-"`
	BaseDir  string `yaml:"-"`

	Issues []Issue `yaml:"-"`
}

// Issue is a validation result attached to an entry.
type Issue struct {
	Severity Severity
	Code     string
	Msg      string
	Script   int
}

// Severity describes how an issue affects usability.
type Severity int

const (
	// SeverityBlocking prevents the entry from being used at all.
	SeverityBlocking Severity = iota
	// SeverityWarning affects a single script but does not block the entry.
	SeverityWarning
	// SeverityInfo is informational.
	SeverityInfo
)

func (s Severity) String() string {
	switch s {
	case SeverityBlocking:
		return "阻塞"
	case SeverityWarning:
		return "警告"
	default:
		return "提示"
	}
}

// Blocking returns the blocking issues of the entry.
func (e *Entry) Blocking() []Issue {
	var out []Issue
	for _, is := range e.Issues {
		if is.Severity == SeverityBlocking {
			out = append(out, is)
		}
	}
	return out
}

// Warnings returns non-blocking issues of the entry.
func (e *Entry) Warnings() []Issue {
	var out []Issue
	for _, is := range e.Issues {
		if is.Severity != SeverityBlocking {
			out = append(out, is)
		}
	}
	return out
}

// ResolvePath resolves the script path relative to the entry file directory.
// The result is always absolute, so it stays correct when the subprocess runs
// with the entry directory as its working directory.
func (e *Entry) ResolvePath(s *Script) string {
	if s == nil || s.Path == "" {
		return ""
	}
	p := s.Path
	if !filepath.IsAbs(p) {
		p = filepath.Join(e.BaseDir, p)
	}
	if abs, err := filepath.Abs(p); err == nil {
		return filepath.Clean(abs)
	}
	return filepath.Clean(p)
}

// FindByAlias returns the unique enabled script matching the alias. It returns
// an error when there is no unique match.
func (e *Entry) FindByAlias(alias string) (*Script, error) {
	alias = strings.ToLower(strings.TrimSpace(alias))
	if alias == "" {
		return nil, fmt.Errorf("别名不能为空")
	}
	var found *Script
	for i := range e.Scripts {
		s := &e.Scripts[i]
		if strings.ToLower(s.Alias) == alias {
			if found != nil {
				return nil, fmt.Errorf("别名 %q 匹配到多个脚本", alias)
			}
			found = s
		}
	}
	if found == nil {
		return nil, fmt.Errorf("未找到别名 %q 对应的脚本", alias)
	}
	if !found.IsEnabled() {
		return nil, fmt.Errorf("脚本 %q 已禁用，无法运行", alias)
	}
	return found, nil
}

// EnabledScripts returns all enabled scripts.
func (e *Entry) EnabledScripts() []Script {
	var out []Script
	for _, s := range e.Scripts {
		if s.IsEnabled() {
			out = append(out, s)
		}
	}
	return out
}
