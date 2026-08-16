package terminal

import (
	"bytes"
	"context"
	"errors"
	"os/exec"
	"strings"
	"testing"
	"time"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/logexport"
	"azkits/internal/runner"
	"azkits/internal/task"
	tea "charm.land/bubbletea/v2"
)

type instantExecutor struct{}

type failingStarter struct{}

type fakeLogExporter struct {
	run    logexport.Run
	result logexport.ExportResult
	err    error
}

func (f *fakeLogExporter) Export(run logexport.Run) (logexport.ExportResult, error) {
	f.run = run
	return f.result, f.err
}

func (failingStarter) Start(string) error      { return errors.New("start failed") }
func (failingStarter) State(string) task.State { return task.Ready }

func (instantExecutor) Run(_ context.Context, _ config.ScriptConfig, sink func(runner.Event)) (runner.Result, error) {
	sink(runner.Event{Stream: runner.Stdout, Data: []byte("完成\n")})
	return runner.Result{PID: 7, ExitCode: 0}, nil
}

func (instantExecutor) Command(ctx context.Context, _ config.ScriptConfig) (*exec.Cmd, error) {
	return exec.CommandContext(ctx, "cmd.exe", "/d", "/c", "exit", "0"), nil
}

func appModel(t *testing.T) Model {
	t.Helper()
	scripts := []config.ScriptConfig{
		{ID: "one", Alias: "device-init", Name: "设备初始化", Description: "初始化设备", Enabled: true},
		{ID: "two", Alias: "device-check", Name: "设备检查", Description: "检查连接", Enabled: true},
	}
	c, err := catalog.New(scripts)
	if err != nil {
		t.Fatal(err)
	}
	cfg := config.ToolboxConfig{Source: `C:\入口\toolbox.yaml`, Toolbox: config.ToolboxMeta{Name: "测试工具箱", Description: "中文说明"}, Scripts: scripts}
	return NewModel(cfg, c, task.New(instantExecutor{}, scripts))
}

func interactiveAppModel(t *testing.T) Model {
	t.Helper()
	scripts := []config.ScriptConfig{{ID: "interactive", Alias: "interactive", Name: "interactive", Enabled: true, Interactive: true, Encoding: "utf-8"}}
	c, err := catalog.New(scripts)
	if err != nil {
		t.Fatal(err)
	}
	cfg := config.ToolboxConfig{Toolbox: config.ToolboxMeta{Name: "test"}, Scripts: scripts}
	return NewModel(cfg, c, task.New(instantExecutor{}, scripts))
}

func key(text string, code rune) tea.KeyPressMsg {
	return tea.KeyPressMsg(tea.Key{Text: text, Code: code})
}

func ctrlP() tea.KeyPressMsg {
	return tea.KeyPressMsg(tea.Key{Code: 'p', Mod: tea.ModCtrl})
}

func update(t *testing.T, model Model, msg tea.Msg) Model {
	t.Helper()
	updated, _ := model.Update(msg)
	result, ok := updated.(Model)
	if !ok {
		t.Fatalf("model type = %T", updated)
	}
	return result
}

func TestModelRendersAndHandlesKeyboard(t *testing.T) {
	m := appModel(t)
	if m.Init() == nil {
		t.Fatal("Init must subscribe to task events")
	}
	m = update(t, m, tea.WindowSizeMsg{Width: 120, Height: 40})
	m = update(t, m, key("/", '/'))
	if !m.core.Visible() || len(m.core.Candidates()) != 2 {
		t.Fatalf("slash candidates = %#v", m.core.Candidates())
	}
	m = update(t, m, key("", tea.KeyDown))
	m = update(t, m, key("", tea.KeyTab))
	if m.input.Value() != "/device-check" {
		t.Fatalf("input = %q", m.input.Value())
	}
	m = update(t, m, key("", tea.KeyEnter))
	if !strings.Contains(m.message, "已启动") {
		t.Fatalf("message = %q", m.message)
	}
	m = update(t, m, key("/", '/'))
	m = update(t, m, key("", tea.KeyEscape))
	if m.core.Visible() {
		t.Fatal("Esc did not close candidates")
	}
	view := m.View()
	if !strings.Contains(view.Content, "测试工具箱") || !strings.Contains(view.Content, "入口") || view.MouseMode != tea.MouseModeNone {
		t.Fatalf("view = %#v", view)
	}
}

