# 金手指文字编辑器（App 内多行文字编辑器）— 设计

日期：2026-09-12
分支：`cheat-text-editor`
相关代码：`UI/CwCheatScreen.cpp`、`Core/CwCheat.cpp`、`Common/UI/View.cpp`、`android/src/org/ppsspp/ppsspp/`

## 1. 目标

在 PPSSPP 的 UI 内提供一个**多行文字编辑器**，让使用者能直接编辑金手指（CwCheat `.ini`）的内容：
新增金手指、修改名称与 `_L` 代码行、删除金手指、以及直接编辑整个金手指文件。

现状只能勾选开/关；桌面版有一个 "Edit Cheat File" 会丢给外部文字编辑器，Android 上完全没有编辑途径。

## 2. 现状（已确认的代码事实）

**数据流**

1. `__CheatInit()`（`Core/CwCheat.cpp:250`）：`g_Config.bEnableCheats` 为真时 `__CheatStart()`。
2. `__CheatStart()`：`new CWCheatEngine(g_paramSFO.GetDiscID())`，文件名 = `GetSysDirectory(DIRECTORY_CHEATS)/<GAMEID>.ini`；`CreateCheatFile()` 在文件不存在时写入 UTF-8 BOM + 换行；`ParseCheats()` 解析。
3. `hleCheat()` 每 `g_Config.iCwCheatRefreshIntervalMs`（默认 77 ms）执行：处理启用/停用切换；若 `g_Config.bReloadCheats` 则 `ParseCheats()` 重新解析，然后 `Run()` 执行所有 op。
4. `CwCheatScreen::TryLoadCheatInfo()` 用 `CWCheatEngine::FileInfo()`（**再次解析文件**）取得 `std::vector<CheatFileInfo>`，每条对应一个 `_C` 行：`{lineNum, name, enabled}`。
5. 勾选 → `CwCheatScreen::OnCheckBox()` → `RebuildCheatFile(index)`：把整个文件读成 `lines`，只改第 `lineNum` 行的 `_C0`/`_C1`，其余行原样写回；更新 `fileCheckHash_`；`g_Config.bReloadCheats = true`。
6. `CwCheatScreen::update()` 每 53 帧用 XXH3 比对文件内容 hash，变了就 `RecreateViews()`。

**所以「写回文件」的机制已经存在**（就是勾选在用的那套），编辑器只需把 `_L` 行也纳入同一套读写。

**解析器行为（`CheatFileParser`）**

- 只保留 `_S`/`_G`/`_C`/`_L`；`//` 与 `#` 注释、无法识别的行都会被丢弃 → **不能用解析结果重新生成整个文件，否则注释会消失**。写回必须逐行 patch。
- `_C` 行的 enabled 判断：`_C1`–`_C9` = 启用，`_C0` = 停用。
- 解析只纳入符合当前 gameID 的区块（`ValidateGameID`，比对时移除 `-`）；第一个 `_S` 不符时有一条 BC 兼容的「只有一个游戏就放行」路径（`gameRiskyEnabled_`）。
- 已知缺陷（本设计要修）：
  - `FlushCheatInfo()` 只在 `pendingLines_` 非空时被调用，所以**没有 `_L` 代码行的 `_C` 永远不会出现在 `FileInfo()`**，UI 列表看不到、也无法编辑它。
  - `ParseDataLine()` 对停用的金手指（`cheatEnabled_ == false`）每一行都调用 `FlushCheatInfo()`，第一行就把 `lastCheatInfo_` 推进去并清空，导致无法得知该金手指完整的 `_L` 行范围。

**平台文字输入能力**

| 平台 | 共用 UI 的输入法通知 | App 内打字 | 读剪贴板 |
|---|---|---|---|
| Android | **缺**（`app-android.cpp` 没有 `NOTIFY_UI_EVENT` 处理） | 只有按键事件（`getUnicodeChar()` → `NativeApp.keyChar`）；输入法的 `commitText`（中文、粘贴）进不来 | 无（`SYSPROP_CLIPBOARD_TEXT` 未实现） |
| Windows | 有 | 实体键盘 | 有 |
| Linux/macOS (SDL) | 有（Switch 用 swkbd） | 实体键盘 | 有 |
| iOS | 有 | `UIKeyInput`（`insertText:` / `deleteBackward` / `showKeyboard`） | — |
| UWP | 有 | `ActivateTextEditInput` + InputPane | 有 |

**各平台现成的输入法接入（本设计照这个架构做，Android 只是缺这一段）**

共用 UI 透过 `System_NotifyUIEvent(UIEventNotification::TEXT_GOTFOCUS / TEXT_LOSTFOCUS)` 告诉平台「现在有文字字段拿到焦点了」，通知来源是控件本身（`TextEdit::FocusChanged()`，`Common/UI/View.cpp:1260`）。各平台的接收端：

| 平台 | 事件处理位置 | 收到 `TEXT_GOTFOCUS` 时做什么 | 文字怎么进来 |
|---|---|---|---|
| iOS | `ios/main.mm:556`（`NOTIFY_UI_EVENT`） | `[sharedViewController showKeyboard]` → `becomeFirstResponder` | 视图控制器实作 `UIKeyInput`：`insertText:` → `SendKeyboardChars()`（`ios/Controls.mm:427`）→ 逐个 code point 送 `KeyInput{flags = CHAR}`；`deleteBackward` → `NKCODE_DEL` |
| UWP | `UWP/PPSSPP_UWPMain.cpp:603` | `ActivateTextEditInput(true)`（`UWP/UWPHelpers/InputHelpers.cpp:117`）→ 显示 InputPane | CoreTextEditContext / `IgnoreInput()`（`InputHelpers.cpp`） |
| SDL / Switch | `SDL/SDLMain.cpp:910` | `g_textFocus = true` → 弹出 swkbd | swkbd |
| **Android** | **没有这个 case** | — | 只有 `PpssppActivity.dispatchKeyEvent` 的按键事件。`PpssppActivity` 虽然已经有 `showKeyboard` / `hideKeyboard` 指令处理（`PpssppActivity.java:1602`），但**没有任何地方送这两个指令**（`System_ShowKeyboard()` 是死代码），而且两个 surface view（`NativeSurfaceView`、`NativeGLSurfaceView`）既没有 `setFocusableInTouchMode(true)`，也没有 `onCreateInputConnection`，所以输入法连不上、`commitText` 也无处可去 |

