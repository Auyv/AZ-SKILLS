package terminal

import (
	"fmt"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/task"
)

type Starter interface {
	Start(string) error
	State(string) task.State
}

type Core struct {
	catalog    *catalog.Catalog
	starter    Starter
	input      string
	candidates []catalog.Candidate
	selected   int
	visible    bool
}

func NewCore(c *catalog.Catalog, starter Starter) *Core {
	return &Core{catalog: c, starter: starter}
}

func (c *Core) SetInput(value string) {
	c.input = value
	c.selected = 0
	c.visible = len(value) > 0 && value[0] == '/'
	if c.visible {
		c.candidates = c.catalog.Match(value)
	} else {
		c.candidates = nil
	}
}

func (c *Core) CommitText(text string, includedEnter bool) {
	// includedEnter 表示同一 Windows 输入批次内的 IME 上屏确认；该批次只提交文本。
	c.SetInput(c.input + text)
	_ = includedEnter
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
	c.candidates = c.catalog.Match(c.input)
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
	return nil
}

func (c *Core) Target() (config.ScriptConfig, error) {
	if !c.visible || len(c.candidates) == 0 {
		return config.ScriptConfig{}, fmt.Errorf("未找到匹配脚本")
	}
	return c.candidates[c.selected].Script, nil
}

func (c *Core) Escape()       { c.visible = false }
func (c *Core) Input() string { return c.input }
func (c *Core) Candidates() []catalog.Candidate {
	return append([]catalog.Candidate(nil), c.candidates...)
}
func (c *Core) Selected() int { return c.selected }
func (c *Core) Visible() bool { return c.visible }
