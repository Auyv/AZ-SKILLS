package store

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"sync"
)

const (
	fileName    = "favorites.json"
	fileVersion = 1
)

// Store persists the set of favorite script ids across terminal instances.
type Store struct {
	mu   sync.Mutex
	path string
	ids  map[string]struct{}
}

// Open loads the favorites store. The file is created lazily on the first
// save. Missing or corrupt files degrade to an empty store instead of failing.
func Open(dir string) (*Store, error) {
	if dir == "" {
		cfg, err := os.UserConfigDir()
		if err != nil {
			return nil, fmt.Errorf("无法定位用户配置目录: %w", err)
		}
		dir = filepath.Join(cfg, "azkits")
	}
	s := &Store{path: filepath.Join(dir, fileName), ids: map[string]struct{}{}}
	if err := s.load(); err != nil {
		return nil, err
	}
	return s, nil
}

// NewEmptyStore returns an in-memory only store that never touches disk. It
// is used as a graceful degradation when the on-disk store cannot be opened.
func NewEmptyStore() *Store {
	return &Store{ids: map[string]struct{}{}}
}

func (s *Store) load() error {
	data, err := os.ReadFile(s.path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return fmt.Errorf("读取收藏文件失败: %w", err)
	}
	var doc struct {
		Version int      `json:"version"`
		IDs     []string `json:"ids"`
	}
	if err := json.Unmarshal(data, &doc); err != nil {
		// Corrupt file: treat as empty and let the next save repair it.
		return nil
	}
	if doc.Version != fileVersion {
		return nil
	}
	s.ids = make(map[string]struct{}, len(doc.IDs))
	for _, id := range doc.IDs {
		if id != "" {
			s.ids[id] = struct{}{}
		}
	}
	return nil
}

// Path returns the store file path.
func (s *Store) Path() string { return s.path }

// IsFavorite reports whether id is a favorite.
func (s *Store) IsFavorite(id string) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	_, ok := s.ids[id]
	return ok
}

// FavoriteIDs returns the stored favorite ids sorted for stable output.
func (s *Store) FavoriteIDs() []string {
	s.mu.Lock()
	defer s.mu.Unlock()
	out := make([]string, 0, len(s.ids))
	for id := range s.ids {
		out = append(out, id)
	}
	sort.Strings(out)
	return out
}

// SetFavorite marks id as favorite (on=true) or removes it (on=false) and
// persists the change atomically.
func (s *Store) SetFavorite(id string, on bool) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if on {
		s.ids[id] = struct{}{}
	} else {
		delete(s.ids, id)
	}
	return s.save()
}

// ToggleFavorite flips the favorite state of id and persists it. It returns
// the new state.
func (s *Store) ToggleFavorite(id string) (bool, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	_, exists := s.ids[id]
	if exists {
		delete(s.ids, id)
	} else {
		s.ids[id] = struct{}{}
	}
	if err := s.save(); err != nil {
		return exists, err
	}
	return !exists, nil
}

func (s *Store) save() error {
	if s.path == "" {
		return nil
	}
	doc := struct {
		Version int      `json:"version"`
		IDs     []string `json:"ids"`
	}{
		Version: fileVersion,
		IDs:     make([]string, 0, len(s.ids)),
	}
	for id := range s.ids {
		doc.IDs = append(doc.IDs, id)
	}
	sort.Strings(doc.IDs)

	data, err := json.MarshalIndent(doc, "", "  ")
	if err != nil {
		return fmt.Errorf("序列化收藏失败: %w", err)
	}
	data = append(data, '\n')

	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return fmt.Errorf("创建收藏目录失败: %w", err)
	}
	tmp, err := os.CreateTemp(filepath.Dir(s.path), ".favorites-*.tmp")
	if err != nil {
		return fmt.Errorf("创建临时收藏文件失败: %w", err)
	}
	tmpName := tmp.Name()
	defer os.Remove(tmpName)

	if _, err := tmp.Write(data); err != nil {
		tmp.Close()
		return fmt.Errorf("写入收藏文件失败: %w", err)
	}
	if err := tmp.Sync(); err != nil {
		tmp.Close()
		return fmt.Errorf("同步收藏文件失败: %w", err)
	}
	if err := tmp.Close(); err != nil {
		return fmt.Errorf("关闭收藏文件失败: %w", err)
	}
	if err := os.Rename(tmpName, s.path); err != nil {
		return fmt.Errorf("替换收藏文件失败: %w", err)
	}
	return nil
}