**结论**：Android 缺的正是这段 —— 补上 `NOTIFY_UI_EVENT` 处理，并给 surface view 加一个输入法目标（Android 版的 `UIKeyInput`，也就是 `InputConnection`）。**不需要新 API、不需要新 JNI**：既有 `NativeApp.keyChar(deviceId, unicodeChar)` 就是 iOS `SendKeyboardChars()` 的等价物，而 `keyDown`/`keyUp` 收的 Android keycode 本身就是 NKCODE（`Common/Input/InputState.h:192`）。

## 3. 范围

### 实施顺序（Android 优先，iOS 不做）

| 阶段 | 内容 | 验收方式 |
|---|---|---|
| **阶段 1：Android（主目标）** | 共用的控件/画面/Core 改动 + 入口 + Android 输入法接入 | 单元测试全绿；Android APK 实机跑通 §7.4 |
| 阶段 2：其他平台（Windows/Linux/macOS） | 同一个控件/画面在桌面的验证与打磨（键盘、鼠标、Ctrl+V、行号栏、滚动） | `./b.sh --debug` + §7.2 的桌面手测清单 |
| 不动 | iOS、UWP、libretro：本设计**不新增任何 `System_` 函数**，所以那六个平台文件都不用改 | — |

共用代码（`MultilineTextEdit`、`CheatEditScreen`、`Core/CwCheat`）只写一次，两个阶段共用；阶段 1 不为了 Android 写任何只有 Android 才能用的东西。

### 做

1. 新控件 `MultilineTextEdit`：多行文字编辑（光标、上下左右、Home/End、PageUp/Down、Enter 换行、Backspace/Delete、垂直＋水平滚动、行号栏、UTF-8 光标、单层撤销、Ctrl+C/V）。
2. 新画面 `CheatEditScreen`，三种模式：
   - `WholeFile`：编辑整个 `.ini`
   - `OneCheat`：编辑某一条金手指的文字区块，并可删除该条
   - `NewCheat`：新增一条（在文件末尾追加）
3. 入口：`CwCheatScreen` 列表每行「✏️」、左栏「编辑金手指文件」、「新增金手指」（原本桌面专属的入口改为全平台显示）。
4. Core 变更：修正 `CheatFileParser` 上述两个缺陷、新增内存字符串解析（验证用）、新增纯文字操作工具函数（可单元测试）。
5. Android 输入法接入（阶段 1）：`app-android.cpp` 补 `NOTIFY_UI_EVENT` → `showKeyboard` / `hideKeyboard`；两个 surface view 加输入法目标（`onCreateInputConnection`）；复用既有 `keyChar` / `keyDown` / `keyUp`，不新增 native 接口。
6. 单元测试（`unittest/TestCwCheat.cpp`）。

### 不做（YAGNI）

- iOS、UWP、libretro 的任何改动（iOS 不考虑）。
- 文字选取、局部复制（Ctrl+C 复制整份文字）、多层撤销、语法高亮、自动换行。
- 桌面版「用外部编辑器打开」的入口（改为一律 App 内编辑）。
- Android 原生输入对话框（`inputBox`）的多行改造：接上输入法之后不需要它，保持现状。
- 金手指执行语义（`CheatOperation`、`Run()`）完全不动。
- 翻译（依 `AGENTS.md` 规则，功能完成、英文文案定稿后再单独一个 commit 做）。

## 4. 架构

| 元件 | 文件 | 职责 |
|---|---|---|
| `MultilineTextEdit` | `Common/UI/View.h` / `Common/UI/View.cpp`（既有文件，与 `TextEdit` 同处） | 纯 UI 控件：显示与编辑一段多行文字 |
| `CheatEditScreen` | `UI/CheatEditScreen.h` / `.cpp`（新文件） | 编辑画面：版面、三种模式、保存、验证错误显示、退出确认 |
| `CheatFileParser` | `Core/CwCheat.h` / `.cpp`（既有） | 解析（文件或内存字符串）、回报 `CheatFileInfo` |
| `CheatFileText`（namespace） | `Core/CwCheat.h` / `.cpp`（既有） | 纯函数：行分割/合并、抽出/替换/删除/追加金手指区块 |
| 入口整合 | `UI/CwCheatScreen.h` / `.cpp`（既有） | 列表加 ✏️、左栏加两个入口、移除桌面外部编辑器 |
| Android 输入法接入 | `android/jni/app-android.cpp`（补 `NOTIFY_UI_EVENT`）、`NativeSurfaceView.java`、`NativeGLSurfaceView.java`（输入法目标） | 让输入法文字经既有 `keyChar` 进到 `KeyInput{CHAR}` 队列 |

分层原则：`CheatFileText` 与 `CheatFileParser` 不依赖 UI，可被 `PPSSPPUnitTest`（链接 `Common Core`）直接测试；`CheatEditScreen` 只做组合与流程。

**新文件**（只有两个）：`UI/CheatEditScreen.h`、`UI/CheatEditScreen.cpp`、`unittest/TestCwCheat.cpp`。
`MultilineTextEdit` 放在既有的 `View.h/.cpp`、文字工具放在既有的 `Core/CwCheat.*`，以避免新增文件带来的多平台构建清单风险（见 §8）。

