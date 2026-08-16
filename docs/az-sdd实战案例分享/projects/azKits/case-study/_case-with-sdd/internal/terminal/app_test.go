package terminal

import (
	"context"
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/favorite"
	"azkits/internal/logexport"
	"azkits/internal/runner"
	"azkits/internal/task"
	tea "charm.land/bubbletea/v2"
)

type modelFixture struct {
	model     Model
	service   *task.Service
	favs      *favorite.FavoritesStore
	recent    *favorite.RecentTracker
	runs      *logexport.Tracker
	exporter  *fakeExporter
	entryPath string
}

type fakeExporter struct {
	exported []logexport.Run
	result   logexport.ExportResult
	err      error
}

func (f *fakeExporter) Export(run logexport.Run) (logexport.ExportResult, error) {
	f.exported = append(f.exported, run)
	return f.result, f.err
}

type noopExecutor struct{}

func (n *noopExecutor) Run(ctx context.Context, cfg config.ScriptConfig, sink func(runner.Event)) (runner.Result, error) {
	return runner.Result{ExitCode: 0}, nil
}

func (n *noopExecutor) Command(ctx context.Context, cfg config.ScriptConfig) (*exec.Cmd, error) {
	return exec.Command("cmd.exe", "/c", "exit 0"), nil
}

