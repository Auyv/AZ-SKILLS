package catalog

import (
	"fmt"
	"strings"

	"azkits/internal/config"
)

type Rank int

const (
	ExactAlias Rank = iota
	Prefix
	Contains
)

type Candidate struct {
	Script config.ScriptConfig
	Rank   Rank
}

type Catalog struct {
	enabled []config.ScriptConfig
	aliases map[string]config.ScriptConfig
}

func New(scripts []config.ScriptConfig) (*Catalog, error) {
	c := &Catalog{aliases: make(map[string]config.ScriptConfig)}
	for _, script := range scripts {
		if !script.Enabled {
			continue
		}
		key := strings.ToLower(script.Alias)
		if _, exists := c.aliases[key]; exists {
			return nil, fmt.Errorf("alias 重复: %s", script.Alias)
		}
		c.aliases[key] = script
		c.enabled = append(c.enabled, script)
	}
	return c, nil
}

func (c *Catalog) Enabled() []config.ScriptConfig {
	return append([]config.ScriptConfig(nil), c.enabled...)
}

func (c *Catalog) FindAlias(alias string) (config.ScriptConfig, bool) {
	value, ok := c.aliases[strings.ToLower(strings.TrimPrefix(strings.TrimSpace(alias), "/"))]
	return value, ok
}

func (c *Catalog) Match(query string) []Candidate {
	query = strings.ToLower(strings.TrimPrefix(strings.TrimSpace(query), "/"))
	if query == "" {
		result := make([]Candidate, 0, len(c.enabled))
		for _, script := range c.enabled {
			result = append(result, Candidate{Script: script, Rank: Prefix})
		}
		return result
	}
	groups := [3][]Candidate{}
	for _, script := range c.enabled {
		alias := strings.ToLower(script.Alias)
		name := strings.ToLower(script.Name)
		description := strings.ToLower(script.Description)
		rank := Rank(-1)
		switch {
		case alias == query:
			rank = ExactAlias
		case strings.HasPrefix(alias, query) || strings.HasPrefix(name, query) || strings.HasPrefix(description, query):
			rank = Prefix
		case strings.Contains(alias, query) || strings.Contains(name, query) || strings.Contains(description, query):
			rank = Contains
		}
		if rank >= ExactAlias {
			groups[rank] = append(groups[rank], Candidate{Script: script, Rank: rank})
		}
	}
	result := make([]Candidate, 0, len(c.enabled))
	for _, group := range groups {
		result = append(result, group...)
	}
	return result
}