## 5. 详细设计

### 5.1 `MultilineTextEdit`（`Common/UI/View.h/.cpp`）

```cpp
class MultilineTextEdit : public View {
public:
	MultilineTextEdit(std::string_view text, std::string_view title, LayoutParams *layoutParams = nullptr);

	void SetText(std::string_view text);
	const std::string &GetText() const { return text_; }
	void SetMaxLen(size_t maxLen) { maxLen_ = maxLen; }
	void SetShowLineNumbers(bool show) { showLineNumbers_ = show; }

	void FocusChanged(FocusFlags focusFlags) override;
	void GetContentDimensions(const UIContext &dc, float &w, float &h) const override;
	void Draw(UIContext &dc) override;
	std::string DescribeText() const override;
	bool Key(const KeyInput &key) override;
	bool Touch(const TouchInput &touch) override;

	bool Backspace();
	void MoveLeft();
	void MoveRight();
	void MoveUp();
	void MoveDown();
	void MoveToLineStart();
	void MoveToLineEnd();
	void PageUp();
	void PageDown();
	void InsertAtCaret(std::string_view text);

	Event OnTextChange;

private:
	void RebuildIndex();              // 重建 lineStarts_（只在文字变动时调用）
	int LineOfOffset(int offset) const;
	int LineStart(int line) const;
	int LineEnd(int line) const;      // 不含换行字符
	int ColumnOfOffset(int line, int offset) const;
	void ClampScrollToCaret(const UIContext &dc);
	void NotifyTextChanged();

	std::string text_;                // 原始字节，含 '\n'
	std::vector<int> lineStarts_;     // 每行起始 byte offset，最后一笔 = text_.size()
	std::string undo_;
	int caret_ = 0;                   // byte offset，保证落在 UTF-8 边界
	int desiredCol_ = -1;             // 上下移动时的目标「显示列」
	int firstVisibleLine_ = 0;
	float scrollX_ = 0.0f;
	size_t maxLen_ = 1 << 20;         // 1 MiB
	bool showLineNumbers_ = true;
};
```

**文字与光标**

- `text_` 是原始字节（保留 UTF-8 与换行原样），不做正规化。
- `caret_` 用 `u8_inc` / `u8_dec`（`Common/Data/Encoding/Utf8.h`）确保落在字符边界。
- `RebuildIndex()` 只在 `SetText()` 与任何文字变动后调用（O(n)）；绘制与 caret 换算用二分查找，成本 O(log n)。

**绘制**

- 字体：`dc.GetTheme().uiFont`（与 `TextEdit` 一致）；行高由 `dc.MeasureText()` 取得。
- 只绘制可见范围的行（`firstVisibleLine_` 到可见行数上限），并用 `dc.PushScissor()` 裁剪。
- 行号栏：宽度 = `MeasureText(最大行号字符串)` + 边距；绘制在左侧、不随水平滚动移动，颜色用暗色。caret 与文字的 x 起点在行号栏右侧。
- `'\t'` 只影响显示：以 4 个空格的宽度绘制（caret 换算用同一规则），不改动 `text_` 内容。
- 光标：`dc.FillRect(UI::Drawable(textColor), Bounds(caretX - 1, caretY, 3, lineHeight))`，与 `TextEdit` 相同风格。
- 焦点外观与 `TextEdit` 一致（`dc.FillRect(HasFocus() ? 0x80000000 : 0x30000000, bounds_)`）。
- 空文字时显示 `title_` 作为 placeholder（`ALIGN_TOP|ALIGN_LEFT`）。

**按键**（比照 `TextEdit::Key` 的结构）

| 按键 | 行为 |
|---|---|
| `NKCODE_DPAD_LEFT` / `RIGHT` | 左/右移一个 UTF-8 字符（可跨行） |
| `NKCODE_DPAD_UP` / `DOWN` | 上/下一行，维持 `desiredCol_`（首次上下移动时记录当前显示列） |
| `NKCODE_MOVE_HOME` / `NKCODE_PAGE_UP` | 行首 / 上一页 |
| `NKCODE_MOVE_END` / `NKCODE_PAGE_DOWN` | 行尾 / 下一页 |
| `NKCODE_FORWARD_DEL` / `NKCODE_DEL` | 删除光标后 / 前一个字符 |
| `NKCODE_ENTER` / `NKCODE_NUMPAD_ENTER` | 在光标处插入 `'\n'` |
| `NKCODE_BACK` / `NKCODE_ESCAPE` | `return false`（交给画面处理） |
| `KeyInputFlags::CHAR` 且 `unichar == '\n'` | 插入换行（Android 输入法送进来的换行） |
| `KeyInputFlags::CHAR` 且 `unichar >= 0x20` | 插入该字符（与 `TextEdit` 相同写法：`u8_wc_toutf8`） |
| `Ctrl+C` | 复制整份文字到剪贴板（`System_CopyStringToClipboard`） |
| `Ctrl+V` | 把剪贴板文字**插入光标处**（含换行；与 `TextEdit` 的「替换全部」不同，这里刻意做成插入） |
| `Ctrl+Z` | 用 `undo_` 还原（单层，与 `TextEdit` 一致） |
| `Ctrl+A` | 不做（无选取） |

- 任何文字变动前先 `undo_ = text_`，变动后 `NotifyTextChanged()`：`RebuildIndex()`、`ClampScrollToCaret()`、触发 `OnTextChange`。
- 插入时检查 `text_.size() + 要插入的长度 <= maxLen_`，超过则整笔拒绝（不部分插入）并 `return true`（事件已被消耗）；画面另外显示 `NoticeView` 提示（见 5.3）。
- 剪贴板读取方式（桌面 Ctrl+V）：`System_GetProperty(SYSPROP_CLIPBOARD_TEXT)`；空字符串就什么都不做。

