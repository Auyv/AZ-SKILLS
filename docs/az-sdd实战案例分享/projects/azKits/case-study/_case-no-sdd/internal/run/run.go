package run

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"azkits/internal/entry"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

// Request carries everything needed to build the subprocess for a script.
type Request struct {
	Entry  *entry.Entry
	Script *entry.Script
}

// ResolveType applies the "auto" type inference.
func ResolveType(s *entry.Script) (entry.Type, error) {
	t := s.Type
	if t == "" {
		t = entry.TypeAuto
	}
	if t == entry.TypeAuto {
		t = entry.InferType(s.Path)
		if t == "" {
			return "", fmt.Errorf("无法从路径 %q 推断 type，请在 YAML 中显式指定", s.Path)
		}
	}
	if !entry.SupportedTypes[t] {
		return "", fmt.Errorf("type %q 不受支持", t)
	}
	return t, nil
}

// BuildCommand returns the configured subprocess for a script. The process
// inherits the current environment and runs with the entry directory as its
// working directory. Arguments come exclusively from the YAML entry; no free
// arguments are accepted at runtime.
func BuildCommand(req *Request) (*exec.Cmd, error) {
	s := req.Script
	e := req.Entry
	if s == nil {
		return nil, fmt.Errorf("脚本为空")
	}
	if !s.IsEnabled() {
		return nil, fmt.Errorf("脚本 %q 已禁用", s.Alias)
	}
	typ, err := ResolveType(s)
	if err != nil {
		return nil, err
	}

	path := e.ResolvePath(s)
	if path == "" {
		return nil, fmt.Errorf("脚本路径为空")
	}
	if _, err := os.Stat(path); err != nil {
		return nil, fmt.Errorf("脚本文件不存在或不可访问: %s (%v)", path, err)
	}

	var argv []string
	switch typ {
	case entry.TypeBat, entry.TypeCmd:
		argv = append([]string{"cmd.exe", "/d", "/c", quoteForCmd(path)}, quoteEach(s.Args)...)
	case entry.TypePowerShell:
		ps, err := lookPowerShell()
		if err != nil {
			return nil, err
		}
		argv = append([]string{ps, "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", path}, s.Args...)
	case entry.TypePython:
		py, err := lookPython()
		if err != nil {
			return nil, err
		}
		argv = append([]string{py, path}, s.Args...)
	case entry.TypeShell:
		sh, err := lookShell()
		if err != nil {
			return nil, err
		}
		argv = append([]string{sh, path}, s.Args...)
	case entry.TypeExe:
		argv = append([]string{path}, s.Args...)
	default:
		return nil, fmt.Errorf("type %q 不受支持", typ)
	}

	cmd := exec.Command(argv[0], argv[1:]...)
	cmd.Dir = e.BaseDir
	cmd.Env = append(os.Environ(),
		"AZKITS_ENTRY="+e.FilePath,
		"AZKITS_ALIAS="+s.Alias,
		"AZKITS_ID="+s.ID,
	)
	return cmd, nil
}

// DecodeReader wraps a raw byte stream with the decoder for the given encoding,
// producing UTF-8 text. utf-8 and utf-8-sig are passed through (with BOM
// stripping for utf-8-sig); gbk and gb18030 are converted from the legacy
// code page.
func DecodeReader(r io.Reader, enc entry.Encoding) io.Reader {
	switch enc {
	case entry.EncodingUTF8Sig:
		return stripBOMReader(r)
	case entry.EncodingGBK:
		return transform.NewReader(r, simplifiedchinese.GBK.NewDecoder())
	case entry.EncodingGB18030:
		return transform.NewReader(r, simplifiedchinese.GB18030.NewDecoder())
	default:
		return r
	}
}

// LineScanner splits a decoded stream into lines. It tolerates CRLF, very long
// lines (dropped) and NUL bytes.
func LineScanner(r io.Reader) *bufio.Scanner {
	sc := bufio.NewScanner(r)
	sc.Buffer(make([]byte, 64*1024), 1024*1024)
	sc.Split(func(data []byte, atEOF bool) (advance int, token []byte, err error) {
		for i := 0; i < len(data); i++ {
			if data[i] == '\n' {
				line := data[:i]
				line = bytes.TrimRight(line, "\r")
				return i + 1, line, nil
			}
		}
		if atEOF && len(data) > 0 {
			return len(data), bytes.TrimRight(data, "\r"), nil
		}
		return 0, nil, nil
	})
	return sc
}

// Result describes the outcome of a foreground run.
type Result struct {
	ExitCode int
	Err      error
}

// ResultFromErr builds a Result from cmd.Wait's error: exit codes are
// preserved, and only non-exit failures are reported as Err.
func ResultFromErr(waitErr error) Result {
	return Result{ExitCode: exitCodeOf(waitErr), Err: errFromExit(waitErr)}
}

