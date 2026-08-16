package entry

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeScript(t *testing.T, dir, name, content string) string {
	t.Helper()
	p := filepath.Join(dir, name)
	if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
	return p
}

func mustParse(t *testing.T, yaml string) *Entry {
	t.Helper()
	e, err := Parse([]byte(yaml), filepath.Join(t.TempDir(), "entry.yaml"))
	if err != nil {
		t.Fatalf("Parse failed: %v", err)
	}
	return e
}

func blockingCodes(e *Entry) []string {
	var out []string
	for _, is := range e.Blocking() {
		out = append(out, is.Code)
	}
	return out
}

func hasBlocking(e *Entry, code string) bool {
	for _, c := range blockingCodes(e) {
		if c == code {
			return true
		}
	}
	return false
}

func TestParseValidEntry(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "@echo off\n")
	e := mustParse(t, `
version: 1
name: 测试
scripts:
  - id: a
    alias: aa
    name: A脚本
    description: 测试脚本
    path: a.bat
    type: auto
    encoding: utf-8
    args: ["-x", "1"]
`)
	if len(e.Blocking()) > 0 {
		t.Fatalf("unexpected blocking issues: %v", blockingCodes(e))
	}
	if len(e.Scripts) != 1 {
		t.Fatalf("want 1 script, got %d", len(e.Scripts))
	}
	if e.Scripts[0].Type != TypeAuto {
		t.Fatalf("want auto type")
	}
	if got := InferType("a.bat"); got != TypeCmd {
		t.Fatalf("auto infer of .bat = %q, want cmd", got)
	}
	if e.BaseDir != filepath.Dir(e.FilePath) {
		t.Fatalf("BaseDir mismatch: %q", e.BaseDir)
	}
}

func TestParseVersionMustBeOne(t *testing.T) {
	e := mustParse(t, `
version: 2
scripts: []
`)
	if !hasBlocking(e, "bad-version") {
		t.Fatalf("want bad-version blocking, got %v", blockingCodes(e))
	}
}

func TestParseVersionMissing(t *testing.T) {
	e := mustParse(t, `
name: x
`)
	if !hasBlocking(e, "bad-version") {
		t.Fatalf("missing version should be blocking (zero value), got %v", blockingCodes(e))
	}
}

func TestUnknownFieldFails(t *testing.T) {
	raw := `
version: 1
bogus: true
scripts: []
`
	if _, err := Parse([]byte(raw), "x.yaml"); err == nil {
		t.Fatal("want error for unknown top-level field")
	}
}

func TestUnknownScriptFieldFails(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	raw := `
version: 1
scripts:
  - id: a
    alias: aa
    name: A
    description: D
    path: a.bat
    type: bat
    bogus: 1
`
	if _, err := Parse([]byte(raw), filepath.Join(dir, "e.yaml")); err == nil {
		t.Fatal("want error for unknown script field")
	}
}

func TestRequiredFieldsForEnabledScripts(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: aa
    name: ""
    description: ""
    path: ""
    type: bat
`)
	for _, code := range []string{"missing-name", "missing-description", "missing-path"} {
		if !hasBlocking(e, code) {
			t.Fatalf("want %s blocking, got %v", code, blockingCodes(e))
		}
	}
}

func TestMissingIDAndAlias(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	e := mustParse(t, `
version: 1
scripts:
  - name: A
    description: D
    path: a.bat
    type: bat
`)
	for _, code := range []string{"missing-id", "missing-alias"} {
		if !hasBlocking(e, code) {
			t.Fatalf("want %s blocking, got %v", code, blockingCodes(e))
		}
	}
}

func TestDuplicateIDAndAlias(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	e := mustParse(t, `
version: 1
scripts:
  - id: same
    alias: x
    name: A
    description: D
    path: a.bat
    type: bat
  - id: same
    alias: X
    name: B
    description: D
    path: a.bat
    type: bat
`)
	for _, code := range []string{"dup-id", "dup-alias"} {
		if !hasBlocking(e, code) {
			t.Fatalf("want %s blocking, got %v", code, blockingCodes(e))
		}
	}
}

func TestBadTypeAndEncoding(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: aa
    name: A
    description: D
    path: a.bat
    type: weird
    encoding: latin1
`)
	for _, code := range []string{"bad-type", "bad-encoding"} {
		if !hasBlocking(e, code) {
			t.Fatalf("want %s blocking, got %v", code, blockingCodes(e))
		}
	}
}