func TestModelClearsSuccessfulCommandAndBrowsesHistory(t *testing.T) {
	m := appModel(t)
	m.input.SetValue("/device-init")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))
	if got := m.input.Value(); got != "" {
		t.Fatalf("successful input = %q, want empty", got)
	}
	if got := m.core.Input(); got != "" {
		t.Fatalf("core input = %q, want empty", got)
	}
	if len(m.history) != 1 || m.history[0] != "/device-init" {
		t.Fatalf("history = %#v", m.history)
	}

	m.input.SetValue("/device-check")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))
	if len(m.history) != 2 {
		t.Fatalf("history = %#v", m.history)
	}

	m = update(t, m, key("", tea.KeyUp))
	if got := m.input.Value(); got != "/device-check" {
		t.Fatalf("latest history = %q", got)
	}
	m = update(t, m, key("", tea.KeyUp))
	if got := m.input.Value(); got != "/device-init" {
		t.Fatalf("older history = %q", got)
	}
	m = update(t, m, key("", tea.KeyUp))
	if got := m.input.Value(); got != "/device-init" {
		t.Fatalf("oldest boundary = %q", got)
	}
	m = update(t, m, key("", tea.KeyDown))
	if got := m.input.Value(); got != "/device-check" {
		t.Fatalf("newer history = %q", got)
	}
	m = update(t, m, key("", tea.KeyDown))
	if got := m.input.Value(); got != "" {
		t.Fatalf("newest boundary = %q, want empty", got)
	}
}

func TestModelKeepsFailedInputOutOfHistoryAndIsolatesInstances(t *testing.T) {
	m := appModel(t)
	m.input.SetValue("/missing")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))
	if got := m.input.Value(); got != "/missing" {
		t.Fatalf("failed input = %q", got)
	}
	if len(m.history) != 0 {
		t.Fatalf("failed history = %#v", m.history)
	}

	other := appModel(t)
	other = update(t, other, key("", tea.KeyUp))
	if got := other.input.Value(); got != "" || len(other.history) != 0 {
		t.Fatalf("isolated instance input=%q history=%#v", got, other.history)
	}
}

func TestModelKeepsStartFailureAndRepeatsEditedHistory(t *testing.T) {
	m := appModel(t)
	core, starter := newCore(t)
	m.core = core
	m.input.SetValue("/device-init")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))

	m = update(t, m, key("", tea.KeyUp))
	m = update(t, m, key("x", 'x'))
	if got := m.input.Value(); got != "/device-initx" {
		t.Fatalf("edited history = %q", got)
	}
	m = update(t, m, key("", tea.KeyUp))
	if got := m.input.Value(); got != "/device-init" {
		t.Fatalf("recalled history = %q", got)
	}
	m = update(t, m, key("", tea.KeyEnter))
	if len(starter.started) != 2 || len(m.history) != 2 || m.input.Value() != "" {
		t.Fatalf("started=%#v history=%#v input=%q", starter.started, m.history, m.input.Value())
	}

	m.core = NewCore(core.catalog, failingStarter{})
	m.input.SetValue("/device-check")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))
	if got := m.input.Value(); got != "/device-check" {
		t.Fatalf("start failure input = %q", got)
	}
	if len(m.history) != 2 {
		t.Fatalf("start failure history = %#v", m.history)
	}
}

