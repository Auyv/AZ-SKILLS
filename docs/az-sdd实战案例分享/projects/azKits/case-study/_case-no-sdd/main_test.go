package main

import (
	"bytes"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"azkits/internal/store"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

func writeEntry(t *testing.T, yaml string, files map[string]string) string {
	t.Helper()
	dir := t.TempDir()
	for name, content := range files {
		p := filepath.Join(dir, name)
		os.MkdirAll(filepath.Dir(p), 0o755)
		os.WriteFile(p, []byte(content), 0o644)
	}
	path := filepath.Join(dir, "entry.yaml")
	os.WriteFile(path, []byte(yaml), 0o644)
	return path
}

func goodEntry() string {
	return `
version: 1
name: CLI测试
description: 描述
scripts:
  - id: a
    alias: alpha
    name: 阿尔法
    description: 第一个脚本
    path: scripts/a.bat
    type: bat
    encoding: utf-8
  - id: b
    alias: beta
    name: 贝塔
    description: 第二个脚本
    path: scripts/b.bat
    type: bat
    encoding: utf-8
`
}

func TestParseArgs(t *testing.T) {
	opts, rest, err := parseArgs([]string{"--favorites-dir", "d", "-h", "e.yaml", "list"})
	if err != nil {
		t.Fatal(err)
	}
	if opts.favoritesDir != "d" || !opts.help {
		t.Fatalf("opts = %+v", opts)
	}
	if len(rest) != 2 || rest[0] != "e.yaml" || rest[1] != "list" {
		t.Fatalf("rest = %v", rest)
	}
	if _, _, err := parseArgs([]string{"--bogus"}); err == nil {
		t.Fatal("want error for unknown flag")
	}
}

func TestCLIListShowsFavoriteMarker(t *testing.T) {
	path := writeEntry(t, goodEntry(), map[string]string{
		"scripts/a.bat": "@echo off\n",
		"scripts/b.bat": "@echo off\n",
	})
	dir := t.TempDir()
	st, _ := store.Open(dir)
	st.SetFavorite("a", true)

	var out, errb bytes.Buffer
	code := cmdList(path, &options{favoritesDir: dir}, &out, &errb)
	if code != 0 {
		t.Fatalf("exit code = %d, stderr=%s", code, errb.String())
	}
	lines := out.String()
	if !strings.Contains(lines, "★") {
		t.Fatalf("list should mark favorites:\n%s", lines)
	}
	if !strings.Contains(lines, "alpha") || !strings.Contains(lines, "beta") {
		t.Fatalf("list should include aliases:\n%s", lines)
	}
	if !strings.Contains(lines, "有效") {
		t.Fatalf("list should include status:\n%s", lines)
	}
}

func TestCLIRunUsesFavoriteCompatibleAlias(t *testing.T) {
	path := writeEntry(t, goodEntry(), map[string]string{
		"scripts/a.bat": "@echo off\necho ALPHA_RUN_OK\n",
		"scripts/b.bat": "@echo off\n",
	})
	var out, errb bytes.Buffer
	code := cmdRun(path, "alpha", &options{}, strings.NewReader(""), &out, &errb)
	if code != 0 {
		t.Fatalf("exit code = %d, stderr=%s", code, errb.String())
	}
	if !strings.Contains(out.String(), "ALPHA_RUN_OK") {
		t.Fatalf("output = %q", out.String())
	}
}

func TestCLIRunUnknownAlias(t *testing.T) {
	path := writeEntry(t, goodEntry(), map[string]string{
		"scripts/a.bat": "@echo off\n",
		"scripts/b.bat": "@echo off\n",
	})
	var out, errb bytes.Buffer
	code := cmdRun(path, "nope", &options{}, strings.NewReader(""), &out, &errb)
	if code != 1 {
		t.Fatalf("exit code = %d", code)
	}
	if !strings.Contains(errb.String(), "未找到") {
		t.Fatalf("stderr = %q", errb.String())
	}
}

func TestCLIRunExitCodePropagated(t *testing.T) {
	path := writeEntry(t, `
version: 1
scripts:
  - id: f
    alias: fail
    name: F
    description: D
    path: scripts/f.bat
    type: bat
`, map[string]string{"scripts/f.bat": "@echo off\r\nexit /b 5\r\n"})
	var out, errb bytes.Buffer
	code := cmdRun(path, "fail", &options{}, strings.NewReader(""), &out, &errb)
	if code != 5 {
		t.Fatalf("exit code = %d, want 5", code)
	}
}

func TestCLICheckExitCodes(t *testing.T) {
	ok := writeEntry(t, goodEntry(), map[string]string{
		"scripts/a.bat": "@echo off\n",
		"scripts/b.bat": "@echo off\n",
	})
	var out bytes.Buffer
	if code := cmdCheck(ok, &out); code != 0 {
		t.Fatalf("good entry check code = %d\n%s", code, out.String())
	}

	bad := writeEntry(t, `version: 2
scripts: []
`, nil)
	out.Reset()
	if code := cmdCheck(bad, &out); code != 1 {
		t.Fatalf("bad entry check code = %d\n%s", code, out.String())
	}
	if !strings.Contains(out.String(), "version") {
		t.Fatalf("check output = %q", out.String())
	}
}

func TestConsoleCPConversion(t *testing.T) {
	var buf bytes.Buffer
	w := wrapConsoleCP(&buf, 936)
	if _, err := w.Write([]byte("中文测试")); err != nil {
		t.Fatal(err)
	}
	got := buf.Bytes()
	// GBK bytes must not be valid UTF-8 for the same string; decode back
	gbk, err := decodeGBK(got)
	if err != nil {
		t.Fatalf("decode: %v", err)
	}
	if gbk != "中文测试" {
		t.Fatalf("round trip = %q", gbk)
	}
}

func decodeGBK(b []byte) (string, error) {
	r := transform.NewReader(bytes.NewReader(b), simplifiedchinese.GBK.NewDecoder())
	out, err := io.ReadAll(r)
	if err != nil {
		return "", err
	}
	return string(out), nil
}

func TestCLIBlockingEntryListReturnsError(t *testing.T) {
	bad := writeEntry(t, `version: 1
scripts:
  - id: a
    alias: same
    name: A
    description: D
    path: x.bat
    type: bat
  - id: b
    alias: same
    name: B
    description: D
    path: x.bat
    type: bat
`, nil)
	var out, errb bytes.Buffer
	code := cmdList(bad, &options{}, &out, &errb)
	if code != 1 {
		t.Fatalf("exit code = %d, want 1", code)
	}
}
