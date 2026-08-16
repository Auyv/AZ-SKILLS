package terminal

import (
	"fmt"
	"strings"
	"time"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/logexport"
	"azkits/internal/runner"
	"azkits/internal/task"
	"charm.land/bubbles/v2/textinput"
	"charm.land/bubbles/v2/viewport"
	tea "charm.land/bubbletea/v2"
)

const maxLogBytes = 16 * 1024 * 1024

type logExporter interface {
	Export(logexport.Run) (logexport.ExportResult, error)
}

type logExportedMsg struct {
	result logexport.ExportResult
	err    error
}

type Model struct {
	config          config.ToolboxConfig
	service         *task.Service
	core            *Core
	input           textinput.Model
	viewport        viewport.Model
	logs            *LogBuffer
	runs            *logexport.Tracker
	exporter        logExporter
	exporting       bool
	message         string
	history         []string
	historyIndex    int
	browsingHistory bool
	editedHistory   bool
	width           int
	height          int
}

func NewModel(cfg config.ToolboxConfig, c *catalog.Catalog, service *task.Service) Model {
	input := textinput.New()
	input.Prompt = "> "
	input.Placeholder = "输入 / 搜索脚本"
	input.CharLimit = 512
	input.SetWidth(80)
	input.Focus()
	vp := viewport.New(viewport.WithWidth(80), viewport.WithHeight(8))
	vp.MouseWheelEnabled = false
	return Model{
		config: cfg, service: service, core: NewCore(c, service), input: input, viewport: vp,
		logs: NewLogBuffer(maxLogBytes), runs: logexport.NewTracker(), exporter: logexport.Service{},
		width: 100, height: 30,
	}
}

func (m Model) Init() tea.Cmd {
	return tea.Batch(m.input.Focus(), waitTaskEvent(m.service.Events()))
}

func (m Model) Update(message tea.Msg) (tea.Model, tea.Cmd) {
	var commands []tea.Cmd
	switch msg := message.(type) {
	case tea.WindowSizeMsg:
		m.width, m.height = msg.Width, msg.Height
		m.input.SetWidth(max(20, msg.Width-4))
		m.viewport.SetWidth(max(20, msg.Width-2))
		m.viewport.SetHeight(max(4, msg.Height-16))
	case task.Event:
		m.handleTaskEvent(msg)
		commands = append(commands, waitTaskEvent(m.service.Events()))
	case interactiveFinishedMsg:
		m.handleInteractiveFinished(msg)
	case logExportedMsg:
		m.exporting = false
		m.handleLogExported(msg)
	case tea.KeyPressMsg:
		switch msg.String() {
		case "ctrl+c":
			return m, tea.Quit
		case "ctrl+p":
			return m, m.exportLatestRun()
		case "esc":
			m.core.Escape()
			m.message = "已取消候选"
			return m, nil
		case "up":
			if m.previousHistory() {
				return m, nil
			}
			m.core.Move(-1)
			return m, nil
		case "down":
			if m.nextHistory() {
				return m, nil
			}
			m.core.Move(1)
			return m, nil
		case "tab":
			if value, ok := m.core.Complete(); ok {
				m.input.SetValue(value)
				m.input.CursorEnd()
				m.resetHistoryNavigation()
				m.message = "已补全，按 Enter 执行"
			} else {
				m.message = "未找到匹配脚本"
			}
			return m, nil
		case "enter":
			command := m.input.Value()
			target, err := m.core.Target()
			if err != nil {
				m.message = err.Error()
				return m, nil
			}
			if target.Interactive {
				process, err := m.service.PrepareInteractive(target.ID)
				if err != nil {
					m.message = err.Error()
					return m, nil
				}
				interactive := newInteractiveCommand(process)
				m.startRun(target)
				m.recordSuccessfulCommand(command)
				m.message = "已进入交互脚本，脚本结束后将返回 azKits"
				return m, tea.Exec(interactive, func(err error) tea.Msg {
					return interactiveFinishedMsg{
						scriptID: target.ID,
						encoding: target.Encoding,
						result:   interactive.Result(),
						stdout:   append([]byte(nil), interactive.stdout.Bytes()...),
						stderr:   append([]byte(nil), interactive.stderr.Bytes()...),
						events:   interactive.Events(),
						err:      err,
					}
				})
			}
			if err := m.core.Execute(); err != nil {
				m.message = err.Error()
				return m, nil
			}
			m.startRun(target)
			m.recordSuccessfulCommand(command)
			m.message = "脚本已启动"
			return m, nil
		}
	}
	previousInput := m.input.Value()
	wasBrowsingHistory := m.browsingHistory
	updated, cmd := m.input.Update(message)
	m.input = updated
	m.core.SetInput(m.input.Value())
	if wasBrowsingHistory && m.input.Value() != previousInput {
		m.browsingHistory = false
		m.editedHistory = true
		m.historyIndex = len(m.history)
	}
	commands = append(commands, cmd)
	return m, tea.Batch(commands...)
}

