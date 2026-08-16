package task

import (
	"context"
	"os/exec"
	"sync"
	"testing"
	"time"

	"azkits/internal/config"
	"azkits/internal/runner"
)

type blockingExecutor struct {
	started chan struct{}
	release chan struct{}
	once    sync.Once
}

type interactiveExecutorStub struct{}

func (interactiveExecutorStub) Run(context.Context, config.ScriptConfig, func(runner.Event)) (runner.Result, error) {
	return runner.Result{}, nil
}

func (interactiveExecutorStub) Command(ctx context.Context, _ config.ScriptConfig) (*exec.Cmd, error) {
	return exec.CommandContext(ctx, "cmd.exe", "/d", "/c", "exit", "0"), nil
}

func (e *blockingExecutor) Run(ctx context.Context, cfg config.ScriptConfig, sink func(runner.Event)) (runner.Result, error) {
	e.once.Do(func() { close(e.started) })
	select {
	case <-e.release:
		return runner.Result{PID: 42, ExitCode: 0}, nil
	case <-ctx.Done():
		return runner.Result{PID: 42, ExitCode: -1}, ctx.Err()
	}
}

func TestServiceRejectsDuplicateAndPublishesState(t *testing.T) {
	exec := &blockingExecutor{started: make(chan struct{}), release: make(chan struct{})}
	svc := New(exec, []config.ScriptConfig{{ID: "one", Alias: "one", Enabled: true}})
	if err := svc.Start("one"); err != nil {
		t.Fatal(err)
	}
	select {
	case <-exec.started:
	case <-time.After(time.Second):
		t.Fatal("runner did not start")
	}
	if err := svc.Start("one"); err != ErrAlreadyRunning {
		t.Fatalf("duplicate Start() error = %v", err)
	}
	if got := svc.State("one"); got != Running && got != Starting {
		t.Fatalf("state = %s", got)
	}
	close(exec.release)
	deadline := time.Now().Add(time.Second)
	for svc.State("one") != Succeeded && time.Now().Before(deadline) {
		time.Sleep(time.Millisecond)
	}
	if got := svc.State("one"); got != Succeeded {
		t.Fatalf("final state = %s", got)
	}
}

func TestServiceStopOnlyOwnTask(t *testing.T) {
	exec := &blockingExecutor{started: make(chan struct{}), release: make(chan struct{})}
	svc := New(exec, []config.ScriptConfig{{ID: "one", Alias: "one", Enabled: true}})
	if err := svc.Start("one"); err != nil {
		t.Fatal(err)
	}
	<-exec.started
	if err := svc.Stop("one"); err != nil {
		t.Fatal(err)
	}
	deadline := time.Now().Add(time.Second)
	for svc.State("one") != Stopped && time.Now().Before(deadline) {
		time.Sleep(time.Millisecond)
	}
	if got := svc.State("one"); got != Stopped {
		t.Fatalf("state = %s", got)
	}
}

func TestServicePreparesAndFinishesInteractiveTask(t *testing.T) {
	svc := New(interactiveExecutorStub{}, []config.ScriptConfig{{ID: "one", Alias: "one", Enabled: true, Interactive: true}})
	if err := svc.Start("one"); err != ErrInteractiveRequiresTerminal {
		t.Fatalf("background Start() error = %v", err)
	}
	cmd, err := svc.PrepareInteractive("one")
	if err != nil {
		t.Fatal(err)
	}
	if cmd == nil || svc.State("one") != Running {
		t.Fatalf("cmd=%v state=%s", cmd, svc.State("one"))
	}
	if _, err := svc.PrepareInteractive("one"); err != ErrAlreadyRunning {
		t.Fatalf("duplicate PrepareInteractive() error = %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatal(err)
	}
	svc.FinishInteractive("one", runner.Result{PID: cmd.Process.Pid, ExitCode: cmd.ProcessState.ExitCode()}, nil)
	if got := svc.State("one"); got != Succeeded {
		t.Fatalf("final state = %s", got)
	}
}
