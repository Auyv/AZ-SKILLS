//go:build !windows

package main

import "io"

func consoleOutputCP() uint { return 65001 }

func setConsoleOutputCP(uint) {}

func consoleWriter(w io.Writer) io.Writer { return w }
