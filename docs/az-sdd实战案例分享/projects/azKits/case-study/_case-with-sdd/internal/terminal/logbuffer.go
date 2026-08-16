package terminal

import (
	"strings"
	"sync"
)

const truncationMarker = "[azKits] 日志已截断，较早内容已丢弃。\n"

type LogBuffer struct {
	mu        sync.RWMutex
	max       int
	data      string
	truncated bool
}

func NewLogBuffer(max int) *LogBuffer {
	if max < 1 {
		max = 1
	}
	return &LogBuffer{max: max}
}

func (b *LogBuffer) Append(value string) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.data += value
	if len(b.data) <= b.max {
		return
	}
	b.truncated = true
	b.data = b.data[len(b.data)-b.max:]
	b.data = strings.ToValidUTF8(b.data, "�")
}

func (b *LogBuffer) String() string {
	b.mu.RLock()
	defer b.mu.RUnlock()
	if b.truncated {
		return truncationMarker + b.data
	}
	return b.data
}

func (b *LogBuffer) maxLen() int {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return b.max
}

func visibleRange(selected, total, limit int) (int, int) {
	if total <= limit {
		return 0, total
	}
	start := selected - limit/2
	if start < 0 {
		start = 0
	}
	if start+limit > total {
		start = total - limit
	}
	return start, start + limit
}
