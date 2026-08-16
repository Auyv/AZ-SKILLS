package main

import (
	"fmt"
	"io"
	"os"
	"strings"

	tea "charm.land/bubbletea/v2"
	"golang.org/x/term"

	"azkits/internal/entry"
	"azkits/internal/run"
	"azkits/internal/store"
	"azkits/internal/tui"
)

const version = "0.1.0"

const usage = `azKits — Windows 离线终端脚本工具箱

用法:
  azkits <入口.yaml>                进入交互式 TUI
  azkits <入口.yaml> list           列出脚本
  azkits <入口.yaml> run <alias>    运行脚本
  azkits <入口.yaml> check          检查入口是否可加载

选项:
  --favorites-dir <dir>   覆盖收藏存储目录
  -h, --help              显示帮助
  -V, --version           显示版本
`

type options struct {
	favoritesDir string
	help         bool
	version      bool
}

func main() {
	os.Exit(runCLI(os.Args[1:], os.Stdin, os.Stdout, os.Stderr))
}

func runCLI(args []string, stdin io.Reader, stdout, stderr io.Writer) int {
	// Re-encode UTF-8 output into the console code page so Chinese text is not
	// garbled in legacy GBK consoles. Only applies when writing to a terminal;
	// redirected output stays UTF-8.
	if f, ok := stdout.(*os.File); ok && term.IsTerminal(int(f.Fd())) {
		stdout = consoleWriter(stdout)
	}
	if f, ok := stderr.(*os.File); ok && term.IsTerminal(int(f.Fd())) {
		stderr = consoleWriter(stderr)
	}

	opts, rest, err := parseArgs(args)
	if err != nil {
		fmt.Fprintln(stderr, "参数错误:", err)
		fmt.Fprint(stderr, usage)
		return 2
	}
	if opts.help {
		fmt.Fprint(stdout, usage)
		return 0
	}
	if opts.version {
		fmt.Fprintf(stdout, "azKits %s\n", version)
		return 0
	}
	if len(rest) == 0 {
		fmt.Fprint(stderr, usage)
		return 2
	}
	entryPath := rest[0]
	cmd := "tui"
	if len(rest) >= 2 {
		cmd = strings.ToLower(rest[1])
	}

	switch cmd {
	case "list":
		return cmdList(entryPath, opts, stdout, stderr)
	case "run":
		if len(rest) < 3 {
			fmt.Fprintln(stderr, "run 需要指定 <alias>")
			return 2
		}
		return cmdRun(entryPath, rest[2], opts, stdin, stdout, stderr)
	case "check":
		return cmdCheck(entryPath, stdout)
	case "tui", "ui":
		return cmdTUI(entryPath, opts, stdout, stderr)
	default:
		fmt.Fprintf(stderr, "未知命令 %q\n\n", cmd)
		fmt.Fprint(stderr, usage)
		return 2
	}
}

func parseArgs(args []string) (*options, []string, error) {
	opts := &options{}
	var rest []string
	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "-h" || a == "--help":
			opts.help = true
		case a == "-V" || a == "--version":
			opts.version = true
		case a == "--favorites-dir":
			if i+1 >= len(args) {
				return nil, nil, fmt.Errorf("--favorites-dir 缺少参数")
			}
			i++
			opts.favoritesDir = args[i]
		case strings.HasPrefix(a, "--favorites-dir="):
			opts.favoritesDir = strings.TrimPrefix(a, "--favorites-dir=")
		case strings.HasPrefix(a, "-"):
			return nil, nil, fmt.Errorf("未知选项 %q", a)
		default:
			rest = append(rest, a)
		}
	}
	return opts, rest, nil
}

// loadEntry loads an entry and, when it has blocking issues, prints them and
// returns nil.
func loadEntry(path string, stderr io.Writer) *entry.Entry {
	e, err := entry.LoadFile(path)
	if err != nil {
		fmt.Fprintln(stderr, "加载失败:", err)
		return nil
	}
	if blk := e.Blocking(); len(blk) > 0 {
		for _, is := range blk {
			fmt.Fprintf(stderr, "  [%s] %s\n", is.Severity, is.Msg)
		}
		return nil
	}
	return e
}

func openStore(opts *options, stderr io.Writer) *store.Store {
	st, err := store.Open(opts.favoritesDir)
	if err != nil {
		fmt.Fprintln(stderr, "提示: 收藏存储不可用（已降级为无收藏）:", err)
		return store.NewEmptyStore()
	}
	return st
}

