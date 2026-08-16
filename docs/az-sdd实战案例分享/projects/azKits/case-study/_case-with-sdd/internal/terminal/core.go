package terminal

import (
	"fmt"
	"strings"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/favorite"
	"azkits/internal/task"
)

type Starter interface {
	Start(string) error
	State(string) task.State
}

type Candidate struct {
	Script     config.ScriptConfig
	Rank       catalog.Rank
	IsFavorite bool
	IsRecent   bool
}

type Core struct {
	catalog    *catalog.Catalog
	starter    Starter
	favorites  *favorite.FavoritesStore
	recent     *favorite.RecentTracker
	entryPath  string
	input      string
	candidates []Candidate
	selected   int
	visible    bool
	lastMsg    string
}

func NewCore(c *catalog.Catalog, starter Starter, favorites *favorite.FavoritesStore, recent *favorite.RecentTracker, entryPath string) *Core {
	return &Core{catalog: c, starter: starter, favorites: favorites, recent: recent, entryPath: entryPath}
}

// IsFavoriteCommand 判断当前输入是否是收藏命令。
// 仅 "/fav" 或 "/fav " 前缀视为命令，避免与别名以 fav 开头的脚本冲突。
func IsFavoriteCommand(input string) bool {
	trimmed := strings.TrimSpace(input)
	return trimmed == "/fav" || strings.HasPrefix(trimmed, "/fav ")
}

func (c *Core) SetInput(value string) {
	c.input = value
	c.selected = 0
	c.visible = len(value) > 0 && value[0] == '/'
	if c.visible {
		if IsFavoriteCommand(value) {
			c.candidates = nil
		} else {
			c.candidates = c.sortCandidates(c.catalog.Match(value))
		}
	} else {
		c.candidates = nil
	}
}

func (c *Core) CommitText(text string, includedEnter bool) {
	// includedEnter 表示同一 Windows 输入批次内的 IME 上屏确认；该批次只提交文本。
	c.SetInput(c.input + text)
	_ = includedEnter
}

// sortCandidates 将 catalog 匹配结果按收藏/最近使用置顶排序：
// 1. 收藏且最近使用（按最近使用倒序）
// 2. 仅收藏（YAML 顺序）
// 3. 仅最近使用（按最近使用倒序）
// 4. 其余（catalog 原始顺序）
func (c *Core) sortCandidates(matches []catalog.Candidate) []Candidate {
	if c.favorites == nil && c.recent == nil {
		result := make([]Candidate, 0, len(matches))
		for _, m := range matches {
			result = append(result, Candidate{Script: m.Script, Rank: m.Rank})
		}
		return result
	}
	var favSet, recentSet map[string]bool
	if c.favorites != nil {
		favSet = make(map[string]bool)
		for _, id := range c.favorites.IDs(c.entryPath) {
			favSet[id] = true
		}
	}
	if c.recent != nil {
		recentSet = make(map[string]bool)
		for _, id := range c.recent.List() {
			recentSet[id] = true
		}
	}
	recentOrder := []string(nil)
	if c.recent != nil {
		recentOrder = c.recent.List()
	}
	recentPos := make(map[string]int, len(recentOrder))
	for i, id := range recentOrder {
		recentPos[id] = i
	}

	var group1, group2, group3, group4 []Candidate
	rest := make([]Candidate, 0, len(matches))
	for _, m := range matches {
		rest = append(rest, Candidate{Script: m.Script, Rank: m.Rank})
	}
	for _, cand := range rest {
		isFav := favSet[cand.Script.ID]
		isRecent := recentSet[cand.Script.ID]
		cand.IsFavorite = isFav
		cand.IsRecent = isRecent
		switch {
		case isFav && isRecent:
			group1 = append(group1, cand)
		case isFav:
			group2 = append(group2, cand)
		case isRecent:
			group3 = append(group3, cand)
		default:
			group4 = append(group4, cand)
		}
	}
	sortByRecent(group1, recentPos)
	sortByRecent(group3, recentPos)
	result := make([]Candidate, 0, len(rest))
	result = append(result, group1...)
	result = append(result, group2...)
	result = append(result, group3...)
	result = append(result, group4...)
	return result
}

func sortByRecent(items []Candidate, recentPos map[string]int) {
	for i := 1; i < len(items); i++ {
		for j := i; j > 0; j-- {
			if recentPos[items[j-1].Script.ID] > recentPos[items[j].Script.ID] {
				items[j-1], items[j] = items[j], items[j-1]
			} else {
				break
			}
		}
	}
}

func (c *Core) Move(delta int) {
	if len(c.candidates) == 0 {
		return
	}
	next := c.selected + delta
	if next < 0 {
		next = 0
	}
	if next >= len(c.candidates) {
		next = len(c.candidates) - 1
	}
	c.selected = next
}

func (c *Core) Complete() (string, bool) {
	if !c.visible || len(c.candidates) == 0 {
		return c.input, false
	}
	c.input = "/" + c.candidates[c.selected].Script.Alias
	c.candidates = c.sortCandidates(c.catalog.Match(c.input))
	c.selected = 0
	return c.input, true
}

func (c *Core) Execute() error {
	target, err := c.Target()
	if err != nil {
		return err
	}
	if err := c.starter.Start(target.ID); err != nil {
		return err
	}
	c.visible = false
	if c.recent != nil {
		c.recent.Record(target.ID)
	}
	return nil
}

func (c *Core) Target() (config.ScriptConfig, error) {
	if !c.visible || len(c.candidates) == 0 {
		return config.ScriptConfig{}, fmt.Errorf("未找到匹配脚本")
	}
	return c.candidates[c.selected].Script, nil
}

// ToggleFavorite 处理 /fav <alias> 命令，返回用户可见反馈。
func (c *Core) ToggleFavorite(input string) (string, error) {
	alias := strings.TrimSpace(strings.TrimPrefix(strings.TrimSpace(input), "/fav"))
	if alias == "" {
		return "", fmt.Errorf("用法: /fav <alias>，alias 不能为空")
	}
	script, ok := c.catalog.FindAlias(alias)
	if !ok {
		return "", fmt.Errorf("未找到当前入口中的启用脚本: %s", alias)
	}
	if c.favorites == nil {
		return "", fmt.Errorf("收藏功能不可用")
	}
	favorite, err := c.favorites.Toggle(c.entryPath, script.ID)
	if err != nil {
		return "", fmt.Errorf("收藏状态已更新但保存失败: %v", err)
	}
	if favorite {
		return fmt.Sprintf("已收藏 /%s（%s）", script.Alias, script.Name), nil
	}
	return fmt.Sprintf("已取消收藏 /%s（%s）", script.Alias, script.Name), nil
}

func (c *Core) Escape()       { c.visible = false }
func (c *Core) Input() string { return c.input }
func (c *Core) Candidates() []Candidate {
	return append([]Candidate(nil), c.candidates...)
}
func (c *Core) Selected() int { return c.selected }
func (c *Core) Visible() bool { return c.visible }