**触控**

- 点击：由 y 决定行（`firstVisibleLine_ + (y - 内容区 y) / 行高`），由 x 减去行号栏与 `scrollX_` 后，逐字符 `MeasureText` 比较决定 caret（作法同 `TextEdit` 的 `selectAtX_`）。
- 垂直拖曳：调整 `firstVisibleLine_`（超过门坎才算拖曳，否则视为点击定位）。
- 只在 `HasFocus()` 时处理键盘事件（与 `TextEdit` 相同，焦点由框架决定）。

**焦点**

- `FocusChanged()`：与 `TextEdit::FocusChanged()`（`Common/UI/View.cpp:1260`）完全一致 —— 取得焦点时 `System_NotifyUIEvent(UIEventNotification::TEXT_GOTFOCUS)`，失去焦点时 `TEXT_LOSTFOCUS`。各平台据此显示/收起自己的输入法（§5.5），控件本身不碰任何平台 API。

### 5.2 Core 变更（`Core/CwCheat.h/.cpp`）

**(a) 修正「没有 `_L` 的 `_C` 不会出现在 `FileInfo()`」**

- `CheatFileParser::Flush()` 改为：只要 `lastCheatInfo_.lineNum != 0` 就把 info 推入 `cheatInfo_`（`cheats_` 仍只在 `pendingLines_` 非空时推入），之后重置 `lastCheatInfo_ = {0}`。
- `CheatFileParser::ParseDataLine()` 移除 `if (!cheatEnabled_) { FlushCheatInfo(); return; }` 这条提前 flush 的路径，改成 `if (!cheatEnabled_) return;`（只记范围、不解析内容）。
- `FlushCheatInfo()` 的职责并入 `Flush()`；函数若无其他使用者则删除。

行为差异（预期且正确）：`FileInfo()` 会多出先前被漏掉的条目（没有 `_L` 的 `_C`），而且停用的金手指也会有一笔。UI 列表因此会显示这些项目，`RebuildCheatFile()`（用 `lineNum` + `name` 校验）对它们也能正确运作。多游戏文件中不属于当前 gameID 的区块仍然不会出现（沿用 `gameEnabled_` 判断）。

**(b) 新增内存字符串解析（验证用）**

```cpp
// 既有的文件版本，改为读入整份内容后调用 ParseText。
bool CheatFileParser::Parse();
// 新增：直接解析一段文字（语义与文件版一致：跳过首行 BOM、TrimString、同样的长度检查）。
bool CheatFileParser::ParseText(std::string_view text);
```

- 内部共用一个 `ParseLines(std::string_view text)`：以 `'\n'` 切行，`TrimString()` 后走与现在完全相同的 `if/else if` 分支。
- 文件版先 `File::ReadTextFileToString()` 读入再调用；读不到文件时 `return false`（比现在「开文件失败仍回传 true」正确；两个既有调用端 `CWCheatEngine::ParseCheats()`、`CWCheatEngine::FileInfo()` 都不检查回传值，故不影响行为）。
- 已知限制：文件版的既有实现以 `fgets` 2048 字节切行，超过 2048 的行会被视为多行；改成整份读入后以 `'\n'` 切行会让行号更正确。金手指行不可能达到 2048 字节，此差异视为修正而非破坏。

**(c) 新增纯文字工具（namespace `CheatFileText`）**

```cpp
namespace CheatFileText {
	// 以 '\n' 切行；每行不含 '\n'（可能含 '\r'，原样保留）。
	std::vector<std::string> SplitLines(std::string_view text);
	// 合并回文字；trailingNewline 决定最后是否补 '\n'。
	std::string JoinLines(const std::vector<std::string> &lines, bool trailingNewline);

	// 区块范围：从第 lineNum 行（1-based）起，到下一个以 "_C"/"_S"/"_G" 开头的行（不含）或文件末尾。
	// 中间的注释、空行都属于该区块。
	int BlockEndLine(const std::vector<std::string> &lines, int lineNum);   // 回传 exclusive 的 0-based 索引
	std::vector<std::string> GetCheatBlock(const std::vector<std::string> &lines, int lineNum);

	// blockLines 取代 [lineNum-1, BlockEndLine) 的内容。
	void ReplaceCheatBlock(std::vector<std::string> &lines, int lineNum, const std::vector<std::string> &blockLines);
	// 移除 [lineNum-1, BlockEndLine) 的内容。
	void RemoveCheatBlock(std::vector<std::string> &lines, int lineNum);

	// 在文件末尾追加；若最后一行非空，先补一个空行分隔。
	void AppendCheatBlock(std::vector<std::string> &lines, const std::vector<std::string> &blockLines);
}
```

区块定义选「到下一个 `_C`/`_S`/`_G` 之前」而不是「`_C` + 紧接的 `_L`」，理由是：
(1) 夹在中间的注释与空行也会显示在编辑器里，使用者编辑的内容与写回的内容完全一致；
(2) 不需要在 `CheatFileInfo` 额外记录 `_L` 行范围。

### 5.3 `CheatEditScreen`（新文件 `UI/CheatEditScreen.h/.cpp`）

