package logexport

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

type LookupFunc func(string) (string, error)
type StartFunc func(executable, document string) error

type EditorOpener struct {
	Lookup LookupFunc
	Start  StartFunc
}

func (o EditorOpener) Open(document string) (string, error) {
	lookup := o.Lookup
	useStandardPaths := lookup == nil
	if lookup == nil {
		lookup = exec.LookPath
	}
	start := o.Start
	if start == nil {
		start = func(executable, document string) error {
			return exec.Command(executable, document).Start()
		}
	}
	var failures []error
	if executable, err := findNotepadPlusPlus(lookup, useStandardPaths); err == nil {
		if err = start(executable, document); err == nil {
			return "Notepad++", nil
		}
		failures = append(failures, fmt.Errorf("Notepad++ 打开失败: %w", err))
	} else {
		failures = append(failures, err)
	}
	if executable, err := findNotepad(lookup, useStandardPaths); err == nil {
		if err = start(executable, document); err == nil {
			return "Notepad", nil
		}
		failures = append(failures, fmt.Errorf("Notepad 打开失败: %w", err))
	} else {
		failures = append(failures, err)
	}
	return "", errors.Join(failures...)
}

func findNotepadPlusPlus(lookup LookupFunc, useStandardPaths bool) (string, error) {
	if path, err := lookup("notepad++.exe"); err == nil {
		return path, nil
	}
	if !useStandardPaths {
		return "", fmt.Errorf("未找到 Notepad++")
	}
	for _, root := range []string{os.Getenv("ProgramFiles"), os.Getenv("ProgramFiles(x86)")} {
		if root == "" {
			continue
		}
		candidate := filepath.Join(root, "Notepad++", "notepad++.exe")
		if info, err := os.Stat(candidate); err == nil && !info.IsDir() {
			return candidate, nil
		}
	}
	return "", fmt.Errorf("未找到 Notepad++")
}

func findNotepad(lookup LookupFunc, useStandardPaths bool) (string, error) {
	if path, err := lookup("notepad.exe"); err == nil {
		return path, nil
	}
	if !useStandardPaths {
		return "", fmt.Errorf("未找到 Notepad")
	}
	if root := os.Getenv("WINDIR"); root != "" {
		candidate := filepath.Join(root, "System32", "notepad.exe")
		if info, err := os.Stat(candidate); err == nil && !info.IsDir() {
			return candidate, nil
		}
	}
	return "", fmt.Errorf("未找到 Notepad")
}

type ExportResult struct {
	Path      string
	Editor    string
	OpenError error
}

type Service struct {
	Writer Writer
	Opener EditorOpener
}

func (s Service) Export(run Run) (ExportResult, error) {
	path, err := s.Writer.Write(run)
	if err != nil {
		return ExportResult{}, err
	}
	editor, openErr := s.Opener.Open(path)
	return ExportResult{Path: path, Editor: editor, OpenError: openErr}, nil
}
