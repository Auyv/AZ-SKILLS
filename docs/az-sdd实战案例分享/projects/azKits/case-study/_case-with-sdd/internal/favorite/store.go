package favorite

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"time"
)

const CurrentVersion = 1

type FavoritesFile struct {
	Version   int                 `json:"version"`
	Favorites map[string][]string `json:"favorites"`
}

// FavoritesStore 持久化收藏集合。键为入口 YAML 绝对路径，值为脚本 id 列表。
type FavoritesStore struct {
	mu   sync.Mutex
	path string
	list map[string][]string
	set  map[string]map[string]bool
}

func DefaultPath() (string, error) {
	base := os.Getenv("LOCALAPPDATA")
	if base == "" {
		var err error
		base, err = os.UserConfigDir()
		if err != nil {
			return "", fmt.Errorf("获取用户配置目录: %w", err)
		}
	}
	return filepath.Join(base, "azKits", "favorites.json"), nil
}

// Load 加载收藏文件。文件不存在、损坏或 version 非 1 时自动降级为空收藏，
// 损坏文件被备份为 favorites.json.bad-{时间戳}。写入失败不影响内存态。
func Load(path string) (*FavoritesStore, error) {
	store := &FavoritesStore{path: path, list: make(map[string][]string), set: make(map[string]map[string]bool)}
	if path == "" {
		return store, nil
	}
	data, err := os.ReadFile(path)
	if errors.Is(err, os.ErrNotExist) {
		return store, nil
	}
	if err != nil {
		backupCorrupted(path, err)
		return store, fmt.Errorf("读取收藏文件 %s: %w", path, err)
	}
	var file FavoritesFile
	if err := json.Unmarshal(data, &file); err != nil {
		backupCorrupted(path, err)
		return store, fmt.Errorf("解析收藏文件 %s: %w", path, err)
	}
	if file.Version != CurrentVersion {
		backupCorrupted(path, fmt.Errorf("version=%d", file.Version))
		return store, fmt.Errorf("收藏文件版本不受支持: %d", file.Version)
	}
	for entry, ids := range file.Favorites {
		for _, id := range ids {
			store.add(entry, id)
		}
	}
	return store, nil
}

func backupCorrupted(path string, cause error) {
	backup := path + fmt.Sprintf(".bad-%d", time.Now().Unix())
	if err := os.Rename(path, backup); err == nil {
		return
	}
}

func (s *FavoritesStore) Path() string { return s.path }

func (s *FavoritesStore) add(entryPath, id string) {
	if s.set[entryPath] == nil {
		s.set[entryPath] = make(map[string]bool)
	}
	if !s.set[entryPath][id] {
		s.set[entryPath][id] = true
		s.list[entryPath] = append(s.list[entryPath], id)
	}
}

func (s *FavoritesStore) IsFavorite(entryPath, id string) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.set[entryPath] != nil && s.set[entryPath][id]
}

// Toggle 切换收藏状态并落盘。返回切换后的收藏状态；写盘失败时返回错误。
func (s *FavoritesStore) Toggle(entryPath, id string) (bool, error) {
	s.mu.Lock()
	if s.set[entryPath] == nil {
		s.set[entryPath] = make(map[string]bool)
	}
	favorite := !s.set[entryPath][id]
	if favorite {
		if !s.set[entryPath][id] {
			s.set[entryPath][id] = true
			s.list[entryPath] = append(s.list[entryPath], id)
		}
	} else {
		delete(s.set[entryPath], id)
		s.list[entryPath] = removeID(s.list[entryPath], id)
	}
	err := s.saveLocked()
	s.mu.Unlock()
	if err != nil {
		return favorite, err
	}
	return favorite, nil
}

// IDs 返回某个入口下按收藏顺序排列的脚本 id 列表。
func (s *FavoritesStore) IDs(entryPath string) []string {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]string(nil), s.list[entryPath]...)
}

// Save 显式保存（供退出或测试使用）。
func (s *FavoritesStore) Save() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.saveLocked()
}

func (s *FavoritesStore) saveLocked() error {
	file := FavoritesFile{Version: CurrentVersion, Favorites: make(map[string][]string)}
	for entry, ids := range s.list {
		if len(ids) > 0 {
			file.Favorites[entry] = append([]string(nil), ids...)
		}
	}
	data, err := json.MarshalIndent(file, "", "  ")
	if err != nil {
		return fmt.Errorf("编码收藏文件: %w", err)
	}
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return fmt.Errorf("创建收藏目录 %s: %w", filepath.Dir(s.path), err)
	}
	tmp := s.path + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return fmt.Errorf("写入收藏文件 %s: %w", tmp, err)
	}
	if err := os.Rename(tmp, s.path); err != nil {
		return fmt.Errorf("替换收藏文件 %s: %w", s.path, err)
	}
	return nil
}

func removeID(ids []string, target string) []string {
	result := make([]string, 0, len(ids))
	for _, id := range ids {
		if id != target {
			result = append(result, id)
		}
	}
	return result
}