```cpp
class CheatEditScreen : public UISimpleBaseDialogScreen {
public:
	enum class Mode { WholeFile, OneCheat, NewCheat };

	CheatEditScreen(const Path &gamePath, const Path &cheatFile, std::string_view gameID, Mode mode, int cheatLineNum = 0);

	const char *tag() const override { return "CheatEdit"; }

protected:
	void CreateDialogViews(UI::ViewGroup *parent) override;
	std::string_view GetTitle() const override;
	void onFinish(DialogResult result) override;

private:
	void LoadInitialText();
	void OnSave(UI::EventParams &);
	void OnCancel(UI::EventParams &);
	void OnDelete(UI::EventParams &);
	bool BuildNewFileText(std::string *out, std::string *error) const;
	void ShowNotice(NoticeLevel level, std::string_view text);

	Path cheatFile_;
	std::string gameID_;           // 验证时传给 CheatFileParser
	Mode mode_;
	int cheatLineNum_ = 0;         // OneCheat 模式：该笔 _C 的行号（1-based）
	std::string originalText_;     // 进入时读到的整份文件
	std::string initialEditText_;  // 进入时编辑框的内容（用来判断是否有变更）
	bool fileTrailingNewline_ = false;  // 原始文件最后是否有换行（round-trip 用）
	UI::MultilineTextEdit *edit_ = nullptr;
	UI::ViewGroup *noticeHolder_ = nullptr;
	std::string noticeText_;
	NoticeLevel noticeLevel_ = NoticeLevel::INFO;
};
```

**版面**（`UISimpleBaseDialogScreen`，`SimpleDialogFlags::Default`；状态列／返回键由 TopBar 提供）

```
[TopBar: 标题 + 返回]
[ 保存 ] [ 取消 ]                              ← 第一列，Button/Choice
[ NoticeView（仅需要时显示）]
┌──────────────────────────────────────────┐
│  1  _C0 显血                              │
│  2  _L 0x20123456 0x000000FF              │
│  3  _L 0x20123458 0x0000000A              │
│  4                                        │
└──────────────────────────────────────────┘  ← MultilineTextEdit, FILL_PARENT, weight 1
[ 删除这个金手指 ]                              ← 仅 OneCheat 模式，底部
```

- 图标沿用既有 atlas：`I_FILE_SAVE`、`I_NAVIGATE_BACK`、`I_TRASHCAN`、`I_EDIT_TEXT`。
- 标题：`WholeFile` → "Edit Cheat File"（既有字符串）；`OneCheat` → "Edit Cheat"（新字符串）；`NewCheat` → "Add Cheat"（既有代码字符串）。
- 编辑框不需要 `ContentsCanScroll`（自己管滚动）。
- 提示消息用 `NoticeView` / `NoticeLevel`（`Common/UI/Notice.h`），放在 `noticeHolder_` 容器里，平常 `SetVisibility(V_GONE)` 隐藏。
- `WholeFile` 模式进入时读整份文件；`OneCheat` 时只放该区块的文字；`NewCheat` 时放范本：

  ```
  _C0 New cheat
  _L 0x00000000 0x00000000
  ```

- 光标初始位置：0（文字开头）。

**保存流程**（`OnSave()`）

1. `edited = edit_->GetText()`。
2. `BuildNewFileText()`：
   - `WholeFile`：新内容 = `edited`。
   - `OneCheat`：`lines = CheatFileText::SplitLines(originalText_)`；`blockLines = CheatFileText::SplitLines(edited)`；`ReplaceCheatBlock(lines, cheatLineNum_, blockLines)`；新内容 = `JoinLines(lines, fileTrailingNewline_)`。
   - `NewCheat`：同上但用 `AppendCheatBlock()`。
   - 错误（见下）→ 回传 false，结束。
3. 验证：`CheatFileParser parser(cheatFile_, gameID_); parser.ParseText(newText);`
   - `GetErrors()` 非空 → `ShowNotice(WARN, "保存后有格式问题：…")`，用 `MessagePopupScreen(title, message, di->T("Yes"), di->T("No"), callback)`（既有 `Common/UI/PopupScreens.h`）询问「仍要保存？」，选择取消则不写文件。
   - `OneCheat` / `NewCheat` 额外硬性规则（不询问，直接挡下）：
     - 编辑框内容不得出现 `_S` 或 `_G` 开头的行（会破坏文件结构）。
     - `NewCheat` 的内容必须至少有一行以 `_C0`/`_C1`…`_C9` 开头。
   - `WholeFile` 模式：整份内容为空 → 允许但先警告（等同删光所有金手指）。
4. 写文件：`File::WriteStringToFile(true, newText, cheatFile_)`（文字模式，与既有 `RebuildCheatFile()` 一致；BOM 是文件内容的一部分，会原样保留）。
5. `g_Config.bReloadCheats = true;`（让 `hleCheat()` 重新解析，游戏内生效）。
6. `TriggerFinish(DR_OK)` 回到金手指列表；列表会在 53 帧内由既有 hash 检查自动重建。

**删除**（`OneCheat` 模式的「删除这个金手指」）

- `MessagePopupScreen`（既有 `Common/UI/PopupScreens.h`）确认后：`lines = SplitLines(originalText_)`；`RemoveCheatBlock(lines, cheatLineNum_)`；写文件；`bReloadCheats = true`；`TriggerFinish(DR_OK)`。
- 不经过编辑框内容（避免使用者改了文字又按删除造成混淆）。

**退出确认**

- `OnCancel()` 或返回键时，若 `edit_->GetText() != initialEditText_` → `MessagePopupScreen` 确认「放弃未保存的变更？」→ 确定才 `TriggerFinish(DR_CANCEL)`。
- `onFinish()`：`MIPSComp::jit->ClearCache()`（与 `CwCheatScreen::onFinish()` 一致），因为文件内容可能已改变。

### 5.4 入口整合（`UI/CwCheatScreen.h/.cpp`）

1. 列表列（`CreateContentViews()`）：每个 `_C` 项目由 `CheckBox` 改为一列 `LinearLayout(ORIENT_HORIZONTAL)`，内含原本的 `CheckBox`（`weight 1`）与一个 `Choice(ImageID("I_EDIT_TEXT"))`，后者 `OnClick` → `screenManager()->push(new CheatEditScreen(gamePath_, engine_->CheatFilename(), gameID_, Mode::OneCheat, fileInfo_[i].lineNum))`。
   - 该列的可点击范围与搜索过滤行为（`search_.ApplySearchFilter`）必须维持：`CheckBox` 仍带 `fileInfo_[i].name` 文字、`SettingHint`/标题/注释的处理不变。
