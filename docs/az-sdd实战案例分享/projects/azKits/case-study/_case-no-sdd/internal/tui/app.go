package tui

import (
	"fmt"
	"os"
	"os/exec"
	"strings"
	"sync"
	"time"
	"unicode"

	tea "charm.land/bubbletea/v2"
	"charm.land/bubbles/v2/viewport"

	"azkits/internal/entry"
	"azkits/internal/matcher"
	"azkits/internal/run"
	"azkits/internal/store"
)

// App carries state shared between the model copies bubble tea creates and
// the background goroutines that stream script output.
type App struct {
	mu        sync.Mutex
	program   *tea.Program
	activeCmd *exec.Cmd
	store     *store.Store
	recent    *matcher.Recent
	// recv is an optional test hook invoked for every streamed message.
	recv func(tea.Msg)
}

// Program hooks the running program reference into the app.
func (a *App) Program(p *tea.Program) {
	a.mu.Lock()
	a.program = p
	a.mu.Unlock()
}

// Send delivers a message to the running program from any goroutine. When a
// recv hook is installed (tests), messages are delivered to it instead.
func (a *App) Send(msg tea.Msg) {
	if a.recv != nil {
		a.recv(msg)
		return
	}
	a.mu.Lock()
	p := a.program
	a.mu.Unlock()
	if p != nil {
		p.Send(msg)
	}
}

func (a *App) killActive() bool {
	a.mu.Lock()
	c := a.activeCmd
	a.mu.Unlock()
	if c == nil || c.Process == nil {
		return false
	}
	_ = c.Process.Kill()
	return true
}

func (a *App) setActive(c *exec.Cmd) {
	a.mu.Lock()
	a.activeCmd = c
	a.mu.Unlock()
}

func (a *App) clearActive() {
	a.mu.Lock()
	a.activeCmd = nil
	a.mu.Unlock()
}

const (
	stateLauncher = iota
	stateRunning
)

// Model is the bubble tea model. All mutating methods use pointer receivers;
// the returned model from Update is the same pointer bubble tea stores, so
// state persists across messages.
type Model struct {
	app   *App
	entry *entry.Entry
	byID  map[string]*entry.Script

	width, height int
	state         int

	// command input
	input  []rune
	cursor int

	// candidate list
	candidates []matcher.Candidate
	sel        int
	lastQuery  string
	scroll     int

	// runner view
	runScript  string
	runLines   []string
	runDone    *run.Result
	runKilled  bool
	vp         viewport.Model
	vpReady    bool

	// history
	history []string
	histIdx int

	toast string
}

// New creates the tea model for the given entry and stores.
func New(e *entry.Entry, st *store.Store) *Model {
	app := &App{store: st, recent: matcher.NewRecent()}
	m := &Model{app: app, entry: e, byID: map[string]*entry.Script{}}
	for i := range e.Scripts {
		s := &e.Scripts[i]
		m.byID[s.ID] = s
	}
	m.recompute()
	return m
}

// App exposes the shared app state so the caller can hook the program.
func (m Model) App() *App { return m.app }

// Init implements tea.Model.
func (m Model) Init() tea.Cmd { return nil }

// Update implements tea.Model.
func (m *Model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width, m.height = msg.Width, msg.Height
		m.resizeViewport()
		return m, nil
	case tea.PasteMsg:
		if m.state == stateLauncher {
			m.insertText(msg.Content)
			m.recompute()
		}
		return m, nil
	case tea.KeyPressMsg:
		return m.handleKey(msg)
	case tea.KeyReleaseMsg:
		return m, nil
	case lineMsg:
		if m.state == stateRunning {
			m.runLines = append(m.runLines, msg.text)
			m.setRunContent()
		}
		return m, nil
	case runStartErrMsg:
		m.state = stateLauncher
		m.toast = "启动失败：" + msg.err.Error()
		m.runScript = ""
		m.runLines = nil
		m.runDone = nil
		return m, nil
	case runDoneMsg:
		m.state = stateRunning
		m.runDone = &msg.result
		if msg.result.ExitCode == 0 && msg.result.Err == nil && !m.runKilled {
			m.app.recent.Mark(msg.scriptID, time.Now())
		}
		return m, nil
	case interactiveDoneMsg:
		m.state = stateLauncher
		if msg.err != nil {
			m.toast = "运行失败：" + msg.err.Error()
		} else {
			m.app.recent.Mark(msg.scriptID, time.Now())
			m.toast = "运行完成"
		}
		m.histIdx = -1
		return m, nil
	}
	return m, nil
}

// ---- key handling ---------------------------------------------------

