package tui

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	tea "charm.land/bubbletea/v2"

	"azkits/internal/entry"
	"azkits/internal/matcher"
	"azkits/internal/store"
)

func testEntry(t *testing.T) *entry.Entry {
	t.Helper()
	dir := t.TempDir()
	files := map[string]string{
		"scripts/hello.bat":  "@echo off\r\necho HELLO_OUTPUT_OK\r\n",
		"scripts/second.bat": "@echo off\r\necho SECOND_OUTPUT_OK\r\n",
		"scripts/slow.bat":   "@echo off\r\necho starting\r\necho done\r\n",
	}
	for name, content := range files {
		p := filepath.Join(dir, name)
		os.MkdirAll(filepath.Dir(p), 0o755)
		os.WriteFile(p, []byte(content), 0o644)
	}
	entryPath := filepath.Join(dir, "entry.yaml")
	os.WriteFile(entryPath, []byte("version: 1\nscripts: []\n"), 0o644)
	raw := `
version: 1
name: 测试入口
scripts:
  - id: hello
    alias: hello
    name: 你好脚本
    description: 问候
    path: scripts/hello.bat
    type: bat
    encoding: utf-8
  - id: second
    alias: second
    name: 第二脚本
    description: 第二个
    path: scripts/second.bat
    type: bat
    encoding: utf-8
  - id: slow
    alias: slow
    name: 慢脚本
    description: 慢速
    path: scripts/slow.bat
    type: bat
    encoding: utf-8
  - id: missing
    alias: missing
    name: 缺失
    description: 缺失文件
    path: scripts/nothere.bat
    type: bat
  - id: off
    alias: off
    name: 禁用
    description: 禁用脚本
    path: scripts/hello.bat
    type: bat
    enabled: false
`
	e, err := entry.Parse([]byte(raw), entryPath)
	if err != nil {
		t.Fatal(err)
	}
	if len(e.Blocking()) > 0 {
		t.Fatalf("entry has blocking issues: %v", e.Blocking())
	}
	return e
}

func newModel(t *testing.T) *Model {
	t.Helper()
	e := testEntry(t)
	st := store.NewEmptyStore()
	m := New(e, st)
	m.width, m.height = 80, 24
	return m
}

func press(key tea.KeyPressMsg) tea.Msg { return key }

func k(r rune, mod tea.KeyMod, text string) tea.KeyPressMsg {
	return tea.KeyPressMsg(tea.Key{Code: r, Mod: mod, Text: text})
}

func imeText(s string) tea.KeyPressMsg {
	runes := []rune(s)
	if len(runes) == 1 {
		return tea.KeyPressMsg(tea.Key{Code: 0, BaseCode: runes[0]})
	}
	// multi-rune IME commit is delivered as text
	return tea.KeyPressMsg(tea.Key{Code: 0, BaseCode: runes[0], Text: s})
}

func typeIn(m *Model, s string) {
	for _, r := range s {
		if r == '/' {
			m.Update(k('/', 0, "/"))
			continue
		}
		m.Update(k(r, 0, string(r)))
	}
}

func TestIMETextNeverExecutes(t *testing.T) {
	m := newModel(t)
	// During composition the committed text arrives as BaseCode / Text events.
	m.Update(press(k('/', 0, "/")))
	m.Update(press(imeText("你好")))
	if string(m.input) != "/你好" {
		t.Fatalf("input = %q, want /你好", string(m.input))
	}
	if m.state != stateLauncher {
		t.Fatalf("IME commit must not execute, state = %d", m.state)
	}
	if m.candidates == nil {
		t.Fatal("candidates should have been recomputed")
	}
	// a candidate whose name contains 你好 must exist
	found := false
	for _, c := range m.candidates {
		if c.Script.ID == "hello" {
			found = true
		}
	}
	if !found {
		t.Fatalf("candidates = %v", ids(m.candidates))
	}
}

