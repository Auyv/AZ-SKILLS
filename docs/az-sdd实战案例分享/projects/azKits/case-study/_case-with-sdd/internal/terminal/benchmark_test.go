package terminal

import (
	"fmt"
	"path/filepath"
	"testing"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/favorite"
)

// BenchmarkCandidateSorting500 验证 500 条启用脚本 + 收藏/最近使用置顶排序
// 满足 0014 验收标准 [016]：单次有效输入后候选更新不超过 50 毫秒。
func BenchmarkCandidateSorting500(b *testing.B) {
	scripts := make([]config.ScriptConfig, 0, 500)
	for i := 0; i < 500; i++ {
		scripts = append(scripts, config.ScriptConfig{
			ID: fmt.Sprintf("id-%03d", i), Alias: fmt.Sprintf("alias-%03d", i),
			Name: fmt.Sprintf("中文名称%03d", i), Description: fmt.Sprintf("功能简述描述%03d", i),
			Enabled: true,
		})
	}
	cat, err := catalog.New(scripts)
	if err != nil {
		b.Fatal(err)
	}
	favs, err := favorite.Load(filepath.Join(b.TempDir(), "favs.json"))
	if err != nil {
		b.Fatal(err)
	}
	entry := `C:\tools\entry.yaml`
	// 收藏 50 条
	for i := 0; i < 50; i++ {
		if _, err := favs.Toggle(entry, fmt.Sprintf("id-%03d", i)); err != nil {
			b.Fatal(err)
		}
	}
	recent := favorite.NewRecentTracker(30)
	// 最近使用 30 条
	for i := 0; i < 30; i++ {
		recent.Record(fmt.Sprintf("id-%03d", i+10))
	}
	core := NewCore(cat, &fakeStarter{}, favs, recent, entry)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		core.SetInput("/alias")
	}
}
