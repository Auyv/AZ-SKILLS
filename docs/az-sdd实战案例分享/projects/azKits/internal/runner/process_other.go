//go:build !windows

package runner

import "os/exec"

func setProcessGroup(_ *exec.Cmd) {}