func (m *Model) recordSuccessfulCommand(command string) {
	m.history = append(m.history, command)
	m.input.SetValue("")
	m.core.SetInput("")
	m.resetHistoryNavigation()
}

func (m *Model) previousHistory() bool {
	if len(m.history) == 0 || (m.input.Value() != "" && !m.browsingHistory && !m.editedHistory) {
		return false
	}
	if !m.browsingHistory {
		m.historyIndex = len(m.history)
	}
	if m.historyIndex > 0 {
		m.historyIndex--
	}
	m.setHistoryInput(m.history[m.historyIndex])
	m.browsingHistory = true
	m.editedHistory = false
	return true
}

func (m *Model) nextHistory() bool {
	if !m.browsingHistory {
		return false
	}
	if m.historyIndex < len(m.history)-1 {
		m.historyIndex++
		m.setHistoryInput(m.history[m.historyIndex])
		return true
	}
	m.input.SetValue("")
	m.core.SetInput("")
	m.resetHistoryNavigation()
	return true
}

func (m *Model) setHistoryInput(value string) {
	m.input.SetValue(value)
	m.input.CursorEnd()
	m.core.SetInput(value)
}

func (m *Model) resetHistoryNavigation() {
	m.historyIndex = len(m.history)
	m.browsingHistory = false
	m.editedHistory = false
}

func (m Model) View() tea.View {
	var body strings.Builder
	fmt.Fprintf(&body, "%s\n%s\n入口: %s\n\n", m.config.Toolbox.Name, m.config.Toolbox.Description, m.config.Source)
	body.WriteString(m.input.View())
	body.WriteString("\n")
	if m.core.Visible() {
		candidates := m.core.Candidates()
		if len(candidates) == 0 {
			body.WriteString("  未找到匹配脚本\n")
		} else {
			start, end := visibleRange(m.core.Selected(), len(candidates), 8)
			for i := start; i < end; i++ {
				marker := "  "
				if i == m.core.Selected() {
					marker = "> "
				}
				script := candidates[i].Script
				fmt.Fprintf(&body, "%s/%s  %s  %s  [%s]\n", marker, script.Alias, script.Name, script.Description, m.service.State(script.ID))
			}
		}
	}
	if m.message != "" {
		fmt.Fprintf(&body, "\n%s\n", m.message)
	}
	body.WriteString("\n运行日志\n")
	body.WriteString(m.viewport.View())
	body.WriteString("\n\n↑/↓ 选择  Tab 补全  Enter 执行  Ctrl+P 导出日志  Esc 取消  Ctrl+C 退出")
	view := tea.NewView(body.String())
	view.AltScreen = true
	view.WindowTitle = fmt.Sprintf("azKits - %s - %s", m.config.Toolbox.Name, m.config.Source)
	view.MouseMode = tea.MouseModeNone
	return view
}

func (m *Model) handleTaskEvent(event task.Event) {
	if len(event.Data) > 0 {
		m.logs.Append(fmt.Sprintf("[%s/%s] %s", event.ScriptID, event.Stream, event.Data))
		m.runs.Append(event.ScriptID, logexport.Entry{Stream: string(event.Stream), Text: string(event.Data)})
	}
	if event.State != "" && len(event.Data) == 0 {
		m.logs.Append(fmt.Sprintf("[%s] 状态=%s PID=%d 退出码=%d", event.ScriptID, event.State, event.PID, event.ExitCode) + "\n")
	}
	if isFinalState(event.State) {
		if event.Err != nil {
			m.runs.Append(event.ScriptID, logexport.Entry{Stream: string(runner.Stderr), Text: event.Err.Error() + "\n"})
		}
		var exitCode *int
		if event.PID != 0 || event.Err == nil {
			value := event.ExitCode
			exitCode = &value
		}
		m.runs.Finish(event.ScriptID, string(event.State), exitCode, time.Now())
	}
	m.viewport.SetContent(m.logs.String())
	m.viewport.GotoBottom()
}

