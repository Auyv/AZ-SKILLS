package store

import (
	"os"
	"path/filepath"
	"testing"
)

func TestSetAndToggle(t *testing.T) {
	s := NewEmptyStore()
	if s.IsFavorite("a") {
		t.Fatal("a should not be favorite initially")
	}
	if err := s.SetFavorite("a", true); err != nil {
		t.Fatal(err)
	}
	if !s.IsFavorite("a") {
		t.Fatal("a should be favorite")
	}
	on, err := s.ToggleFavorite("a")
	if err != nil || on {
		t.Fatalf("toggle off: on=%v err=%v", on, err)
	}
	if s.IsFavorite("a") {
		t.Fatal("a should not be favorite after toggle off")
	}
}

func TestPersistenceAcrossOpen(t *testing.T) {
	dir := t.TempDir()
	s1, err := Open(dir)
	if err != nil {
		t.Fatal(err)
	}
	if err := s1.SetFavorite("alpha", true); err != nil {
		t.Fatal(err)
	}
	if err := s1.SetFavorite("beta", true); err != nil {
		t.Fatal(err)
	}

	s2, err := Open(dir)
	if err != nil {
		t.Fatal(err)
	}
	if !s2.IsFavorite("alpha") || !s2.IsFavorite("beta") {
		t.Fatalf("favorites not persisted: %v", s2.FavoriteIDs())
	}
	if s2.Path() != filepath.Join(dir, "favorites.json") {
		t.Fatalf("path = %q", s2.Path())
	}
}

func TestCorruptFileDegradesToEmpty(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, fileName), []byte("{ not json"), 0o644); err != nil {
		t.Fatal(err)
	}
	s, err := Open(dir)
	if err != nil {
		t.Fatalf("corrupt file should degrade, got %v", err)
	}
	if len(s.FavoriteIDs()) != 0 {
		t.Fatalf("want empty store, got %v", s.FavoriteIDs())
	}
	// saving repairs the file
	if err := s.SetFavorite("x", true); err != nil {
		t.Fatal(err)
	}
	s2, _ := Open(dir)
	if !s2.IsFavorite("x") {
		t.Fatal("repair failed")
	}
}

func TestWrongVersionFileIgnored(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, fileName), []byte(`{"version":99,"ids":["a"]}`), 0o644); err != nil {
		t.Fatal(err)
	}
	s, err := Open(dir)
	if err != nil {
		t.Fatal(err)
	}
	if s.IsFavorite("a") {
		t.Fatal("wrong version file must be ignored")
	}
}

func TestMissingFileIsEmptyStore(t *testing.T) {
	s, err := Open(filepath.Join(t.TempDir(), "sub"))
	if err != nil {
		t.Fatal(err)
	}
	if len(s.FavoriteIDs()) != 0 {
		t.Fatal("want empty")
	}
}

func TestSetFavoriteIdempotent(t *testing.T) {
	s := NewEmptyStore()
	if err := s.SetFavorite("a", true); err != nil {
		t.Fatal(err)
	}
	if err := s.SetFavorite("a", true); err != nil {
		t.Fatal(err)
	}
	if err := s.SetFavorite("a", false); err != nil {
		t.Fatal(err)
	}
	if err := s.SetFavorite("a", false); err != nil {
		t.Fatal(err)
	}
	if len(s.FavoriteIDs()) != 0 {
		t.Fatal("expected empty after toggles")
	}
}
