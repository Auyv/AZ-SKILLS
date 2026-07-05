---
name: az-todo-tracker
description: |-
  浠诲姟寰呭姙璁板綍杩借釜宸ュ叿銆傜敤浜庡垱寤烘柊浠诲姟銆佽拷鍔犳帹杩涜褰曘€佹洿鏂颁换鍔＄姸鎬併€佺敓鎴愪换鍔℃€荤粨姹囨姤銆佸垪鍑鸿繎鏈熶换鍔°€佸綊妗ｅ凡瀹屾垚/宸茬粓姝换鍔°€?
  涓?`issue-tracker`(闂鎺掓煡璁板綍)骞跺垪锛屼絾鏈川涓嶅悓锛?  - `issue-tracker` 鏄?浜嬪悗杩芥函":闂宸插彂鐢燂紝鎸夋棩杩藉姞鎺掓煡闃舵
  - `az-todo-tracker` 鏄?浜嬪墠绔嬮」":浠诲姟寰呭紑濮嬶紝鎸夐渶杩藉姞鎺ㄨ繘闃舵
version: 1.0.0

  瑙﹀彂鍦烘櫙:
  - 鐢ㄦ埛鎻愬埌"璁板綍寰呭姙""鏂板缓浠诲姟""鍒涘缓浠诲姟""鐧昏寰呭姙""浠诲姟绔嬮」"
  - 鐢ㄦ埛鎻愬埌"鏇存柊浠诲姟鐘舵€?"浠诲姟鎺ㄨ繘""浠诲姟杩涘害""杩藉姞鎺ㄨ繘"
  - 鐢ㄦ埛鎻愬埌"鐢熸垚浠诲姟姹囨姤""浠诲姟鎬荤粨""浠诲姟褰掓。"
  - 鐢ㄦ埛鎻愬埌"鏌ョ湅寰呭姙""鍒楀嚭浠诲姟""鏈夊摢浜涘緟鍔?
---

# Todo Tracker

## 鐩爣

鎶婁复鏃剁殑宸ヤ綔鎯虫硶鎴栦富浜烘寚浠ゅ彉鎴?*鍙拷韪€佸彲鎺ㄨ繘銆佸彲姹囨姤銆佸彲褰掓。**鐨勪换鍔¤褰曘€傛墍鏈夋爣鍑?Markdown 鏂囦欢閫氳繃鑴氭湰 + 妯℃澘鐢熸垚锛岄伩鍏?AI 鎵嬪啓閫犳垚鏍煎紡婕傜Щ銆?
## 涓?issue-tracker 鐨勫叧绯?
| 缁村害 | issue-tracker | az-todo-tracker |
| --- | --- | --- |
| 閫傜敤鍦烘櫙 | 闂鎺掓煡锛堜簨鍚庯級 | 浠诲姟绔嬮」锛堜簨鍓嶏級 |
| 鏂囦欢缁撴瀯 | 3 涓枃浠讹紙璇︽儏/鎺掓煡璁板綍/鎬荤粨锛?| 3 涓枃浠讹紙璇︽儏 README / 鎺ㄨ繘璁板綍 / 鎬荤粨锛?|
| 鐘舵€佸瓧娈?| 寰呮帓鏌?宸茶В鍐?宸叉寕璧?| 寰呭惎鍔?杩涜涓?宸插畬鎴?宸叉寕璧?宸插彇娑?闃诲 |
| 杩藉姞瀛愬懡浠?| `daily`锛堟寜鏃ヨ拷鍔犻樁娈碉級 | `progress`锛堟寜闇€杩藉姞闃舵锛?|
| 鐘舵€佸彉鏇?| 閫氳繃鐢熸垚姹囨姤闅愬紡娴佽浆 | `update` 瀛愬懡浠ゆ樉寮忔敼鐘舵€?|
| 褰掓。鐩爣 | `椤圭洰闆?{椤圭洰}/鍘嗗彶闂/` | `濂旈渼-note/浠诲姟褰掓。/` |
| 淇℃伅琛?| 闂鍚嶇О/鍙嶉浜?浼樺厛绾?... | 浠诲姟鍚嶇О/椤圭洰缂栧彿/璐熻矗浜?... |

