package cli

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"azkits/internal/favorite"
)

// buildBinary 编译当前包为可执行文件，供进程级集成测试使用。
func buildBinary(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	bin := filepath.Join(dir, "azkits-test.exe")
	cmd := exec.Command("go", "build", "-o", bin, "azkits/cmd/azkits")
	cmd.Dir = "../.."
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("构建失败: %v\n%s", err, output)
	}
	return bin
}

func runBinary(t *testing.T, bin string, args ...string) (int, string, string) {
	t.Helper()
	cmd := exec.Command(bin, args...)
	var stdout, stderr strings.Builder
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	err := cmd.Run()
	code := 0
	if exitErr, ok := err.(*exec.ExitError); ok {
		code = exitErr.ExitCode()
	} else if err != nil {
		t.Fatalf("运行失败: %v", err)
	}
	return code, stdout.String(), stderr.String()
}

// TestProcessListAndRun 验证真实进程的 list 与 run。
func TestProcessListAndRun(t *testing.T) {
	bin := buildBinary(t)
	entry := newEntry(t).script("s1", "hello", "你好", "问候", "echo 进程运行成功\r\n").write("entry.yaml")
	code, out, errOut := runBinary(t, bin, "--config", entry, "list")
	if code != 0 {
		t.Fatalf("list exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "/hello") {
		t.Errorf("list 输出: %s", out)
	}
	code, out, errOut = runBinary(t, bin, "--config", entry, "run", "hello")
	if code != 0 {
		t.Fatalf("run exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "进程运行成功") {
		t.Errorf("run 输出: %s", out)
	}
}

// TestProcessFavoritesPersistence 验证收藏在两个独立进程间保持一致。
func TestProcessFavoritesPersistence(t *testing.T) {
	bin := buildBinary(t)
	entry := newEntry(t).script("s1", "hello", "你好", "问候", "echo x\r\n").write("entry.yaml")
	favPath := filepath.Join(t.TempDir(), "favs.json")
	abs, _ := filepath.Abs(entry)

	store, err := favorite.Load(favPath)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := store.Toggle(abs, "s1"); err != nil {
		t.Fatal(err)
	}

	// 第二个独立进程：list 应显示收藏标识
	code, out, errOut := runBinary(t, bin, "--config", entry, "--favorites", favPath, "list")
	if code != 0 {
		t.Fatalf("list exit=%d stderr=%s", code, errOut)
	}
	if !strings.Contains(out, "★") {
		t.Errorf("第二个进程应看到收藏标识: %s", out)
	}
}

// TestProcessIsolation 验证两个入口进程互不干扰。
func TestProcessIsolation(t *testing.T) {
	bin := buildBinary(t)
	newEntry(t).script("s1", "hello", "你好", "问候", "echo x\r\n").write("entryA.yaml")
	entryB := newEntry(t).script("s2", "other", "其他", "别的", "echo y\r\n").write("entryB.yaml")
	code, out, _ := runBinary(t, bin, "--config", entryB, "list")
	if code != 0 {
		t.Fatalf("exit=%d", code)
	}
	if strings.Contains(out, "/hello") {
		t.Errorf("B 入口不应看到 A 的脚本: %s", out)
	}
	if !strings.Contains(out, "/other") {
		t.Errorf("B 入口应显示自身脚本: %s", out)
	}
}

// TestProcessCorruptedFavorites 验证损坏收藏文件不导致进程失败。
func TestProcessCorruptedFavorites(t *testing.T) {
	bin := buildBinary(t)
	entry := newEntry(t).script("s1", "hello", "你好", "问候", "echo x\r\n").write("entry.yaml")
	favPath := filepath.Join(t.TempDir(), "favs.json")
	if err := writeFileRaw(favPath, "{broken"); err != nil {
		t.Fatal(err)
	}
	code, out, errOut := runBinary(t, bin, "--config", entry, "--favorites", favPath, "list")
	if code != 0 {
		t.Fatalf("损坏收藏不应导致失败: exit=%d stderr=%s", code, errOut)
	}
	if strings.Contains(out, "★") {
		t.Errorf("损坏收藏应降级为空: %s", out)
	}
}

func writeFileRaw(path, content string) error {
	return os.WriteFile(path, []byte(content), 0o600)
}
