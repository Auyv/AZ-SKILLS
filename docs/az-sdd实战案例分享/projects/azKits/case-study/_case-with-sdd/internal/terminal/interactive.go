package terminal

import (
	"bytes"
	"io"
	"os/exec"
	"sync"

	"azkits/internal/runner"
)

// interactiveCommand lets Bubble Tea release the terminal to the child
// process while teeing its output into an in-memory copy for the TUI log.
type interactiveCommand struct {
	cmd    *exec.Cmd
	stdout bytes.Buffer
	stderr bytes.Buffer
	mu     sync.Mutex
	events []runner.Event
}

func newInteractiveCommand(cmd *exec.Cmd) *interactiveCommand {
	return &interactiveCommand{cmd: cmd}
}

func (c *interactiveCommand) SetStdin(source io.Reader) {
	c.cmd.Stdin = source
}

func (c *interactiveCommand) SetStdout(target io.Writer) {
	c.cmd.Stdout = interactiveCaptureWriter{command: c, stream: runner.Stdout, target: target}
}

func (c *interactiveCommand) SetStderr(target io.Writer) {
	c.cmd.Stderr = interactiveCaptureWriter{command: c, stream: runner.Stderr, target: target}
}

func (c *interactiveCommand) Events() []runner.Event {
	c.mu.Lock()
	defer c.mu.Unlock()
	result := make([]runner.Event, len(c.events))
	for index, event := range c.events {
		result[index] = runner.Event{Stream: event.Stream, Data: append([]byte(nil), event.Data...)}
	}
	return result
}

type interactiveCaptureWriter struct {
	command *interactiveCommand
	stream  runner.Stream
	target  io.Writer
}

func (w interactiveCaptureWriter) Write(data []byte) (int, error) {
	w.command.mu.Lock()
	defer w.command.mu.Unlock()
	written, err := w.target.Write(data)
	if written > 0 {
		captured := append([]byte(nil), data[:written]...)
		w.command.events = append(w.command.events, runner.Event{Stream: w.stream, Data: captured})
		if w.stream == runner.Stdout {
			_, _ = w.command.stdout.Write(captured)
		} else {
			_, _ = w.command.stderr.Write(captured)
		}
	}
	return written, err
}

func (c *interactiveCommand) Run() error {
	return c.cmd.Run()
}

func (c *interactiveCommand) Result() runner.Result {
	result := runner.Result{ExitCode: -1}
	if c.cmd.Process != nil {
		result.PID = c.cmd.Process.Pid
	}
	if c.cmd.ProcessState != nil {
		result.ExitCode = c.cmd.ProcessState.ExitCode()
	}
	return result
}

type interactiveFinishedMsg struct {
	scriptID string
	encoding string
	result   runner.Result
	stdout   []byte
	stderr   []byte
	err      error
	events   []runner.Event
}
