package tui

import (
	"fmt"
	"strings"

	tea "charm.land/bubbletea/v2"
	"charm.land/lipgloss/v2"

	"azkits/internal/matcher"
)

var (
	styleTitle    = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("45"))
	styleSubtitle = lipgloss.NewStyle().Foreground(lipgloss.Color("250"))
	styleDim      = lipgloss.NewStyle().Foreground(lipgloss.Color("244"))
	stylePrompt   = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("45"))
	styleSep      = lipgloss.NewStyle().Foreground(lipgloss.Color("240"))
	styleSelected = lipgloss.NewStyle().Reverse(true)
	styleFav      = lipgloss.NewStyle().Foreground(lipgloss.Color("214"))
	styleRecent   = lipgloss.NewStyle().Foreground(lipgloss.Color("39"))
	styleOK       = lipgloss.NewStyle().Foreground(lipgloss.Color("42"))
	styleMissing  = lipgloss.NewStyle().Foreground(lipgloss.Color("226"))
	styleHelp     = lipgloss.NewStyle().Foreground(lipgloss.Color("250"))
	styleErr      = lipgloss.NewStyle().Foreground(lipgloss.Color("196")).Bold(true)
)

func statusLabel(s matcher.Status) (string, lipgloss.Style) {
	switch s {
	case matcher.StatusFileMissing:
		return "文件缺失", styleMissing
	case matcher.StatusDisabled:
		return "已禁用", styleDim
	default:
		return "有效", styleOK
	}
}

// headerLines returns the fixed line count above the candidate area.
const headerLines = 5

func (m Model) listHeight() int {
	h := m.height - headerLines - 2
	if h < 1 {
		h = 1
	}
	return h
}

func (m Model) listPageSize() int { return m.listHeight() }

// View implements tea.Model.
func (m Model) View() tea.View {
	if m.state == stateRunning {
		return m.viewRunning()
	}
	return m.viewLauncher()
}

func (m Model) viewRunning() tea.View {
	title := styleTitle.Render(fmt.Sprintf("运行中：%s", m.runScript))
	sep := styleSep.Render(strings.Repeat("─", max(20, m.width)))
	var status string
	if m.runDone == nil {
		status = styleHelp.Render("[ 运行中... Esc 终止，↑/↓ 滚动 ]")
	} else if m.runDone.Err != nil {
		status = styleErr.Render(fmt.Sprintf("[ 运行失败：%v — Enter/Esc 返回 ]", m.runDone.Err))
	} else if m.runKilled {
		status = styleErr.Render("[ 已终止 — Enter/Esc 返回 ]")
	} else {
		status = styleHelp.Render(fmt.Sprintf("[ 完成 · 退出码 %d — Enter/Esc 返回 ]", m.runDone.ExitCode))
	}

	body := title + "\n" + sep + "\n" + m.vpView() + "\n" + sep + "\n" + status
	v := tea.NewView(body)
	v.AltScreen = true
	v.WindowTitle = "azKits — 运行中"
	return v
}

func (m Model) vpView() string {
	if !m.vpReady {
		return ""
	}
	return m.vp.View()
}

func (m Model) viewLauncher() tea.View {
	var b strings.Builder

	title := styleTitle.Render("azKits — " + m.entry.Name)
	if m.entry.Name == "" {
		title = styleTitle.Render("azKits")
	}
	b.WriteString(title)
	b.WriteString("\n")
	if m.entry.Description != "" {
		b.WriteString(styleSubtitle.Render(m.entry.Description))
		b.WriteString("\n")
	} else {
		b.WriteString("\n")
	}

	sep := styleSep.Render(strings.Repeat("─", max(20, m.width)))
	b.WriteString(sep)
	b.WriteString("\n")

	inputLine := stylePrompt.Render(">") + " " + string(m.input)
	b.WriteString(inputLine)
	b.WriteString("\n")
	b.WriteString(sep)
	b.WriteString("\n")

	cursorX := lipgloss.Width("> ") + lipgloss.Width(string(m.input[:m.cursor]))

	if m.candidateMode() {
		b.WriteString(m.renderCandidates())
	} else {
		b.WriteString(m.renderHint())
	}

	b.WriteString("\n")
	b.WriteString(sep)
	b.WriteString("\n")
	if m.toast != "" {
		b.WriteString(styleErr.Render(m.toast))
	} else if m.candidateMode() && len(m.candidates) > 0 {
		b.WriteString(styleDim.Render(fmt.Sprintf("%d 个匹配 — Ctrl+F 收藏/取消", len(m.candidates))))
	}
	b.WriteString("\n")
	b.WriteString(m.renderHelp())

	v := tea.NewView(b.String())
	v.AltScreen = true
	v.WindowTitle = "azKits — " + m.entry.Name
	v.Cursor = tea.NewCursor(cursorX, 3)
	return v
}

