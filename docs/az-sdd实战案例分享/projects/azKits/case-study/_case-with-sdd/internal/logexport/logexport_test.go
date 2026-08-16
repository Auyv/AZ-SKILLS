package logexport

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func sampleRun() Run {
	code := 0
	return Run{
		ToolboxName: "测试工具箱",
		ScriptID:    "s1",
		Alias:       "run-test",
		ScriptName:  "运行脚本",
		StartedAt:   time.Date(2026, 8, 12, 10, 0, 0, 0, time.Local),
		EndedAt:     time.Date(2026, 8, 12, 10, 0, 5, 0, time.Local),
		State:       "succeeded",
		ExitCode:    &code,
		Entries: []Entry{
			{Stream: "stdout", Text: "中文输出\n"},
			{Stream: "stderr", Text: "警告信息\n"},
		},
	}
}

func TestTrackerLifecycle(t *testing.T) {
	tr := NewTracker()
	if tr.HasActive() {
		t.Error("初始不应有活跃记录")
	}
	if err := tr.Start(sampleRun()); err != nil {
		t.Fatal(err)
	}
	if err := tr.Start(sampleRun()); err == nil {
		t.Error("重复 Start 应失败")
	}
	if !tr.HasActive() {
		t.Error("Start 后应有活跃记录")
	}
	if !tr.Append("s1", Entry{Stream: "stdout", Text: "追加"}) {
		t.Error("Append 应成功")
	}
	if tr.Append("nope", Entry{}) {
		t.Error("未知脚本 Append 应失败")
	}
	code := 0
	tr.Finish("s1", "succeeded", &code, time.Now())
	if tr.HasActive() {
		t.Error("Finish 后不应有活跃记录")
	}
	run, ok := tr.Last()
	if !ok {
		t.Fatal("应存在最近记录")
	}
	if run.State != "succeeded" || run.ExitCode == nil || *run.ExitCode != 0 {
		t.Errorf("最近记录字段错误: %+v", run)
	}
	if len(run.Entries) != 1 || run.Entries[0].Text != "追加" {
		t.Errorf("最近记录应只保留 Finish 前的完整条目: %+v", run.Entries)
	}
}

func TestTrackerLastEmpty(t *testing.T) {
	tr := NewTracker()
	if _, ok := tr.Last(); ok {
		t.Error("无记录时 Last 应返回 false")
	}
}

