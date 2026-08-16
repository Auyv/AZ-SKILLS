package terminal

import (
	"errors"
	"path/filepath"
	"testing"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/favorite"
	"azkits/internal/task"
)

type fakeStarter struct {
	started []string
	states  map[string]task.State
}

func (f *fakeStarter) Start(id string) error {
	f.started = append(f.started, id)
	return nil
}
func (f *fakeStarter) State(id string) task.State {
	if f.states == nil {
		return task.Ready
	}
	return f.states[id]
}

func testCatalog(scripts []config.ScriptConfig) *catalog.Catalog {
	c, err := catalog.New(scripts)
	if err != nil {
		panic(err)
	}
	return c
}

func mkScripts() []config.ScriptConfig {
	return []config.ScriptConfig{
		{ID: "a", Alias: "alpha", Name: "阿尔法", Description: "第一个", Enabled: true},
		{ID: "b", Alias: "beta", Name: "贝塔", Description: "第二个", Enabled: true},
		{ID: "c", Alias: "gamma", Name: "伽马", Description: "第三个", Enabled: true},
	}
}

func newTestCore(t *testing.T) (*Core, *fakeStarter, *favorite.FavoritesStore, *favorite.RecentTracker) {
	t.Helper()
	starter := &fakeStarter{}
	favs, err := favorite.Load(filepath.Join(t.TempDir(), "favorites.json"))
	if err != nil {
		t.Fatal(err)
	}
	recent := favorite.NewRecentTracker(30)
	core := NewCore(testCatalog(mkScripts()), starter, favs, recent, `C:\tools\entry.yaml`)
	return core, starter, favs, recent
}

func TestSetInputVisibilityAndMatch(t *testing.T) {
	core, _, _, _ := newTestCore(t)
	core.SetInput("/")
	if !core.Visible() {
		t.Error("输入 / 应显示候选")
	}
	if got := len(core.Candidates()); got != 3 {
		t.Errorf("候选数 = %d", got)
	}
	core.SetInput("/alph")
	if len(core.Candidates()) != 1 || core.Candidates()[0].Script.Alias != "alpha" {
		t.Errorf("过滤错误: %+v", core.Candidates())
	}
	core.SetInput("hello")
	if core.Visible() {
		t.Error("不以 / 开头不应显示候选")
	}
	core.SetInput("")
	if core.Visible() {
		t.Error("空输入不应显示候选")
	}
}

func TestMoveBoundaries(t *testing.T) {
	core, _, _, _ := newTestCore(t)
	core.SetInput("/")
	core.Move(-1)
	if core.Selected() != 0 {
		t.Errorf("上移越界: %d", core.Selected())
	}
	core.Move(100)
	if core.Selected() != 2 {
		t.Errorf("下移越界: %d", core.Selected())
	}
}

func TestCompleteDoesNotExecute(t *testing.T) {
	core, starter, _, _ := newTestCore(t)
	core.SetInput("/a")
	value, ok := core.Complete()
	if !ok || value != "/alpha" {
		t.Errorf("补全结果 = %q, %v", value, ok)
	}
	if len(starter.started) != 0 {
		t.Error("Tab 补全不应执行")
	}
	if !core.Visible() {
		t.Error("补全后候选仍应可见")
	}
}

func TestExecuteRunsAndRecordsRecent(t *testing.T) {
	core, starter, _, recent := newTestCore(t)
	core.SetInput("/beta")
	core.Move(0)
	if err := core.Execute(); err != nil {
		t.Fatal(err)
	}
	if len(starter.started) != 1 || starter.started[0] != "b" {
		t.Errorf("未启动目标: %v", starter.started)
	}
	if got := recent.List(); len(got) != 1 || got[0] != "b" {
		t.Errorf("最近使用未记录: %v", got)
	}
	if core.Visible() {
		t.Error("执行成功后候选应关闭")
	}
}

func TestExecuteNoMatch(t *testing.T) {
	core, _, _, _ := newTestCore(t)
	core.SetInput("/none")
	if err := core.Execute(); err == nil {
		t.Error("无匹配时 Execute 应失败")
	}
}

func TestEscape(t *testing.T) {
	core, _, _, _ := newTestCore(t)
	core.SetInput("/")
	core.Escape()
	if core.Visible() {
		t.Error("Esc 后候选应关闭")
	}
}

func TestCommitTextIME(t *testing.T) {
	core, starter, _, _ := newTestCore(t)
	// 同一批次内 IME 上屏 + Enter：只提交文本，不执行
	core.SetInput("/")
	core.CommitText("设", true)
	if len(starter.started) != 0 {
		t.Error("IME 上屏批次不应执行")
	}
	if core.Input() != "/设" {
		t.Errorf("输入 = %q", core.Input())
	}
	// 上屏完成后独立 Enter 才执行
	core.SetInput("/alpha")
	if err := core.Execute(); err != nil {
		t.Fatal(err)
	}
	if len(starter.started) != 1 {
		t.Error("独立 Enter 应执行")
	}
}

