package logexport

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func intPtr(value int) *int { return &value }

func sampleRun() Run {
	return Run{
		ToolboxName: "设备测试工具箱",
		ScriptID:    "device-init",
		Alias:       "device-init",
		ScriptName:  "设备初始化",
		StartedAt:   time.Date(2026, 8, 11, 10, 0, 0, 0, time.Local),
		EndedAt:     time.Date(2026, 8, 11, 10, 0, 2, 0, time.Local),
		State:       "succeeded",
		ExitCode:    intPtr(0),
		Entries: []Entry{
			{Stream: "stdout", Text: "中文输出\n"},
			{Stream: "stderr", Text: "\x1b[31m错误提示\x1b[0m\x00\n"},
		},
	}
}

func TestTrackerKeepsMostRecentlyFinishedRunAndEntryOrder(t *testing.T) {
	tracker := NewTracker()
	startA := sampleRun()
	startA.ScriptID, startA.Alias, startA.ScriptName = "a", "a", "脚本 A"
	startA.Entries, startA.EndedAt, startA.State, startA.ExitCode = nil, time.Time{}, "", nil
	startB := startA
	startB.ScriptID, startB.Alias, startB.ScriptName = "b", "b", "脚本 B"
	if err := tracker.Start(startA); err != nil {
		t.Fatal(err)
	}
	if err := tracker.Start(startB); err != nil {
		t.Fatal(err)
	}
	tracker.Append("a", Entry{Stream: "stdout", Text: "A1"})
	tracker.Append("a", Entry{Stream: "stderr", Text: "A2"})
	tracker.Append("b", Entry{Stream: "stdout", Text: "B1"})
	if !tracker.HasActive() {
		t.Fatal("tracker should report active runs")
	}
	if !tracker.Finish("a", "failed", intPtr(7), startA.StartedAt.Add(time.Second)) {
		t.Fatal("finish A failed")
	}
	if !tracker.Finish("b", "succeeded", intPtr(0), startB.StartedAt.Add(2*time.Second)) {
		t.Fatal("finish B failed")
	}
	last, ok := tracker.Last()
	if !ok || last.ScriptID != "b" || len(last.Entries) != 1 || last.Entries[0].Text != "B1" {
		t.Fatalf("last = %#v, ok=%v", last, ok)
	}
	if tracker.HasActive() {
		t.Fatal("all runs should be finished")
	}
	// 返回值必须是副本，不能污染 tracker。
	last.Entries[0].Text = "changed"
	again, _ := tracker.Last()
	if again.Entries[0].Text != "B1" {
		t.Fatal("Last returned shared entries")
	}
}

func TestTrackersAreInstanceIsolated(t *testing.T) {
	first := NewTracker()
	second := NewTracker()
	runA := sampleRun()
	runA.ScriptID = "first"
	runA.Entries, runA.EndedAt, runA.State, runA.ExitCode = nil, time.Time{}, "", nil
	runB := runA
	runB.ScriptID = "second"

	if err := first.Start(runA); err != nil {
		t.Fatal(err)
	}
	if err := second.Start(runB); err != nil {
		t.Fatal(err)
	}
	first.Append("first", Entry{Stream: "stdout", Text: "first only"})
	second.Append("second", Entry{Stream: "stderr", Text: "second only"})
	first.Finish("first", "succeeded", intPtr(0), time.Now())
	second.Finish("second", "failed", intPtr(1), time.Now())

	firstLast, firstOK := first.Last()
	secondLast, secondOK := second.Last()
	if !firstOK || firstLast.ScriptID != "first" || firstLast.Entries[0].Text != "first only" {
		t.Fatalf("first tracker = %#v, ok=%v", firstLast, firstOK)
	}
	if !secondOK || secondLast.ScriptID != "second" || secondLast.Entries[0].Text != "second only" {
		t.Fatalf("second tracker = %#v, ok=%v", secondLast, secondOK)
	}
}

func TestWriterCreatesUTF8ReadableDocumentWithoutOverwrite(t *testing.T) {
	dir := t.TempDir()
	fixed := time.Date(2026, 8, 11, 10, 1, 0, 123000000, time.Local)
	writer := Writer{Dir: dir, Now: func() time.Time { return fixed }}
	first, err := writer.Write(sampleRun())
	if err != nil {
		t.Fatal(err)
	}
	second, err := writer.Write(sampleRun())
	if err != nil {
		t.Fatal(err)
	}
	if first == second {
		t.Fatal("writer overwrote an existing document")
	}
	data, err := os.ReadFile(first)
	if err != nil {
		t.Fatal(err)
	}
	text := string(data)
	for _, want := range []string{"设备测试工具箱", "设备初始化", "/device-init", "succeeded", "退出码: 0", "[stdout]", "中文输出", "[stderr]", "错误提示"} {
		if !strings.Contains(text, want) {
			t.Fatalf("document missing %q:\n%s", want, text)
		}
	}
	if strings.Contains(text, "\x1b") || strings.ContainsRune(text, '\x00') {
		t.Fatalf("document contains terminal control characters: %q", text)
	}
	if len(data) >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf {
		t.Fatal("document should be UTF-8 without BOM")
	}
}