**鍒ゆ柇鐢ㄥ摢涓?*:
- 涓讳汉璇?杩欎釜闂" / "瀹㈡埛鍙嶉" / "鎺掓煡" 鈫?issue-tracker
- 涓讳汉璇?寰呭姙" / "瑕佸仛" / "鏂伴渶姹? / "鎬ц兘娴嬭瘯" 鈫?az-todo-tracker

## 鐩綍鑱岃矗

```text
az-todo-tracker/
  SKILL.md
  MANUAL.md
  scripts/
    task_tracker.py
  templates/
    todo_detail.md
    progress_log.md
    progress_log_placeholder.md
    report.md
    report_placeholder.md
    notification.txt
  evals/
    evals.json
```

妯℃澘鏄枃妗ｆ牸寮忕殑鍗曚竴鏉ユ簮銆備慨鏀规牱寮忎紭鍏堟敼 `templates/`锛屼笉瑕佸湪 SKILL.md 涓鍒躲€?
## 鍥哄畾璺緞

- 浠诲姟寰呭姙鏍圭洰褰曪細`C:\JPZ_Workspace\绗旇浠撳簱-jpz\hq-jpz\濂旈渼-note\浠诲姟寰呭姙\`
- 褰掓。鏍圭洰褰曪細`C:\JPZ_Workspace\绗旇浠撳簱-jpz\hq-jpz\濂旈渼-note\浠诲姟褰掓。\`
- 閫氱煡璐熻矗浜猴細宸ュ彿 `101706`锛屽鍚?`闈虫櫘璇廯

濡傛灉褰撳墠鐜鏃犳硶璁块棶涓婅堪璺緞锛屽厛璇存槑 `ERR_PERMISSION_DENIED` 鎴栬矾寰勪笉瀛樺湪锛屼笉瑕佹敼鍐欏埌鏈‘璁ょ殑浣嶇疆銆?
## 鎵ц鍘熷垯

1. **鎵€鏈夊啓鏂囦欢鍔ㄤ綔閮介€氳繃 `scripts/task_tracker.py`**锛孉I 涓嶈鎵嬪啓 README / 鎺ㄨ繘璁板綍 / 姹囨姤銆?2. **鐘舵€佸彉鏇村繀椤荤敤 `update` 瀛愬懡浠?*锛屼笉瑕佺洿鎺?patch README.md 閲岀殑鐘舵€佸瓧娈点€?3. **浠诲姟瀹屾垚鏃讹紝鍏堢敤 `update --status 宸插畬鎴恅 + `progress` 鍐欐渶鍚庨樁娈碉紝鍐嶇敤 `report` 鍑烘眹鎶ワ紝鏈€鍚?`archive` 绉诲姩鍒板綊妗ｇ洰褰?*銆?4. **AI 涓诲姩褰掓。鍓嶅繀椤荤‘璁や换鍔″凡瀹岀粨**銆?5. **濡傝鏀规牸寮忥紝鏀?`templates/`锛屽啀璺戜竴娆＄鍒扮楠岃瘉**銆?6. 璇诲彇鍜屽啓鍏ヤ腑鏂囨枃浠剁粺涓€浣跨敤 UTF-8銆?7. 鑴氭湰杩斿洖 JSON銆傚洖澶嶇敤鎴锋椂鎻愬彇鍏抽敭瀛楁锛屼笉瑕佹妸瀹屾暣 JSON 鍊惧€掔粰鐢ㄦ埛銆?
---

## WSL 璺緞闄烽槺(閲嶈)

`task_tracker.py` 涓?`DEFAULT_ROOT` 纭紪鐮佷负 Windows 鍙嶆枩绾胯矾寰?`C:\\...`銆傚湪 WSL 鐜涓嬭繍琛?Python 鏃讹紝姝ょ被璺緞浼氳 `pathlib.Path` 褰撲綔**鐩稿璺緞**澶勭悊锛屽鑷存枃浠惰鍐欏埌鎶€鑳界洰褰曞唴閮ㄨ€岄潪鐭ヨ瘑搴撱€?
**琛ㄧ幇**:鍒涘缓鐨勬枃浠跺湪 `/home/jpz/.hermes/skills/.../C:\\...` 涓嬶紝鑰屼笉鏄煡璇嗗簱涓€?
**棰勯槻**:鍦?WSL 涓嬫墽琛屾椂锛屽繀椤讳娇鐢?`--root` 鍙傛暟浼犲叆 WSL 鏍煎紡缁濆璺緞:

```bash
python3 scripts/task_tracker.py create \
  --root "/mnt/c/JPZ_Workspace/绗旇浠撳簱-jpz/hq-jpz/濂旈渼-note/浠诲姟寰呭姙" \
  --title "xxx" --project "yyy" --owner "zzz"
