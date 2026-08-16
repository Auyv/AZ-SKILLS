package logexport

import (
	"fmt"
	"sync"
	"time"
)

type Entry struct {
	Stream string
	Text   string
}

type Run struct {
	ToolboxName string
	ScriptID    string
	Alias       string
	ScriptName  string
	StartedAt   time.Time
	EndedAt     time.Time
	State       string
	ExitCode    *int
	Entries     []Entry
}

type Tracker struct {
	mu      sync.RWMutex
	active  map[string]Run
	last    Run
	hasLast bool
}

func NewTracker() *Tracker {
	return &Tracker{active: make(map[string]Run)}
}

func (t *Tracker) Start(run Run) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if _, exists := t.active[run.ScriptID]; exists {
		return fmt.Errorf("执行记录已存在: %s", run.ScriptID)
	}
	run.EndedAt = time.Time{}
	run.State = ""
	run.ExitCode = nil
	run.Entries = nil
	t.active[run.ScriptID] = run
	return nil
}

func (t *Tracker) Append(scriptID string, entry Entry) bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	run, ok := t.active[scriptID]
	if !ok {
		return false
	}
	run.Entries = append(run.Entries, entry)
	t.active[scriptID] = run
	return true
}

func (t *Tracker) Finish(scriptID, state string, exitCode *int, endedAt time.Time) bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	run, ok := t.active[scriptID]
	if !ok {
		return false
	}
	delete(t.active, scriptID)
	run.State = state
	run.EndedAt = endedAt
	if exitCode != nil {
		value := *exitCode
		run.ExitCode = &value
	}
	t.last = cloneRun(run)
	t.hasLast = true
	return true
}

func (t *Tracker) HasActive() bool {
	t.mu.RLock()
	defer t.mu.RUnlock()
	return len(t.active) > 0
}

func (t *Tracker) Last() (Run, bool) {
	t.mu.RLock()
	defer t.mu.RUnlock()
	if !t.hasLast {
		return Run{}, false
	}
	return cloneRun(t.last), true
}

func cloneRun(run Run) Run {
	run.Entries = append([]Entry(nil), run.Entries...)
	if run.ExitCode != nil {
		value := *run.ExitCode
		run.ExitCode = &value
	}
	return run
}