func TestModelHandlesTaskEventsAndVisibleRange(t *testing.T) {
	m := appModel(t)
	m = update(t, m, task.Event{ScriptID: "one", State: task.Running, Stream: runner.Stdout, Data: []byte("日志\n")})
	m = update(t, m, task.Event{ScriptID: "one", State: task.Succeeded, PID: 7, ExitCode: 0})
	if content := m.viewport.GetContent(); !strings.Contains(content, "日志") || !strings.Contains(content, "succeeded") {
		t.Fatalf("log content = %q", content)
	}
	if start, end := visibleRange(10, 20, 8); start > 10 || end-start != 8 || end > 20 {
		t.Fatalf("visibleRange = %d,%d", start, end)
	}
}

func TestCtrlPRejectsMissingAndRunningExecution(t *testing.T) {
	m := appModel(t)
	updated, command := m.Update(ctrlP())
	m = updated.(Model)
	if command != nil || !strings.Contains(m.message, "没有可导出") {
		t.Fatalf("missing command=%v message=%q", command, m.message)
	}

	m.input.SetValue("/device-init")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))
	updated, command = m.Update(ctrlP())
	m = updated.(Model)
	if command != nil || !strings.Contains(m.message, "脚本结束") {
		t.Fatalf("running command=%v message=%q", command, m.message)
	}
}

func TestCtrlPExportsLatestFinishedExecutionAndReportsEditor(t *testing.T) {
	m := appModel(t)
	exporter := &fakeLogExporter{result: logexport.ExportResult{Path: `C:\logs\run.txt`, Editor: "Notepad++"}}
	m.exporter = exporter
	m.input.SetValue("/device-init")
	m.core.SetInput(m.input.Value())
	m = update(t, m, key("", tea.KeyEnter))
	m = update(t, m, task.Event{ScriptID: "one", State: task.Running, Stream: runner.Stdout, Data: []byte("输出 1\n")})
	m = update(t, m, task.Event{ScriptID: "one", State: task.Running, Stream: runner.Stderr, Data: []byte("错误 2\n")})
	m = update(t, m, task.Event{ScriptID: "one", State: task.Succeeded, PID: 7, ExitCode: 0})

	updated, command := m.Update(ctrlP())
	m = updated.(Model)
	if command == nil || !m.exporting {
		t.Fatalf("command=%v exporting=%v", command, m.exporting)
	}
	message := command()
	m = update(t, m, message)
	if exporter.run.ScriptID != "one" || exporter.run.ScriptName != "设备初始化" || exporter.run.Alias != "device-init" || len(exporter.run.Entries) != 2 {
		t.Fatalf("exported run = %#v", exporter.run)
	}
	if exporter.run.Entries[0].Text != "输出 1\n" || exporter.run.Entries[1].Text != "错误 2\n" {
		t.Fatalf("entries = %#v", exporter.run.Entries)
	}
	if exporter.run.ExitCode == nil || *exporter.run.ExitCode != 0 || exporter.run.EndedAt.Before(exporter.run.StartedAt) {
		t.Fatalf("times/result = %#v", exporter.run)
	}
	if !strings.Contains(m.message, `C:\logs\run.txt`) || !strings.Contains(m.message, "Notepad++") || m.exporting {
		t.Fatalf("message=%q exporting=%v", m.message, m.exporting)
	}
}

func TestCtrlPSeparatesExportAndEditorFailure(t *testing.T) {
	m := appModel(t)
	run := logexport.Run{ToolboxName: "test", ScriptID: "one", Alias: "one", ScriptName: "one", StartedAt: time.Now()}
	if err := m.runs.Start(run); err != nil {
		t.Fatal(err)
	}
	code := 1
	m.runs.Finish("one", "failed", &code, time.Now())

	exporter := &fakeLogExporter{result: logexport.ExportResult{Path: `C:\logs\failed.txt`, OpenError: errors.New("editors missing")}}
	m.exporter = exporter
	updated, command := m.Update(ctrlP())
	m = updated.(Model)
	m = update(t, m, command())
	if !strings.Contains(m.message, "日志已导出") || !strings.Contains(m.message, "编辑器打开失败") || !strings.Contains(m.message, `C:\logs\failed.txt`) {
		t.Fatalf("open failure message = %q", m.message)
	}

	exporter.err = errors.New("disk full")
	updated, command = m.Update(ctrlP())
	m = updated.(Model)
	m = update(t, m, command())
	if !strings.Contains(m.message, "日志导出失败") || strings.Contains(m.message, "编辑器") {
		t.Fatalf("write failure message = %q", m.message)
	}
}

