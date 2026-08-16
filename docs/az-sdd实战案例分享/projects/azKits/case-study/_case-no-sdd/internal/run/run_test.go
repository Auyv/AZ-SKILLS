package run

import (
	"bytes"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"azkits/internal/entry"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

func mkEntry(t *testing.T, files map[string]string) *entry.Entry {
	t.Helper()
	dir := t.TempDir()
	for name, content := range files {
		p := filepath.Join(dir, name)
		if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	path := filepath.Join(dir, "entry.yaml")
	if err := os.WriteFile(path, []byte("version: 1\nscripts: []\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	e, err := entry.Parse([]byte("version: 1\nscripts: []\n"), path)
	if err != nil {
		t.Fatal(err)
	}
	return e
}

func scriptOf(t *testing.T, e *entry.Entry, overrides map[string]string) *entry.Script {
	t.Helper()
	s := entry.Script{
		ID:          "s1",
		Alias:       "s1",
		Name:        "脚本",
		Description: "脚本描述",
		Path:        "scripts/a.bat",
		Type:        entry.TypeBat,
		Encoding:    entry.EncodingUTF8,
	}
	for k, v := range overrides {
		switch k {
		case "path":
			s.Path = v
		case "type":
			s.Type = entry.Type(v)
		case "encoding":
			s.Encoding = entry.Encoding(v)
		case "alias":
			s.Alias = v
		}
	}
	return &s
}

func TestBuildCommandBat(t *testing.T) {
	e := mkEntry(t, map[string]string{"scripts/a.bat": "@echo off\n"})
	s := scriptOf(t, e, nil)
	cmd, err := BuildCommand(&Request{Entry: e, Script: s})
	if err != nil {
		t.Fatal(err)
	}
	if cmd.Dir != e.BaseDir {
		t.Fatalf("working dir = %q, want %q", cmd.Dir, e.BaseDir)
	}
	if filepath.Base(cmd.Args[0]) != "cmd.exe" {
		t.Fatalf("argv0 = %v", cmd.Args)
	}
	joined := strings.Join(cmd.Args[1:], " ")
	if !strings.Contains(joined, "/c") {
		t.Fatalf("cmdline = %v", cmd.Args)
	}
	if !strings.Contains(joined, "a.bat") {
		t.Fatalf("cmdline = %v", cmd.Args)
	}
}

func TestBuildCommandPathWithSpaces(t *testing.T) {
	e := mkEntry(t, map[string]string{"scripts/my bat/a & b.bat": "@echo off\n"})
	s := scriptOf(t, e, map[string]string{"path": "scripts/my bat/a & b.bat"})
	cmd, err := BuildCommand(&Request{Entry: e, Script: s})
	if err != nil {
		t.Fatal(err)
	}
	joined := strings.Join(cmd.Args[1:], " ")
	if !strings.Contains(joined, `"`) {
		t.Fatalf("expected quoted cmdline, got: %v", cmd.Args)
	}
}

func TestBuildCommandDisabled(t *testing.T) {
	e := mkEntry(t, map[string]string{"scripts/a.bat": "@echo off\n"})
	enabled := false
	s := scriptOf(t, e, nil)
	s.Enabled = &enabled
	if _, err := BuildCommand(&Request{Entry: e, Script: s}); err == nil {
		t.Fatal("want error for disabled script")
	}
}

func TestBuildCommandMissingFile(t *testing.T) {
	e := mkEntry(t, map[string]string{"scripts/other.bat": "@echo off\n"})
	s := scriptOf(t, e, map[string]string{"path": "scripts/nothere.bat"})
	if _, err := BuildCommand(&Request{Entry: e, Script: s}); err == nil {
		t.Fatal("want error for missing file")
	}
}

func TestRunOnceExitCode(t *testing.T) {
	e := mkEntry(t, map[string]string{"scripts/fail.bat": "@echo off\r\nexit /b 3\r\n"})
	s := scriptOf(t, e, map[string]string{"path": "scripts/fail.bat"})
	var out, errb bytes.Buffer
	res := RunOnce(&Request{Entry: e, Script: s}, strings.NewReader(""), &out, &errb)
	if res.Err != nil {
		t.Fatalf("unexpected err: %v", res.Err)
	}
	if res.ExitCode != 3 {
		t.Fatalf("exit code = %d, want 3", res.ExitCode)
	}
}

func TestRunOnceArgsPassed(t *testing.T) {
	e := mkEntry(t, map[string]string{
		"scripts/a.bat": "@echo off\necho first=%1\necho second=%2\n",
	})
	s := scriptOf(t, e, map[string]string{"path": "scripts/a.bat"})
	s.Args = []string{"abc", "123"}
	var out, errb bytes.Buffer
	res := RunOnce(&Request{Entry: e, Script: s}, strings.NewReader(""), &out, &errb)
	if res.Err != nil || res.ExitCode != 0 {
		t.Fatalf("result = %+v err=%v", res, errb.String())
	}
	if !strings.Contains(out.String(), "first=abc") {
		t.Fatalf("args not passed, output: %q", out.String())
	}
}

func TestRunOnceGBKDecode(t *testing.T) {
	e := mkEntry(t, map[string]string{
		"scripts/g.py": "import sys\nsys.stdout.buffer.write('中文GBK'.encode('gbk')+b'\\n')\n",
	})
	s := scriptOf(t, e, map[string]string{"path": "scripts/g.py", "type": "python", "encoding": "gbk"})
	if _, err := exec.LookPath("python"); err != nil {
		if _, err2 := exec.LookPath("py"); err2 != nil {
			t.Skip("no python available")
		}
	}
	var out, errb bytes.Buffer
	res := RunOnce(&Request{Entry: e, Script: s}, strings.NewReader(""), &out, &errb)
	if res.Err != nil || res.ExitCode != 0 {
		t.Fatalf("result = %+v err=%v", res, errb.String())
	}
	if !strings.Contains(out.String(), "中文GBK") {
		t.Fatalf("GBK output not decoded: %q", out.String())
	}
}

func TestRunOnceUTF8Passthrough(t *testing.T) {
	e := mkEntry(t, map[string]string{"scripts/a.bat": "@echo off\r\necho ASCII_MARKER_UTF8\r\n"})
	s := scriptOf(t, e, map[string]string{"path": "scripts/a.bat", "encoding": "utf-8"})
	var out, errb bytes.Buffer
	res := RunOnce(&Request{Entry: e, Script: s}, strings.NewReader(""), &out, &errb)
	if res.Err != nil || res.ExitCode != 0 {
		t.Fatalf("result = %+v", res)
	}
	if !strings.Contains(out.String(), "ASCII_MARKER_UTF8") {
		t.Fatalf("utf-8 output lost: %q", out.String())
	}
}

func TestRunOnceGBKBat(t *testing.T) {
	// A real GBK-encoded batch file echoing Chinese must decode correctly.
	batBody := "@echo off\r\nchcp 936 >nul\r\necho 中文问候\r\n"
	gbkBytes, _, err := transform.String(simplifiedchinese.GB18030.NewEncoder(), batBody)
	if err != nil {
		t.Fatal(err)
	}
	e := mkEntry(t, map[string]string{"scripts/gbk.bat": gbkBytes})
	s := scriptOf(t, e, map[string]string{"path": "scripts/gbk.bat", "encoding": "gbk"})
	var out, errb bytes.Buffer
	res := RunOnce(&Request{Entry: e, Script: s}, strings.NewReader(""), &out, &errb)
	if res.Err != nil || res.ExitCode != 0 {
		t.Fatalf("result = %+v err=%s", res, errb.String())
	}
	if !strings.Contains(out.String(), "中文问候") {
		t.Fatalf("GBK bat output not decoded: %q", out.String())
	}
}

func TestDecodeReaderGBK(t *testing.T) {
	// encode a Chinese string into GBK bytes with the real code page encoder
	enc := simplifiedchinese.GB18030.NewEncoder()
	gbytes, _, err := transform.String(enc, "你好，测试")
	if err != nil {
		t.Fatal(err)
	}
	r := DecodeReader(bytes.NewReader([]byte(gbytes)), entry.EncodingGBK)
	out, err := io.ReadAll(r)
	if err != nil {
		t.Fatal(err)
	}
	if string(out) != "你好，测试" {
		t.Fatalf("decoded = %q", string(out))
	}
}

func TestDecodeReaderUTF8Sig(t *testing.T) {
	bom := append([]byte{0xEF, 0xBB, 0xBF}, []byte("中文")...)
	r := DecodeReader(bytes.NewReader(bom), entry.EncodingUTF8Sig)
	out, err := io.ReadAll(r)
	if err != nil {
		t.Fatal(err)
	}
	if string(out) != "中文" {
		t.Fatalf("BOM not stripped: %q", string(out))
	}
}

func TestDecodeReaderUTF8Direct(t *testing.T) {
	r := DecodeReader(bytes.NewBufferString("直接"), entry.EncodingUTF8)
	out, _ := io.ReadAll(r)
	if string(out) != "直接" {
		t.Fatalf("got %q", string(out))
	}
}

func TestResolveTypeAuto(t *testing.T) {
	cases := []struct{ path, typ string }{
		{"a.bat", "cmd"},
		{"a.ps1", "powershell"},
		{"a.py", "python"},
		{"a.sh", "shell"},
		{"a.exe", "exe"},
	}
	for _, c := range cases {
		s := &entry.Script{Path: c.path, Type: entry.TypeAuto}
		got, err := ResolveType(s)
		if err != nil {
			t.Fatalf("%s: %v", c.path, err)
		}
		if string(got) != c.typ {
			t.Fatalf("%s: type = %s, want %s", c.path, got, c.typ)
		}
	}
	s := &entry.Script{Path: "a.unknown", Type: entry.TypeAuto}
	if _, err := ResolveType(s); err == nil {
		t.Fatal("want error for unresolvable auto type")
	}
}

func TestLineScannerCRLFAndLongLine(t *testing.T) {
	input := "a\r\nb\r\nc"
	sc := LineScanner(strings.NewReader(input))
	var got []string
	for sc.Scan() {
		got = append(got, sc.Text())
	}
	if len(got) != 3 || got[0] != "a" || got[1] != "b" || got[2] != "c" {
		t.Fatalf("got %v", got)
	}
}
