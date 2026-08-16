package terminal

import (
	"strings"
	"testing"

	"charm.land/bubbles/v2/textinput"
)

func TestLogBufferBasic(t *testing.T) {
	b := NewLogBuffer(100)
	b.Append("hello")
	b.Append(" world")
	if got := b.String(); got != "hello world" {
		t.Errorf("String = %q", got)
	}
}

func TestLogBufferTruncation(t *testing.T) {
	b := NewLogBuffer(10)
	b.Append(strings.Repeat("x", 20))
	out := b.String()
	if !strings.HasPrefix(out, truncationMarker) {
		t.Errorf("应含截断标记: %q", out[:20])
	}
	if len(out) <= len(truncationMarker) {
		t.Errorf("截断后应保留末尾数据，长度=%d", len(out))
	}
}

func TestLogBufferMultiAppendTruncation(t *testing.T) {
	b := NewLogBuffer(20)
	b.Append("1234567890")
	b.Append("abcdefghij")
	b.Append("XXXX")
	out := b.String()
	if strings.Contains(out, "12345") {
		t.Errorf("旧内容应被丢弃: %q", out)
	}
	if !strings.Contains(out, "XXXX") {
		t.Errorf("新内容应保留: %q", out)
	}
}

func TestLogBufferMinSize(t *testing.T) {
	b := NewLogBuffer(0)
	if b.maxLen() != 1 {
		t.Errorf("max = %d", b.maxLen())
	}
}

func TestVisibleRange(t *testing.T) {
	cases := []struct {
		selected, total, limit int
		start, end             int
	}{
		{0, 3, 8, 0, 3},
		{0, 10, 8, 0, 8},
		{5, 10, 8, 1, 9},
		{9, 10, 8, 2, 10},
		{0, 1, 8, 0, 1},
	}
	for _, tc := range cases {
		start, end := visibleRange(tc.selected, tc.total, tc.limit)
		if start != tc.start || end != tc.end {
			t.Errorf("visibleRange(%d,%d,%d) = (%d,%d), want (%d,%d)",
				tc.selected, tc.total, tc.limit, start, end, tc.start, tc.end)
		}
	}
}

func TestTextInputIntegration(t *testing.T) {
	input := textinput.New()
	input.Focus()
	input.SetValue("/")
	if got := input.Value(); got != "/" {
		t.Errorf("Value = %q", got)
	}
}