func TestTextAndEnterLaunch(t *testing.T) {
	m := newModel(t)
	// capture streamed output lines through the app hook and feed them back
	m.app.recv = func(msg tea.Msg) {
		if lm, ok := msg.(lineMsg); ok {
			m.Update(lm)
		}
	}
	typeIn(m, "/hello")
	if string(m.input) != "/hello" {
		t.Fatalf("input = %q", string(m.input))
	}
	cm, cmd := m.Update(press(k(tea.KeyEnter, 0, "")))
	m = cm.(*Model)
	if m.state != stateRunning {
		t.Fatalf("state = %d, want running", m.state)
	}
	if cmd == nil {
		t.Fatal("expected launch cmd")
	}
	res := cmd()
	cm, _ = m.Update(res)
	m = cm.(*Model)
	if m.runDone == nil || m.runDone.ExitCode != 0 {
		t.Fatalf("runDone = %+v", m.runDone)
	}
	if !m.app.recent.Has("hello") {
		t.Fatal("recent should be marked on success")
	}
	if len(m.runLines) == 0 {
		t.Fatal("run output should be captured")
	}
	found := false
	for _, l := range m.runLines {
		if strings.Contains(l, "HELLO_OUTPUT_OK") {
			found = true
		}
	}
	if !found {
		t.Fatalf("run output missing marker: %v", m.runLines)
	}
}

func TestTabCompletesWithoutExecuting(t *testing.T) {
	m := newModel(t)
	typeIn(m, "/hell")
	if string(m.input) != "/hell" {
		t.Fatalf("input = %q", string(m.input))
	}
	_, cmd := m.Update(press(k(tea.KeyTab, 0, "")))
	if cmd != nil {
		t.Fatal("Tab must not execute")
	}
	if string(m.input) != "/hello" {
		t.Fatalf("after Tab input = %q, want /hello", string(m.input))
	}
	if m.state != stateLauncher {
		t.Fatal("Tab must not change state")
	}
}

func TestEscClosesCandidates(t *testing.T) {
	m := newModel(t)
	typeIn(m, "/zz")
	if !m.candidateMode() {
		t.Fatal("expected candidate mode")
	}
	_, cmd := m.Update(press(k(tea.KeyEscape, 0, "")))
	if cmd != nil {
		t.Fatal("Esc must not execute")
	}
	if len(m.input) != 0 || m.candidateMode() {
		t.Fatalf("input = %q, candidates should close", string(m.input))
	}
}

func TestEscOnEmptyQuits(t *testing.T) {
	m := newModel(t)
	_, cmd := m.Update(press(k(tea.KeyEscape, 0, "")))
	if cmd == nil || cmd() == nil {
		t.Fatal("Esc on empty input should quit")
	}
}

func TestUpDownNavigation(t *testing.T) {
	m := newModel(t)
	m.Update(press(k('/', 0, "/")))
	if len(m.candidates) < 3 {
		t.Fatalf("candidates = %d", len(m.candidates))
	}
	if m.sel != 0 {
		t.Fatalf("initial sel = %d", m.sel)
	}
	m.Update(press(k(tea.KeyDown, 0, "")))
	if m.sel != 1 {
		t.Fatalf("sel after down = %d", m.sel)
	}
	m.Update(press(k(tea.KeyUp, 0, "")))
	if m.sel != 0 {
		t.Fatalf("sel after up = %d", m.sel)
	}
}

func TestFavoriteToggle(t *testing.T) {
	m := newModel(t)
	m.Update(press(k('/', 0, "/")))
	first := m.candidates[0].Script.ID
	if m.app.store.IsFavorite(first) {
		t.Fatal("should not be favorite yet")
	}
	m.Update(press(k('f', tea.ModCtrl, "")))
	if !m.app.store.IsFavorite(first) {
		t.Fatal("ctrl+f should favorite the selected candidate")
	}
	m.Update(press(k('f', tea.ModCtrl, "")))
	if m.app.store.IsFavorite(first) {
		t.Fatal("second ctrl+f should unfavorite")
	}
}

