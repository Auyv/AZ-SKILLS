package terminal

import (
	"testing"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/task"
)

type starterStub struct {
	started []string
	states  map[string]task.State
}

func (s *starterStub) Start(id string) error      { s.started = append(s.started, id); return nil }
func (s *starterStub) State(id string) task.State { return s.states[id] }

func newCore(t *testing.T) (*Core, *starterStub) {
	t.Helper()
	c, err := catalog.New([]config.ScriptConfig{
		{ID: "1", Alias: "device-init", Name: "设备初始化", Description: "初始化设备", Enabled: true},
		{ID: "2", Alias: "device-check", Name: "设备检查", Description: "检查设备连接", Enabled: true},
	})
	if err != nil {
		t.Fatal(err)
	}
	stub := &starterStub{states: map[string]task.State{"1": task.Ready, "2": task.Ready}}
	return NewCore(c, stub), stub
}

func TestSlashFilterNavigationAndEscape(t *testing.T) {
	core, _ := newCore(t)
	core.SetInput("/")
	if !core.Visible() || len(core.Candidates()) != 2 || core.Selected() != 0 {
		t.Fatalf("initial state visible=%v candidates=%d selected=%d", core.Visible(), len(core.Candidates()), core.Selected())
	}
	core.SetInput("/检查")
	if len(core.Candidates()) != 1 || core.Candidates()[0].Script.ID != "2" {
		t.Fatalf("filtered = %#v", core.Candidates())
	}
	core.Move(1)
	core.Move(1)
	if core.Selected() != 0 {
		t.Fatalf("selection crossed boundary: %d", core.Selected())
	}
	core.Escape()
	if core.Visible() {
		t.Fatal("Escape must close candidates")
	}
}

func TestTabOnlyCompletesAndEnterExecutes(t *testing.T) {
	core, starter := newCore(t)
	core.SetInput("/device")
	core.Move(1)
	if completed, ok := core.Complete(); !ok || completed != "/device-check" {
		t.Fatalf("Complete() = %q, %v", completed, ok)
	}
	if len(starter.started) != 0 {
		t.Fatal("Tab completion must not execute")
	}
	if err := core.Execute(); err != nil {
		t.Fatal(err)
	}
	if len(starter.started) != 1 || starter.started[0] != "2" {
		t.Fatalf("started = %#v", starter.started)
	}
}

func TestIMECombinedCommitAndEnterDoesNotExecute(t *testing.T) {
	core, starter := newCore(t)
	core.SetInput("/")
	core.CommitText("设备", true)
	if len(starter.started) != 0 {
		t.Fatal("IME commit batch must not execute")
	}
	if got := core.Input(); got != "/设备" {
		t.Fatalf("input = %q", got)
	}
	if err := core.Execute(); err != nil {
		t.Fatal(err)
	}
	if len(starter.started) != 1 {
		t.Fatalf("independent Enter should execute, got %#v", starter.started)
	}
}

func TestNoMatchCannotCompleteOrExecute(t *testing.T) {
	core, starter := newCore(t)
	core.SetInput("/missing")
	if _, ok := core.Complete(); ok {
		t.Fatal("no match completed")
	}
	if err := core.Execute(); err == nil {
		t.Fatal("no match executed")
	}
	if len(starter.started) != 0 {
		t.Fatal("no match started script")
	}
}

func TestTargetReturnsSelectedScriptWithoutStartingIt(t *testing.T) {
	core, starter := newCore(t)
	core.SetInput("/device")
	core.Move(1)
	target, err := core.Target()
	if err != nil {
		t.Fatal(err)
	}
	if target.ID != "2" || len(starter.started) != 0 {
		t.Fatalf("target=%s started=%#v", target.ID, starter.started)
	}
}
