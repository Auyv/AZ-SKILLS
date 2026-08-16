package terminal

import (
	"strings"
	"testing"
)

func TestLogBufferIsBoundedAndMarksTruncation(t *testing.T) {
	b := NewLogBuffer(32)
	b.Append(strings.Repeat("a", 24))
	b.Append(strings.Repeat("b", 24))
	got := b.String()
	if len(got) > 96 {
		t.Fatalf("buffer unexpectedly large: %d", len(got))
	}
	if !strings.Contains(got, "日志已截断") || !strings.Contains(got, "b") {
		t.Fatalf("buffer = %q", got)
	}
}