func TestArgsMustBeStringArray(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	raw := `
version: 1
scripts:
  - id: a
    alias: aa
    name: A
    description: D
    path: a.bat
    type: bat
    args: ["ok", 42]
`
	if _, err := Parse([]byte(raw), filepath.Join(dir, "e.yaml")); err == nil {
		t.Fatal("want error for non-string arg")
	}
}

func TestDisabledScriptSkipsFieldChecks(t *testing.T) {
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: aa
    enabled: false
`)
	if len(e.Blocking()) > 0 {
		t.Fatalf("disabled script should not require name/desc/path, got %v", blockingCodes(e))
	}
	if e.Scripts[0].IsEnabled() {
		t.Fatal("script should be disabled")
	}
	if len(e.EnabledScripts()) != 0 {
		t.Fatal("no enabled scripts expected")
	}
}

func TestMissingScriptFileIsWarning(t *testing.T) {
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: aa
    name: A
    description: D
    path: not_here.bat
    type: bat
`)
	if len(e.Blocking()) > 0 {
		t.Fatalf("missing file should be a warning, got %v", blockingCodes(e))
	}
	found := false
	for _, is := range e.Warnings() {
		if is.Code == "missing-file" {
			found = true
		}
	}
	if !found {
		t.Fatal("want missing-file warning")
	}
}

func TestAutoTypeUnknownExtensionBlocking(t *testing.T) {
	writeScript(t, t.TempDir(), "a.txt", "hello")
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: aa
    name: A
    description: D
    path: a.txt
    type: auto
`)
	if !hasBlocking(e, "bad-auto-type") {
		t.Fatalf("want bad-auto-type blocking, got %v", blockingCodes(e))
	}
}

func TestFindByAlias(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	writeScript(t, dir, "b.bat", "")
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: Alpha
    name: A
    description: D
    path: a.bat
    type: bat
  - id: b
    alias: beta
    name: B
    description: D
    path: b.bat
    type: bat
    enabled: false
`)
	if _, err := e.FindByAlias("alpha"); err != nil {
		t.Fatalf("case-insensitive match failed: %v", err)
	}
	if _, err := e.FindByAlias("nope"); err == nil {
		t.Fatal("want not-found error")
	}
	if _, err := e.FindByAlias("beta"); err == nil {
		t.Fatal("disabled script must not run")
	}
	if _, err := e.FindByAlias("  "); err == nil {
		t.Fatal("blank alias must error")
	}
}

func TestBOMStripped(t *testing.T) {
	dir := t.TempDir()
	writeScript(t, dir, "a.bat", "")
	raw := append([]byte{0xEF, 0xBB, 0xBF}, []byte("version: 1\nname: bom\nscripts: []\n")...)
	e, err := Parse(raw, filepath.Join(dir, "e.yaml"))
	if err != nil {
		t.Fatalf("BOM entry should parse: %v", err)
	}
	if e.Name != "bom" {
		t.Fatalf("name = %q", e.Name)
	}
}

func TestResolvePath(t *testing.T) {
	e := mustParse(t, `
version: 1
scripts:
  - id: a
    alias: aa
    name: A
    description: D
    path: sub/a.bat
    type: bat
`)
	got := e.ResolvePath(&e.Scripts[0])
	if !strings.HasSuffix(got, string(filepath.Separator)+"sub"+string(filepath.Separator)+"a.bat") {
		t.Fatalf("resolved path = %q", got)
	}
}

func TestLoadFileError(t *testing.T) {
	if _, err := LoadFile(filepath.Join(t.TempDir(), "nope.yaml")); err == nil {
		t.Fatal("want error for missing file")
	}
}