func newModelFixture(t *testing.T) *modelFixture {
	t.Helper()
	dir := t.TempDir()
	runBat := filepath.Join(dir, "run.bat")
	if err := os.WriteFile(runBat, []byte("@echo off\r\necho done\r\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	entry := filepath.Join(dir, "entry.yaml")
	scripts := []config.ScriptConfig{
		{ID: "a", Alias: "alpha", Name: "阿尔法", Description: "第一个", Enabled: true, Path: runBat, Type: config.TypeBAT, CWD: dir, Encoding: "utf-8", Interactive: false},
		{ID: "b", Alias: "beta", Name: "贝塔", Description: "第二个", Enabled: true, Path: runBat, Type: config.TypeBAT, CWD: dir, Encoding: "utf-8", Interactive: false},
		{ID: "c", Alias: "gamma", Name: "伽马", Description: "第三个", Enabled: true, Path: runBat, Type: config.TypeBAT, CWD: dir, Encoding: "utf-8", Interactive: true},
	}
	cat, err := catalog.New(scripts)
	if err != nil {
		t.Fatal(err)
	}
	cfg := config.ToolboxConfig{Version: 1, Source: entry, Toolbox: config.ToolboxMeta{Name: "测试工具箱", Description: "描述"}, Scripts: scripts}
	service := task.New(&noopExecutor{}, scripts)
	favs, _ := favorite.Load(filepath.Join(t.TempDir(), "favs.json"))
	recent := favorite.NewRecentTracker(30)
	model := NewModel(cfg, cat, service, favs, recent)
	exporter := &fakeExporter{result: logexport.ExportResult{Path: `C:\logs\run.txt`, Editor: "Notepad"}}
	model.exporter = exporter
	return &modelFixture{model: model, service: service, favs: favs, recent: recent, runs: model.runs, exporter: exporter, entryPath: cfg.Source}
}

func keyMsg(value string) tea.KeyPressMsg {
	switch value {
	case "enter":
		return tea.KeyPressMsg{Code: tea.KeyEnter}
	case "esc":
		return tea.KeyPressMsg{Code: tea.KeyEscape}
	case "up":
		return tea.KeyPressMsg{Code: tea.KeyUp}
	case "down":
		return tea.KeyPressMsg{Code: tea.KeyDown}
	case "tab":
		return tea.KeyPressMsg{Code: tea.KeyTab}
	case "ctrl+p":
		return tea.KeyPressMsg{Code: 'p', Mod: tea.ModCtrl}
	case "ctrl+c":
		return tea.KeyPressMsg{Code: 'c', Mod: tea.ModCtrl}
	default:
		panic("未知按键: " + value)
	}
}

func setInput(fx *modelFixture, value string) {
	fx.model.input.SetValue(value)
	fx.model.core.SetInput(value)
}

func TestModelFavoriteCommand(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/fav alpha")
	updated, cmd := fx.model.Update(keyMsg("enter"))
	fx.model = updated.(Model)
	if cmd != nil {
		t.Errorf("收藏命令不应产生启动命令: %+v", cmd)
	}
	if !fx.favs.IsFavorite(fx.entryPath, "a") {
		t.Error("收藏未生效")
	}
	if fx.model.message == "" {
		t.Error("应显示收藏反馈")
	}
	if fx.model.input.Value() != "" {
		t.Errorf("收藏后输入应清空: %q", fx.model.input.Value())
	}
}

func TestModelFavoriteCommandInvalid(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/fav nope")
	updated, _ := fx.model.Update(keyMsg("enter"))
	fx.model = updated.(Model)
	if fx.favs.IsFavorite(fx.entryPath, "a") {
		t.Error("无效命令不应改变收藏")
	}
	if fx.model.message == "" {
		t.Error("应提示无效")
	}
}

func TestModelExecuteTracksRecent(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/beta")
	updated, _ := fx.model.Update(keyMsg("enter"))
	fx.model = updated.(Model)
	if got := fx.recent.List(); len(got) != 1 || got[0] != "b" {
		t.Errorf("最近使用未记录: %v", got)
	}
	if fx.model.input.Value() != "" {
		t.Errorf("成功后输入应清空: %q", fx.model.input.Value())
	}
	if len(fx.model.history) != 1 || fx.model.history[0] != "/beta" {
		t.Errorf("命令历史错误: %v", fx.model.history)
	}
}

func TestModelExportLatestRun(t *testing.T) {
	fx := newModelFixture(t)
	fx.runs.Start(logexport.Run{ToolboxName: "测试工具箱", ScriptID: "a", Alias: "alpha", ScriptName: "阿尔法", StartedAt: time.Now()})
	fx.runs.Finish("a", "succeeded", intPtr(0), time.Now())
	cmd := fx.model.exportLatestRun()
	if cmd == nil {
		t.Fatal("应产生导出命令")
	}
	msg := cmd()
	exported, ok := msg.(logExportedMsg)
	if !ok {
		t.Fatalf("消息类型错误: %T", msg)
	}
	if exported.err != nil {
		t.Fatalf("导出失败: %v", exported.err)
	}
	if exported.result.Path == "" || exported.result.Editor != "Notepad" {
		t.Errorf("导出结果错误: %+v", exported.result)
	}
	if len(fx.exporter.exported) != 1 || fx.exporter.exported[0].Alias != "alpha" {
		t.Errorf("导出目标错误: %+v", fx.exporter.exported)
	}
}

func TestModelExportNoRun(t *testing.T) {
	fx := newModelFixture(t)
	if cmd := fx.model.exportLatestRun(); cmd != nil {
		t.Fatal("无记录时不应产生导出命令")
	}
	if fx.model.message == "" {
		t.Error("应提示没有可导出的日志")
	}
}

func TestModelExportActiveRunBlocked(t *testing.T) {
	fx := newModelFixture(t)
	fx.runs.Start(logexport.Run{ScriptID: "a", Alias: "alpha"})
	if cmd := fx.model.exportLatestRun(); cmd != nil {
		t.Fatal("运行中不应导出")
	}
	if !strings.Contains(fx.model.message, "运行") {
		t.Errorf("应提示运行中: %s", fx.model.message)
	}
}

func TestModelIMEGate(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/")
	fx.model.core.CommitText("阿", true)
	if len(fx.recent.List()) != 0 {
		t.Error("IME 批次不应启动脚本")
	}
	if fx.model.core.Input() != "/阿" {
		t.Errorf("输入 = %q", fx.model.core.Input())
	}
}

func TestModelEscape(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/")
	updated, _ := fx.model.Update(keyMsg("esc"))
	fx.model = updated.(Model)
	if fx.model.core.Visible() {
		t.Error("Esc 后候选应关闭")
	}
}

func TestModelViewContainsFavMarker(t *testing.T) {
	fx := newModelFixture(t)
	fx.favs.Toggle(fx.entryPath, "a")
	setInput(fx, "/")
	view := fx.model.View().Content
	if !strings.Contains(view, "★") {
		t.Errorf("候选视图应含收藏标识: %s", view)
	}
	if !strings.Contains(view, "/alpha") || !strings.Contains(view, "阿尔法") {
		t.Errorf("候选视图缺少脚本信息")
	}
}

func TestModelEmptyState(t *testing.T) {
	fx := newModelFixture(t)
	emptyCat, _ := catalog.New(nil)
	fx.model.core.catalog = emptyCat
	fx.model.core.SetInput("/")
	if len(fx.model.core.Candidates()) != 0 {
		t.Errorf("空入口候选应为 0")
	}
	view := fx.model.View().Content
	if !strings.Contains(view, "未找到匹配脚本") {
		t.Errorf("空态提示缺失: %s", view)
	}
}

func TestModelHistoryBrowsing(t *testing.T) {
	fx := newModelFixture(t)
	fx.model.history = []string{"/alpha", "/beta"}
	fx.model.historyIndex = len(fx.model.history)
	if !fx.model.previousHistory() {
		t.Fatal("应能浏览历史")
	}
	if fx.model.input.Value() != "/beta" {
		t.Errorf("历史输入 = %q", fx.model.input.Value())
	}
	if !fx.model.nextHistory() {
		t.Fatal("应能回到较新历史")
	}
	if fx.model.input.Value() != "" {
		t.Errorf("越过最新应清空: %q", fx.model.input.Value())
	}
}

func intPtr(v int) *int { return &v }

func TestModelHandleTaskEventDataAndState(t *testing.T) {
	fx := newModelFixture(t)
	fx.model.handleTaskEvent(task.Event{ScriptID: "a", State: task.Running, Stream: "stdout", Data: []byte("输出内容\n")})
	content := fx.model.logs.String()
	if !strings.Contains(content, "输出内容") {
		t.Errorf("日志缺少输出: %s", content)
	}
	fx.model.handleTaskEvent(task.Event{ScriptID: "a", State: task.Succeeded, PID: 123, ExitCode: 0})
	if !strings.Contains(fx.model.logs.String(), "状态=succeeded") {
		t.Errorf("日志缺少状态: %s", fx.model.logs.String())
	}
}

func TestModelHandleTaskEventFailure(t *testing.T) {
	fx := newModelFixture(t)
	fx.runs.Start(logexport.Run{ScriptID: "a", Alias: "alpha"})
	fx.model.handleTaskEvent(task.Event{ScriptID: "a", State: task.Failed, ExitCode: 5})
	run, ok := fx.runs.Last()
	if !ok {
		t.Fatal("应存在最近记录")
	}
	if run.State != "failed" || run.ExitCode == nil || *run.ExitCode != 5 {
		t.Errorf("失败状态未记录: %+v", run)
	}
}

func TestModelHandleLogExported(t *testing.T) {
	fx := newModelFixture(t)
	fx.model.handleLogExported(logExportedMsg{result: logexport.ExportResult{Path: `C:\logs\a.txt`, Editor: "Notepad++"}})
	if !strings.Contains(fx.model.message, "Notepad++") {
		t.Errorf("成功消息错误: %s", fx.model.message)
	}
	fx.model.handleLogExported(logExportedMsg{result: logexport.ExportResult{Path: `C:\logs\b.txt`, OpenError: errors.New("失败")}})
	if !strings.Contains(fx.model.message, "编辑器打开失败") {
		t.Errorf("打开失败消息错误: %s", fx.model.message)
	}
	fx.model.handleLogExported(logExportedMsg{err: errors.New("写入失败")})
	if !strings.Contains(fx.model.message, "导出失败") {
		t.Errorf("导出失败消息错误: %s", fx.model.message)
	}
}

func TestModelHandleInteractiveFinished(t *testing.T) {
	fx := newModelFixture(t)
	code := 0
	fx.model.handleInteractiveFinished(interactiveFinishedMsg{
		scriptID: "c",
		encoding: "utf-8",
		result:   runner.Result{ExitCode: code, PID: 77},
		stdout:   []byte("交互输出\n"),
		err:      nil,
	})
	if !strings.Contains(fx.model.logs.String(), "交互输出") {
		t.Errorf("交互输出未记录: %s", fx.model.logs.String())
	}
	if fx.service.State("c") != task.Succeeded {
		t.Errorf("State = %s", fx.service.State("c"))
	}
	if !strings.Contains(fx.model.message, "执行完成") {
		t.Errorf("完成消息错误: %s", fx.model.message)
	}
}

func TestModelHandleInteractiveFinishedFailure(t *testing.T) {
	fx := newModelFixture(t)
	fx.model.handleInteractiveFinished(interactiveFinishedMsg{
		scriptID: "c",
		encoding: "utf-8",
		result:   runner.Result{ExitCode: 3, PID: 1},
		err:      errors.New("异常退出"),
	})
	if fx.service.State("c") != task.Failed {
		t.Errorf("State = %s", fx.service.State("c"))
	}
	if !strings.Contains(fx.model.message, "执行失败") {
		t.Errorf("失败消息错误: %s", fx.model.message)
	}
}

func TestModelExportWhileExporting(t *testing.T) {
	fx := newModelFixture(t)
	fx.runs.Start(logexport.Run{ScriptID: "a", Alias: "alpha"})
	fx.runs.Finish("a", "succeeded", intPtr(0), time.Now())
	fx.model.exporting = true
	if cmd := fx.model.exportLatestRun(); cmd != nil {
		t.Fatal("导出中不应重复触发")
	}
	if !strings.Contains(fx.model.message, "正在导出") {
		t.Errorf("应提示导出中: %s", fx.model.message)
	}
}

func TestModelTabCompletion(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/alph")
	updated, _ := fx.model.Update(keyMsg("tab"))
	fx.model = updated.(Model)
	if fx.model.input.Value() != "/alpha" {
		t.Errorf("补全输入 = %q", fx.model.input.Value())
	}
	if fx.recent.List() != nil && len(fx.recent.List()) != 0 {
		t.Error("补全不应触发最近使用")
	}
}

func TestModelInteractiveStart(t *testing.T) {
	fx := newModelFixture(t)
	setInput(fx, "/gamma")
	updated, cmd := fx.model.Update(keyMsg("enter"))
	fx.model = updated.(Model)
	if cmd == nil {
		t.Fatal("交互脚本应产生 Exec 命令")
	}
	if got := fx.recent.List(); len(got) != 1 || got[0] != "c" {
		t.Errorf("交互启动应记录最近使用: %v", got)
	}
	if fx.model.message == "" {
		t.Error("应显示进入交互提示")
	}
}

func TestModelWindowSize(t *testing.T) {
	fx := newModelFixture(t)
	updated, _ := fx.model.Update(tea.WindowSizeMsg{Width: 120, Height: 40})
	fx.model = updated.(Model)
	if fx.model.width != 120 || fx.model.height != 40 {
		t.Errorf("窗口尺寸未更新: %dx%d", fx.model.width, fx.model.height)
	}
}
