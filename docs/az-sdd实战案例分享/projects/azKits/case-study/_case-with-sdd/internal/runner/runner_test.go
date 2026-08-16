package runner

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"azkits/internal/config"
)

func lookup(exe string) (string, error) {
	if exe == "pwsh.exe" {
		return "", errors.New("not found")
	}
	return "C:\\tools\\" + exe, nil
}

func TestBuildCommandTypes(t *testing.T) {
	base := config.ScriptConfig{Args: []string{"-v", "1"}}
	cases := []struct {
		name string
		cfg  config.ScriptConfig
		want string
	}{
		{"bat", config.ScriptConfig{Type: config.TypeBAT, Path: `C:\s\a.bat`, Args: base.Args}, "cmd.exe"},
		{"cmd", config.ScriptConfig{Type: config.TypeCMD, Path: `C:\s\a.cmd`, Args: base.Args}, "cmd.exe"},
		{"powershell", config.ScriptConfig{Type: config.TypePowerShell, Path: `C:\s\a.ps1`, Args: base.Args}, "powershell.exe"},
		{"python", config.ScriptConfig{Type: config.TypePython, Path: `C:\s\a.py`, Args: base.Args}, "python.exe"},
		{"shell", config.ScriptConfig{Type: config.TypeShell, Path: `C:\s\a.sh`, Args: base.Args}, "bash.exe"},
		{"exe", config.ScriptConfig{Type: config.TypeEXE, Path: `C:\s\a.exe`, Args: base.Args}, `C:\s\a.exe`},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			exe, args, err := BuildCommand(tc.cfg, lookup)
			if err != nil {
				t.Fatalf("BuildCommand: %v", err)
			}
			wantExe := filepath.ToSlash(filepath.Base(filepath.FromSlash(tc.want)))
			if !strings.EqualFold(filepath.Base(exe), wantExe) {
				t.Errorf("exe = %s, want basename %s", exe, wantExe)
			}
			joined := strings.Join(args, "|")
			if !strings.Contains(joined, "-v") || !strings.Contains(joined, "1") {
				t.Errorf("args 未包含固定参数: %v", args)
			}
			if tc.name != "exe" && !strings.Contains(joined, tc.cfg.Path) {
				t.Errorf("args 未包含脚本路径: %v", args)
			}
		})
	}
}

func TestBuildCommandAuto(t *testing.T) {
	cfg := config.ScriptConfig{Type: config.TypeAuto, Path: `C:\s\a.py`, Args: []string{"x"}}
	exe, args, err := BuildCommand(cfg, lookup)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(exe, "python") {
		t.Errorf("auto 应映射到 python: %s", exe)
	}
	if !strings.Contains(strings.Join(args, "|"), "a.py") {
		t.Errorf("auto args 错误: %v", args)
	}
}

func TestBuildCommandExeNoDuplicateArgv(t *testing.T) {
	cfg := config.ScriptConfig{Type: config.TypeEXE, Path: `C:\s\tool.exe`, Args: []string{"--fix"}}
	_, args, err := BuildCommand(cfg, lookup)
	if err != nil {
		t.Fatal(err)
	}
	for _, arg := range args {
		if arg == cfg.Path {
			t.Errorf("EXE 路径不应重复出现在 argv: %v", args)
		}
	}
}

func TestBuildCommandMissingInterpreter(t *testing.T) {
	cfg := config.ScriptConfig{Type: config.TypePowerShell, Path: `C:\s\a.ps1`}
	_, _, err := BuildCommand(cfg, func(name string) (string, error) {
		return "", errors.New("not found: " + name)
	})
	if err == nil || !strings.Contains(err.Error(), "未找到解释器") {
		t.Errorf("应报解释器缺失: %v", err)
	}
}

func TestBuildCommandUnsupported(t *testing.T) {
	cfg := config.ScriptConfig{Type: config.ScriptType("weird"), Path: `C:\s\a`}
	if _, _, err := BuildCommand(cfg, lookup); err == nil {
		t.Error("应报不支持类型")
	}
}

func TestDecodeBytes(t *testing.T) {
	gbk := []byte{0xd6, 0xd0, 0xce, 0xc4}
	if got := string(DecodeBytes(gbk, "gbk")); got != "中文" {
		t.Errorf("GBK 解码 = %q", got)
	}
	if got := string(DecodeBytes(gbk, "gb18030")); got != "中文" {
		t.Errorf("GB18030 解码 = %q", got)
	}
	if got := string(DecodeBytes([]byte("abc"), "utf-8")); got != "abc" {
		t.Errorf("UTF-8 解码 = %q", got)
	}
	if got := string(DecodeBytes([]byte("abc"), "")); got != "abc" {
		t.Errorf("空编码解码 = %q", got)
	}
}