```

**搴旀€ュ鐞?*:鑻ユ枃浠惰閿欒鍐欏叆鎶€鑳界洰褰?

```bash
# 1. 鎵惧埌閿欒璺緞
ls -la "/home/jpz/.hermes/skills/note-taking/az-todo-tracker-execution/"
# 2. 绉诲姩鍒版纭綅缃?cp -r "<閿欒璺緞>/<浠诲姟鐩綍>" \
      "/mnt/c/JPZ_Workspace/绗旇浠撳簱-jpz/hq-jpz/濂旈渼-note/浠诲姟寰呭姙/"
# 3. 娓呯悊閿欒浣嶇疆
rm -rf "<閿欒璺緞>"
```

---

## 甯歌鎵ц閿欒

| 閿欒 | 鍘熷洜 | 姝ｇ‘鍋氭硶 |
|------|------|----------|
| AI 鐩存帴鍐?README.md / 02_鎺ㄨ繘璁板綍.md | 鏈寜瑙勮寖鎵ц锛屾ā浠夸簡鏃ф牸寮?| 涓ユ牸浣跨敤 `scripts/task_tracker.py` |
| 璺宠繃 skill_view 鐩存帴璋冪敤 | 娌跨敤鏃т範鎯?| 蹇呴』鍏?`skill_view(az-todo-tracker)` |
| `update --status` 鍚庢病鏀?`绗?8 鑺?鎺ㄨ繘璁板綍` 琛ㄦ牸 | 鍙敼浜嗛《閮ㄤ俊鎭〃 | 鍚屾浼?`--append-progress-row "鍔ㄤ綔鎻忚堪"` |
| 褰掓。鏃?`--archive-root` 鏄?Windows 鍙嶆枩鏉?| WSL 涓?`pathlib` 瑙嗕负鐩稿璺緞 | WSL 涓嬬敤 `/mnt/c/...` 鍓嶇紑缁濆璺緞 |
| `archive` 鎶?success 浣嗗綊妗ｇ洰褰曚笉鍦?`濂旈渼-note/浠诲姟褰掓。/`,鏂囦欢"澶辫釜" | `DEFAULT_ARCHIVE_ROOT = Path(r"C:\...")` 鏄?Windows 鍙嶆枩鏉犺矾寰?WSL 涓?`pathlib` 鎶婂畠褰?*鐩稿璺緞**鎷煎埌 CWD;`shutil.move` 瀹為檯鎶婄洰褰曠Щ鍒?`{CWD}/C:\...\浠诲姟褰掓。\...` 杩欑鐣稿舰浣嶇疆(濡?`C:\Users\jpz\C:\JPZ_Workspace\...\`),鑴氭湰浠嶇劧"success"杩斿洖銆?*v1.0.1 璧?DEFAULT_* 宸叉敼涓?WSL 鏍煎紡**;濡傛灉浣犱箣鍓嶇敤杩?v1.0.0 骞跺彂鐜扮洰褰?澶辫釜",鎸変互涓嬪懡浠ゆ壘鍥炲苟娓呯悊:`find / -path "*/浠诲姟褰掓。/*MCUD*" -type d 2>/dev/null` 鎵句綅缃?鈫?`mv` 鍥炴纭洰褰?鈫?`rm -rf "/mnt/c/Users/jpz/C:"` 娓呯悊鐣稿舰鐖剁洰褰?|
| `--root` 鎶?unrecognized arguments | argparse 瀛愯В鏋愬櫒瑕佹眰椤跺眰鍙傛暟蹇呴』鍦ㄥ瓙鍛戒护**涔嬪墠** | 姝ｇ‘椤哄簭:`task_tracker.py --root <path> create --title ...`;`--root` 鍐欏湪 `create` 涔嬪悗浼氳 argparse 鎷掔粷 |
| `progress` 绗簩娆¤皟鐢ㄦ病鍔?`--append` | 鑴氭湰妫€娴嬪埌宸叉湁闃舵灏遍€€鍑?existed=true | 鍔?`--append` 鎵嶄細鍐欏叆闃舵 N+1 |

---

## 鍔ㄤ綔鍒ゅ畾

| 鍔ㄤ綔 | 瑙﹀彂琛ㄨ揪 | 鑴氭湰鍛戒护 |
| --- | --- | --- |
| `create_task` | 璁板綍寰呭姙銆佹柊寤轰换鍔°€佷换鍔＄珛椤广€佺櫥璁颁换鍔?| `python scripts/task_tracker.py create ...` |
| `add_progress` | 浠诲姟鎺ㄨ繘銆佹洿鏂拌繘搴︺€佸仛浜?X銆佹帹杩?X | `python scripts/task_tracker.py progress ...` |
| `update_task` | 鏀圭姸鎬併€佹敼浼樺厛绾с€佸欢鏈熴€佺姸鎬佸彉鏇?| `python scripts/task_tracker.py update ...` |
| `generate_report` | 鐢熸垚姹囨姤銆佷换鍔℃€荤粨銆佸鐩?| `python scripts/task_tracker.py report ...` |
| `list_tasks` | 鏌ョ湅寰呭姙銆佸垪鍑轰换鍔°€佹湁鍝簺寰呭姙 | `python scripts/task_tracker.py list` |
| `archive_task` | 褰掓。浠诲姟銆佸凡瀹屾垚銆佸叧闂换鍔?| `python scripts/task_tracker.py archive ...` |

濡傛灉鍔ㄤ綔涓嶆槑纭紝鍏堢敤涓€鍙ヨ瘽纭鐩爣鍔ㄤ綔銆?*涓嶈鍚屾椂鎵ц澶氫釜鍐欐枃浠跺姩浣?*锛岄櫎闈炵敤鎴锋槑纭姹傘€?
---

## create_task

### 蹇呴渶淇℃伅

- 浠诲姟鍚嶇О / 涓€鍙ヨ瘽绠€杩帮細`--title`
- 椤圭洰缂栧彿锛歚--project`
- 璐熻矗浜猴細`--owner`