func TestFileMissingScriptCannotLaunch(t *testing.T) {
	m := newModel(t)
	typeIn(m, "/missing")
	var target matcher.Candidate
	for _, c := range m.candidates {
		if c.Script.ID == "missing" {
			target = c
		}
	}
	if target.Script.ID == "" {
		t.Fatal("missing script should be a candidate")
	}
	if target.Script.Status != matcher.StatusFileMissing {
		t.Fatalf("status = %v", target.Script.Status)
	}
	_, cmd := m.Update(press(k(tea.KeyEnter, 0, "")))
	if cmd != nil {
		t.Fatal("missing file must not launch")
	}
	if !strings.Contains(m.toast, "缺失") {
		t.Fatalf("toast = %q", m.toast)
	}
}

func TestDisabledScriptExcludedFromCandidates(t *testing.T) {
	m := newModel(t)
	m.Update(press(k('/', 0, "/")))
	for _, c := range m.candidates {
		if c.Script.ID == "off" {
			t.Fatal("disabled script must not appear in candidates")
		}
	}
}

func TestFailedRunNotMarkedRecent(t *testing.T) {
	dir := t.TempDir()
	os.MkdirAll(filepath.Join(dir, "scripts"), 0o755)
	os.WriteFile(filepath.Join(dir, "scripts/f.bat"), []byte("@echo off\nexit /b 7\n"), 0o644)
	raw := `
version: 1
scripts:
  - id: f
    alias: f
    name: F
    description: D
    path: scripts/f.bat
    type: bat
`
	e, err := entry.Parse([]byte(raw), filepath.Join(dir, "e.yaml"))
	if err != nil {
		t.Fatal(err)
	}
	m := New(e, store.NewEmptyStore())
	m.width, m.height = 80, 24

	typeIn(m, "/f")
	_, cmd := m.Update(press(k(tea.KeyEnter, 0, "")))
	if cmd == nil {
		t.Fatal("expected launch cmd")
	}
	res := cmd()
	m.Update(res)
	if m.runDone == nil || m.runDone.ExitCode != 7 {
		t.Fatalf("runDone = %+v", m.runDone)
	}
	if m.app.recent.Has("f") {
		t.Fatal("failed run must not be marked recent")
	}
}

func TestHistoryBrowsing(t *testing.T) {
	m := newModel(t)
	typeIn(m, "/hello")
	_, cmd := m.Update(press(k(tea.KeyEnter, 0, "")))
	// run to completion, then dismiss the runner view
	res := cmd()
	m.Update(res)
	m.Update(press(k(tea.KeyEnter, 0, "")))
	if m.state != stateLauncher {
		t.Fatalf("state = %d, want launcher", m.state)
	}
	if len(m.input) != 0 {
		t.Fatalf("input after launch = %q", string(m.input))
	}
	// without candidates, up browses history
	m.Update(press(k(tea.KeyUp, 0, "")))
	if string(m.input) != "/hello" {
		t.Fatalf("history restored input = %q", string(m.input))
	}
	if !m.candidateMode() || len(m.candidates) == 0 {
		t.Fatal("restored history should re-open candidates")
	}
}

func TestHistoryPersistsAcrossRuns(t *testing.T) {
	m := newModel(t)
	typeIn(m, "/second")
	m.Update(press(k(tea.KeyEnter, 0, "")))
	if len(m.input) != 0 {
		t.Fatalf("input after launch = %q", string(m.input))
	}
	if len(m.history) != 1 || m.history[0] != "/second" {
		t.Fatalf("history = %v", m.history)
	}
}

func TestDeferredFavoritesDegradeWhenDeleted(t *testing.T) {
	e := testEntry(t)
	dir := t.TempDir()
	st, err := store.Open(dir)
	if err != nil {
		t.Fatal(err)
	}
	// favorite the "hello" script id, then simulate its removal from the entry
	if err := st.SetFavorite("hello", true); err != nil {
		t.Fatal(err)
	}
	var scripts []entry.Script
	for _, s := range e.Scripts {
		if s.ID != "hello" {
			scripts = append(scripts, s)
		}
	}
	e.Scripts = scripts
	m := New(e, st)
	m.Update(press(k('/', 0, "/")))
	for _, c := range m.candidates {
		if c.Favorite {
			t.Fatalf("deleted script must not surface as favorite candidate")
		}
	}
}

func ids(cs []matcher.Candidate) []string {
	var out []string
	for _, c := range cs {
		out = append(out, c.Script.ID)
	}
	return out
}