func (m Model) renderCandidates() string {
	n := len(m.candidates)
	if n == 0 {
		if m.favoriteCount() == 0 && m.hasEnabledScripts() {
			return styleDim.Render("没有匹配的脚本（尚无收藏，选中后按 Ctrl+F 收藏）")
		}
		return styleDim.Render("没有匹配的脚本，按 Esc 关闭")
	}
	page := m.listHeight()
	if m.scroll > n {
		m.scroll = 0
	}
	end := m.scroll + page
	if end > n {
		end = n
	}
	vis := m.candidates[m.scroll:end]

	var b strings.Builder
	for i, c := range vis {
		line := m.candidateLine(c)
		if m.scroll+i == m.sel {
			line = styleSelected.Render(line)
		}
		b.WriteString(line)
		b.WriteString("\n")
	}
	if n > page {
		b.WriteString(styleDim.Render(fmt.Sprintf("… 共 %d 个（↑/↓ 滚动）", n)))
		b.WriteString("\n")
	}
	return b.String()
}

func (m Model) candidateLine(c matcher.Candidate) string {
	var mark string
	if c.Favorite {
		mark += styleFav.Render("★")
	}
	if c.Recent {
		mark += styleRecent.Render("↺")
	}
	if mark == "" {
		mark = " "
	}

	alias := "[" + c.Script.Alias + "]"
	name := c.Script.Name
	if name == "" {
		name = c.Script.Alias
	}
	st, stStyle := statusLabel(c.Script.Status)
	return fmt.Sprintf("%s %-14s %-18s %s  %s",
		mark,
		alias,
		truncateW(name, 18),
		truncateW(c.Script.Description, max(10, m.width-60)),
		stStyle.Render(st),
	)
}

func (m Model) renderHint() string {
	if !m.hasEnabledScripts() {
		return styleDim.Render("无可执行脚本：请检查入口文件中脚本的 enabled 状态")
	}
	if m.favoriteCount() > 0 {
		return styleDim.Render(fmt.Sprintf("已收藏 %d 个脚本。输入 / 显示候选；↑/↓ 浏览历史。", m.favoriteCount()))
	}
	return styleDim.Render("输入 / 显示脚本候选；↑/↓ 浏览命令历史；Ctrl+F 收藏")
}

func (m Model) renderHelp() string {
	if m.state == stateRunning {
		return styleHelp.Render("Esc 终止    Enter 返回    ↑/↓ 滚动    Ctrl+C 退出")
	}
	return styleHelp.Render("输入 / 显示候选     ↑/↓ 选择     Tab 补全     Enter 运行     Ctrl+F 收藏/取消     Esc 关闭     Ctrl+C 退出")
}

func (m Model) hasEnabledScripts() bool {
	for i := range m.entry.Scripts {
		if m.entry.Scripts[i].IsEnabled() {
			return true
		}
	}
	return false
}

func (m Model) favoriteCount() int {
	if m.app == nil || m.app.store == nil {
		return 0
	}
	var n int
	for _, id := range m.app.store.FavoriteIDs() {
		if _, ok := m.byID[id]; ok {
			n++
		}
	}
	return n
}

// truncateW cuts s at width w, appending "…" when truncated.
func truncateW(s string, w int) string {
	if w <= 0 {
		return ""
	}
	if lipgloss.Width(s) <= w {
		return s
	}
	out := ""
	cur := 0
	for _, r := range s {
		rw := lipgloss.Width(string(r))
		if cur+rw > w-1 {
			break
		}
		out += string(r)
		cur += rw
	}
	return out + "…"
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