缂哄皯鏃惰拷闂己澶遍」銆傜敤鎴蜂竴娆℃€ф彁渚涘厖鍒嗕俊鎭椂涓嶈閲嶅杩介棶銆?
### 鍛戒护鏍煎紡

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py create `
  --title "MCUD 鎬ц兘娴嬭瘯 鈥?鎼缓娴嬭瘯鏈嶅姟杩涚▼" `
  --project "MCUD" `
  --owner "闈虫櫘璇?101706)" `
  --priority "涓? `
  --status "寰呭惎鍔? `
  --background "MCUD 鎺ュ彛鍦ㄥ疄杞︾幆澧冧笅鐨勭ǔ瀹氭€ч渶瑕侀獙璇? `
  --goal "鎼缓娴嬭瘯鏈嶅姟杩涚▼妯℃嫙瀹炶溅鐜,浜у嚭鎬ц兘/绋冲畾鎬ф姤鍛? `
  --blocker-1 "MCUD 鎺ュ彛鍗忚鏂囨。浣嶇疆鏈煡" `
  --blocker-2 "瀹炶溅鐜妯℃嫙瑕佺礌寰呬富浜虹‘璁? `
  --trigger-source "涓讳汉 2026-06-26 浼佸井瀵硅瘽"
```

鑴氭湰浼氬垱寤?

```text
{root}/
  {yyyy-MM-dd}路{椤圭洰缂栧彿}路{浠诲姟鍚峿/
    README.md              # 浠诲姟璇︽儏(鍚?8 涓爣鍑嗗皬鑺?+ 鎺ㄨ繘璁板綍琛?
    02_鎺ㄨ繘璁板綍.md         # 鍗犱綅鏂囦欢
    03_鎬荤粨姹囨姤.md         # 鍗犱綅鏂囦欢
```

骞惰繑鍥炰紒寰€氱煡鏂囨銆?*榛樿鍙敓鎴愭枃妗堬紝涓嶇洿鎺ュ彂閫?*銆?
---

## add_progress

浼樺厛浣跨敤鐢ㄦ埛缁欏嚭鐨?`--task-path`銆傛病鏈夎矾寰勬椂浣跨敤 `--query` 鍦ㄤ换鍔″緟鍔炴牴鐩綍涓尮閰嶃€?
```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py progress `
  --query "MCUD 鎬ц兘娴嬭瘯" `
  --action-text "瀹屾垚浜?SPI 椹卞姩瀛︿範" `
  --result "鑳藉啓鍑哄熀纭€瀛楃璁惧椹卞姩" `
  --next-plan "杩涘叆 CIU98M25 鍗忚鍒嗘瀽"
```

鍙€夊弬鏁?

```powershell
--date "2026-06-27"           # 涓嶄紶鍒欑敤浠婂ぉ
--append                       # 02_鎺ㄨ繘璁板綍.md 宸叉湁闃舵鏃惰拷鍔犳柊闃舵(闃舵鍙?= 鐜版湁鏈€澶?+ 1)
--blocker "鍗″湪 X 璧勬枡涓嶅叏"    # 鍐崇瓥/鍗＄偣
```

### 琛屼负璇存槑

- **棣栨鍐欏叆闃舵**:`02_鎺ㄨ繘璁板綍.md` 浠嶄负鍗犱綅鍐呭鏃?鍐欏叆"闃舵 1(HH:MM)"灏忚妭
- **杩藉姞(`--append`)**:`02_鎺ㄨ繘璁板綍.md` 宸叉湁闃舵鏃?鎵弿鐜版湁"## 闃舵 N"鏍囬,鍙栨渶澶?N + 1,杩藉姞鏂伴樁娈靛皬鑺?- **鏈姞 `--append` 涓旀枃浠跺凡鏈夐樁娈?*:杩斿洖 `existed=true` 鎻愮ず鍔?`--append`,涓嶈鐩?
濡傛灉 `02_鎺ㄨ繘璁板綍.md` 闇€瑕佹暣浣撻噸鏂扮敓鎴?缃曡),璇峰厛鎵嬪姩澶囦唤骞跺垹闄ゆ棫鏂囦欢鍐嶈皟鐢?`progress` 瀛愬懡浠ゃ€?
---

## update_task

**鍙敼 README.md 椤堕儴淇℃伅琛?+ 绗?8 鑺傛帹杩涜褰曡〃**銆?*涓?*鏀瑰叾浠栫珷鑺傘€?
```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py update `
  --query "MCUD 鎬ц兘娴嬭瘯" `
  --status "杩涜涓? `
  --append-progress-row "鍚姩浠诲姟 0:鐮旂┒ MCUD 鍗忚鏂囨。"
```

鍙敼瀛楁:

- `--status`: 鐘舵€?寰呭惎鍔?杩涜涓?宸插畬鎴?宸叉寕璧?宸插彇娑?闃诲)
- `--priority`: 浼樺厛绾?楂?涓?浣?
- `--due-date`: 鎴鏃ユ湡
- `--append-progress-row`: 鍦ㄧ 8 鑺傝拷鍔犱竴琛屾帹杩涜褰?鍔ㄤ綔鎻忚堪)

**鑷冲皯闇€瑕佷紶涓€涓瓧娈?*,鍚﹀垯杩斿洖 `ERR_MISSING_REQUIRED_INFO`銆?
---

## generate_report

浣跨敤鍥哄畾姹囨姤妯℃澘鐢熸垚 `03_鎬荤粨姹囨姤.md`:

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py report `
  --query "MCUD 鎬ц兘娴嬭瘯"
```

鑴氭湰浼氳鍙?`README.md` 鍜?`02_鎺ㄨ繘璁板綍.md`,鍩轰簬 `templates/report.md` 鐢熸垚 `03_鎬荤粨姹囨姤.md`銆?
> 濡傛灉 `02_鎺ㄨ繘璁板綍.md` 涓虹┖,浼氬湪 `缁忛獙娌夋穩` 娈垫彃鍏?鈿狅笍 缂哄皯杩囩▼鏁版嵁"鎻愮ず銆?
**涓嶈鐩存帴鏀瑰啓姹囨姤缁撴瀯**;濡傝鏀?淇敼 `templates/report.md`銆?
---

## list_tasks

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py list --limit 10
```

鍙鐩綍鍏冩暟鎹?杩斿洖:

- 鐩綍鍚?- 璺緞
- 鐘舵€?浠?README 椤堕儴淇℃伅琛ㄨ鍙?
- 鏄惁鏈夋帹杩涜褰?- 鏄惁鏈夋€荤粨姹囨姤
- 鏈€鍚庝慨鏀规椂闂?
---

## archive_task

褰掓。鍓嶅繀椤荤‘璁や换鍔″凡缁撴潫(鐘舵€?= 宸插畬鎴?/ 宸叉寕璧?/ 宸插彇娑?/ 闃诲),骞惰兘纭畾鍞竴浠诲姟鐩綍銆?
```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py archive `
  --query "MCUD 鎬ц兘娴嬭瘯"
```

鑴氭湰浼氱Щ鍔ㄦ暣涓换鍔＄洰褰曞埌:

```text
C:\JPZ_Workspace\绗旇浠撳簱-jpz\hq-jpz\濂旈渼-note\浠诲姟褰掓。\
```

濡傛灉褰掓。鐩爣宸插瓨鍦?涓嶈鐩栥€?
---

## 妯℃澘缁存姢瑙勫垯

- `templates/todo_detail.md`:浠诲姟璇︽儏 README.md 妯℃澘(鍚?8 涓爣鍑嗗皬鑺?
- `templates/progress_log.md`:鎺ㄨ繘闃舵鏍煎紡(鍐欏叆 `02_鎺ㄨ繘璁板綍.md`,鍚屼竴鏂囦欢鎸?闃舵 N"杩藉姞)
- `templates/progress_log_placeholder.md`:鏂板缓璁板綍鏃剁殑鍗犱綅鏂囦欢
- `templates/report.md`:鎬荤粨姹囨姤鏍煎紡
- `templates/report_placeholder.md`:鏂板缓璁板綍鏃剁殑鍗犱綅鏂囦欢
- `templates/notification.txt`:浼佸井閫氱煡鏂囨

妯℃澘鍙橀噺浣跨敤 `{{鍙橀噺鍚峿}`銆傝剼鏈湭鏀跺埌鐨勫彉閲忎細鏇挎崲涓?`寰呰ˉ鍏呫€俙銆?
---

## 瀹屾垚鍥炲鏍煎紡

鎵ц鎴愬姛鍚?鍥炲鍖呭惈:

- 宸叉墽琛屽姩浣?- 鍒涘缓鎴栨洿鏂扮殑鏂囦欢璺緞
- 鏄惁鏈夊緟琛ュ厖椤?- 涓嬩竴姝ュ缓璁?
鎵ц澶辫触鎴栦俊鎭笉瓒虫椂,鍥炲鍖呭惈:

- 閿欒鐮?- 缂哄け鎴栧啿绐佺偣
- 鐢ㄦ埛涓嬩竴姝ラ渶瑕佹彁渚涚殑淇℃伅