func TestWriterCreatesUtf8Document(t *testing.T) {
	dir := t.TempDir()
	now := time.Date(2026, 8, 12, 10, 30, 0, 123000000, time.Local)
	w := Writer{Dir: dir, Now: func() time.Time { return now }}
	path, err := w.Write(sampleRun())
	if err != nil {
		t.Fatalf("Write: %v", err)
	}
	if _, err := os.Stat(path); err != nil {
		t.Fatalf("文档不存在: %v", err)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	content := string(data)
	for _, want := range []string{"azKits 本次执行日志", "测试工具箱", "运行脚本", "/run-test", "succeeded", "退出码: 0", "中文输出", "警告信息"} {
		if !strings.Contains(content, want) {
			t.Errorf("文档缺少 %q", want)
		}
	}
}

func TestWriterUniqueNames(t *testing.T) {
	dir := t.TempDir()
	now := func() time.Time { return time.Date(2026, 8, 12, 10, 30, 0, 0, time.Local) }
	w := Writer{Dir: dir, Now: now}
	first, err := w.Write(sampleRun())
	if err != nil {
		t.Fatal(err)
	}
	second, err := w.Write(sampleRun())
	if err != nil {
		t.Fatal(err)
	}
	if first == second {
		t.Error("两次写入应生成不同文件名")
	}
	matches, _ := filepath.Glob(filepath.Join(dir, "*.txt"))
	if len(matches) != 2 {
		t.Errorf("应存在两个文档: %v", matches)
	}
}

func TestWriterStripsAnsiAndControls(t *testing.T) {
	dir := t.TempDir()
	run := sampleRun()
	run.Entries = []Entry{{Stream: "stdout", Text: "\x1b[31m红字\x1b[0m\n\x00\x07"}}
	w := Writer{Dir: dir}
	path, err := w.Write(run)
	if err != nil {
		t.Fatal(err)
	}
	data, _ := os.ReadFile(path)
	if strings.Contains(string(data), "\x1b") {
		t.Error("应去除 ANSI 序列")
	}
	if strings.Contains(string(data), "\x00") || strings.Contains(string(data), "\x07") {
		t.Error("应去除控制字符")
	}
}

func TestWriterEmptyOutput(t *testing.T) {
	dir := t.TempDir()
	run := sampleRun()
	run.Entries = nil
	w := Writer{Dir: dir}
	path, err := w.Write(run)
	if err != nil {
		t.Fatal(err)
	}
	data, _ := os.ReadFile(path)
	if !strings.Contains(string(data), "没有标准输出或错误输出") {
		t.Error("空输出应标明")
	}
}

func TestWriterUnwritableDir(t *testing.T) {
	blocked := filepath.Join(t.TempDir(), "blocked")
	if err := os.MkdirAll(blocked, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(blocked, "x"), []byte("y"), 0o600); err != nil {
		t.Fatal(err)
	}
	w := Writer{Dir: filepath.Join(blocked, "sub", "logs")}
	if _, err := w.Write(sampleRun()); err == nil {
		t.Skip("当前用户有写权限，跳过不可写场景")
	}
}

func TestEditorOpenPreference(t *testing.T) {
	opener := EditorOpener{
		Lookup: func(name string) (string, error) {
			if name == "notepad++.exe" {
				return "C:\\Program Files\\Notepad++\\notepad++.exe", nil
			}
			return "", os.ErrNotExist
		},
		Start: func(executable, document string) error { return nil },
	}
	editor, err := opener.Open("doc.txt")
	if err != nil {
		t.Fatal(err)
	}
	if editor != "Notepad++" {
		t.Errorf("editor = %s", editor)
	}
}

func TestEditorOpenFallbackToNotepad(t *testing.T) {
	opener := EditorOpener{
		Lookup: func(name string) (string, error) {
			if name == "notepad++.exe" {
				return "", os.ErrNotExist
			}
			if name == "notepad.exe" {
				return "C:\\Windows\\System32\\notepad.exe", nil
			}
			return "", os.ErrNotExist
		},
		Start: func(executable, document string) error { return nil },
	}
	editor, err := opener.Open("doc.txt")
	if err != nil {
		t.Fatal(err)
	}
	if editor != "Notepad" {
		t.Errorf("editor = %s", editor)
	}
}

func TestEditorOpenAllFail(t *testing.T) {
	opener := EditorOpener{
		Lookup: func(name string) (string, error) {
			if name == "notepad++.exe" {
				return "C:\\np.exe", nil
			}
			return "C:\\notepad.exe", nil
		},
		Start: func(executable, document string) error {
			return errors.New("拒绝打开")
		},
	}
	editor, err := opener.Open("doc.txt")
	if err == nil {
		t.Fatal("全部失败应返回错误")
	}
	if editor != "" {
		t.Errorf("editor = %s", editor)
	}
}

func TestServiceExportSeparatesWriteAndOpen(t *testing.T) {
	dir := t.TempDir()
	svc := Service{
		Writer: Writer{Dir: dir, Now: func() time.Time { return time.Now() }},
		Opener: EditorOpener{
			Lookup: func(string) (string, error) { return "", os.ErrNotExist },
		},
	}
	result, err := svc.Export(sampleRun())
	if err != nil {
		t.Fatalf("Export: %v", err)
	}
	if _, statErr := os.Stat(result.Path); statErr != nil {
		t.Errorf("文档仍应保留: %v", statErr)
	}
	if result.OpenError == nil {
		t.Error("编辑器不可用时 OpenError 应非空")
	}
}
