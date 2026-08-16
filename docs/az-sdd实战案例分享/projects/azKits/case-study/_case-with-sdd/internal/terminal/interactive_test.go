package terminal

import (
	"os/exec"
	"strings"
	"testing"

	"azkits/internal/runner"
)

func TestInteractiveCommandCapture(t *testing.T) {
	cmd := exec.Command("cmd.exe", "/c", "echo hi")
	ic := newInteractiveCommand(cmd)
	var out strings.Builder
	ic.SetStdout(&out)
	ic.SetStderr(&out)
	if err := ic.Run(); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if !strings.Contains(out.String(), "hi") {
		t.Errorf("输出 = %q", out.String())
	}
	result := ic.Result()
	if result.ExitCode != 0 {
		t.Errorf("退出码 = %d", result.ExitCode)
	}
	if result.PID == 0 {
		t.Error("PID 应为非零")
	}
	events := ic.Events()
	if len(events) == 0 {
		t.Fatal("应捕获到输出事件")
	}
	found := false
	for _, e := range events {
		if e.Stream == runner.Stdout && strings.Contains(string(e.Data), "hi") {
			found = true
		}
	}
	if !found {
		t.Errorf("事件未捕获 stdout: %+v", events)
	}
}

func TestInteractiveCommandResultBeforeRun(t *testing.T) {
	cmd := exec.Command("cmd.exe", "/c", "exit 0")
	ic := newInteractiveCommand(cmd)
	result := ic.Result()
	if result.ExitCode != -1 {
		t.Errorf("未运行时应返回 -1: %d", result.ExitCode)
	}
}

func TestInteractiveCaptureWriterSplit(t *testing.T) {
	cmd := exec.Command("cmd.exe", "/c", "exit 0")
	ic := newInteractiveCommand(cmd)
	var target strings.Builder
	w := interactiveCaptureWriter{command: ic, stream: runner.Stderr, target: &target}
	n, err := w.Write([]byte("警告"))
	if err != nil {
		t.Fatal(err)
	}
	if n != len("警告") {
		t.Errorf("写入字节数 = %d", n)
	}
	if target.String() != "警告" {
		t.Errorf("目标输出 = %q", target.String())
	}
	events := ic.Events()
	if len(events) != 1 || events[0].Stream != runner.Stderr || string(events[0].Data) != "警告" {
		t.Errorf("事件错误: %+v", events)
	}
	if ic.stdout.Len() != 0 || ic.stderr.Len() == 0 {
		t.Errorf("stderr 缓存错误: stdout=%d stderr=%d", ic.stdout.Len(), ic.stderr.Len())
	}
}

func TestInteractiveSetStdin(t *testing.T) {
	cmd := exec.Command("cmd.exe", "/c", "set /p x=<con 2>nul & exit 0")
	ic := newInteractiveCommand(cmd)
	ic.SetStdin(strings.NewReader("input"))
	if ic.cmd.Stdin == nil {
		t.Error("SetStdin 应设置 Stdin")
	}
}
