package matcher

import (
	"sort"
	"strings"
	"time"
)

// Status describes the runnability of a script in a candidate list.
type Status int

const (
	// StatusOK means the script is enabled and its file exists.
	StatusOK Status = iota
	// StatusFileMissing means the script is enabled but its file is gone.
	StatusFileMissing
	// StatusDisabled means the script was disabled in the YAML entry.
	StatusDisabled
)

// Script is the matching-relevant projection of an entry script.
type Script struct {
	ID          string
	Alias       string
	Name        string
	Description string
	Status      Status
	Index       int
}

// Candidate is a matching script plus its ordering metadata.
type Candidate struct {
	Script   Script
	Score    int
	Favorite bool
	Recent   bool
	RecentAt time.Time
}

// Recent tracks recent successful launches within a single TUI instance.
type Recent struct {
	byID map[string]time.Time
}

// NewRecent returns an empty recent-usage tracker.
func NewRecent() *Recent {
	return &Recent{byID: map[string]time.Time{}}
}

// Mark records a successful launch at time t.
func (r *Recent) Mark(id string, t time.Time) {
	if r == nil {
		return
	}
	r.byID[id] = t
}

// At returns the last successful launch time for id.
func (r *Recent) At(id string) (time.Time, bool) {
	if r == nil {
		return time.Time{}, false
	}
	t, ok := r.byID[id]
	return t, ok
}

// Has reports whether id was launched successfully in this instance.
func (r *Recent) Has(id string) bool {
	if r == nil {
		return false
	}
	_, ok := r.byID[id]
	return ok
}

// Len returns the number of distinct recently used scripts.
func (r *Recent) Len() int {
	if r == nil {
		return 0
	}
	return len(r.byID)
}

// Match scores. Lower is better. Aliases are preferred over names.
const (
	ScoreNone          = -1
	ScoreExactAlias    = 0
	ScoreExactName     = 1
	ScorePrefixAlias   = 2
	ScorePrefixName    = 3
	ScoreContainsAlias = 4
	ScoreContainsName  = 5
	ScoreAny           = 6
)

// scoreMatch computes the match score of one script against the query.
// An empty query matches everything with ScoreAny.
func scoreMatch(s Script, q string) int {
	if q == "" {
		return ScoreAny
	}
	if eqFold(s.Alias, q) {
		return ScoreExactAlias
	}
	if eqFold(s.Name, q) {
		return ScoreExactName
	}
	if strings.HasPrefix(lower(s.Alias), q) {
		return ScorePrefixAlias
	}
	if strings.HasPrefix(lower(s.Name), q) {
		return ScorePrefixName
	}
	if strings.Contains(lower(s.Alias), q) {
		return ScoreContainsAlias
	}
	if strings.Contains(lower(s.Name), q) {
		return ScoreContainsName
	}
	return ScoreNone
}

// Filter orders matching scripts for the candidate list:
//
//  1. Favorites (deduplicated; a favorite that was also recently used appears
//     only here, ordered by last use time, otherwise by match quality).
//  2. Recently used scripts that are not favorites, ordered by last use time.
//  3. Remaining matches ordered by match quality (exact alias, exact name,
//     prefix, contains) then declaration order.
func Filter(scripts []Script, query string, isFav func(id string) bool, recent *Recent) []Candidate {
	q := strings.ToLower(strings.TrimSpace(query))

	var favs, recents, others []Candidate
	for _, s := range scripts {
		sc := scoreMatch(s, q)
		if sc == ScoreNone {
			continue
		}
		c := Candidate{Script: s, Score: sc}
		fav := isFav != nil && isFav(s.ID)
		at, rec := recent.At(s.ID)
		c.Favorite = fav
		c.Recent = rec
		c.RecentAt = at
		switch {
		case fav:
			favs = append(favs, c)
		case rec:
			recents = append(recents, c)
		default:
			others = append(others, c)
		}
	}

	sort.SliceStable(favs, func(a, b int) bool {
		fa, fb := favs[a], favs[b]
		if fa.Recent != fb.Recent {
			return fa.Recent
		}
		if fa.Recent && fb.Recent && !fa.RecentAt.Equal(fb.RecentAt) {
			return fa.RecentAt.After(fb.RecentAt)
		}
		return better(fa, fb)
	})
	sort.SliceStable(recents, func(a, b int) bool {
		if !recents[a].RecentAt.Equal(recents[b].RecentAt) {
			return recents[a].RecentAt.After(recents[b].RecentAt)
		}
		return better(recents[a], recents[b])
	})
	sort.SliceStable(others, func(a, b int) bool { return better(others[a], others[b]) })

	out := make([]Candidate, 0, len(favs)+len(recents)+len(others))
	out = append(out, favs...)
	out = append(out, recents...)
	out = append(out, others...)
	return out
}

// better compares two candidates by match score then declaration order.
func better(a, b Candidate) bool {
	if a.Score != b.Score {
		return a.Score < b.Score
	}
	return a.Script.Index < b.Script.Index
}

func lower(s string) string { return strings.ToLower(s) }

func eqFold(a, b string) bool { return strings.EqualFold(a, b) }