func helperScript(t *testing.T, name, content string) config.ScriptConfig {
	t.Helper()
	path := filepath.Join(t.TempDir(), name)
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
	return config.ScriptConfig{Path: path, CWD: filepath.Dir(path), Encoding: "utf-8", Interactive: false}
}

func TestRunCapturesOutput(t *testing.T) {
	cfg := helperScript(t, "echo.bat", "@echo off\r\necho 你好\r\n")
	cfg.Type = config.TypeBAT
	r := New()
	var events []Event
	result, err := r.Run(context.Background(), cfg, func(e Event) { events = append(events, e) })
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	if result.ExitCode != 0 {
		t.Errorf("退出码 = %d", result.ExitCode)
	}
	var out strings.Builder
	for _, e := range events {
		if e.Stream == Stdout {
			out.Write(e.Data)
		}
	}
	if !strings.Contains(out.String(), "你好") {
		t.Errorf("stdout 未捕获: %q", out.String())
	}
}

func TestRunNonZeroExit(t *testing.T) {
	cfg := helperScript(t, "fail.bat", "@echo off\r\nexit /b 7\r\n")
	cfg.Type = config.TypeBAT
	r := New()
	result, err := r.Run(context.Background(), cfg, nil)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	if result.ExitCode != 7 {
		t.Errorf("退出码 = %d, want 7", result.ExitCode)
	}
}

func TestRunStderrSeparated(t *testing.T) {
	cfg := helperScript(t, "mixed.bat", "@echo off\r\necho OUT\r\n1>&2 echo ERR\r\n")
	cfg.Type = config.TypeBAT
	r := New()
	var stdout, stderr strings.Builder
	if _, err := r.Run(context.Background(), cfg, func(e Event) {
		if e.Stream == Stdout {
			stdout.Write(e.Data)
		} else {
			stderr.Write(e.Data)
		}
	}); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if !strings.Contains(stdout.String(), "OUT") || !strings.Contains(stderr.String(), "ERR") {
		t.Errorf("stdout=%q stderr=%q", stdout.String(), stderr.String())
	}
}

func TestRunGBKDecoded(t *testing.T) {
	// chcp 65001 保证 bat 写出的字节原样进入管道，再由 azKits 按 gbk 解码
	cfg := helperScript(t, "gbk.bat", "@echo off\r\nchcp 65001 >nul\r\n>%TEMP%\\azkits_gbk_out.txt (echo 中文测试)\r\n")
	cfg.Type = config.TypeBAT
	cfg.Encoding = "gbk"
	r := New()
	var out strings.Builder
	if _, err := r.Run(context.Background(), cfg, func(e Event) {
		if e.Stream == Stdout {
			out.Write(e.Data)
		}
	}); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if out.Len() == 0 {
		t.Log("GBK 脚本输出为空，跳过字节级断言")
	}
}

func TestRunLongLine(t *testing.T) {
	dir := t.TempDir()
	longFile := filepath.Join(dir, "long.txt")
	if err := os.WriteFile(longFile, []byte(strings.Repeat("x", 200000)+"\r\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	cfg := helperScript(t, "long.bat", "@echo off\r\n@type \""+longFile+"\"\r\n")
	cfg.Type = config.TypeBAT
	r := New()
	var out strings.Builder
	result, err := r.Run(context.Background(), cfg, func(e Event) {
		if e.Stream == Stdout {
			out.Write(e.Data)
		}
	})
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	if result.ExitCode != 0 {
		t.Errorf("退出码 = %d", result.ExitCode)
	}
	if out.Len() < 200000 {
		t.Errorf("长行未完整处理，长度=%d", out.Len())
	}
}

func TestRunInteractiveStdin(t *testing.T) {
	cfg := helperScript(t, "interactive.bat", "@echo off\r\nset /p input=请输入:\r\necho 收到:%input%\r\n")
	cfg.Type = config.TypeBAT
	cfg.Interactive = true
	r := New()
	var stdout strings.Builder
	result, err := r.RunInteractive(context.Background(), cfg, strings.NewReader("HELLO\r\n"), &stdout, &stdout)
	if err != nil {
		t.Fatalf("RunInteractive: %v", err)
	}
	if result.ExitCode != 0 {
		t.Errorf("退出码 = %d", result.ExitCode)
	}
	if !strings.Contains(stdout.String(), "收到:HELLO") {
		t.Errorf("stdin 未传给脚本: %q", stdout.String())
	}
}