// ---- 0014 排序与收藏 ----

func TestFavoriteSortingTop(t *testing.T) {
	core, _, favs, recent := newTestCore(t)
	recent.Record("c")  // 最近使用 c
	recent.Record("b")  // 最近使用 b
	favs.Toggle(`C:\tools\entry.yaml`, "b") // 收藏 b（且最近使用）
	favs.Toggle(`C:\tools\entry.yaml`, "a") // 收藏 a（非最近使用）
	core.SetInput("/")
	ids := []string{}
	for _, cand := range core.Candidates() {
		ids = append(ids, cand.Script.ID)
	}
	want := []string{"b", "a", "c"}
	if !equalStrings(ids, want) {
		t.Errorf("排序 = %v, want %v", ids, want)
	}
	if !core.Candidates()[0].IsFavorite || !core.Candidates()[0].IsRecent {
		t.Errorf("b 应同时带收藏与最近使用标识: %+v", core.Candidates()[0])
	}
	if !core.Candidates()[1].IsFavorite || core.Candidates()[1].IsRecent {
		t.Errorf("a 应仅带收藏标识: %+v", core.Candidates()[1])
	}
}

func TestFavoriteSortingNonEmptyQuery(t *testing.T) {
	core, _, favs, recent := newTestCore(t)
	recent.Record("b")
	favs.Toggle(`C:\tools\entry.yaml`, "a")
	core.SetInput("/a")
	ids := []string{}
	for _, cand := range core.Candidates() {
		ids = append(ids, cand.Script.ID)
	}
	// 非空查询时收藏 a 应置顶
	if len(ids) == 0 || ids[0] != "a" {
		t.Errorf("非空查询置顶失败: %v", ids)
	}
}

func TestFavoriteDedup(t *testing.T) {
	core, _, favs, recent := newTestCore(t)
	favs.Toggle(`C:\tools\entry.yaml`, "a")
	recent.Record("a")
	core.SetInput("/")
	count := 0
	for _, cand := range core.Candidates() {
		if cand.Script.ID == "a" {
			count++
		}
	}
	if count != 1 {
		t.Errorf("收藏且最近使用的脚本应去重只出现一次，出现 %d 次", count)
	}
}

func TestToggleFavoriteCommand(t *testing.T) {
	core, _, favs, _ := newTestCore(t)
	msg, err := core.ToggleFavorite("/fav alpha")
	if err != nil {
		t.Fatalf("ToggleFavorite: %v", err)
	}
	if !favs.IsFavorite(`C:\tools\entry.yaml`, "a") {
		t.Error("收藏未生效")
	}
	if msg == "" {
		t.Error("应返回反馈消息")
	}
	msg, err = core.ToggleFavorite("/fav alpha")
	if err != nil {
		t.Fatal(err)
	}
	if favs.IsFavorite(`C:\tools\entry.yaml`, "a") {
		t.Error("二次切换应取消收藏")
	}
	if msg == "" {
		t.Error("取消收藏应返回反馈")
	}
}

func TestToggleFavoriteInvalid(t *testing.T) {
	core, _, favs, _ := newTestCore(t)
	if _, err := core.ToggleFavorite("/fav"); err == nil {
		t.Error("空 alias 应报错")
	}
	if _, err := core.ToggleFavorite("/fav nope"); err == nil {
		t.Error("未知 alias 应报错")
	}
	if favs.IsFavorite(`C:\tools\entry.yaml`, "a") {
		t.Error("无效命令不应改变收藏")
	}
}

func TestIsFavoriteCommand(t *testing.T) {
	cases := map[string]bool{
		"/fav":        true,
		"/fav alpha":  true,
		"/fav alpha ": true,
		"/fa":         false,
		"/favx":       false,
		"/alpha":      false,
		"/favfoo":     false,
	}
	for input, want := range cases {
		if got := IsFavoriteCommand(input); got != want {
			t.Errorf("IsFavoriteCommand(%q) = %v, want %v", input, got, want)
		}
	}
}

func TestStartFailureDoesNotRecordRecent(t *testing.T) {
	starter := &failStarter{}
	favs, _ := favorite.Load(filepath.Join(t.TempDir(), "f.json"))
	recent := favorite.NewRecentTracker(30)
	core := NewCore(testCatalog(mkScripts()), starter, favs, recent, `C:\tools\entry.yaml`)
	core.SetInput("/alpha")
	if err := core.Execute(); err == nil {
		t.Fatal("启动失败应返回错误")
	}
	if got := recent.List(); len(got) != 0 {
		t.Errorf("启动失败不应记录最近使用: %v", got)
	}
}

type failStarter struct{}

func (f *failStarter) Start(string) error { return errors.New("启动失败") }
func (f *failStarter) State(string) task.State {
	return task.Ready
}

func equalStrings(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
