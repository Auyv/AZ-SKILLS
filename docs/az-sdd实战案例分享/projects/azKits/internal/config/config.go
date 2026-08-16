package config

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"go.yaml.in/yaml/v4"
)

type ScriptType string

const (
	TypeAuto       ScriptType = "auto"
	TypeBAT        ScriptType = "bat"
	TypeCMD        ScriptType = "cmd"
	TypePowerShell ScriptType = "powershell"
	TypePython     ScriptType = "python"
	TypeShell      ScriptType = "shell"
	TypeEXE        ScriptType = "exe"
)

type ToolboxMeta struct {
	Name        string `yaml:"name"`
	Description string `yaml:"description"`
}

type ScriptConfig struct {
	ID          string
	Alias       string
	Name        string
	Description string
	Path        string
	Type        ScriptType
	Enabled     bool
	Args        []string
	CWD         string
	Env         map[string]string
	Encoding    string
	Interactive bool
}

type ToolboxConfig struct {
	Version int
	Source  string
	Toolbox ToolboxMeta
	Scripts []ScriptConfig
}

type rawConfig struct {
	Version int         `yaml:"version"`
	Toolbox ToolboxMeta `yaml:"toolbox"`
	Scripts []rawScript `yaml:"scripts"`
}

type rawScript struct {
	ID          string            `yaml:"id"`
	Alias       string            `yaml:"alias"`
	Name        string            `yaml:"name"`
	Description string            `yaml:"description"`
	Path        string            `yaml:"path"`
	Type        ScriptType        `yaml:"type"`
	Enabled     *bool             `yaml:"enabled"`
	Args        []string          `yaml:"args"`
	CWD         string            `yaml:"cwd"`
	Env         map[string]string `yaml:"env"`
	Encoding    string            `yaml:"encoding"`
	Interactive *bool             `yaml:"interactive"`
}

func Load(path string) (ToolboxConfig, error) {
	ext := strings.ToLower(filepath.Ext(path))
	if ext != ".yaml" && ext != ".yml" {
		return ToolboxConfig{}, fmt.Errorf("入口只支持 .yaml 或 .yml: %s", path)
	}
	abs, err := filepath.Abs(path)
	if err != nil {
		return ToolboxConfig{}, fmt.Errorf("解析入口路径: %w", err)
	}
	data, err := os.ReadFile(abs)
	if err != nil {
		return ToolboxConfig{}, fmt.Errorf("读取入口 %s: %w", abs, err)
	}
	data = bytes.TrimPrefix(data, []byte{0xef, 0xbb, 0xbf})
	var raw rawConfig
	if err := yaml.Load(data, &raw, yaml.WithKnownFields()); err != nil {
		return ToolboxConfig{}, fmt.Errorf("YAML 配置错误: %w", err)
	}
	if raw.Version != 1 {
		return ToolboxConfig{}, fmt.Errorf("version 必须为 1，实际为 %d", raw.Version)
	}
	raw.Toolbox.Name = strings.TrimSpace(raw.Toolbox.Name)
	raw.Toolbox.Description = strings.TrimSpace(raw.Toolbox.Description)
	if raw.Toolbox.Name == "" || raw.Toolbox.Description == "" {
		return ToolboxConfig{}, fmt.Errorf("toolbox.name 和 toolbox.description 必须非空")
	}
	if raw.Scripts == nil {
		return ToolboxConfig{}, fmt.Errorf("scripts 字段必须存在")
	}

	result := ToolboxConfig{Version: raw.Version, Source: abs, Toolbox: raw.Toolbox, Scripts: make([]ScriptConfig, 0, len(raw.Scripts))}
	ids := make(map[string]struct{}, len(raw.Scripts))
	aliases := make(map[string]struct{}, len(raw.Scripts))
	base := filepath.Dir(abs)
	for i, item := range raw.Scripts {
		prefix := fmt.Sprintf("scripts[%d]", i)
		item.ID = strings.TrimSpace(item.ID)
		item.Alias = strings.TrimSpace(item.Alias)
		item.Name = strings.TrimSpace(item.Name)
		item.Description = strings.TrimSpace(item.Description)
		item.Path = strings.TrimSpace(item.Path)
		if item.ID == "" || item.Alias == "" || item.Name == "" || item.Description == "" || item.Path == "" {
			return ToolboxConfig{}, fmt.Errorf("%s 的 id、alias、name、description、path 必须非空", prefix)
		}
		aliasKey := strings.ToLower(item.Alias)
		if _, ok := ids[item.ID]; ok {
			return ToolboxConfig{}, fmt.Errorf("%s.id 重复: %s", prefix, item.ID)
		}
		if _, ok := aliases[aliasKey]; ok {
			return ToolboxConfig{}, fmt.Errorf("%s.alias 重复: %s", prefix, item.Alias)
		}
		ids[item.ID] = struct{}{}
		aliases[aliasKey] = struct{}{}

		typ := item.Type
		if typ == "" {
			typ = TypeAuto
		}
		if !validType(typ) {
			return ToolboxConfig{}, fmt.Errorf("%s.type 不支持: %s", prefix, typ)
		}
		encoding := strings.ToLower(strings.TrimSpace(item.Encoding))
		if encoding == "" {
			encoding = "utf-8"
		}
		if !validEncoding(encoding) {
			return ToolboxConfig{}, fmt.Errorf("%s.encoding 不支持: %s", prefix, encoding)
		}
		enabled := true
		if item.Enabled != nil {
			enabled = *item.Enabled
		}
		interactive := true
		if item.Interactive != nil {
			interactive = *item.Interactive
		}
		scriptPath := resolve(base, item.Path)
		cwd := filepath.Dir(scriptPath)
		if item.CWD != "" {
			cwd = resolve(base, item.CWD)
		}
		if enabled {
			info, statErr := os.Stat(scriptPath)
			if statErr != nil || !info.Mode().IsRegular() {
				return ToolboxConfig{}, fmt.Errorf("%s.path 不存在或不是文件: %s", prefix, scriptPath)
			}
			if info, statErr = os.Stat(cwd); statErr != nil || !info.IsDir() {
				return ToolboxConfig{}, fmt.Errorf("%s.cwd 不存在或不是目录: %s", prefix, cwd)
			}
		}
		result.Scripts = append(result.Scripts, ScriptConfig{
			ID: item.ID, Alias: item.Alias, Name: item.Name, Description: item.Description,
			Path: scriptPath, Type: typ, Enabled: enabled, Args: append([]string(nil), item.Args...),
			CWD: cwd, Env: cloneMap(item.Env), Encoding: encoding, Interactive: interactive,
		})
	}
	return result, nil
}

func resolve(base, value string) string {
	value = os.ExpandEnv(value)
	if !filepath.IsAbs(value) {
		value = filepath.Join(base, value)
	}
	abs, err := filepath.Abs(value)
	if err != nil {
		return filepath.Clean(value)
	}
	return filepath.Clean(abs)
}

func validType(value ScriptType) bool {
	switch value {
	case TypeAuto, TypeBAT, TypeCMD, TypePowerShell, TypePython, TypeShell, TypeEXE:
		return true
	default:
		return false
	}
}

func validEncoding(value string) bool {
	switch value {
	case "utf-8", "utf-8-sig", "gbk", "gb18030":
		return true
	default:
		return false
	}
}

func cloneMap(source map[string]string) map[string]string {
	if source == nil {
		return map[string]string{}
	}
	result := make(map[string]string, len(source))
	for key, value := range source {
		result[key] = value
	}
	return result
}