2. 左栏（`CreateSettingsViews()`）：
   - 保留 `Import Cheats` 区块与 `Download cheat database`。
   - `#if !defined(MOBILE_DEVICE)` 的「Edit Cheat File」改为**所有平台都显示**，`OnClick` 改为 `push(new CheatEditScreen(..., Mode::WholeFile))`；移除 `OnEditCheatFile()`（外部编辑器）与其相关 include。
   - 新增「Add Cheat」按钮（原本被注释掉的那行），`OnClick` → `push(new CheatEditScreen(..., Mode::NewCheat))`；移除空的 `OnAddCheat()`。
3. 其余（`OnDisableAll`、`RebuildCheatFile`、导入、下载、hash 轮询）不动。

### 5.5 平台输入支援

**(a) 照既有架构：共用 UI 发通知，平台负责输入法**

共用 UI 已有一致的机制：控件拿到焦点时发 `System_NotifyUIEvent(UIEventNotification::TEXT_GOTFOCUS)`，各平台据此显示自己的输入法（iOS/UWP/SDL 都已实现，见 §2）。所以：

- `MultilineTextEdit::FocusChanged()` 与 `TextEdit::FocusChanged()` 做**完全相同**的事（`Common/UI/View.cpp:1260`）：`GOT_FOCUS` → `TEXT_GOTFOCUS`；`LOST_FOCUS` → `TEXT_LOSTFOCUS`。控件不呼叫任何平台 API。
- **不需要**新增 `System_ShowKeyboard()` / `System_HideKeyboard()`（它们本来就是没人调用的死代码），也不需要新增 JNI。

**(b) Android：补上缺的那一段**

今天 Android 缺两件事：`app-android.cpp` 没有 `NOTIFY_UI_EVENT` 处理；surface view 没有输入法目标（Android 版的 `UIKeyInput`）。

1. **`android/jni/app-android.cpp`：新增 `NOTIFY_UI_EVENT` 处理**（照 `ios/main.mm:556` 与 `UWP/PPSSPP_UWPMain.cpp:603` 的形状；Java 端 `PpssppActivity.java:1602-1609` 已有 `showKeyboard` / `hideKeyboard` 指令处理，不用改）：
   - `UIEventNotification::TEXT_GOTFOCUS` → `PushCommand("showKeyboard", "")`
   - `UIEventNotification::TEXT_LOSTFOCUS`、`POPUP_CLOSED`、`DIALOG_CLOSED` → `PushCommand("hideKeyboard", "")`
   - 其余事件忽略；`default: return false;` 与既有各平台一致。
2. **两个 surface view 加输入法目标**（`NativeSurfaceView.java`、`NativeGLSurfaceView.java`；GL 与 Vulkan 走不同分支，两个都要改；共用逻辑放进一个小 helper 或静态方法，避免两份代码）：
   - `setFocusable(true)`、`setFocusableInTouchMode(true)`（目前没有，所以 `showSoftInput(surfView)` 一直连不上）。
   - `onCreateInputConnection(EditorInfo outAttrs)` 回传自定义 `BaseInputConnection`（就是 Android 版的 `UIKeyInput`，对应 iOS 的 `insertText:` / `deleteBackward`）：
     - `commitText(text, newCursorPosition)` → 逐个 code point 呼叫**既有**的 `NativeApp.keyChar(NativeApp.DEVICE_ID_KEYBOARD, codePoint)`（等价于 iOS 的 `SendKeyboardChars()`，`ios/Controls.mm:427`）。换行字元照送，由控件当成插入换行处理。
     - `setComposingText(...)` / `finishComposingText()` → **刻意不插入**（中文组字过程中的拼音不该进编辑器），等 `commitText` 才插入 —— 与 iOS 行为一致。
     - `deleteSurroundingText(...)` → `NativeApp.keyDown(DEVICE_ID_KEYBOARD, KeyEvent.KEYCODE_DEL, false)` + `keyUp(...)`（等价于 iOS 的 `deleteBackward`；Android keycode 本身就是 NKCODE，`Common/Input/InputState.h:192`）。
     - `sendKeyEvent(event)` → 交给 `super`（走 View 的按键路径 → `dispatchKeyEvent` → 既有 `keyDown` / `keyChar`），回车等按键因此走既有路径。
   - `EditorInfo` 设定：`inputType = TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_MULTI_LINE | TYPE_TEXT_FLAG_NO_SUGGESTIONS`；`imeOptions = IME_FLAG_NO_ENTER_ACTION`（让回车以按键事件过来，而不是被输入法当成「完成」）。
3. **不需要**新增「粘贴/系统输入」按钮：输入法自己就能长按粘贴（走 `commitText`），中文也能直接打。

**(c) 桌面粘贴**

- Ctrl+V 直接读 `SYSPROP_CLIPBOARD_TEXT` 插入光标处（见 5.1）。桌面没有输入法问题，不需要额外工作。

**附带效果**：surface view 接上输入法目标之后，PPSSPP 其他自制输入框（搜索栏等）在 Android 上也会开始收得到输入法文字。这是预期中的改善，但本设计不主动改那些画面。

**风险与验证**：这条链路在 Android 上从没被启用过（`System_ShowKeyboard()` 本来没人调用、`NOTIFY_UI_EVENT` 没实现、surface view 没有输入法目标），必须实机验证（§7.4）。潜在副作用：surface view 变成 focusable 可能影响现有按键/焦点行为（例如输入法吃掉方向键）→ §7.4 第 6 项的回归清单。

## 6. 边界情况与已知限制

