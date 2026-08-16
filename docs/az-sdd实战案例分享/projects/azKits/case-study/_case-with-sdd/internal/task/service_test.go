package task

import (
	"context"
	"errors"
	"os/exec"
	"sync"
	"testing"
	"time"

	"azkits/internal/config"
	"azkits/internal/runner"
)

type fakeExecutor struct {
	mu      sync.Mutex
	started map[string]int
	delay   time.Duration
}

func (f *fakeExecutor) Run(ctx context.Context, cfg config.ScriptConfig, sink func(runner.Event)) (runner.Result, error) {
	f.mu.Lock()
	f.started[cfg.ID]++
	f.mu.Unlock()
	if f.delay > 0 {
		select {
		case <-time.After(f.delay):
		case <-ctx.Done():
			return runner.Result{ExitCode: -1}, ctx.Err()
		}
	}
	sink(runner.Event{Stream: runner.Stdout, Data: []byte("输出 " + cfg.ID)})
	return runner.Result{ExitCode: 0}, nil
}

type fakeInteractive struct {
	fakeExecutor
}

func (f *fakeInteractive) Command(ctx context.Context, cfg config.ScriptConfig) (*exec.Cmd, error) {
	return exec.Command("cmd.exe", "/c", "exit 0"), nil
}

func script(id string, interactive bool) config.ScriptConfig {
	return config.ScriptConfig{ID: id, Alias: id, Name: id, Enabled: true, Interactive: interactive}
}

func drain(events <-chan Event) []Event {
	var result []Event
	timeout := time.After(2 * time.Second)
	for {
		select {
		case event, ok := <-events:
			if !ok {
				return result
			}
			result = append(result, event)
		case <-timeout:
			return result
		}
	}
}

func TestStartSucceeded(t *testing.T) {
	executor := &fakeExecutor{started: make(map[string]int)}
	s := New(executor, []config.ScriptConfig{script("a", false)})
	if err := s.Start("a"); err != nil {
		t.Fatalf("Start: %v", err)
	}
	if err := s.Start("a"); !errors.Is(err, ErrAlreadyRunning) {
		t.Errorf("重复启动应拒绝: %v", err)
	}
	if err := s.Start("missing"); err == nil {
		t.Error("未知脚本应失败")
	}
	events := drain(s.Events())
	var sawSucceeded bool
	for _, event := range events {
		if event.State == Succeeded && event.ScriptID == "a" {
			sawSucceeded = true
		}
	}
	if !sawSucceeded {
		t.Errorf("未收到成功事件: %+v", events)
	}
}

func TestInteractiveRequiresTerminal(t *testing.T) {
	s := New(&fakeExecutor{started: make(map[string]int)}, []config.ScriptConfig{script("a", true)})
	if err := s.Start("a"); !errors.Is(err, ErrInteractiveRequiresTerminal) {
		t.Errorf("交互脚本后台启动应失败: %v", err)
	}
}

func TestPrepareAndFinishInteractive(t *testing.T) {
	s := New(&fakeInteractive{fakeExecutor: fakeExecutor{started: make(map[string]int)}}, []config.ScriptConfig{script("a", true)})
	cmd, err := s.PrepareInteractive("a")
	if err != nil {
		t.Fatalf("PrepareInteractive: %v", err)
	}
	if cmd == nil {
		t.Fatal("cmd 为空")
	}
	if _, err := s.PrepareInteractive("a"); !errors.Is(err, ErrAlreadyRunning) {
		t.Errorf("重复交互启动应拒绝: %v", err)
	}
	s.FinishInteractive("a", runner.Result{ExitCode: 0}, nil)
	if got := s.State("a"); got != Succeeded {
		t.Errorf("State = %s", got)
	}
}

func TestStop(t *testing.T) {
	executor := &fakeExecutor{started: make(map[string]int), delay: 200 * time.Millisecond}
	s := New(executor, []config.ScriptConfig{script("a", false)})
	if err := s.Start("a"); err != nil {
		t.Fatal(err)
	}
	time.Sleep(30 * time.Millisecond)
	if err := s.Stop("a"); err != nil {
		t.Fatalf("Stop: %v", err)
	}
	events := drain(s.Events())
	var sawStopped bool
	for _, event := range events {
		if event.State == Stopped {
			sawStopped = true
		}
	}
	if !sawStopped {
		t.Errorf("未收到 Stopped 事件: %+v", events)
	}
	if err := s.Stop("a"); err == nil {
		t.Error("任务结束后二次 Stop 应失败")
	}
}

func TestStateInitial(t *testing.T) {
	s := New(&fakeExecutor{started: make(map[string]int)}, []config.ScriptConfig{script("a", false)})
	if got := s.State("a"); got != Ready {
		t.Errorf("State = %s", got)
	}
	if got := s.State("zzz"); got != Ready {
		t.Errorf("未知脚本 State = %s", got)
	}
}

func TestEventOrderingMutexSafe(t *testing.T) {
	executor := &fakeExecutor{started: make(map[string]int), delay: 200 * time.Millisecond}
	s := New(executor, []config.ScriptConfig{script("a", false), script("b", false)})
	var wg sync.WaitGroup
	for _, id := range []string{"a", "b", "a", "b"} {
		wg.Add(1)
		go func(id string) {
			defer wg.Done()
			_ = s.Start(id)
		}(id)
	}
	wg.Wait()
	time.Sleep(50 * time.Millisecond)
	executor.mu.Lock()
	if executor.started["a"] != 1 {
		t.Errorf("脚本 a 启动次数 = %d, want 1", executor.started["a"])
	}
	if executor.started["b"] != 1 {
		t.Errorf("脚本 b 启动次数 = %d, want 1", executor.started["b"])
	}
	executor.mu.Unlock()
}
