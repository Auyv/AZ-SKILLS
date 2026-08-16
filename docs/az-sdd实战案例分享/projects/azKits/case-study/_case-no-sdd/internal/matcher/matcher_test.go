package matcher

import (
	"fmt"
	"testing"
	"time"
)

func mkScript(id, alias, name string, index int) Script {
	return Script{ID: id, Alias: alias, Name: name, Description: "desc", Status: StatusOK, Index: index}
}

func mkScripts(n int) []Script {
	out := make([]Script, 0, n)
	for i := 0; i < n; i++ {
		out = append(out, mkScript(fmt.Sprintf("id%03d", i), fmt.Sprintf("alias%03d", i), fmt.Sprintf("名称%03d", i), i))
	}
	return out
}

func noFav(string) bool { return false }

func TestExactPrefixContainsOrder(t *testing.T) {
	scripts := []Script{
		mkScript("1", "build", "构建", 0),
		mkScript("2", "rebuild", "重建", 1),
		mkScript("3", "xbuildx", "其他", 2),
		mkScript("4", "deploy", "部署", 3),
	}
	cs := Filter(scripts, "build", noFav, NewRecent())
	var order []string
	for _, c := range cs {
		order = append(order, c.Script.Alias)
	}
	want := []string{"build", "rebuild", "xbuildx"}
	if len(order) != len(want) {
		t.Fatalf("order = %v, want %v", order, want)
	}
	for i := range want {
		if order[i] != want[i] {
			t.Fatalf("order = %v, want %v", order, want)
		}
	}
}

func TestFavoritesFirstAndDedupe(t *testing.T) {
	scripts := []Script{
		mkScript("fav", "fav", "收藏脚本", 0),
		mkScript("recent", "recent", "最近脚本", 1),
		mkScript("both", "both", "收藏且最近", 2),
		mkScript("plain", "plain", "普通脚本", 3),
	}
	isFav := func(id string) bool { return id == "fav" || id == "both" }
	r := NewRecent()
	// Marking order: plain first, then both — both should still appear in the
	// favorites section only.
	r.Mark("recent", time.Unix(100, 0))
	r.Mark("plain", time.Unix(90, 0))
	r.Mark("both", time.Unix(99, 0))

	cs := Filter(scripts, "", isFav, r)

	var order []string
	for _, c := range cs {
		order = append(order, c.Script.ID)
	}
	// favorites (both, fav) first, then recent non-fav (recent), then others.
	want := []string{"both", "fav", "recent", "plain"}
	if fmt.Sprint(order) != fmt.Sprint(want) {
		t.Fatalf("order = %v, want %v", order, want)
	}
	if cs[0].Favorite != true || cs[0].Recent != true {
		t.Fatalf("first candidate should be favorite+recent, got %+v", cs[0])
	}
	if !cs[1].Favorite {
		t.Fatal("second candidate should be favorite")
	}
	if !cs[2].Recent {
		t.Fatal("third candidate should be recent")
	}
}

func TestFavoritesSortedByRecency(t *testing.T) {
	scripts := []Script{
		mkScript("f1", "f1", "收藏1", 0),
		mkScript("f2", "f2", "收藏2", 1),
		mkScript("f3", "f3", "收藏3", 2),
	}
	isFav := func(string) bool { return true }
	r := NewRecent()
	r.Mark("f3", time.Unix(300, 0))
	r.Mark("f1", time.Unix(100, 0))

	cs := Filter(scripts, "", isFav, r)
	want := []string{"f3", "f1", "f2"}
	var order []string
	for _, c := range cs {
		order = append(order, c.Script.ID)
	}
	if fmt.Sprint(order) != fmt.Sprint(want) {
		t.Fatalf("favorites order = %v, want %v", order, want)
	}
}

func TestQueryFiltersByAliasAndName(t *testing.T) {
	scripts := []Script{
		mkScript("1", "abc", "甲名", 0),
		mkScript("2", "xyz", "含中文", 1),
		mkScript("3", "other", "完全无关", 2),
	}
	cs := Filter(scripts, "中文", noFav, NewRecent())
	if len(cs) != 1 || cs[0].Script.ID != "2" {
		t.Fatalf("中文 should match name 含中文 only, got %d candidates", len(cs))
	}
}

func TestNoMatchReturnsEmpty(t *testing.T) {
	scripts := []Script{mkScript("1", "abc", "甲", 0)}
	cs := Filter(scripts, "zzz", noFav, NewRecent())
	if len(cs) != 0 {
		t.Fatalf("want no candidates, got %d", len(cs))
	}
}

func TestRecentSingleEntryPerScript(t *testing.T) {
	r := NewRecent()
	r.Mark("a", time.Unix(1, 0))
	r.Mark("a", time.Unix(2, 0))
	r.Mark("b", time.Unix(3, 0))
	if r.Len() != 2 {
		t.Fatalf("want 2 distinct entries, got %d", r.Len())
	}
	at, ok := r.At("a")
	if !ok || at.Unix() != 2 {
		t.Fatalf("want latest time for a, got %v", at)
	}
}

func TestPerformance500Scripts(t *testing.T) {
	scripts := mkScripts(500)
	isFav := func(id string) bool { return id == "id250" || id == "id499" }
	r := NewRecent()
	r.Mark("id100", time.Now())

	start := time.Now()
	for i := 0; i < 20; i++ {
		_ = Filter(scripts, "alias", isFav, r)
	}
	elapsed := time.Since(start)
	perQuery := elapsed / 20
	if perQuery > 50*time.Millisecond {
		t.Fatalf("candidate update took %v, exceeds 50ms requirement", perQuery)
	}
}

func BenchmarkFilter500(b *testing.B) {
	scripts := mkScripts(500)
	isFav := func(id string) bool { return id == "id250" }
	r := NewRecent()
	r.Mark("id100", time.Now())
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = Filter(scripts, "alias", isFav, r)
	}
}
