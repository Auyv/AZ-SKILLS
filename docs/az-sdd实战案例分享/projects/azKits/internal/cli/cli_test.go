package cli

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func entry(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	script := filepath.Join(dir, "demo.bat")
	if err := os.WriteFile(script, []byte("@echo ok\r\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(dir, "entry.yaml")
	body := `version: 1
toolbox: {name: 测试工具箱, description: CLI 测试}
scripts:
  - id: demo
    alias: demo
    name: 示例脚本
    description: 输出 ok
    path: demo.bat
    type: bat
    encoding: utf-8
`
	if err := os.WriteFile(path, []byte(body), 0o600); err != nil {
		t.Fatal(err)
	}
	return path
}

func run(t *testing.T, args ...string) (int, string, string) {
	t.Helper()
	var stdout, stderr bytes.Buffer
	code := Run(args, strings.NewReader(""), &stdout, &stderr)
	return code, stdout.String(), stderr.String()
}

func TestCheckAndListUseSameEntry(t *testing.T) {
	path := entry(t)
	code, out, stderr := run(t, "--config", path, "check")
	if code != 0 || !strings.Contains(out, "配置有效") || stderr != "" {
		t.Fatalf("check code=%d out=%q err=%q", code, out, stderr)
	}
	code, out, stderr = run(t, "--config", path, "list")
	if code != 0 || !strings.Contains(out, "测试工具箱") || !strings.Contains(out, "/demo") || stderr != "" {
		t.Fatalf("list code=%d out=%q err=%q", code, out, stderr)
	}
}

func TestRunAndErrors(t *testing.T) {
	path := entry(t)
	code, out, stderr := run(t, "--config", path, "run", "demo")
	if code != 0 || !strings.Contains(out, "ok") || stderr != "" {
		t.Fatalf("run code=%d out=%q err=%q", code, out, stderr)
	}
	code, _, stderr = run(t, "--config", path, "run", "missing")
	if code != 3 || !strings.Contains(stderr, "未找到") {
		t.Fatalf("missing code=%d err=%q", code, stderr)
	}
}

func TestRunInteractiveScriptUsesCallerIO(t *testing.T) {
	dir := t.TempDir()
	script := filepath.Join(dir, "interactive.bat")
	if err := os.WriteFile(script, []byte("@set /p value=prompt:\r\n@echo got:%value%\r\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(dir, "entry.yaml")
	body := `version: 1
toolbox: {name: 交互工具箱, description: CLI 交互测试}
scripts:
  - id: interactive
    alias: interactive
    name: 交互脚本
    description: 读取输入
    path: interactive.bat
    type: bat
    interactive: true
`
	if err := os.WriteFile(path, []byte(body), 0o600); err != nil {
		t.Fatal(err)
	}
	var stdout, stderr bytes.Buffer
	code := Run([]string{"--config", path, "run", "interactive"}, strings.NewReader("hello\r\n"), &stdout, &stderr)
	if code != 0 || !strings.Contains(stdout.String(), "got:hello") || stderr.Len() != 0 {
		t.Fatalf("code=%d stdout=%q stderr=%q", code, stdout.String(), stderr.String())
	}
}

func TestInvalidArgumentsReturnTwo(t *testing.T) {
	code, _, stderr := run(t, "list")
	if code != 2 || !strings.Contains(stderr, "--config") {
		t.Fatalf("code=%d err=%q", code, stderr)
	}
	code, _, stderr = run(t, "--config", "missing.yaml", "check")
	if code != 2 || stderr == "" {
		t.Fatalf("code=%d err=%q", code, stderr)
	}
}

func TestHelpCancelAndUnknownCommand(t *testing.T) {
	code, _, stderr := run(t, "--help")
	if code != 0 || !strings.Contains(stderr, "用法") {
		t.Fatalf("help code=%d err=%q", code, stderr)
	}
	code, out, stderr := run(t)
	if code != 5 || !strings.Contains(out, "YAML") || stderr != "" {
		t.Fatalf("cancel code=%d out=%q err=%q", code, out, stderr)
	}
	path := entry(t)
	code, _, stderr = run(t, "--config", path, "unknown")
	if code != 2 || !strings.Contains(stderr, "未知命令") {
		t.Fatalf("unknown code=%d err=%q", code, stderr)
	}
	code, _, stderr = run(t, "--config", path, "list", "extra")
	if code != 2 || !strings.Contains(stderr, "额外参数") {
		t.Fatalf("extra code=%d err=%q", code, stderr)
	}
}