func (m *Model) handleKey(msg tea.KeyPressMsg) (tea.Model, tea.Cmd) {
	key := msg.Key()

	if key.Mod.Contains(tea.ModCtrl) && key.Code == 'c' {
		if m.state == stateRunning {
			m.runKilled = true
			m.app.killActive()
			return m, nil
		}
		return m, tea.Quit
	}

	if key.Mod.Contains(tea.ModCtrl) && key.Code == 'f' {
		if m.state == stateLauncher {
			m.toggleFavoriteSelected()
		}
		return m, nil
	}

	// Printable text: a single character, bracketed paste text, or Chinese
	// IME committed composition. Text-carrying events must never execute; on
	// the Windows console, IME commits arrive as Code==0 / BaseCode set.
	if text, ok := textFromKey(key); ok {
		if m.state != stateLauncher {
			return m, nil
		}
		if strings.ContainsAny(text, "\x00\r\n") {
			return m, nil
		}
		m.insertText(text)
		m.recompute()
		return m, nil
	}

	if m.state == stateRunning {
		return m.handleRunningKey(key)
	}
	return m.handleLauncherKey(key)
}

func textFromKey(key tea.Key) (string, bool) {
	if key.Text != "" {
		return key.Text, true
	}
	if key.Code == 0 && key.BaseCode != 0 && unicode.IsPrint(key.BaseCode) {
		return string(key.BaseCode), true
	}
	return "", false
}

func (m *Model) handleRunningKey(key tea.Key) (tea.Model, tea.Cmd) {
	finished := m.runDone != nil
	switch {
	case finished && (key.Code == tea.KeyEnter || key.Code == tea.KeyEscape):
		m.state = stateLauncher
		m.runDone = nil
	case !finished && key.Code == tea.KeyEscape:
		m.runKilled = true
		m.app.killActive()
	case key.Code == tea.KeyUp:
		m.vp.ScrollUp(1)
	case key.Code == tea.KeyDown:
		m.vp.ScrollDown(1)
	case key.Code == tea.KeyPgUp:
		m.vp.HalfPageUp()
	case key.Code == tea.KeyPgDown:
		m.vp.HalfPageDown()
	case key.Code == tea.KeyHome:
		m.vp.GotoTop()
	case key.Code == tea.KeyEnd:
		m.vp.GotoBottom()
	}
	return m, nil
}

func (m *Model) handleLauncherKey(key tea.Key) (tea.Model, tea.Cmd) {
	hasCandidates := m.candidateMode()

	switch key.Code {
	case tea.KeyEnter:
		return m.executeSelection()
	case tea.KeyTab:
		if hasCandidates && len(m.candidates) > 0 {
			idx := m.sel
			if idx < 0 || idx >= len(m.candidates) {
				idx = 0
			}
			m.setInputToAlias(m.candidates[idx].Script.Alias)
			return m, nil
		}
	case tea.KeyEscape:
		if len(m.input) > 0 {
			m.clearInput()
		} else {
			return m, tea.Quit
		}
	case tea.KeyUp:
		if hasCandidates && len(m.candidates) > 0 {
			m.moveSel(-1)
		} else {
			m.historyUp()
		}
	case tea.KeyDown:
		if hasCandidates && len(m.candidates) > 0 {
			m.moveSel(1)
		} else {
			m.historyDown()
		}
	case tea.KeyPgUp:
		if hasCandidates {
			m.moveSel(-m.listPageSize())
		}
	case tea.KeyPgDown:
		if hasCandidates {
			m.moveSel(m.listPageSize())
		}
	case tea.KeyHome:
		if hasCandidates && len(m.candidates) > 0 {
			m.sel = 0
		} else {
			m.cursor = 0
		}
	case tea.KeyEnd:
		if hasCandidates && len(m.candidates) > 0 {
			m.sel = len(m.candidates) - 1
		} else {
			m.cursor = len(m.input)
		}
	case tea.KeyBackspace:
		if m.cursor > 0 {
			m.input = append(m.input[:m.cursor-1], m.input[m.cursor:]...)
			m.cursor--
			m.recompute()
		}
	case tea.KeyDelete:
		if m.cursor < len(m.input) {
			m.input = append(m.input[:m.cursor], m.input[m.cursor+1:]...)
			m.recompute()
		}
	case tea.KeyLeft:
		if m.cursor > 0 {
			m.cursor--
		}
	case tea.KeyRight:
		if m.cursor < len(m.input) {
			m.cursor++
		}
	default:
		if key.Mod.Contains(tea.ModCtrl) && key.Code == 'u' {
			m.clearInput()
		} else if key.Mod.Contains(tea.ModCtrl) && key.Code == 'w' {
			m.deleteWord()
		}
	}
	return m, nil
}