func TestModelStartsAndFinishesInteractiveCommand(t *testing.T) {
	m := interactiveAppModel(t)
	m.input.SetValue("/interactive")
	m.core.SetInput(m.input.Value())

	updated, command := m.Update(key("", tea.KeyEnter))
	m = updated.(Model)
	if command == nil {
		t.Fatal("interactive Enter must return an Exec command")
	}
	if m.input.Value() != "" || m.service.State("interactive") != task.Running {
		t.Fatalf("input=%q state=%s", m.input.Value(), m.service.State("interactive"))
	}

	m = update(t, m, interactiveFinishedMsg{
		scriptID: "interactive",
		encoding: "utf-8",
		result:   runner.Result{PID: 7, ExitCode: 0},
		stdout:   []byte("interactive output\n"),
	})
	if m.service.State("interactive") != task.Succeeded {
		t.Fatalf("state=%s", m.service.State("interactive"))
	}
	if !strings.Contains(m.viewport.GetContent(), "interactive output") {
		t.Fatalf("logs=%q", m.viewport.GetContent())
	}
	last, ok := m.runs.Last()
	if !ok || last.ScriptID != "interactive" || last.State != "succeeded" || len(last.Entries) != 1 || last.Entries[0].Text != "interactive output\n" {
		t.Fatalf("interactive export snapshot = %#v, ok=%v", last, ok)
	}
}

func TestInteractiveCommandConnectsAndCapturesTerminalIO(t *testing.T) {
	cmd := exec.Command("cmd.exe", "/d", "/v:on", "/c", "set /p answer=prompt: & echo got:!answer!")
	interactive := newInteractiveCommand(cmd)
	var liveOut bytes.Buffer
	interactive.SetStdin(strings.NewReader("hello\r\n"))
	interactive.SetStdout(&liveOut)
	interactive.SetStderr(&liveOut)
	if err := interactive.Run(); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(liveOut.String(), "got:hello") || !strings.Contains(interactive.stdout.String(), "got:hello") {
		t.Fatalf("live=%q captured=%q", liveOut.String(), interactive.stdout.String())
	}
	events := interactive.Events()
	var captured string
	for _, event := range events {
		if event.Stream == runner.Stdout {
			captured += string(event.Data)
		}
	}
	if !strings.Contains(captured, "got:hello") {
		t.Fatalf("events=%#v", events)
	}
}

func TestInteractiveCapturePreservesSharedWriteOrder(t *testing.T) {
	interactive := newInteractiveCommand(exec.Command("cmd.exe", "/d", "/c", "exit", "0"))
	var target bytes.Buffer
	stdout := interactiveCaptureWriter{command: interactive, stream: runner.Stdout, target: &target}
	stderr := interactiveCaptureWriter{command: interactive, stream: runner.Stderr, target: &target}
	if _, err := stdout.Write([]byte("out-1\n")); err != nil {
		t.Fatal(err)
	}
	if _, err := stderr.Write([]byte("err-2\n")); err != nil {
		t.Fatal(err)
	}
	if _, err := stdout.Write([]byte("out-3\n")); err != nil {
		t.Fatal(err)
	}
	events := interactive.Events()
	if len(events) != 3 || string(events[0].Data) != "out-1\n" || string(events[1].Data) != "err-2\n" || string(events[2].Data) != "out-3\n" {
		t.Fatalf("events=%#v", events)
	}
}

func TestLogBufferMinimumAndEmptyState(t *testing.T) {
	b := NewLogBuffer(0)
	if b.String() != "" {
		t.Fatalf("new buffer = %q", b.String())
	}
	b.Append("中文")
	if !strings.Contains(b.String(), "日志已截断") {
		t.Fatalf("buffer = %q", b.String())
	}
}
