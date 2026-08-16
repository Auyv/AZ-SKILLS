package logexport

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"
	"unicode"
)

var ansiSequence = regexp.MustCompile(`\x1b\[[0-?]*[ -/]*[@-~]`)

type Writer struct {
	Dir string
	Now func() time.Time
}

func DefaultDir() (string, error) {
	base, err := os.UserCacheDir()
	if err != nil {
		return "", fmt.Errorf("获取用户本地目录: %w", err)
	}
	return filepath.Join(base, "azKits", "logs"), nil
}

func (w Writer) Write(run Run) (string, error) {
	dir := w.Dir
	if dir == "" {
		var err error
		dir, err = DefaultDir()
		if err != nil {
			return "", err
		}
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return "", fmt.Errorf("创建日志导出目录 %s: %w", dir, err)
	}
	now := time.Now()
	if w.Now != nil {
		now = w.Now()
	}
	base := fmt.Sprintf("%s_%s", now.Format("20060102_150405.000"), safeName(run.Alias))
	content := []byte(formatRun(run))
	for index := 0; index < 1000; index++ {
		name := base
		if index > 0 {
			name = fmt.Sprintf("%s_%03d", base, index)
		}
		path := filepath.Join(dir, name+".txt")
		file, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o600)
		if os.IsExist(err) {
			continue
		}
		if err != nil {
			return "", fmt.Errorf("创建日志文档 %s: %w", path, err)
		}
		if _, err = file.Write(content); err != nil {
			_ = file.Close()
			_ = os.Remove(path)
			return "", fmt.Errorf("写入日志文档 %s: %w", path, err)
		}
		if err = file.Close(); err != nil {
			_ = os.Remove(path)
			return "", fmt.Errorf("关闭日志文档 %s: %w", path, err)
		}
		absolute, err := filepath.Abs(path)
		if err != nil {
			return path, nil
		}
		return absolute, nil
	}
	return "", fmt.Errorf("无法生成唯一日志文件名: %s", base)
}

func formatRun(run Run) string {
	var body strings.Builder
	body.WriteString("azKits 本次执行日志\n")
	body.WriteString("====================\n")
	fmt.Fprintf(&body, "工具集合: %s\n", cleanText(run.ToolboxName))
	fmt.Fprintf(&body, "脚本名称: %s\n", cleanText(run.ScriptName))
	fmt.Fprintf(&body, "脚本别名: /%s\n", cleanText(run.Alias))
	fmt.Fprintf(&body, "脚本 ID: %s\n", cleanText(run.ScriptID))
	fmt.Fprintf(&body, "开始时间: %s\n", run.StartedAt.Format(time.RFC3339Nano))
	fmt.Fprintf(&body, "结束时间: %s\n", run.EndedAt.Format(time.RFC3339Nano))
	fmt.Fprintf(&body, "执行结果: %s\n", cleanText(run.State))
	if run.ExitCode == nil {
		body.WriteString("退出码: 不可用\n")
	} else {
		fmt.Fprintf(&body, "退出码: %d\n", *run.ExitCode)
	}
	body.WriteString("\n执行输出（按捕获顺序）\n")
	body.WriteString("====================\n")
	if len(run.Entries) == 0 {
		body.WriteString("[没有标准输出或错误输出]\n")
	}
	for _, entry := range run.Entries {
		fmt.Fprintf(&body, "[%s]\n%s", cleanText(entry.Stream), cleanText(entry.Text))
		if !strings.HasSuffix(entry.Text, "\n") {
			body.WriteByte('\n')
		}
	}
	return body.String()
}

func cleanText(value string) string {
	value = strings.ToValidUTF8(value, "�")
	value = ansiSequence.ReplaceAllString(value, "")
	return strings.Map(func(r rune) rune {
		if r == '\n' || r == '\r' || r == '\t' || !unicode.IsControl(r) {
			return r
		}
		return -1
	}, value)
}

func safeName(value string) string {
	value = strings.TrimSpace(value)
	if value == "" {
		return "run"
	}
	var result strings.Builder
	for _, r := range value {
		if unicode.IsLetter(r) || unicode.IsDigit(r) || r == '-' || r == '_' || r == '.' {
			result.WriteRune(r)
		} else {
			result.WriteByte('_')
		}
	}
	return strings.Trim(result.String(), ". ")
}
