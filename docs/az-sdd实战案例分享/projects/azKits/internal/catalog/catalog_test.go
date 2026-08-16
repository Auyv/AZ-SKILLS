package catalog

import (
	"fmt"
	"testing"

	"azkits/internal/config"
)

func scripts() []config.ScriptConfig {
	return []config.ScriptConfig{
		{ID: "1", Alias: "device-init", Name: "设备初始化", Description: "初始化设备", Enabled: true},
		{ID: "2", Alias: "device-check", Name: "设备检查", Description: "检查连接", Enabled: true},
		{ID: "3", Alias: "regression", Name: "回归测试", Description: "设备批量检查", Enabled: true},
		{ID: "4", Alias: "disabled", Name: "禁用", Description: "不可见", Enabled: false},
	}
}

func TestEnabledAndFindAlias(t *testing.T) {
	c, err := New(scripts())
	if err != nil {
		t.Fatal(err)
	}
	if got := len(c.Enabled()); got != 3 {
		t.Fatalf("Enabled() len = %d, want 3", got)
	}
	if got, ok := c.FindAlias("DEVICE-INIT"); !ok || got.ID != "1" {
		t.Fatalf("FindAlias() = %#v, %v", got, ok)
	}
	if _, ok := c.FindAlias("disabled"); ok {
		t.Fatal("disabled script must not be found")
	}
}

func TestMatchStableRankingAndChinese(t *testing.T) {
	c, err := New(scripts())
	if err != nil {
		t.Fatal(err)
	}
	got := c.Match("/device-check")
	if len(got) != 1 || got[0].Script.ID != "2" || got[0].Rank != ExactAlias {
		t.Fatalf("exact match = %#v", got)
	}
	got = c.Match("device")
	if len(got) != 2 || got[0].Script.ID != "1" || got[1].Script.ID != "2" {
		t.Fatalf("stable prefix order = %#v", got)
	}
	got = c.Match("设备")
	if len(got) != 3 || got[0].Script.ID != "1" || got[1].Script.ID != "2" || got[2].Script.ID != "3" {
		t.Fatalf("Chinese match order = %#v", got)
	}
}

func BenchmarkMatch500(b *testing.B) {
	items := make([]config.ScriptConfig, 500)
	for i := range items {
		items[i] = config.ScriptConfig{ID: fmt.Sprint(i), Alias: fmt.Sprintf("script-%03d", i), Name: fmt.Sprintf("脚本%03d", i), Description: "性能测试脚本", Enabled: true}
	}
	c, err := New(items)
	if err != nil {
		b.Fatal(err)
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = c.Match("script-49")
	}
}
