package runner

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"

	"azkits/internal/config"
	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

type Stream string

const (
	Stdout Stream = "stdout"
	Stderr Stream = "stderr"
)

type Event struct {
	Stream Stream
	Data   []byte
}

type Result struct {
	PID      int
	ExitCode int
}

type LookupFunc func(string) (string, error)

type Runner struct {
	lookup LookupFunc
}

func New() *Runner {
	return &Runner{lookup: exec.LookPath}
}

func BuildCommand(cfg config.ScriptConfig, lookup LookupFunc) (string, []string, error) {
	typ := cfg.Type
	if typ == "" || typ == config.TypeAuto {
		switch strings.ToLower(filepath.Ext(cfg.Path)) {
		case ".bat":
			typ = config.TypeBAT
		case ".cmd":
			typ = config.TypeCMD
		case ".ps1":
			typ = config.TypePowerShell
		case ".py":
			typ = config.TypePython
		case ".sh":
			typ = config.TypeShell
		case ".exe":
			typ = config.TypeEXE
		default:
			typ = config.TypeEXE
		}
	}
	var exe string
	var args []string
	var err error
	switch typ {
	case config.TypeBAT, config.TypeCMD:
		exe, err = firstFound(lookup, "cmd.exe")
		args = []string{"/d", "/s", "/c", cfg.Path}
	case config.TypePowerShell:
		exe, err = firstFound(lookup, "pwsh.exe", "powershell.exe")
		args = []string{"-NoLogo", "-NoProfile", "-File", cfg.Path}
	case config.TypePython:
		exe, err = firstFound(lookup, "python.exe", "python3.exe")
		args = []string{"-u", cfg.Path}
	case config.TypeShell:
		exe, err = firstFound(lookup, "bash.exe", "sh.exe")
		args = []string{cfg.Path}
	case config.TypeEXE:
		exe = cfg.Path
		args = []string{cfg.Path}
	default:
		return "", nil, fmt.Errorf("不支持的脚本类型: %s", typ)
	}
	if err != nil {
		return "", nil, err
	}
	// EXE 的路径已经作为 executable 使用，不应在 argv 中重复。
	if typ == config.TypeEXE {
		args = nil
	}
	args = append(args, cfg.Args...)
	return exe, args, nil
}

func firstFound(lookup LookupFunc, names ...string) (string, error) {
	var last error
	for _, name := range names {
		path, err := lookup(name)
		if err == nil {
			return path, nil
		}
		last = err
	}
	return "", fmt.Errorf("未找到解释器 %s: %w", strings.Join(names, "/"), last)
}

func (r *Runner) Run(ctx context.Context, cfg config.ScriptConfig, sink func(Event)) (Result, error) {
	cmd, err := r.Command(ctx, cfg)
	if err != nil {
		return Result{}, err
	}
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return Result{}, fmt.Errorf("连接 stdout: %w", err)
	}
	stderr, err := cmd.StderrPipe()
	if err != nil {
		return Result{}, fmt.Errorf("连接 stderr: %w", err)
	}
	if err := cmd.Start(); err != nil {
		return Result{}, fmt.Errorf("启动 %s: %w", cfg.Name, err)
	}
	result := Result{PID: cmd.Process.Pid, ExitCode: -1}
	var sinkMu sync.Mutex
	emit := func(event Event) {
		sinkMu.Lock()
		defer sinkMu.Unlock()
		if sink != nil {
			sink(event)
		}
	}
	var wg sync.WaitGroup
	wg.Add(2)
	go copyStream(&wg, decodedReader(stdout, cfg.Encoding), Stdout, emit)
	go copyStream(&wg, decodedReader(stderr, cfg.Encoding), Stderr, emit)
	waitErr := cmd.Wait()
	wg.Wait()
	result.ExitCode = cmd.ProcessState.ExitCode()
	if ctx.Err() != nil {
		return result, ctx.Err()
	}
	var exitErr *exec.ExitError
	if waitErr != nil && !errors.As(waitErr, &exitErr) {
		return result, fmt.Errorf("等待进程: %w", waitErr)
	}
	return result, nil
}

func (r *Runner) Command(ctx context.Context, cfg config.ScriptConfig) (*exec.Cmd, error) {
	exe, args, err := BuildCommand(cfg, r.lookup)
	if err != nil {
		return nil, err
	}
	cmd := exec.CommandContext(ctx, exe, args...)
	cmd.Dir = cfg.CWD
	cmd.Env = append(os.Environ(), envPairs(cfg.Env)...)
	setProcessGroup(cmd)
	return cmd, nil
}

func (r *Runner) RunInteractive(ctx context.Context, cfg config.ScriptConfig, stdin io.Reader, stdout, stderr io.Writer) (Result, error) {
	cmd, err := r.Command(ctx, cfg)
	if err != nil {
		return Result{}, err
	}
	cmd.Stdin = stdin
	cmd.Stdout = stdout
	cmd.Stderr = stderr
	runErr := cmd.Run()
	result := Result{ExitCode: -1}
	if cmd.Process != nil {
		result.PID = cmd.Process.Pid
	}
	if cmd.ProcessState != nil {
		result.ExitCode = cmd.ProcessState.ExitCode()
	}
	if ctx.Err() != nil {
		return result, ctx.Err()
	}
	var exitErr *exec.ExitError
	if runErr != nil && !errors.As(runErr, &exitErr) {
		return result, fmt.Errorf("运行交互进程: %w", runErr)
	}
	return result, nil
}

func copyStream(wg *sync.WaitGroup, source io.Reader, stream Stream, sink func(Event)) {
	defer wg.Done()
	buffer := make([]byte, 32*1024)
	for {
		n, err := source.Read(buffer)
		if n > 0 {
			data := append([]byte(nil), buffer[:n]...)
			sink(Event{Stream: stream, Data: data})
		}
		if err != nil {
			return
		}
	}
}

func decodedReader(source io.Reader, encoding string) io.Reader {
	switch strings.ToLower(encoding) {
	case "gbk":
		return transform.NewReader(source, simplifiedchinese.GBK.NewDecoder())
	case "gb18030":
		return transform.NewReader(source, simplifiedchinese.GB18030.NewDecoder())
	default:
		return source
	}
}

// DecodeBytes converts captured process output to UTF-8 using the script's
// configured encoding.
func DecodeBytes(data []byte, encoding string) []byte {
	decoded, err := io.ReadAll(decodedReader(bytes.NewReader(data), encoding))
	if err != nil {
		return append([]byte(nil), data...)
	}
	return decoded
}

func envPairs(values map[string]string) []string {
	result := make([]string, 0, len(values))
	for key, value := range values {
		result = append(result, key+"="+value)
	}
	return result
}