func TestWriterMarksExecutionWithoutOutput(t *testing.T) {
	run := sampleRun()
	run.Entries = nil
	path, err := (Writer{Dir: t.TempDir()}).Write(run)
	if err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(data), "没有标准输出或错误输出") {
		t.Fatalf("missing no-output marker:\n%s", data)
	}
}

func TestWriterExports16MiBWithinTwoSeconds(t *testing.T) {
	run := sampleRun()
	run.Entries = []Entry{{Stream: "stdout", Text: strings.Repeat("x", 16*1024*1024)}}
	started := time.Now()
	path, err := (Writer{Dir: t.TempDir()}).Write(run)
	if err != nil {
		t.Fatal(err)
	}
	if elapsed := time.Since(started); elapsed > 2*time.Second {
		t.Fatalf("16 MiB export took %s, want <= 2s", elapsed)
	}
	info, err := os.Stat(path)
	if err != nil {
		t.Fatal(err)
	}
	if info.Size() < 16*1024*1024 {
		t.Fatalf("exported size = %d", info.Size())
	}
}

func TestWriterReportsFailureAndKeepsExistingFile(t *testing.T) {
	root := t.TempDir()
	blocked := filepath.Join(root, "not-a-directory")
	if err := os.WriteFile(blocked, []byte("keep"), 0o600); err != nil {
		t.Fatal(err)
	}
	writer := Writer{Dir: blocked}
	if _, err := writer.Write(sampleRun()); err == nil {
		t.Fatal("Write should fail")
	}
	data, err := os.ReadFile(blocked)
	if err != nil || string(data) != "keep" {
		t.Fatalf("existing file changed: %q, %v", data, err)
	}
}

type fakeEditorEnvironment struct {
	paths       map[string]string
	startErrors map[string]error
	started     []string
}

func (f *fakeEditorEnvironment) lookup(name string) (string, error) {
	if path, ok := f.paths[name]; ok {
		return path, nil
	}
	return "", errors.New("not found")
}

func (f *fakeEditorEnvironment) start(executable, document string) error {
	f.started = append(f.started, executable+"|"+document)
	return f.startErrors[executable]
}

func TestEditorPrefersNotepadPlusPlusAndFallsBackToNotepad(t *testing.T) {
	env := &fakeEditorEnvironment{paths: map[string]string{"notepad++.exe": "npp.exe", "notepad.exe": "notepad.exe"}, startErrors: map[string]error{}}
	opener := EditorOpener{Lookup: env.lookup, Start: env.start}
	editor, err := opener.Open("log.txt")
	if err != nil || editor != "Notepad++" || len(env.started) != 1 || !strings.HasPrefix(env.started[0], "npp.exe|") {
		t.Fatalf("editor=%q err=%v started=%#v", editor, err, env.started)
	}

	env.started = nil
	env.startErrors["npp.exe"] = errors.New("npp failed")
	editor, err = opener.Open("log.txt")
	if err != nil || editor != "Notepad" || len(env.started) != 2 || !strings.HasPrefix(env.started[1], "notepad.exe|") {
		t.Fatalf("fallback editor=%q err=%v started=%#v", editor, err, env.started)
	}
}

func TestServiceSeparatesWriteAndEditorFailures(t *testing.T) {
	dir := t.TempDir()
	env := &fakeEditorEnvironment{paths: map[string]string{}, startErrors: map[string]error{}}
	service := Service{Writer: Writer{Dir: dir}, Opener: EditorOpener{Lookup: env.lookup, Start: env.start}}
	result, err := service.Export(sampleRun())
	if err != nil {
		t.Fatalf("file export failed: %v", err)
	}
	if result.Path == "" || result.OpenError == nil || result.Editor != "" {
		t.Fatalf("result = %#v", result)
	}
	if _, statErr := os.Stat(result.Path); statErr != nil {
		t.Fatalf("exported file missing: %v", statErr)
	}

	blocked := filepath.Join(dir, "blocked")
	if err := os.WriteFile(blocked, []byte("keep"), 0o600); err != nil {
		t.Fatal(err)
	}
	env.started = nil
	service.Writer.Dir = blocked
	if _, err := service.Export(sampleRun()); err == nil {
		t.Fatal("write failure should be returned")
	}
	if len(env.started) != 0 {
		t.Fatalf("editor started after write failure: %#v", env.started)
	}
}
