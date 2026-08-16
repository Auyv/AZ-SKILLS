package catalog

import (
	"fmt"
	"testing"

	"azkits/internal/config"
)

func script(id, alias, name, desc string, enabled bool) config.ScriptConfig {
	return config.ScriptConfig{ID: id, Alias: alias, Name: name, Description: desc, Enabled: enabled}
}

func TestNewFiltersDisabled(t *testing.T) {
	scripts := []config.ScriptConfig{
		script("a", "alpha", "阿尔法", "第一个", true),
		script("b", "beta", "贝塔", "第二个", false),
		script("c", "gamma", "伽马", "第三个", true),
	}
	c, err := New(scripts)
	if err != nil {
		t.Fatalf("New 失败: %v", err)
	}
	if got := len(c.Enabled()); got != 2 {
		t.Errorf("Enabled = %d, want 2", got)
	}
}

func TestNewDuplicateAlias(t *testing.T) {
	scripts := []config.ScriptConfig{
		script("a", "RUN", "甲", "一", true),
		script("b", "run", "乙", "二", true),
	}
	if _, err := New(scripts); err == nil {
		t.Fatal("应检测到重复 alias")
	}
}

func TestFindAlias(t *testing.T) {
	c, err := New([]config.ScriptConfig{script("a", "Run-Test", "甲", "一", true)})
	if err != nil {
		t.Fatal(err)
	}
	for _, query := range []string{"run-test", "Run-Test", "/run-test", " run-test "} {
		if got, ok := c.FindAlias(query); !ok || got.ID != "a" {
			t.Errorf("FindAlias(%q) 失败", query)
		}
	}
	if _, ok := c.FindAlias("missing"); ok {
		t.Error("不应找到缺失别名")
	}
}

func TestMatchEmptyQuery(t *testing.T) {
	c, _ := New([]config.ScriptConfig{script("a", "aa", "甲", "一", true), script("b", "bb", "乙", "二", true)})
	result := c.Match("/")
	if len(result) != 2 {
		t.Fatalf("空查询应返回全部启用脚本, got %d", len(result))
	}
	if result[0].Script.ID != "a" || result[1].Script.ID != "b" {
		t.Error("空查询应按 YAML 顺序")
	}
}

func TestMatchRankingAndOrder(t *testing.T) {
	c, _ := New([]config.ScriptConfig{
		script("contains", "xload", "加载数据", "数据加载工具", true),
		script("prefix", "loader", "加载器", "工具", true),
		script("exact", "load", "加载", "工具", true),
	})
	result := c.Match("load")
	var order []string
	for _, item := range result {
		order = append(order, item.Script.ID)
	}
	want := []string{"exact", "prefix", "contains"}
	if fmt.Sprint(order) != fmt.Sprint(want) {
		t.Errorf("排序 = %v, want %v", order, want)
	}
	if result[0].Rank != ExactAlias || result[1].Rank != Prefix || result[2].Rank != Contains {
		t.Errorf("等级错误: %+v", result)
	}
}

func TestMatchChineseCaseInsensitive(t *testing.T) {
	c, _ := New([]config.ScriptConfig{script("a", "Alpha", "初始化设备", "用于设备测试", true)})
	for _, q := range []string{"alpha", "ALPHA", "初始化", "设备测试"} {
		if got := c.Match(q); len(got) != 1 {
			t.Errorf("Match(%q) = %d 条", q, len(got))
		}
	}
	if got := c.Match("不存在"); len(got) != 0 {
		t.Errorf("Match(不存在) = %d 条", len(got))
	}
}

func TestMatchStableOrderSameRank(t *testing.T) {
	c, _ := New([]config.ScriptConfig{
		script("a", "test1", "测试一", "甲", true),
		script("b", "test2", "测试二", "乙", true),
		script("c", "test3", "测试三", "丙", true),
	})
	result := c.Match("test")
	var order []string
	for _, item := range result {
		order = append(order, item.Script.ID)
	}
	if fmt.Sprint(order) != "[a b c]" {
		t.Errorf("同等级应保持 YAML 顺序: %v", order)
	}
}

func BenchmarkMatch500(b *testing.B) {
	scripts := make([]config.ScriptConfig, 0, 500)
	for i := 0; i < 500; i++ {
		scripts = append(scripts, script(
			fmt.Sprintf("id-%03d", i),
			fmt.Sprintf("alias-%03d", i),
			fmt.Sprintf("中文名称%03d", i),
			fmt.Sprintf("功能简述描述%03d", i),
			true,
		))
	}
	c, err := New(scripts)
	if err != nil {
		b.Fatal(err)
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = c.Match("alias")
	}
}
