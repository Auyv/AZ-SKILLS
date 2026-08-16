package tui

import (
	"azkits/internal/run"
)

// lineMsg carries one decoded output line from a running script.
type lineMsg struct {
	text  string
	isErr bool
}

// runStartErrMsg reports a failure to start a non-interactive script.
type runStartErrMsg struct {
	err error
}

// runDoneMsg reports the completion of a non-interactive script.
type runDoneMsg struct {
	scriptID string
	result   run.Result
}

// interactiveDoneMsg reports the completion of an interactive script run
// through tea.ExecProcess.
type interactiveDoneMsg struct {
	scriptID string
	err      error
}