func cmdList(path string, opts *options, stdout, stderr io.Writer) int {
	e, err := entry.LoadFile(path)
	if err != nil {
		fmt.Fprintln(stderr, "加载失败:", err)
		return 1
	}
	blk := e.Blocking()
	if len(blk) > 0 {
		for _, is := range blk {
			fmt.Fprintf(stderr, "[%s] %s\n", is.Severity, is.Msg)
		}
	}

	st := openStore(opts, stderr)
	favs := map[string]bool{}
	for _, id := range st.FavoriteIDs() {
		for i := range e.Scripts {
			if e.Scripts[i].ID == id {
				favs[id] = true
				break
			}
		}
	}

	fmt.Fprintf(stdout, "入口: %s\n", path)
	fmt.Fprintf(stdout, "名称: %s\n", e.Name)
	if e.Description != "" {
		fmt.Fprintf(stdout, "描述: %s\n", e.Description)
	}
	fmt.Fprintln(stdout)

	var okCount, missingCount, disabledCount int
	for i := range e.Scripts {
		s := &e.Scripts[i]
		mark := " "
		if favs[s.ID] {
			mark = "★"
		}
		status := "已禁用"
		if s.IsEnabled() {
			abs := e.ResolvePath(s)
			if _, serr := os.Stat(abs); serr != nil {
				status = "文件缺失"
				missingCount++
			} else {
				status = "有效"
				okCount++
			}
		} else {
			disabledCount++
		}
		fmt.Fprintf(stdout, "%s [%-14s] %-18s %s  %s  %s\n",
			mark, s.Alias, truncate(s.Name, 18), truncate(s.Description, 40), status, s.Path)
	}
	fmt.Fprintln(stdout)
	fmt.Fprintf(stdout, "共 %d 个脚本（有效 %d，文件缺失 %d，已禁用 %d）\n", len(e.Scripts), okCount, missingCount, disabledCount)
	fmt.Fprintf(stdout, "收藏: %d\n", len(favs))

	if len(blk) > 0 {
		return 1
	}
	return 0
}

func cmdCheck(path string, stdout io.Writer) int {
	e, err := entry.LoadFile(path)
	if err != nil {
		fmt.Fprintln(stdout, "检查结果: 失败（无法加载）")
		fmt.Fprintln(stdout, "  原因:", err)
		return 1
	}
	fmt.Fprintf(stdout, "入口: %s\n", path)
	fmt.Fprintf(stdout, "名称: %s\n", e.Name)
	if e.Description != "" {
		fmt.Fprintf(stdout, "描述: %s\n", e.Description)
	}
	fmt.Fprintln(stdout)

	blocking := e.Blocking()
	warnings := e.Warnings()
	for _, is := range blocking {
		label := scriptLabel(e, is)
		fmt.Fprintf(stdout, "[%s] %s%s\n", is.Severity, label, is.Msg)
	}
	for _, is := range warnings {
		label := scriptLabel(e, is)
		fmt.Fprintf(stdout, "[%s] %s%s\n", is.Severity, label, is.Msg)
	}

	enabled := len(e.EnabledScripts())
	fmt.Fprintln(stdout)
	if len(blocking) == 0 {
		fmt.Fprintf(stdout, "结果: 可加载（%d 个脚本，其中启用 %d，阻塞 0）\n", len(e.Scripts), enabled)
		return 0
	}
	fmt.Fprintf(stdout, "结果: 不可加载（阻塞 %d，警告 %d）\n", len(blocking), len(warnings))
	return 1
}

func scriptLabel(e *entry.Entry, is entry.Issue) string {
	if is.Script < 0 || is.Script >= len(e.Scripts) {
		return ""
	}
	return fmt.Sprintf("[%s] ", e.Scripts[is.Script].Alias)
}

func cmdRun(path, alias string, opts *options, stdin io.Reader, stdout, stderr io.Writer) int {
	e := loadEntry(path, stderr)
	if e == nil {
		return 1
	}
	s, err := e.FindByAlias(alias)
	if err != nil {
		fmt.Fprintln(stderr, "运行失败:", err)
		return 1
	}
	req := &run.Request{Entry: e, Script: s}
	if err := run.ValidateScriptFile(req); err != nil {
		fmt.Fprintln(stderr, "运行失败:", err)
		return 1
	}
	res := run.RunOnce(req, stdin, stdout, stderr)
	if res.Err != nil {
		fmt.Fprintln(stderr, "运行失败:", res.Err)
		return 1
	}
	if res.ExitCode != 0 {
		fmt.Fprintf(stderr, "运行结束，退出码 %d\n", res.ExitCode)
	} else {
		fmt.Fprintf(stderr, "运行完成，退出码 %d\n", res.ExitCode)
	}
	return res.ExitCode
}

func cmdTUI(path string, opts *options, stdout, stderr io.Writer) int {
	if !term.IsTerminal(int(os.Stdin.Fd())) {
		fmt.Fprintln(stderr, "交互式 TUI 需要终端（请使用 list / run / check 非交互命令）")
		return 1
	}
	e := loadEntry(path, stderr)
	if e == nil {
		return 1
	}
	st := openStore(opts, stderr)

	m := tui.New(e, st)
	p := tea.NewProgram(m)
	m.App().Program(p)
	restoreCP := setUTF8Console()
	defer restoreCP()
	if _, err := p.Run(); err != nil {
		fmt.Fprintln(stderr, "TUI 退出异常:", err)
		return 1
	}
	return 0
}

// setUTF8Console switches the console output code page to UTF-8 for the TUI
// and returns a restore function.
func setUTF8Console() func() {
	old := consoleOutputCP()
	setConsoleOutputCP(65001)
	return func() {
		if old != 65001 {
			setConsoleOutputCP(old)
		}
	}
}

func truncate(s string, w int) string {
	if len(s) <= w {
		return s
	}
	r := []rune(s)
	if len(r) <= w {
		return s
	}
	return string(r[:w-1]) + "…"
}
