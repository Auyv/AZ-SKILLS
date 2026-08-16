package config

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeFile(t *testing.T, dir, name, content string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("写入 %s: %v", path, err)
	}
	return path
}

func validEntry(t *testing.T, dir string, extraScripts ...string) string {
	t.Helper()
	if err := os.WriteFile(filepath.Join(dir, "run.bat"), []byte("@echo off\r\necho ok\r\n"), 0o644); err != nil {
		t.Fatalf("写入脚本: %v", err)
	}
	var body strings.Builder
	body.WriteString("version: 1\n")
	body.WriteString("toolbox:\n  name: 测试工具箱\n  description: 用于测试\n")
	body.WriteString("scripts:\n")
	body.WriteString("  - id: s1\n    alias: run\n    name: 运行脚本\n    description: 说明\n    path: run.bat\n    type: bat\n    enabled: true\n    args: [\"a\", \"b\"]\n    encoding: utf-8\n    interactive: false\n")
	for _, item := range extraScripts {
		body.WriteString(item)
	}
	return writeFile(t, dir, "toolbox.yaml", body.String())
}

func TestLoadValid(t *testing.T) {
	dir := t.TempDir()
	path := validEntry(t, dir)
	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("加载失败: %v", err)
	}
	if cfg.Version != 1 {
		t.Errorf("Version = %d", cfg.Version)
	}
	if cfg.Toolbox.Name != "测试工具箱" {
		t.Errorf("Name = %q", cfg.Toolbox.Name)
	}
	if len(cfg.Scripts) != 1 {
		t.Fatalf("Scripts = %d", len(cfg.Scripts))
	}
	s := cfg.Scripts[0]
	if s.ID != "s1" || s.Alias != "run" || s.Encoding != "utf-8" || s.Type != TypeBAT || s.Interactive {
		t.Errorf("脚本字段错误: %+v", s)
	}
	if s.Enabled != true || len(s.Args) != 2 {
		t.Errorf("Enabled=%v Args=%v", s.Enabled, s.Args)
	}
	if !filepath.IsAbs(s.Path) {
		t.Errorf("Path 应为绝对路径: %s", s.Path)
	}
}

func TestLoadDefaults(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "x.sh"), []byte("echo hi\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	path := writeFile(t, dir, "entry.yml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: x.sh\n")
	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("加载失败: %v", err)
	}
	if len(cfg.Scripts) != 1 {
		t.Fatalf("Scripts=%d", len(cfg.Scripts))
	}
	s := cfg.Scripts[0]
	if s.Type != TypeAuto || s.Encoding != "utf-8" || !s.Enabled || !s.Interactive {
		t.Errorf("默认值错误: type=%s enc=%s enabled=%v interactive=%v", s.Type, s.Encoding, s.Enabled, s.Interactive)
	}
}

func TestLoadBOM(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "r.bat"), []byte("@echo off\r\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	content := "\xef\xbb\xbfversion: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: r.bat\n"
	path := filepath.Join(dir, "bom.yaml")
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
	if _, err := Load(path); err != nil {
		t.Fatalf("BOM 入口加载失败: %v", err)
	}
}

func TestLoadRejections(t *testing.T) {
	dir := t.TempDir()
	_ = validEntry(t, dir)

	cases := []struct {
		name    string
		file    string
		content string
		want    string
	}{
		{"非yaml扩展", "case.txt", "version: 1", "只支持 .yaml 或 .yml"},
		{"version非1", "case.yaml", "version: 2\ntoolbox:\n  name: A\n  description: B\nscripts: []", "version 必须为 1"},
		{"缺少toolbox", "case.yaml", "version: 1\nscripts: []", "toolbox.name 和 toolbox.description 必须非空"},
		{"缺少scripts", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\n", "scripts 字段必须存在"},
		{"未知顶层字段", "case.yaml", "version: 1\nfoo: 1\ntoolbox:\n  name: A\n  description: B\nscripts: []", "YAML 配置错误"},
		{"脚本未知字段", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: run.bat\n    zzz: 1\n", "YAML 配置错误"},
		{"脚本缺少必填", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    name: 甲\n    description: 乙\n    path: run.bat\n", "必须非空"},
		{"脚本id重复", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a1\n    name: 甲\n    description: 乙\n    path: run.bat\n  - id: a\n    alias: a2\n    name: 丙\n    description: 丁\n    path: run.bat\n", "id 重复"},
		{"脚本alias重复(大小写)", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: RUN\n    name: 甲\n    description: 乙\n    path: run.bat\n  - id: b\n    alias: run\n    name: 丙\n    description: 丁\n    path: run.bat\n", "alias 重复"},
		{"type不支持", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: run.bat\n    type: weird\n", "type 不支持"},
		{"encoding不支持", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: run.bat\n    encoding: latin1\n", "encoding 不支持"},
		{"path不存在", "case.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: nope.bat\n", "path 不存在"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			path := writeFile(t, dir, tc.file, tc.content)
			_, err := Load(path)
			if err == nil {
				t.Fatalf("应失败但成功: %s", tc.name)
			}
			if !strings.Contains(err.Error(), tc.want) {
				t.Errorf("错误信息 %q 未包含 %q", err.Error(), tc.want)
			}
		})
	}
}

func TestLoadDisabledScriptPathNotChecked(t *testing.T) {
	dir := t.TempDir()
	path := writeFile(t, dir, "entry.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: missing.bat\n    enabled: false\n")
	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("禁用脚本不应检查路径: %v", err)
	}
	if len(cfg.Scripts) != 1 || cfg.Scripts[0].Enabled {
		t.Errorf("禁用脚本加载错误: %+v", cfg.Scripts)
	}
}

func TestLoadChineseAndRelativePath(t *testing.T) {
	dir := t.TempDir()
	sub := filepath.Join(dir, "脚本目录")
	if err := os.MkdirAll(sub, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(sub, "中文脚本.bat"), []byte("@echo off\r\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	path := writeFile(t, dir, "entry.yaml", "version: 1\ntoolbox:\n  name: 中文工具箱\n  description: 中文描述\nscripts:\n  - id: 中文\n    alias: zh\n    name: 中文名称\n    description: 中文简述\n    path: 脚本目录/中文脚本.bat\n    cwd: 脚本目录\n")
	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("中文路径加载失败: %v", err)
	}
	if cfg.Scripts[0].Name != "中文名称" {
		t.Errorf("中文名称错误: %q", cfg.Scripts[0].Name)
	}
	if !strings.Contains(cfg.Scripts[0].Path, "中文脚本.bat") {
		t.Errorf("中文路径错误: %s", cfg.Scripts[0].Path)
	}
}

func TestLoadEnvExpansion(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "r.bat"), []byte("@echo off\r\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	t.Setenv("AZKITS_TEST_DIR", dir)
	path := writeFile(t, dir, "entry.yaml", "version: 1\ntoolbox:\n  name: A\n  description: B\nscripts:\n  - id: a\n    alias: a\n    name: 甲\n    description: 乙\n    path: $AZKITS_TEST_DIR/r.bat\n    env:\n      MODE: test\n    args: [\"1\", \"2\"]\n")
	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("加载失败: %v", err)
	}
	s := cfg.Scripts[0]
	if !strings.Contains(s.Path, "r.bat") {
		t.Errorf("环境变量未展开: %s", s.Path)
	}
	if s.Env["MODE"] != "test" {
		t.Errorf("env 未保留: %v", s.Env)
	}
}
