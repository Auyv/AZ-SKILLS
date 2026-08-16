package cli

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

type entryBuilder struct {
	dir      string
	scripts  []string
	disabled []string
}

func newEntry(t *testing.T) *entryBuilder {
	t.Helper()
	return &entryBuilder{dir: t.TempDir()}
}

func (b *entryBuilder) script(id, alias, name, desc, content string) *entryBuilder {
	path := filepath.Join(b.dir, alias+".bat")
	if err := os.WriteFile(path, []byte("@echo off\r\n"+content), 0o644); err != nil {
		panic(err)
	}
	b.scripts = append(b.scripts, "  - id: "+id+"\n    alias: "+alias+"\n    name: "+name+"\n    description: "+desc+"\n    path: "+alias+".bat\n    type: bat\n    encoding: utf-8\n    interactive: false\n")
	return b
}

func (b *entryBuilder) disabledScript(id, alias, name, desc string) *entryBuilder {
	b.disabled = append(b.disabled, "  - id: "+id+"\n    alias: "+alias+"\n    name: "+name+"\n    description: "+desc+"\n    path: missing.bat\n    type: bat\n    enabled: false\n")
	return b
}

func (b *entryBuilder) write(name string) string {
	var body strings.Builder
	body.WriteString("version: 1\n")
	body.WriteString("toolbox:\n  name: 测试集合\n  description: 集成测试入口\n")
	if len(b.scripts) == 0 && len(b.disabled) == 0 {
		body.WriteString("scripts: []\n")
	} else {
		body.WriteString("scripts:\n")
		for _, s := range b.scripts {
			body.WriteString(s)
		}
		for _, s := range b.disabled {
			body.WriteString(s)
		}
	}
	path := filepath.Join(b.dir, name)
	if err := os.WriteFile(path, []byte(body.String()), 0o644); err != nil {
		panic(err)
	}
	return path
}

func runCLI(t *testing.T, args ...string) (int, string, string) {
	t.Helper()
	var stdout, stderr bytes.Buffer
	code := Run(args, strings.NewReader(""), &stdout, &stderr)
	return code, stdout.String(), stderr.String()
}