| 情况 | 处理 |
|---|---|
| 文件开头 BOM（`CreateCheatFile()` 会写入） | 视为第一行的内容原样保留；`Parse()` 只在验证时跳过 BOM，写回时不动它 |
| CRLF 文件 | 读写沿用既有文字模式（Windows 会正规化成 CRLF、Linux 保留原样）。与现有勾选功能行为相同 |
| 没有 `_L` 的 `_C` | §5.2(a) 修正后会出现在列表，可编辑/删除 |
| 多游戏 `.ini`（多个 `_S`） | 列表只显示当前 gameID 的区块；`WholeFile` 编辑器显示整个文件（其他游戏区块也看得到、可编辑）。验证沿用解析器既有行为：非当前 gameID 的 `_L` 内容不会被解析，但未知的行类型（例如 `_M`）仍会回报错误 |
| 空文件 / 全部删光 | 允许，先警告 |
| 超大文件 | 编辑框上限 1 MiB；超过时拒绝插入并提示。绘制只做可见行 |
| 行号与 `fgets` 的差异 | §5.2(b) 改成整份读入后切行，行号与编辑器一致 |
| 文件同时被外部修改 | 保存时以「进入编辑器时读到的内容」为基础重建 → 外部变更会被覆盖。这是刻意的简化（单人使用情境）；不做三方合并 |
| Hardcore 模式（`Achievements::HardcoreModeActive()`） | 不特别处理，维持现状（`Run()` 本来就会跳过） |
| 保存失败（磁盘满/权限） | `WriteStringToFile()` 失败 → `ShowNotice(ERROR, "Unable to save the cheat file")`，不关闭画面 |

## 7. 测试策略

**阶段 1（Android）的门槛**：§7.1 单元测试全绿 + §7.4 Android 实机通过。桌面手测（§7.2）属于阶段 2。

### 7.1 单元测试（阶段 1，先写，TDD）— `unittest/TestCwCheat.cpp`

新增测试函数并注册到 `unittest/UnitTest.cpp` 的 `availableTests`（名称 `CwCheat`）。构建：`./b.sh --unittest`（会 configure 出带 `-DUNITTEST=ON` 的 build 目录），执行：`build/PPSSPPUnitTest CwCheat`。

这些测试全部是纯文字/纯解析逻辑，不开窗口、不需要 GPU。

| 测试 | 验证内容 |
|---|---|
| `TestCheatParseTextValid` | `ParseText()` 对合法内容回传 true、`GetFileInfo()` 有正确的 `lineNum`/`name`/`enabled` |
| `TestCheatParseTextErrors` | 非法行（`_X`、`_L` 值不足、`_C` 缺名称）会产生错误，且错误消息含正确行号 |
| `TestCheatParseBomAndComments` | 首行 BOM 被忽略；`//`、`#`、空行不影响解析 |
| `TestCheatInfoWithoutCodeLines` | 没有 `_L` 的 `_C` 仍出现在 `GetFileInfo()`（修正前会失败） |
| `TestCheatInfoDisabledWithCodeLines` | `_C0` 带多行 `_L`：`enabled == false` 且行号正确 |
| `TestCheatParseMultiGame` | 多个 `_S` 区块只回报当前 gameID 的项目 |
| `TestCheatTextSplitJoin` | `SplitLines`/`JoinLines` 在有/无结尾换行、含 `\r`、空字符串下的往返一致 |
| `TestCheatTextBlockExtent` | `BlockEndLine` 在「中间有注释/空行」「紧接下一笔 `_C`」「碰到 `_S`/`_G`」「文件末尾」都正确 |
| `TestCheatTextReplaceRemoveAppend` | 替换/删除/追加区块后其他行完全不变 |
| `TestCheatTextRoundTrip` | 读一份含注释与多笔金手指的文件 → 抽出区块 → 原样写回 → 产生的文字与原始字节完全相同 |

### 7.2 桌面手动验证（阶段 2，Linux SDL 版）

`./b.sh --debug` → 运行 build 出来的 PPSSPP，载入任一款 PSP 游戏（或 homebrew），开暂停菜单 → 金手指：

1. 每个项目有 ✏️；点进去显示该笔文字（含注释/空行）。
2. 改名、改 `_L`、增行、删行 → 保存 → 用外部 `cat`/`diff` 检查 `.ini` 文件内容正确，其他区块未被改动。
3. 「新增金手指」→ 保存后在文件末尾出现正确区块。
4. 「删除这个金手指」→ 该区块整段消失、其他内容不变。
5. 「编辑金手指文件」→ 不做任何修改直接保存 → `diff` 显示文件完全相同。
6. 开关勾选在编辑后仍正常运作；游戏内 77 ms 内生效（例如用「金钱最大」类金手指观察）。
7. 键盘：打字、Ctrl+V 粘贴多行、Ctrl+C、Ctrl+Z、上下左右、Home/End、PageUp/Down、Enter、Backspace/Delete；光标到边界时画面自动滚动；行号栏正确。
8. 大量内容（粘贴 500 行）时画面滚动与输入不卡顿。

### 7.3 回归测试

- `build/PPSSPPUnitTest all`（依 `AGENTS.md`，于每个工作段落结束时执行）。
- `python test.py -g --graphics=software`（pspautotests）。

### 7.4 Android 验证（阶段 1，需实机或 CI）

分层验证，逐层确认（前一层不过就不用看后面）：

1. 进编辑器、点一下编辑框 → `logcat` 看到 `showKeyboard` 指令（`PpssppActivity` 收到）且输入法真的弹出来。
2. 在输入法上打英文/数字/符号 → 文字出现在自制编辑框里（此时走的是 `commitText` → `keyChar`；若这里空，问题就在 `InputConnection`）。
3. 中文：用中文输入法打「显血」这类名称，候选词 commit 后正确插入（组字过程不插入），保存后文件是正确 UTF-8。
4. 输入法内长按粘贴一段多行金手指代码 → 正确插入到光标处、换行正确。
5. 退格与回车：退格只删一个字（`deleteSurroundingText` → `KEYCODE_DEL`）、回车换行；编辑器关闭后输入法收起。
6. 回归：surface view 变成 focusable 之后，进游戏、开菜单、触屏、实体/手柄按键（含方向键是否被输入法吃掉）都正常。

