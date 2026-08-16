package task

import (
	"context"
	"errors"
	"fmt"
	"os/exec"
	"sync"

	"azkits/internal/config"
	"azkits/internal/runner"
)

type State string

const (
	Ready     State = "ready"
	Starting  State = "starting"
	Running   State = "running"
	Stopping  State = "stopping"
	Succeeded State = "succeeded"
	Failed    State = "failed"
	Stopped   State = "stopped"
)

var ErrAlreadyRunning = errors.New("脚本正在运行")
var ErrInteractiveRequiresTerminal = errors.New("交互脚本需要前台终端")

type Executor interface {
	Run(context.Context, config.ScriptConfig, func(runner.Event)) (runner.Result, error)
}

type InteractiveExecutor interface {
	Command(context.Context, config.ScriptConfig) (*exec.Cmd, error)
}

type Event struct {
	ScriptID string
	State    State
	Stream   runner.Stream
	Data     []byte
	PID      int
	ExitCode int
	Err      error
}

type Service struct {
	mu       sync.RWMutex
	executor Executor
	scripts  map[string]config.ScriptConfig
	states   map[string]State
	cancels  map[string]context.CancelFunc
	events   chan Event
}

func New(executor Executor, scripts []config.ScriptConfig) *Service {
	s := &Service{executor: executor, scripts: make(map[string]config.ScriptConfig), states: make(map[string]State), cancels: make(map[string]context.CancelFunc), events: make(chan Event, 256)}
	for _, script := range scripts {
		if script.Enabled {
			s.scripts[script.ID] = script
			s.states[script.ID] = Ready
		}
	}
	return s
}

func (s *Service) Start(id string) error {
	s.mu.Lock()
	cfg, ok := s.scripts[id]
	if !ok {
		s.mu.Unlock()
		return fmt.Errorf("脚本不存在或未启用: %s", id)
	}
	state := s.states[id]
	if cfg.Interactive {
		s.mu.Unlock()
		return ErrInteractiveRequiresTerminal
	}
	if state == Starting || state == Running || state == Stopping {
		s.mu.Unlock()
		return ErrAlreadyRunning
	}
	ctx, cancel := context.WithCancel(context.Background())
	s.cancels[id] = cancel
	s.states[id] = Starting
	s.mu.Unlock()
	s.publish(Event{ScriptID: id, State: Starting})
	go s.run(ctx, cfg)
	return nil
}

func (s *Service) PrepareInteractive(id string) (*exec.Cmd, error) {
	s.mu.Lock()
	cfg, ok := s.scripts[id]
	if !ok {
		s.mu.Unlock()
		return nil, fmt.Errorf("脚本不存在或未启用: %s", id)
	}
	if !cfg.Interactive {
		s.mu.Unlock()
		return nil, fmt.Errorf("脚本未配置为交互模式: %s", id)
	}
	state := s.states[id]
	if state == Starting || state == Running || state == Stopping {
		s.mu.Unlock()
		return nil, ErrAlreadyRunning
	}
	builder, ok := s.executor.(InteractiveExecutor)
	if !ok {
		s.mu.Unlock()
		return nil, fmt.Errorf("执行器不支持交互脚本")
	}
	s.states[id] = Starting
	s.mu.Unlock()
	s.publish(Event{ScriptID: id, State: Starting})

	cmd, err := builder.Command(context.Background(), cfg)
	if err != nil {
		s.FinishInteractive(id, runner.Result{ExitCode: -1}, err)
		return nil, err
	}
	s.setState(id, Running)
	return cmd, nil
}

func (s *Service) FinishInteractive(id string, result runner.Result, runErr error) {
	state := Succeeded
	if runErr != nil || result.ExitCode != 0 {
		state = Failed
	}
	s.mu.Lock()
	s.states[id] = state
	s.mu.Unlock()
	s.publish(Event{ScriptID: id, State: state, PID: result.PID, ExitCode: result.ExitCode, Err: runErr})
}

func (s *Service) run(ctx context.Context, cfg config.ScriptConfig) {
	s.setState(cfg.ID, Running)
	result, err := s.executor.Run(ctx, cfg, func(event runner.Event) {
		s.publish(Event{ScriptID: cfg.ID, State: Running, Stream: event.Stream, Data: event.Data})
	})
	s.mu.Lock()
	delete(s.cancels, cfg.ID)
	state := Succeeded
	if ctx.Err() != nil {
		state = Stopped
	} else if err != nil || result.ExitCode != 0 {
		state = Failed
	}
	s.states[cfg.ID] = state
	s.mu.Unlock()
	s.publish(Event{ScriptID: cfg.ID, State: state, PID: result.PID, ExitCode: result.ExitCode, Err: err})
}

func (s *Service) Stop(id string) error {
	s.mu.Lock()
	cancel, ok := s.cancels[id]
	if !ok {
		s.mu.Unlock()
		return fmt.Errorf("脚本未运行: %s", id)
	}
	s.states[id] = Stopping
	s.mu.Unlock()
	s.publish(Event{ScriptID: id, State: Stopping})
	cancel()
	return nil
}

func (s *Service) State(id string) State {
	s.mu.RLock()
	defer s.mu.RUnlock()
	state, ok := s.states[id]
	if !ok {
		return Ready
	}
	return state
}

func (s *Service) Events() <-chan Event { return s.events }

func (s *Service) setState(id string, state State) {
	s.mu.Lock()
	s.states[id] = state
	s.mu.Unlock()
	s.publish(Event{ScriptID: id, State: state})
}

func (s *Service) publish(event Event) {
	select {
	case s.events <- event:
	default:
		// 状态保存在 registry 中；慢消费者只可能丢失中间日志事件。
	}
}