func (m *Model) toggleFavoriteSelected() {
	if len(m.candidates) == 0 {
		m.toast = "没有可收藏的脚本"
		return
	}
	idx := m.sel
	if idx < 0 || idx >= len(m.candidates) {
		idx = 0
	}
	c := m.candidates[idx]
	on, err := m.app.store.ToggleFavorite(c.Script.ID)
	if err != nil {
		m.toast = "收藏保存失败：" + err.Error()
		return
	}
	if on {
		m.toast = fmt.Sprintf("已收藏：%s", c.Script.Alias)
	} else {
		m.toast = fmt.Sprintf("已取消收藏：%s", c.Script.Alias)
	}
	m.recompute()
}

func (m *Model) executeSelection() (tea.Model, tea.Cmd) {
	if !m.candidateMode() || len(m.candidates) == 0 {
		if strings.TrimSpace(string(m.input)) == "" {
			return m, nil
		}
		m.toast = "未匹配到脚本，输入 / 查看候选"
		return m, nil
	}
	idx := m.sel
	if idx < 0 || idx >= len(m.candidates) {
		idx = 0
	}
	return m.launch(m.candidates[idx])
}

func (m *Model) launch(c matcher.Candidate) (tea.Model, tea.Cmd) {
	if c.Script.Status == matcher.StatusFileMissing {
		m.toast = fmt.Sprintf("脚本文件缺失，无法运行：%s", c.Script.Alias)
		return m, nil
	}
	script, ok := m.byID[c.Script.ID]
	if !ok {
		m.toast = "脚本不存在或已被移除：" + c.Script.Alias
		return m, nil
	}
	req := &run.Request{Entry: m.entry, Script: script}
	if err := run.ValidateScriptFile(req); err != nil {
		m.toast = "无法运行：" + err.Error()
		return m, nil
	}
	cmd, err := run.BuildCommand(req)
	if err != nil {
		m.toast = "无法运行：" + err.Error()
		return m, nil
	}

	m.pushHistory(string(m.input))
	m.clearInput()

	if script.Interactive {
		return m, tea.ExecProcess(cmd, func(err error) tea.Msg {
			return interactiveDoneMsg{scriptID: script.ID, err: err}
		})
	}

	m.state = stateRunning
	m.runScript = c.Script.Alias
	m.runLines = nil
	m.runDone = nil
	m.runKilled = false
	m.resizeViewport()
	m.histIdx = -1
	return m, m.startBackgroundRun(script, req)
}

// startBackgroundRun returns a Cmd that runs a non-interactive script in the
// background, streaming its decoded output as lineMsg and finishing with
// runDoneMsg.
func (m *Model) startBackgroundRun(script *entry.Script, req *run.Request) tea.Cmd {
	app := m.app
	enc := script.Encoding
	scriptID := script.ID
	return func() tea.Msg {
		cmd, err := run.BuildCommand(req)
		if err != nil {
			return runStartErrMsg{err: err}
		}
		stdout, err := cmd.StdoutPipe()
		if err != nil {
			return runStartErrMsg{err: err}
		}
		stderr, err := cmd.StderrPipe()
		if err != nil {
			return runStartErrMsg{err: err}
		}
		app.setActive(cmd)
		defer app.clearActive()
		if err := cmd.Start(); err != nil {
			return runStartErrMsg{err: err}
		}

		var wg sync.WaitGroup
		wg.Add(2)
		go func() {
			defer wg.Done()
			streamLines(stdout, enc, app, false)
		}()
		go func() {
			defer wg.Done()
			streamLines(stderr, enc, app, true)
		}()
		werr := cmd.Wait()
		wg.Wait()
		return runDoneMsg{scriptID: scriptID, result: run.ResultFromErr(werr)}
	}
}

// streamLines decodes a pipe and sends each line to the program.
func streamLines(r interface{ Read([]byte) (int, error) }, enc entry.Encoding, app *App, isErr bool) {
	sc := run.LineScanner(run.DecodeReader(r, enc))
	for sc.Scan() {
		app.Send(lineMsg{text: sc.Text(), isErr: isErr})
	}
}

// ---- input helpers ---------------------------------------------------

func (m *Model) insertText(s string) {
	runes := []rune(s)
	if len(runes) == 0 {
		return
	}
	out := make([]rune, 0, len(m.input)+len(runes))
	out = append(out, m.input[:m.cursor]...)
	out = append(out, runes...)
	out = append(out, m.input[m.cursor:]...)
	m.input = out
	m.cursor += len(runes)
}