func (m *Model) handleInteractiveFinished(msg interactiveFinishedMsg) {
	if len(msg.events) > 0 {
		for _, event := range msg.events {
			decoded := runner.DecodeBytes(event.Data, msg.encoding)
			m.logs.Append(fmt.Sprintf("[%s/%s] %s", msg.scriptID, event.Stream, decoded))
			m.runs.Append(msg.scriptID, logexport.Entry{Stream: string(event.Stream), Text: string(decoded)})
		}
	} else {
		if len(msg.stdout) > 0 {
			decoded := runner.DecodeBytes(msg.stdout, msg.encoding)
			m.logs.Append(fmt.Sprintf("[%s/%s] %s", msg.scriptID, runner.Stdout, decoded))
			m.runs.Append(msg.scriptID, logexport.Entry{Stream: string(runner.Stdout), Text: string(decoded)})
		}
		if len(msg.stderr) > 0 {
			decoded := runner.DecodeBytes(msg.stderr, msg.encoding)
			m.logs.Append(fmt.Sprintf("[%s/%s] %s", msg.scriptID, runner.Stderr, decoded))
			m.runs.Append(msg.scriptID, logexport.Entry{Stream: string(runner.Stderr), Text: string(decoded)})
		}
	}
	m.viewport.SetContent(m.logs.String())
	m.viewport.GotoBottom()
	m.service.FinishInteractive(msg.scriptID, msg.result, msg.err)
	state := task.Succeeded
	if msg.err != nil || msg.result.ExitCode != 0 {
		state = task.Failed
		if msg.err != nil {
			m.runs.Append(msg.scriptID, logexport.Entry{Stream: string(runner.Stderr), Text: msg.err.Error() + "\n"})
		}
	}
	exitCode := msg.result.ExitCode
	m.runs.Finish(msg.scriptID, string(state), &exitCode, time.Now())
	if state == task.Failed {
		m.message = fmt.Sprintf("交互脚本执行失败，退出码=%d", msg.result.ExitCode)
		return
	}
	m.message = "交互脚本执行完成，已返回 azKits"
}

func (m *Model) startRun(script config.ScriptConfig) {
	_ = m.runs.Start(logexport.Run{
		ToolboxName: m.config.Toolbox.Name,
		ScriptID:    script.ID,
		Alias:       script.Alias,
		ScriptName:  script.Name,
		StartedAt:   time.Now(),
	})
}

func (m *Model) exportLatestRun() tea.Cmd {
	if m.exporting {
		m.message = "日志正在导出，请稍候"
		return nil
	}
	if m.runs.HasActive() {
		m.message = "脚本仍在运行，请等待脚本结束并返回主界面后再导出"
		return nil
	}
	run, ok := m.runs.Last()
	if !ok {
		m.message = "没有可导出的本次执行日志"
		return nil
	}
	m.exporting = true
	m.message = "正在导出本次执行日志..."
	exporter := m.exporter
	return func() tea.Msg {
		result, err := exporter.Export(run)
		return logExportedMsg{result: result, err: err}
	}
}

func (m *Model) handleLogExported(msg logExportedMsg) {
	if msg.err != nil {
		m.message = fmt.Sprintf("日志导出失败：%v", msg.err)
		return
	}
	if msg.result.OpenError != nil {
		m.message = fmt.Sprintf("日志已导出：%s；编辑器打开失败：%v，请手动打开", msg.result.Path, msg.result.OpenError)
		return
	}
	m.message = fmt.Sprintf("日志已导出：%s；已使用 %s 打开", msg.result.Path, msg.result.Editor)
}

func isFinalState(state task.State) bool {
	return state == task.Succeeded || state == task.Failed || state == task.Stopped
}

func waitTaskEvent(events <-chan task.Event) tea.Cmd {
	return func() tea.Msg { return <-events }
}

func visibleRange(selected, total, limit int) (int, int) {
	if total <= limit {
		return 0, total
	}
	start := selected - limit/2
	if start < 0 {
		start = 0
	}
	if start+limit > total {
		start = total - limit
	}
	return start, start + limit
}

func Run(cfg config.ToolboxConfig, c *catalog.Catalog, service *task.Service) error {
	_, err := tea.NewProgram(NewModel(cfg, c, service)).Run()
	return err
}