## 8. 构建文件更新清单

| 新文件 | 必须加入 |
|---|---|
| `UI/CheatEditScreen.h/.cpp` | `UI/CMakeLists.txt`、`UI/UI.vcxproj`、`UI/UI.vcxproj.filters`、`UWP/UI_UWP/UI_UWP.vcxproj`、`UWP/UI_UWP/UI_UWP.vcxproj.filters`、`android/jni/Android.mk`（6 处；头文件进 CMakeLists 与两个 vcxproj 的 ClInclude） |
| `unittest/TestCwCheat.cpp` | `CMakeLists.txt`（`PPSSPPUnitTest` 来源）、`unittest/UnitTests.vcxproj`、`unittest/UnitTests.vcxproj.filters`（3 处） |

- `libretro/Makefile.common` 不含 UI/ 与 Common/UI 文件（已确认），不需要更新。
- iOS 没有 Xcode 项目文件（repo 内没有 `.pbxproj`），改用 CMake：root `CMakeLists.txt` 的 `add_subdirectory(UI)`（`NOT LIBRETRO` 时）→ `UI/CMakeLists.txt` 的 `ppsspp_ui` 静态库。所以更新 `UI/CMakeLists.txt` 就同时涵盖 iOS 与其他 CMake 目标。
- 确认方式：`grep -rn "CheatEditScreen\|TestCwCheat" --include=*.txt --include=*.mk --include=*.vcxproj --include=*.filters .` 应命中上表每一处。
- 新增行请用 Edit 工具（精确字符串替换），不要用会重写整个文件的脚本（`AGENTS.md` 第 5 条：行尾格式必须原样保留）。

## 9. 风险

| 风险 | 影响 | 对策 |
|---|---|---|
| Android 输入法链路第一次被启用 | 输入法弹不出来、`commitText` 收不到、或方向键被输入法吃掉 | 分层验证：先确认 `showKeyboard` 指令有送出且输入法弹出，再确认 `keyChar` 收到文字，最后确认编辑器插入正确；退路是不加输入法目标、只用 `keyChar`（ASCII 可用）并把问题记下来 |
| surface view 变 focusable 影响既有按键/焦点 | 游戏内操作异常 | §7.4 第 6 项的回归清单；必要时只在「UI 正在等待文字输入」时开启 focusable（对应 UWP 的 `isTextEditActive()` / `IgnoreInput()` 做法，`UWP/UWPHelpers/InputHelpers.cpp`） |
| 自制多行控件的 caret/滚动边界错误 | 编辑体验差 | 明确的 caret 不变量（永远在 UTF-8 边界、行内，且滚动会跟随）；桌面手动测试涵盖上下左右/Home/End/PageUp/Down |
| 大文件绘制性能 | 卡顿 | 只画可见行 + 行索引用二分查找，O(可见行 × 行长度) |
| 忘记把新文件加进某个构建文件 | 该平台编译不过 | §8 的 grep 确认 |
| `CheatFileParser` 修正改变既有列表行为 | 列表多出先前看不到的项目 | 视为修正；单元测试固定新行为；`RebuildCheatFile` 的 `lineNum` + `name` 校验不变 |
| 保存覆盖外部变更 | 使用者在别处的修改遗失 | 已知限制（§6）。保存前重读文件并比对 `originalText_`，不同就提示「文件已被其他程序修改」（低成本，纳入实现） |

## 10. 验收标准

**阶段 1（Android）必须满足第 1–6、8、9 项**（第 7 项的桌面键鼠属于阶段 2）。

1. 列表每笔有 ✏️；进入后显示该笔完整文字（`_C` + `_L` + 中间注释/空行）。
2. 改名/改代码行/增行/删行 → 保存后文件内容正确，其他区块字节不变。
3. 「新增金手指」在文件末尾产生合法区块；「删除这个金手指」移除整个区块。
4. `WholeFile` 模式不做修改直接保存 → 文件完全相同（round-trip）。
5. 保存后 `g_Config.bReloadCheats` 为真，游戏内生效（≤ 77 ms）。
6. Android：能用手机输入法直接打字（含中文）、能粘贴多行代码，退格/回车正确，编辑器关闭后输入法收起。
7. （阶段 2）桌面键盘与鼠标操作、滚动全部正常；行号栏与实际行号一致。
8. `build/PPSSPPUnitTest all` 全绿、`python test.py -g --graphics=software` 为 `0 tests failed`。
9. 构建成功：阶段 1 = Android（`./gradlew :android:assembleDebug`，或 fork 的 `manual_generate_apk.yml` CI 出 APK）；阶段 2 = `./b.sh --debug`（Linux）。新文件已加入 §8 所有构建清单。

**本机能力说明**：这台机器可以自己编 Linux 桌面版并跑起来验证阶段 2；Android 只能靠 `gradlew` 下载 SDK/NDK（数百 MB 到 GB，当前网络较慢）或由 fork 的 CI 出 APK，实机安装与测试需要使用者配合。

## 11. 后续工作（不在本次范围）

- 翻译：功能完成、英文文案定稿后，另开一个 commit，用 `Tools/langtool` 加入 `zh_CN`/`zh_TW` 等语言（依 `AGENTS.md`，动手前先让使用者确认英文措辞）。
- 若 `MultilineTextEdit` 之后要被别处重用，再考虑搬到独立文件。
- 文字选取、多层撤销、查找/替换。