// RunOnce runs a script in the foreground and writes its (decoded) output to
// stdout/stderr. When the script is interactive, the child inherits the real
// terminal instead of capturing output.
func RunOnce(req *Request, stdin io.Reader, stdout, stderr io.Writer) Result {
	cmd, err := BuildCommand(req)
	if err != nil {
		return Result{ExitCode: 1, Err: err}
	}

	if req.Script.Interactive {
		cmd.Stdin = os.Stdin
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return Result{ExitCode: exitCodeOf(err), Err: nil}
		}
		return Result{ExitCode: 0}
	}

	cmd.Stdin = stdin
	outPipe, err := cmd.StdoutPipe()
	if err != nil {
		return Result{ExitCode: 1, Err: err}
	}
	errPipe, err := cmd.StderrPipe()
	if err != nil {
		return Result{ExitCode: 1, Err: err}
	}
	if err := cmd.Start(); err != nil {
		return Result{ExitCode: 1, Err: err}
	}

	dec := DecodeReader(outPipe, req.Script.Encoding)
	decErr := DecodeReader(errPipe, req.Script.Encoding)
	done := make(chan struct{}, 2)
	copyOut := func(r io.Reader, w io.Writer) {
		defer func() { done <- struct{}{} }()
		sc := LineScanner(r)
		for sc.Scan() {
			fmt.Fprintln(w, sc.Text())
		}
	}
	go copyOut(dec, stdout)
	go copyOut(decErr, stderr)
	<-done
	<-done

	err = cmd.Wait()
	return Result{ExitCode: exitCodeOf(err), Err: errFromExit(err)}
}

func exitCodeOf(err error) int {
	if err == nil {
		return 0
	}
	var ee *exec.ExitError
	if ok := asExitError(err, &ee); ok {
		if code := ee.ExitCode(); code >= 0 {
			return code
		}
	}
	return 1
}

func errFromExit(err error) error {
	if err == nil {
		return nil
	}
	var ee *exec.ExitError
	if ok := asExitError(err, &ee); ok {
		return nil
	}
	return err
}

// asExitError is a small indirection kept over errors.As so the exec helper
// stays testable without exec types leaking everywhere.
func asExitError(err error, out **exec.ExitError) bool {
	if ee, ok := err.(*exec.ExitError); ok {
		*out = ee
		return true
	}
	return false
}

// ---- executable discovery -------------------------------------------

func lookPowerShell() (string, error) {
	if p, err := exec.LookPath("pwsh.exe"); err == nil {
		return p, nil
	}
	if p, err := exec.LookPath("powershell.exe"); err == nil {
		return p, nil
	}
	return "", fmt.Errorf("未找到 PowerShell（powershell.exe / pwsh.exe）")
}

func lookPython() (string, error) {
	for _, name := range []string{"python.exe", "python3.exe", "py.exe"} {
		if p, err := exec.LookPath(name); err == nil {
			return p, nil
		}
	}
	return "", fmt.Errorf("未找到 Python 解释器（python / python3 / py）")
}

func lookShell() (string, error) {
	for _, name := range []string{"bash.exe", "sh.exe", "git-bash.exe"} {
		if p, err := exec.LookPath(name); err == nil {
			return p, nil
		}
	}
	return "", fmt.Errorf("未找到 Shell 解释器（bash / sh）")
}

// quoteEach quotes every argument for the cmd.exe command line.
func quoteEach(args []string) []string {
	out := make([]string, len(args))
	for i, a := range args {
		out[i] = quoteForCmd(a)
	}
	return out
}

// quoteForCmd quotes a token for cmd.exe when it needs it.
func quoteForCmd(s string) string {
	if s == "" || strings.ContainsAny(s, " \t\"&|<>^()%!") {
		return `"` + strings.ReplaceAll(s, `"`, `""`) + `"`
	}
	return s
}

// stripBOMReader drops a leading UTF-8 BOM from the stream.
func stripBOMReader(r io.Reader) io.Reader {
	br := bufio.NewReader(r)
	if b, _ := br.Peek(3); len(b) == 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF {
		_, _ = br.Discard(3)
	}
	return br
}

// ValidateScriptFile performs an upfront check of the script target, used by
// commands that want a friendly error before spawning anything.
func ValidateScriptFile(req *Request) error {
	if req.Script == nil {
		return fmt.Errorf("脚本为空")
	}
	path := req.Entry.ResolvePath(req.Script)
	if path == "" {
		return fmt.Errorf("脚本路径为空")
	}
	if _, err := os.Stat(path); err != nil {
		return fmt.Errorf("脚本文件不存在或不可访问: %s (%v)", path, err)
	}
	if _, err := ResolveType(req.Script); err != nil {
		return err
	}
	return nil
}

// AbsPath returns the resolved absolute script path (used for messages).
func AbsPath(e *entry.Entry, s *entry.Script) string {
	return filepath.Clean(e.ResolvePath(s))
}
