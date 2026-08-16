package config

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeEntry(t *testing.T, body string) string {
	t.Helper()
	dir := t.TempDir()
	script := filepath.Join(dir, "脚本.ps1")
	if err := os.WriteFile(script, []byte("Write-Output ok"), 0o600); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(dir, "工具箱.yaml")
	body = strings.ReplaceAll(body, "SCRIPT_PATH", "脚本.ps1")
	if err := os.WriteFile(path, []byte(body), 0o600); err != nil {
		t.Fatal(err)
	}
	return path
}

func TestLoadValidEntryAndResolvePaths(t *testing.T) {
	path := writeEntry(t, `version: 1
toolbox:
  name: 设备工具箱
  description: 中文入口
scripts:
  - id: init
    alias: device-init
    name: 设备初始化
    description: 初始化设备
    path: SCRIPT_PATH
    type: powershell
    args: [COM3, "115200"]
    env: {MODE: test}
    encoding: utf-8
    interactive: true
`)

	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.Toolbox.Name != "设备工具箱" || len(cfg.Scripts) != 1 {
		t.Fatalf("unexpected config: %#v", cfg)
	}
	script := cfg.Scripts[0]
	if !script.Enabled || !script.Interactive || !filepath.IsAbs(script.Path) || script.Args[1] != "115200" {
		t.Fatalf("unexpected script: %#v", script)
	}
	if script.CWD != filepath.Dir(script.Path) {
		t.Fatalf("cwd = %q, want %q", script.CWD, filepath.Dir(script.Path))
	}
}

func TestLoadDefaultsInteractiveToTrueAndAllowsExplicitFalse(t *testing.T) {
	path := writeEntry(t, `version: 1
toolbox: {name: test, description: test}
scripts:
  - {id: one, alias: one, name: one, description: one, path: SCRIPT_PATH}
  - {id: two, alias: two, name: two, description: two, path: SCRIPT_PATH, interactive: false}
`)
	cfg, err := Load(path)
	if err != nil {
		t.Fatal(err)
	}
	if !cfg.Scripts[0].Interactive {
		t.Fatal("interactive must default to true")
	}
	if cfg.Scripts[1].Interactive {
		t.Fatal("explicit interactive false must select background mode")
	}
}

func TestLoadRejectsUnknownField(t *testing.T) {
	path := writeEntry(t, `version: 1
toolbox: {name: test, description: test}
scripts: []
unknown: true
`)
	if _, err := Load(path); err == nil || !strings.Contains(err.Error(), "unknown") {
		t.Fatalf("Load() error = %v, want unknown field", err)
	}
}

func TestLoadRejectsDuplicateAliasAndMissingPath(t *testing.T) {
	path := writeEntry(t, `version: 1
toolbox: {name: test, description: test}
scripts:
  - {id: one, alias: same, name: one, description: one, path: SCRIPT_PATH}
  - {id: two, alias: same, name: two, description: two, path: missing.exe}
`)
	if _, err := Load(path); err == nil || !strings.Contains(err.Error(), "alias") {
		t.Fatalf("Load() error = %v, want duplicate alias", err)
	}
}

func TestLoadRejectsUnsupportedEncodingAndExtension(t *testing.T) {
	path := writeEntry(t, `version: 1
toolbox: {name: test, description: test}
scripts:
  - {id: one, alias: one, name: one, description: one, path: SCRIPT_PATH, encoding: big5}
`)
	if _, err := Load(path); err == nil || !strings.Contains(err.Error(), "encoding") {
		t.Fatalf("Load() error = %v, want encoding error", err)
	}
	bad := filepath.Join(t.TempDir(), "entry.json")
	if err := os.WriteFile(bad, []byte("{}"), 0o600); err != nil {
		t.Fatal(err)
	}
	if _, err := Load(bad); err == nil || !strings.Contains(err.Error(), ".yaml") {
		t.Fatalf("Load() error = %v, want extension error", err)
	}
}
