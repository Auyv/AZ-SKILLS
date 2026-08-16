package favorite

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestToggleRoundTrip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "favorites.json")
	store, err := Load(path)
	if err != nil {
		t.Fatal(err)
	}
	entry := `C:\tools\entry.yaml`
	fav, err := store.Toggle(entry, "s1")
	if err != nil {
		t.Fatalf("Toggle: %v", err)
	}
	if !fav {
		t.Error("首次收藏应为 true")
	}
	if !store.IsFavorite(entry, "s1") {
		t.Error("IsFavorite 应为 true")
	}
	fav, err = store.Toggle(entry, "s1")
	if err != nil {
		t.Fatalf("Toggle: %v", err)
	}
	if fav {
		t.Error("再次收藏应为 false（已取消）")
	}
	if store.IsFavorite(entry, "s1") {
		t.Error("IsFavorite 应为 false")
	}
}

func TestPersistenceAcrossInstances(t *testing.T) {
	path := filepath.Join(t.TempDir(), "favorites.json")
	store, err := Load(path)
	if err != nil {
		t.Fatal(err)
	}
	entry := `C:\tools\entry.yaml`
	if _, err := store.Toggle(entry, "s1"); err != nil {
		t.Fatal(err)
	}
	if _, err := store.Toggle(entry, "s2"); err != nil {
		t.Fatal(err)
	}
	second, err := Load(path)
	if err != nil {
		t.Fatalf("第二个实例加载: %v", err)
	}
	if !second.IsFavorite(entry, "s1") || !second.IsFavorite(entry, "s2") {
		t.Error("第二个实例应看到一致收藏")
	}
	if ids := second.IDs(entry); len(ids) != 2 || ids[0] != "s1" || ids[1] != "s2" {
		t.Errorf("收藏顺序错误: %v", ids)
	}
}

func TestLoadMissingFile(t *testing.T) {
	store, err := Load(filepath.Join(t.TempDir(), "nope.json"))
	if err != nil {
		t.Fatalf("缺失文件应降级为空收藏: %v", err)
	}
	if store.IsFavorite("e", "s") {
		t.Error("空收藏不应命中")
	}
}

func TestLoadCorruptedBacksUp(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "favorites.json")
	if err := os.WriteFile(path, []byte("{invalid json"), 0o600); err != nil {
		t.Fatal(err)
	}
	store, err := Load(path)
	if err == nil {
		t.Fatal("损坏文件应返回错误")
	}
	if store.IsFavorite("e", "s") {
		t.Error("损坏后应降级为空收藏")
	}
	matches, _ := filepath.Glob(filepath.Join(dir, "favorites.json.bad-*"))
	if len(matches) != 1 {
		t.Errorf("应备份损坏文件: %v", matches)
	}
}

func TestLoadUnsupportedVersionBacksUp(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "favorites.json")
	data, _ := json.Marshal(FavoritesFile{Version: 999, Favorites: map[string][]string{"e": {"s"}}})
	if err := os.WriteFile(path, data, 0o600); err != nil {
		t.Fatal(err)
	}
	store, err := Load(path)
	if err == nil {
		t.Fatal("version 非 1 应返回错误")
	}
	if store.IsFavorite("e", "s") {
		t.Error("version 非 1 应降级为空收藏")
	}
}

func TestEntryIsolation(t *testing.T) {
	store, _ := Load(filepath.Join(t.TempDir(), "f.json"))
	store.Toggle(`C:\a\entry.yaml`, "s1")
	store.Toggle(`C:\b\entry.yaml`, "s2")
	if store.IsFavorite(`C:\b\entry.yaml`, "s1") {
		t.Error("入口间不应互相污染")
	}
	if ids := store.IDs(`C:\a\entry.yaml`); len(ids) != 1 || ids[0] != "s1" {
		t.Errorf("a 入口收藏错误: %v", ids)
	}
}

func TestWriteFailureKeepsMemory(t *testing.T) {
	dir := t.TempDir()
	// 用目录路径充当文件，使写入失败
	blocked := filepath.Join(dir, "favorites.json")
	if err := os.MkdirAll(blocked, 0o755); err != nil {
		t.Fatal(err)
	}
	store := &FavoritesStore{path: blocked, list: make(map[string][]string), set: make(map[string]map[string]bool)}
	fav, err := store.Toggle("e", "s")
	if err == nil {
		t.Fatal("写入失败应返回错误")
	}
	if !fav || !store.IsFavorite("e", "s") {
		t.Error("写盘失败后内存态应保留收藏")
	}
}

func TestRecentTracker(t *testing.T) {
	r := NewRecentTracker(2)
	if got := r.List(); len(got) != 0 {
		t.Errorf("初始列表应为空: %v", got)
	}
	r.Record("a")
	r.Record("b")
	if got := r.List(); strings.Join(got, ",") != "b,a" {
		t.Errorf("顺序错误: %v", got)
	}
	r.Record("a") // 重复上移
	if got := r.List(); strings.Join(got, ",") != "a,b" {
		t.Errorf("重复上移错误: %v", got)
	}
	r.Record("c") // 超出上限淘汰最旧
	if got := r.List(); strings.Join(got, ",") != "c,a" {
		t.Errorf("容量淘汰错误: %v", got)
	}
}

func TestRecentTrackerZeroLimit(t *testing.T) {
	r := NewRecentTracker(0)
	r.Record("a")
	if got := r.List(); strings.Join(got, ",") != "a" {
		t.Errorf("limit 0 应退化为 30 且保留记录: %v", got)
	}
}