func (m *Model) clearInput() {
	m.input = nil
	m.cursor = 0
	m.sel = 0
	m.scroll = 0
	m.lastQuery = ""
}

func (m *Model) setInputToAlias(alias string) {
	m.input = []rune("/" + alias)
	m.cursor = len(m.input)
	m.sel = 0
	m.scroll = 0
	m.recompute()
}

func (m *Model) deleteWord() {
	if m.cursor == 0 {
		return
	}
	i := m.cursor
	for i > 0 && m.input[i-1] == ' ' {
		i--
	}
	for i > 0 && m.input[i-1] != ' ' && m.input[i-1] != '/' {
		i--
	}
	m.input = append(m.input[:i], m.input[m.cursor:]...)
	m.cursor = i
	m.recompute()
}

// ---- candidate computation -------------------------------------------

func (m *Model) candidateMode() bool {
	return m.state == stateLauncher && len(m.input) > 0 && m.input[0] == '/'
}

func (m *Model) query() string {
	if !m.candidateMode() {
		return ""
	}
	return string(m.input[1:])
}

func (m *Model) recompute() {
	q := m.query()
	if q == m.lastQuery && m.candidates != nil {
		return
	}
	m.lastQuery = q

	scripts := make([]matcher.Script, 0, len(m.entry.EnabledScripts()))
	for i := range m.entry.Scripts {
		s := &m.entry.Scripts[i]
		if !s.IsEnabled() {
			continue
		}
		status := matcher.StatusOK
		abs := m.entry.ResolvePath(s)
		if abs == "" {
			status = matcher.StatusFileMissing
		} else if _, err := os.Stat(abs); err != nil {
			status = matcher.StatusFileMissing
		}
		scripts = append(scripts, matcher.Script{
			ID:          s.ID,
			Alias:       s.Alias,
			Name:        s.Name,
			Description: s.Description,
			Status:      status,
			Index:       i,
		})
	}

	m.candidates = matcher.Filter(scripts, q, m.app.store.IsFavorite, m.app.recent)
	if m.sel >= len(m.candidates) {
		m.sel = 0
		m.scroll = 0
	}
}

// ---- selection / scrolling -------------------------------------------

func (m *Model) moveSel(delta int) {
	n := len(m.candidates)
	if n == 0 {
		return
	}
	m.sel += delta
	if m.sel < 0 {
		m.sel = 0
	}
	if m.sel >= n {
		m.sel = n - 1
	}
	page := m.listPageSize()
	if m.sel < m.scroll {
		m.scroll = m.sel
	}
	if m.sel >= m.scroll+page {
		m.scroll = m.sel - page + 1
	}
}

// ---- history ---------------------------------------------------------

func (m *Model) pushHistory(line string) {
	if line == "" {
		return
	}
	if n := len(m.history); n > 0 && m.history[n-1] == line {
		return
	}
	m.history = append(m.history, line)
	if len(m.history) > 200 {
		m.history = m.history[len(m.history)-200:]
	}
}

func (m *Model) historyUp() {
	if len(m.history) == 0 {
		return
	}
	if m.histIdx < 0 {
		m.histIdx = len(m.history) - 1
	} else if m.histIdx > 0 {
		m.histIdx--
	}
	m.setInputFromHistory()
}

func (m *Model) historyDown() {
	if m.histIdx < 0 {
		return
	}
	m.histIdx++
	if m.histIdx >= len(m.history) {
		m.histIdx = -1
		m.clearInput()
		return
	}
	m.setInputFromHistory()
}

func (m *Model) setInputFromHistory() {
	m.input = []rune(m.history[m.histIdx])
	m.cursor = len(m.input)
	m.recompute()
}

// ---- viewport helpers -------------------------------------------------

func (m *Model) resizeViewport() {
	if m.width <= 0 || m.height <= 0 {
		return
	}
	if m.state != stateRunning {
		return
	}
	h := m.height - 3
	if h < 1 {
		h = 1
	}
	if !m.vpReady {
		m.vp = viewport.New(viewport.WithWidth(m.width), viewport.WithHeight(h))
		m.vp.SoftWrap = true
		m.vpReady = true
		m.setRunContent()
	} else {
		m.vp.SetWidth(m.width)
		m.vp.SetHeight(h)
		m.vp.GotoBottom()
	}
}

func (m *Model) setRunContent() {
	if m.vpReady {
		m.vp.SetContent(strings.Join(m.runLines, "\n"))
		m.vp.GotoBottom()
	}
}
