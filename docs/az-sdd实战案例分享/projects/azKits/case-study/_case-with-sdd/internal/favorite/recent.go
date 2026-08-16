package favorite

import (
	"sync"
)

// RecentTracker 记录当前实例最近成功启动的脚本，仅内存态，不落盘。
// 同一脚本重复启动时移到最前；超出上限时淘汰最旧。
type RecentTracker struct {
	mu    sync.Mutex
	order []string
	limit int
}

func NewRecentTracker(limit int) *RecentTracker {
	if limit < 1 {
		limit = 30
	}
	return &RecentTracker{limit: limit}
}

func (r *RecentTracker) Record(id string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.order = removeID(r.order, id)
	r.order = append([]string{id}, r.order...)
	if r.limit > 0 && len(r.order) > r.limit {
		r.order = r.order[:r.limit]
	}
}

// List 返回按最近使用时间倒序的脚本 id 列表（最新在前）。
func (r *RecentTracker) List() []string {
	r.mu.Lock()
	defer r.mu.Unlock()
	return append([]string(nil), r.order...)
}
