package tui

import (
	"bytes"
	"os"
	"strings"
	"testing"
	"time"

	tea "charm.land/bubbletea/v2"

	"azkits/internal/entry"
	"azkits/internal/store"
)

// TestProgramIntegration drives the real bubble tea program through key input
// and verifies the candidate flow plus a clean quit.
func TestProgramIntegration(t *testing.T) {
	e, err := entry.LoadFile(writeTmpEntry(t))
	if err != nil {
		t.Fatal(err)
	}
	m := New(e, store.NewEmptyStore())

	input := bytes.NewReader([]byte("/hello\x03"))
	var out bytes.Buffer
	p := tea.NewProgram(m,
		tea.WithInput(input),
		tea.WithOutput(&out),
		tea.WithWindowSize(90, 28),
	)
	m.App().Program(p)

	done := make(chan error, 1)
	go func() { _, err := p.Run(); done <- err }()
	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("program error: %v", err)
		}
	case <-time.After(20 * time.Second):
		t.Fatal("program did not quit in time")
	}

	final := m
	if string(final.input) != "/hello" {
		t.Fatalf("final input = %q, want /hello", string(final.input))
	}
	if len(final.candidates) == 0 {
		t.Fatal("candidates should be computed")
	}
	found := false
	for _, c := range final.candidates {
		if c.Script.ID == "hello" {
			found = true
		}
	}
	if !found {
		t.Fatalf("hello candidate missing: %v", ids(final.candidates))
	}
	if out.Len() == 0 {
		t.Fatal("program produced no output")
	}
	rendered := out.String()
	if !strings.Contains(rendered, "azKits") {
		t.Fatalf("rendered content missing title: %q", truncateForTest(rendered, 200))
	}
}

func writeTmpEntry(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	entryPath := dir + "\\entry.yaml"
	os.WriteFile(entryPath, []byte("version: 1\nscripts: []\n"), 0o644)
	raw := `
version: 1
name: 集成测试
description: 描述
scripts:
  - id: hello
    alias: hello
    name: 你好脚本
    description: 问候
    path: scripts/hello.bat
    type: bat
    encoding: utf-8
`
	scriptPath := dir + "\\scripts\\hello.bat"
	os.MkdirAll(dir+"\\scripts", 0o755)
	os.WriteFile(scriptPath, []byte("@echo off\r\necho HI\r\n"), 0o644)
	os.WriteFile(entryPath, []byte(raw), 0o644)
	return entryPath
}

func truncateForTest(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n]
}
