package runner

import (
	"bytes"
	"context"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"azkits/internal/config"
)

func fakeLookup(name string) (string, error) {
	return `C:\bin\` + name, nil
}

func TestBuildCommandKeepsArgumentBoundaries(t *testing.T) {
	tests := []struct {
		name       string
		typeName   config.ScriptType
		path       string
		wantExe    string
		wantPrefix []string
	}{
		{"bat", config.TypeBAT, `C:\中文 路径\run.bat`, `C:\bin\cmd.exe`, []string{"/d", "/s", "/c"}},
		{"powershell", config.TypePowerShell, `C:\scripts\run.ps1`, `C:\bin\pwsh.exe`, []string{"-NoLogo", "-NoProfile", "-File"}},
		{"python", config.TypePython, `C:\scripts\run.py`, `C:\bin\python.exe`, []string{"-u"}},
		{"shell", config.TypeShell, `C:\scripts\run.sh`, `C:\bin\bash.exe`, nil},
		{"exe", config.TypeEXE, `C:\apps\run.exe`, `C:\apps\run.exe`, nil},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg := config.ScriptConfig{Type: tt.typeName, Path: tt.path, Args: []string{"a b", "&unsafe"}}
			exe, args, err := BuildCommand(cfg, fakeLookup)
			if err != nil {
				t.Fatal(err)
			}
			if exe != tt.wantExe {
				t.Fatalf("exe = %q, want %q", exe, tt.wantExe)
			}
			joined := append([]string{}, tt.wantPrefix...)
			if tt.typeName != config.TypeEXE {
				joined = append(joined, tt.path)
			}
			joined = append(joined, "a b", "&unsafe")
			if strings.Join(args, "|") != strings.Join(joined, "|") {
				t.Fatalf("args = %#v, want %#v", args, joined)
			}
		})
	}
}

func TestBuildCommandAutoAndMissingInterpreter(t *testing.T) {
	cfg := config.ScriptConfig{Type: config.TypeAuto, Path: `C:\scripts\run.ps1`}
	if exe, _, err := BuildCommand(cfg, fakeLookup); err != nil || !strings.HasSuffix(exe, "pwsh.exe") {
		t.Fatalf("BuildCommand() = %q, %v", exe, err)
	}
	missing := func(string) (string, error) { return "", errors.New("not found") }
	if _, _, err := BuildCommand(cfg, missing); err == nil {
		t.Fatal("missing interpreter must fail")
	}
}

func TestRunCapturesStdoutStderrAndExitCode(t *testing.T) {
	dir := t.TempDir()
	helper := filepath.Join(dir, "helper.bat")
	if err := os.WriteFile(helper, []byte("@echo stdout-line\r\n@echo stderr-line 1>&2\r\n@exit /b 7\r\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	cfg := config.ScriptConfig{ID: "helper", Type: config.TypeBAT, Path: helper, CWD: dir, Encoding: "utf-8"}
	var events []Event
	result, err := New().Run(context.Background(), cfg, func(event Event) { events = append(events, event) })
	if err != nil {
		t.Fatalf("Run() error = %v", err)
	}
	if result.ExitCode != 7 {
		t.Fatalf("exit code = %d, want 7", result.ExitCode)
	}
	var out, stderr string
	for _, event := range events {
		if event.Stream == Stdout {
			out += string(event.Data)
		}
		if event.Stream == Stderr {
			stderr += string(event.Data)
		}
	}
	if !strings.Contains(out, "stdout-line") || !strings.Contains(stderr, "stderr-line") {
		t.Fatalf("stdout=%q stderr=%q", out, stderr)
	}
}

func TestRunInteractiveConnectsStdinStdoutAndStderr(t *testing.T) {
	dir := t.TempDir()
	helper := filepath.Join(dir, "interactive.bat")
	body := "@set /p value=prompt:\r\n@echo got:%value%\r\n@echo err:%value% 1>&2\r\n"
	if err := os.WriteFile(helper, []byte(body), 0o600); err != nil {
		t.Fatal(err)
	}
	cfg := config.ScriptConfig{ID: "interactive", Type: config.TypeBAT, Path: helper, CWD: dir, Encoding: "utf-8", Interactive: true}
	var stdout, stderr bytes.Buffer
	result, err := New().RunInteractive(context.Background(), cfg, strings.NewReader("hello\r\n"), &stdout, &stderr)
	if err != nil {
		t.Fatalf("RunInteractive() error = %v", err)
	}
	if result.ExitCode != 0 || !strings.Contains(stdout.String(), "got:hello") || !strings.Contains(stderr.String(), "err:hello") {
		t.Fatalf("result=%#v stdout=%q stderr=%q", result, stdout.String(), stderr.String())
	}
}
