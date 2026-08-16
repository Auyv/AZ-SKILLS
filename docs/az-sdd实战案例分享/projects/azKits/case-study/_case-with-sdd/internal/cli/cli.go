package cli

import (
	"bufio"
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"strings"

	"azkits/internal/catalog"
	"azkits/internal/config"
	"azkits/internal/favorite"
	"azkits/internal/runner"
	"azkits/internal/task"
	"azkits/internal/terminal"
)

const defaultRecentLimit = 30

func Run(args []string, stdin io.Reader, stdout, stderr io.Writer) int {
	flags := flag.NewFlagSet("azkits", flag.ContinueOnError)
	flags.SetOutput(stderr)
	configPath := flags.String("config", "", "YAML 入口文件")
	favPath := flags.String("favorites", "", "收藏文件路径（默认 %LOCALAPPDATA%\\azKits\\favorites.json）")
	flags.Usage = func() {
		fmt.Fprintln(stderr, "用法: azkits --config <entry.yaml> [list|run <alias>|check]")
	}
	if err := flags.Parse(args); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return 0
		}
		return 2
	}
	remaining := flags.Args()
	command := "tui"
	if len(remaining) > 0 {
		command = remaining[0]
	}
	if *configPath == "" {
		if command != "tui" {
			fmt.Fprintln(stderr, "ERROR: list、run、check 必须指定 --config")
			return 2
		}
		fmt.Fprint(stdout, "请输入 YAML 入口路径（留空取消）: ")
		line, _ := bufio.NewReader(stdin).ReadString('\n')
		*configPath = strings.TrimSpace(line)
		if *configPath == "" {
			return 5
		}
	}
	cfg, err := config.Load(*configPath)
	if err != nil {
		fmt.Fprintf(stderr, "ERROR: %v\n", err)
		return 2
	}
	c, err := catalog.New(cfg.Scripts)
	if err != nil {
		fmt.Fprintf(stderr, "ERROR: %v\n", err)
		return 2
	}

	favs := loadFavorites(*favPath, stderr)

	switch command {
	case "check":
		if len(remaining) != 1 {
			return usageError(stderr, "check 不接受额外参数")
		}
		fmt.Fprintf(stdout, "配置有效: %s（%d 个启用脚本）\n", cfg.Toolbox.Name, len(c.Enabled()))
		return 0
	case "list":
		if len(remaining) != 1 {
			return usageError(stderr, "list 不接受额外参数")
		}
		fmt.Fprintf(stdout, "%s - %s\n入口: %s\n", cfg.Toolbox.Name, cfg.Toolbox.Description, cfg.Source)
		if len(c.Enabled()) == 0 {
			fmt.Fprintln(stdout, "当前没有可用脚本")
		}
		for _, script := range c.Enabled() {
			marker := "  "
			if favs != nil && favs.IsFavorite(cfg.Source, script.ID) {
				marker = "★ "
			}
			fmt.Fprintf(stdout, "%s/%s\t%s\t%s\t就绪\n", marker, script.Alias, script.Name, script.Description)
		}
		return 0
	case "run":
		if len(remaining) != 2 {
			return usageError(stderr, "run 需要唯一的 alias")
		}
		script, ok := c.FindAlias(remaining[1])
		if !ok {
			fmt.Fprintf(stderr, "ERROR: 未找到当前入口中的启用脚本: %s\n", remaining[1])
			return 3
		}
		r := runner.New()
		var result runner.Result
		var runErr error
		if script.Interactive {
			result, runErr = r.RunInteractive(context.Background(), script, stdin, stdout, stderr)
		} else {
			result, runErr = r.Run(context.Background(), script, func(event runner.Event) {
				if event.Stream == runner.Stderr {
					_, _ = stderr.Write(event.Data)
				} else {
					_, _ = stdout.Write(event.Data)
				}
			})
		}
		if runErr != nil {
			fmt.Fprintf(stderr, "ERROR: %v\n", runErr)
			return 4
		}
		if result.ExitCode == 0 {
			return 0
		}
		if result.ExitCode == 2 || result.ExitCode == 3 || result.ExitCode == 4 || result.ExitCode == 5 {
			fmt.Fprintf(stderr, "脚本原始退出码: %d\n", result.ExitCode)
			return 4
		}
		return result.ExitCode
	case "tui":
		if len(remaining) != 0 {
			return usageError(stderr, "未知命令: "+command)
		}
		service := task.New(runner.New(), cfg.Scripts)
		recent := favorite.NewRecentTracker(defaultRecentLimit)
		if err := terminal.Run(cfg, c, service, favs, recent); err != nil {
			fmt.Fprintf(stderr, "ERROR: TUI 运行失败: %v\n", err)
			return 4
		}
		return 0
	default:
		return usageError(stderr, "未知命令: "+command)
	}
}

// loadFavorites 加载收藏；失败时返回 nil 并提示一次，不阻断命令执行。
func loadFavorites(path string, stderr io.Writer) *favorite.FavoritesStore {
	if path == "" {
		var err error
		path, err = favorite.DefaultPath()
		if err != nil {
			fmt.Fprintf(stderr, "提示: 无法确定收藏文件位置: %v\n", err)
			return nil
		}
	}
	store, err := favorite.Load(path)
	if err != nil {
		fmt.Fprintf(stderr, "提示: %v（已按空收藏继续）\n", err)
	}
	return store
}

func usageError(stderr io.Writer, message string) int {
	fmt.Fprintf(stderr, "ERROR: %s\n", message)
	return 2
}
