//go:build windows

package main

import (
	"io"

	"golang.org/x/sys/windows"
	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

// consoleOutputCP returns the current console output code page.
func consoleOutputCP() uint {
	cp, err := windows.GetConsoleOutputCP()
	if err != nil {
		return 65001
	}
	return uint(cp)
}

// setConsoleOutputCP sets the console output code page.
func setConsoleOutputCP(cp uint) {
	_ = windows.SetConsoleOutputCP(uint32(cp))
}

// consoleWriter wraps w so that UTF-8 text written to it is re-encoded into
// the console's output code page. UTF-8 consoles pass through untouched.
func consoleWriter(w io.Writer) io.Writer {
	return wrapConsoleCP(w, consoleOutputCP())
}

// wrapConsoleCP applies the code-page conversion without touching the real
// console, keeping the logic unit-testable.
func wrapConsoleCP(w io.Writer, cp uint) io.Writer {
	switch cp {
	case 65001:
		return w
	case 936:
		return transform.NewWriter(w, simplifiedchinese.GBK.NewEncoder())
	case 54936:
		return transform.NewWriter(w, simplifiedchinese.GB18030.NewEncoder())
	default:
		return w
	}
}