func TestCheckValid(t *testing.T) {
	path := newEntry(t).script("s1", "hello", "你好", "问候", "echo hi\r\n").write("entry.yaml")
	code, out, errOut := runCLI(t, "--config", path, "check")
	if code != 0 {
		t.Fatalf("exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "配置有效") || !strings.Contains(out, "1 个启用脚本") {
		t.Errorf("check 输出错误: %s", out)
	}
}

func TestListEmpty(t *testing.T) {
	path := newEntry(t).write("entry.yaml")
	code, out, _ := runCLI(t, "--config", path, "list")
	if code != 0 {
		t.Fatalf("exit=%d", code)
	}
	if !strings.Contains(out, "当前没有可用脚本") {
		t.Errorf("空列表应提示: %s", out)
	}
}

func TestListOutput(t *testing.T) {
	entry := newEntry(t)
	entry.script("s1", "hello", "你好", "问候", "echo hi\r\n")
	entry.script("s2", "reg", "回归", "回归测试", "echo ok\r\n")
	entry.disabledScript("s3", "hidden", "隐藏", "不应出现")
	path := entry.write("entry.yaml")
	code, out, errOut := runCLI(t, "--config", path, "list")
	if code != 0 {
		t.Fatalf("exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "/hello") || !strings.Contains(out, "/reg") {
		t.Errorf("缺少脚本: %s", out)
	}
	if strings.Contains(out, "/hidden") {
		t.Errorf("禁用脚本不应出现: %s", out)
	}
	if strings.Contains(out, "★") {
		t.Errorf("初始无收藏不应有标识: %s", out)
	}
}

func TestListShowsFavorite(t *testing.T) {
	entry := newEntry(t)
	entry.script("s1", "hello", "你好", "问候", "echo hi\r\n")
	path := entry.write("entry.yaml")
	abs, _ := filepath.Abs(path)
	favPath := filepath.Join(t.TempDir(), "favs.json")
	favs := loadFavorites(favPath, &bytes.Buffer{})
	if _, err := favs.Toggle(abs, "s1"); err != nil {
		t.Fatal(err)
	}
	favs.Save()
	code, out, errOut := runCLI(t, "--config", path, "--favorites", favPath, "list")
	if code != 0 {
		t.Fatalf("exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "★") {
		t.Errorf("收藏脚本应带标识: %s", out)
	}
}

func TestRunAlias(t *testing.T) {
	path := newEntry(t).script("s1", "hello", "你好", "问候", "echo 运行成功\r\n").write("entry.yaml")
	code, out, errOut := runCLI(t, "--config", path, "run", "hello")
	if code != 0 {
		t.Fatalf("exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "运行成功") {
		t.Errorf("run 输出错误: %s", out)
	}
}

func TestRunUnknownAlias(t *testing.T) {
	path := newEntry(t).script("s1", "hello", "你好", "问候", "echo hi\r\n").write("entry.yaml")
	code, _, errOut := runCLI(t, "--config", path, "run", "nope")
	if code != 3 {
		t.Errorf("exit=%d, want 3", code)
	}
	if !strings.Contains(errOut, "未找到") {
		t.Errorf("stderr=%s", errOut)
	}
}

func TestRunMissingConfig(t *testing.T) {
	code, _, errOut := runCLI(t, "list")
	if code != 2 {
		t.Errorf("exit=%d, want 2", code)
	}
	if !strings.Contains(errOut, "必须指定 --config") {
		t.Errorf("stderr=%s", errOut)
	}
}

func TestRunInvalidEntry(t *testing.T) {
	path := filepath.Join(t.TempDir(), "bad.yaml")
	os.WriteFile(path, []byte("version: 2\n"), 0o644)
	code, _, errOut := runCLI(t, "--config", path, "check")
	if code != 2 {
		t.Errorf("exit=%d, want 2", code)
	}
	if !strings.Contains(errOut, "ERROR") {
		t.Errorf("stderr=%s", errOut)
	}
}

func TestRunAliasMissingInterpreter(t *testing.T) {
	path := newEntry(t).script("s1", "hello", "你好", "问候", "echo x\r\n").write("entry.yaml")
	t.Setenv("PATH", "")
	code, _, errOut := runCLI(t, "--config", path, "run", "hello")
	if code != 4 {
		t.Errorf("exit=%d, want 4; stderr=%s", code, errOut)
	}
}

func TestUsageError(t *testing.T) {
	path := newEntry(t).script("s1", "hello", "你好", "问候", "echo hi\r\n").write("entry.yaml")
	code, _, errOut := runCLI(t, "--config", path, "list", "extra")
	if code != 2 {
		t.Errorf("exit=%d, want 2", code)
	}
	if !strings.Contains(errOut, "ERROR") {
		t.Errorf("stderr=%s", errOut)
	}
}

func TestUnknownCommand(t *testing.T) {
	path := newEntry(t).script("s1", "hello", "你好", "问候", "echo hi\r\n").write("entry.yaml")
	code, _, errOut := runCLI(t, "--config", path, "bogus")
	if code != 2 {
		t.Errorf("exit=%d, want 2", code)
	}
	if !strings.Contains(errOut, "未知命令") {
		t.Errorf("stderr=%s", errOut)
	}
}

func TestHelpFlag(t *testing.T) {
	code, _, _ := runCLI(t, "--help")
	if code != 0 {
		t.Errorf("exit=%d, want 0", code)
	}
}

func TestCancelEntryPrompt(t *testing.T) {
	var stdout bytes.Buffer
	code := Run([]string{}, strings.NewReader("\n"), &stdout, &bytes.Buffer{})
	if code != 5 {
		t.Errorf("exit=%d, want 5", code)
	}
}

func TestNoConfigRequiredForNonTUI(t *testing.T) {
	code, _, errOut := runCLI(t, "check")
	if code != 2 {
		t.Errorf("exit=%d", code)
	}
	if !strings.Contains(errOut, "必须指定 --config") {
		t.Errorf("stderr=%s", errOut)
	}
}
