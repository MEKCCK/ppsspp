// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <string>

#include "Common/UI/Root.h"
#include "Common/UI/View.h"

#include "UnitTest.h"

// The caret and line arithmetic behind MultilineTextEdit, which is the part that is easy to
// get wrong and impossible to eyeball reliably in the emulator.
using UI::MultilineTextBuffer;

static MultilineTextBuffer BufferWith(std::string_view text) {
	MultilineTextBuffer buffer;
	buffer.SetText(std::string(text));
	return buffer;
}

static bool TestBufferLineIndex() {
	MultilineTextBuffer b = BufferWith("a\nbb\nccc");
	EXPECT_EQ_INT(b.LineCount(), 3);
	EXPECT_EQ_INT(b.LineStart(0), 0);
	EXPECT_EQ_INT(b.LineEnd(0), 1);
	EXPECT_EQ_INT(b.LineStart(1), 2);
	EXPECT_EQ_INT(b.LineEnd(1), 4);
	EXPECT_EQ_INT(b.LineStart(2), 5);
	EXPECT_EQ_INT(b.LineEnd(2), 8);

	// A trailing newline still leaves an empty line for the caret to sit on.
	b.SetText("a\n");
	EXPECT_EQ_INT(b.LineCount(), 2);
	EXPECT_EQ_INT(b.LineEnd(0), 1);
	EXPECT_EQ_INT(b.LineStart(1), 2);
	EXPECT_EQ_INT(b.LineEnd(1), 2);

	// Empty text is a single empty line, never zero lines.
	b.SetText("");
	EXPECT_EQ_INT(b.LineCount(), 1);
	EXPECT_EQ_INT(b.LineStart(0), 0);
	EXPECT_EQ_INT(b.LineEnd(0), 0);

	// Out of range lines clamp instead of reading out of bounds.
	b.SetText("xy");
	EXPECT_EQ_INT(b.LineStart(-1), 0);
	EXPECT_EQ_INT(b.LineEnd(-1), 0);
	EXPECT_EQ_INT(b.LineStart(5), 2);
	EXPECT_EQ_INT(b.LineEnd(5), 2);
	return true;
}

static bool TestBufferCaretUtf8() {
	MultilineTextBuffer b = BufferWith("\xE6\x98\xBE\xE8\xA1\x80");  // "显血", 3 bytes each.
	EXPECT_EQ_INT((int)b.Text().size(), 6);
	b.SetCaret(0);
	b.MoveRight();
	EXPECT_EQ_INT(b.Caret(), 3);
	b.MoveRight();
	EXPECT_EQ_INT(b.Caret(), 6);
	b.MoveRight();
	EXPECT_EQ_INT(b.Caret(), 6);
	b.MoveLeft();
	EXPECT_EQ_INT(b.Caret(), 3);
	b.MoveLeft();
	EXPECT_EQ_INT(b.Caret(), 0);
	b.MoveLeft();
	EXPECT_EQ_INT(b.Caret(), 0);

	// A caret landing inside a multi-byte character snaps back to its start.
	b.SetCaret(1);
	EXPECT_EQ_INT(b.Caret(), 0);
	b.SetCaret(4);
	EXPECT_EQ_INT(b.Caret(), 3);
	b.SetCaret(5);
	EXPECT_EQ_INT(b.Caret(), 3);
	b.SetCaret(99);
	EXPECT_EQ_INT(b.Caret(), 6);
	b.SetCaret(-3);
	EXPECT_EQ_INT(b.Caret(), 0);
	return true;
}

static bool TestBufferUpDownKeepsColumn() {
	MultilineTextBuffer b = BufferWith("abcdef\nx\nabcdef");
	b.SetCaret(6);  // End of the first line, column 6.
	b.MoveDown();
	EXPECT_EQ_INT(b.Caret(), 8);  // "x" is shorter, so the caret sits at its end.
	b.MoveDown();
	EXPECT_EQ_INT(b.Caret(), 15);  // Column 6 again, not column 1.
	b.MoveUp();
	EXPECT_EQ_INT(b.Caret(), 8);

	// A horizontal move forgets the remembered column.
	b.MoveDown();
	b.MoveLeft();
	EXPECT_EQ_INT(b.Caret(), 14);
	b.MoveUp();
	EXPECT_EQ_INT(b.Caret(), 8);  // Column 5 now.

	// Up on the first line and down on the last do nothing.
	b.SetCaret(0);
	b.MoveUp();
	EXPECT_EQ_INT(b.Caret(), 0);
	b.SetCaret((int)b.Text().size());
	b.MoveDown();
	EXPECT_EQ_INT(b.Caret(), (int)b.Text().size());
	return true;
}

static bool TestBufferHomeEndAndPages() {
	MultilineTextBuffer b = BufferWith("abc\ndef\nghi\njkl");
	b.SetCaret(0);
	b.MoveLineEnd();
	EXPECT_EQ_INT(b.Caret(), 3);
	b.MoveLineStart();
	EXPECT_EQ_INT(b.Caret(), 0);

	b.MoveDown();
	EXPECT_EQ_INT(b.Caret(), 4);
	b.MoveLineEnd();
	EXPECT_EQ_INT(b.Caret(), 7);
	EXPECT_EQ_INT(b.LineOfCaret(), 1);
	EXPECT_EQ_INT(b.ColumnOfCaret(), 3);

	// Pages move by whole lines and keep the column.
	b.MovePageDown(2);
	EXPECT_EQ_INT(b.LineOfCaret(), 3);
	EXPECT_EQ_INT(b.ColumnOfCaret(), 3);
	b.MovePageDown(5);
	EXPECT_EQ_INT(b.LineOfCaret(), 3);
	b.MovePageUp(5);
	EXPECT_EQ_INT(b.LineOfCaret(), 0);
	EXPECT_EQ_INT(b.ColumnOfCaret(), 3);

	// Page moves of zero lines are ignored rather than jumping.
	b.MovePageDown(0);
	EXPECT_EQ_INT(b.LineOfCaret(), 0);
	return true;
}

static bool TestBufferEditAndMaxLen() {
	MultilineTextBuffer b = BufferWith("abc\ndef");
	b.SetCaret(3);
	EXPECT_TRUE(b.Insert("X"));
	EXPECT_EQ_STR(b.Text(), std::string("abcX\ndef"));
	EXPECT_EQ_INT(b.Caret(), 4);
	EXPECT_EQ_INT(b.LineCount(), 2);

	EXPECT_TRUE(b.Backspace());
	EXPECT_EQ_STR(b.Text(), std::string("abc\ndef"));
	EXPECT_EQ_INT(b.Caret(), 3);

	// Deleting forward at the end of a line joins it with the next one.
	EXPECT_TRUE(b.DeleteForward());
	EXPECT_EQ_STR(b.Text(), std::string("abcdef"));
	EXPECT_EQ_INT(b.LineCount(), 1);
	EXPECT_EQ_INT(b.Caret(), 3);

	// Backspace at the start and delete at the end do nothing.
	b.SetCaret(0);
	EXPECT_FALSE(b.Backspace());
	b.SetCaret((int)b.Text().size());
	EXPECT_FALSE(b.DeleteForward());
	EXPECT_FALSE(b.Insert(""));

	// Once the limit would be exceeded nothing is inserted at all.
	b.SetMaxLen(b.Text().size() + 2);
	EXPECT_TRUE(b.Insert("xy"));
	EXPECT_FALSE(b.Insert("z"));
	EXPECT_EQ_STR(b.Text(), std::string("abcdefxy"));

	// Inserting a newline splits the line (need room again after hitting the limit above).
	b.SetMaxLen(16);
	b.SetCaret(3);
	EXPECT_TRUE(b.Insert("\n"));
	EXPECT_EQ_STR(b.Text(), std::string("abc\ndefxy"));
	EXPECT_EQ_INT(b.LineCount(), 2);
	EXPECT_EQ_INT(b.LineOfCaret(), 1);
	EXPECT_EQ_INT(b.ColumnOfCaret(), 0);
	return true;
}

static bool TestBufferColumnsWithTabs() {
	MultilineTextBuffer b = BufferWith("\tx\ny");
	EXPECT_EQ_INT(b.OffsetForColumn(0, 0), 0);
	EXPECT_EQ_INT(b.OffsetForColumn(0, 1), 0);  // Inside the tab.
	EXPECT_EQ_INT(b.OffsetForColumn(0, 4), 1);  // Right after the tab.
	EXPECT_EQ_INT(b.OffsetForColumn(0, 5), 2);  // End of the line.
	EXPECT_EQ_INT(b.OffsetForColumn(0, 9), 2);  // Past the end clamps.

	b.SetCaret(1);
	EXPECT_EQ_INT(b.ColumnOfCaret(), 4);
	b.SetCaret(2);
	EXPECT_EQ_INT(b.ColumnOfCaret(), 5);

	// Moving down keeps the display column, landing at the end of "y".
	b.SetCaret(1);
	b.MoveDown();
	EXPECT_EQ_INT(b.Caret(), 4);
	return true;
}

// --- MultilineTextEdit: the key and mouse handling ------------------------------------------

// Focuses a view for the duration of a test and clears the global focus afterwards, so a dead
// stack object never stays registered as the focused view.
struct ScopedFocus {
	explicit ScopedFocus(UI::View *view) { UI::SetFocusedView(view, UI::FocusFlags::CAUSE_FORCED, true); }
	~ScopedFocus() { UI::SetFocusedView(nullptr, UI::FocusFlags::CAUSE_OTHER); }
};

static KeyInput CharKey(int unicodeChar) {
	KeyInput key{};
	key.deviceId = DEVICE_ID_KEYBOARD;
	key.flags = KeyInputFlags::CHAR;
	key.unicodeChar = unicodeChar;
	return key;
}

static KeyInput DownKey(InputKeyCode code, KeyInputFlags extra = KeyInputFlags{}) {
	KeyInput key{};
	key.deviceId = DEVICE_ID_KEYBOARD;
	key.flags = KeyInputFlags::DOWN | extra;
	key.keyCode = code;
	return key;
}

static bool TestEditTypingAndEnter() {
	UI::MultilineTextEdit edit("", "");
	ScopedFocus focus(&edit);
	EXPECT_TRUE(edit.HasFocus());

	EXPECT_TRUE(edit.Key(CharKey('a')));
	EXPECT_TRUE(edit.Key(CharKey('b')));
	EXPECT_EQ_STR(edit.GetText(), std::string("ab"));

	// Enter starts a new line.
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_ENTER)));
	EXPECT_EQ_STR(edit.GetText(), std::string("ab\n"));
	EXPECT_EQ_INT(edit.Buffer().LineCount(), 2);
	EXPECT_EQ_INT(edit.Buffer().LineOfCaret(), 1);

	// Android's IME sends newlines as characters, which has to work too.
	EXPECT_TRUE(edit.Key(CharKey('\n')));
	EXPECT_EQ_STR(edit.GetText(), std::string("ab\n\n"));
	EXPECT_EQ_INT(edit.Buffer().LineCount(), 3);

	// Multi-byte characters go in whole.
	EXPECT_TRUE(edit.Key(CharKey(0x663E)));  // U+663E
	EXPECT_EQ_STR(edit.GetText(), std::string("ab\n\n\xE6\x98\xBE"));
	EXPECT_EQ_INT(edit.Buffer().Caret(), (int)edit.GetText().size());

	// Other control characters are ignored.
	EXPECT_TRUE(edit.Key(CharKey(0x01)));
	EXPECT_EQ_STR(edit.GetText(), std::string("ab\n\n\xE6\x98\xBE"));

	// A widget without focus ignores keys.
	UI::MultilineTextEdit unfocused("abc", "");
	EXPECT_FALSE(unfocused.Key(CharKey('x')));
	EXPECT_EQ_STR(unfocused.GetText(), std::string("abc"));
	return true;
}

static bool TestEditBackspaceDeleteAndArrows() {
	UI::MultilineTextEdit edit("abc\ndef", "");
	ScopedFocus focus(&edit);
	edit.Buffer().SetCaret(0);

	// Backspace at the start does nothing.
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_DEL)));
	EXPECT_EQ_STR(edit.GetText(), std::string("abc\ndef"));

	// Right, then backspace removes the character behind the caret.
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_DPAD_RIGHT)));
	EXPECT_EQ_INT(edit.Buffer().Caret(), 1);
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_DEL)));
	EXPECT_EQ_STR(edit.GetText(), std::string("bc\ndef"));
	EXPECT_EQ_INT(edit.Buffer().Caret(), 0);

	// Forward delete removes what is under the caret.
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_FORWARD_DEL)));
	EXPECT_EQ_STR(edit.GetText(), std::string("c\ndef"));

	// Down to the second line, then End and Home.
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_DPAD_DOWN)));
	EXPECT_EQ_INT(edit.Buffer().LineOfCaret(), 1);
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_MOVE_END)));
	EXPECT_EQ_INT(edit.Buffer().ColumnOfCaret(), 3);
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_MOVE_HOME)));
	EXPECT_EQ_INT(edit.Buffer().ColumnOfCaret(), 0);

	// Back and Escape are left to the screen.
	EXPECT_FALSE(edit.Key(DownKey(NKCODE_BACK)));
	EXPECT_FALSE(edit.Key(DownKey(NKCODE_ESCAPE)));
	return true;
}

static bool TestEditUndoAndClipboard() {
	UI::MultilineTextEdit edit("original", "");
	ScopedFocus focus(&edit);
	edit.Buffer().SetCaret((int)edit.GetText().size());

	EXPECT_TRUE(edit.Key(CharKey('!')));
	EXPECT_EQ_STR(edit.GetText(), std::string("original!"));

	// Ctrl+Z restores it (one level, like TextEdit).
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_Z, KeyInputFlags::ModCtrl)));
	EXPECT_EQ_STR(edit.GetText(), std::string("original"));

	// The clipboard is empty in the unit test build, so Ctrl+V must change nothing. Ctrl+C is
	// accepted and must not disturb the text either.
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_V, KeyInputFlags::ModCtrl)));
	EXPECT_EQ_STR(edit.GetText(), std::string("original"));
	EXPECT_TRUE(edit.Key(DownKey(NKCODE_C, KeyInputFlags::ModCtrl)));
	EXPECT_EQ_STR(edit.GetText(), std::string("original"));
	return true;
}

static bool TestEditMaxLenAndSetText() {
	UI::MultilineTextEdit edit("abc", "");
	ScopedFocus focus(&edit);
	edit.Buffer().SetCaret(3);
	edit.SetMaxLen(4);

	EXPECT_TRUE(edit.Key(CharKey('d')));
	EXPECT_EQ_STR(edit.GetText(), std::string("abcd"));

	// Over the limit: nothing is inserted at all.
	EXPECT_TRUE(edit.Key(CharKey('e')));
	EXPECT_EQ_STR(edit.GetText(), std::string("abcd"));

	// SetText replaces everything and puts the caret and the scroll position back at the start.
	edit.SetText("xyz\n123");
	EXPECT_EQ_STR(edit.GetText(), std::string("xyz\n123"));
	EXPECT_EQ_INT(edit.Buffer().Caret(), 0);
	EXPECT_EQ_INT(edit.FirstVisibleLine(), 0);
	return true;
}

static KeyInput WheelKey(InputKeyCode code) {
	KeyInput key{};
	key.deviceId = DEVICE_ID_KEYBOARD;
	// The amount lives in the upper bits of the flags, which is how both MainWindow and
	// ScrollView pass it around.
	key.flags = KeyInputFlags::DOWN | KeyInputFlags::HAS_WHEEL_DELTA | (KeyInputFlags)(120 << 16);
	key.keyCode = code;
	return key;
}

static bool TestEditWheelScrollsWithoutMovingTheCaret() {
	UI::MultilineTextEdit edit("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15", "");
	ScopedFocus focus(&edit);

	EXPECT_EQ_INT(edit.FirstVisibleLine(), 0);
	EXPECT_TRUE(edit.Key(WheelKey(NKCODE_EXT_MOUSEWHEEL_DOWN)));
	// One notch is 120, which is three lines here.
	EXPECT_EQ_INT(edit.FirstVisibleLine(), 3);
	// Scrolling must not move the caret.
	EXPECT_EQ_INT(edit.Buffer().Caret(), 0);

	EXPECT_TRUE(edit.Key(WheelKey(NKCODE_EXT_MOUSEWHEEL_UP)));
	EXPECT_EQ_INT(edit.FirstVisibleLine(), 0);

	// It cannot scroll past the last or the first line.
	for (int i = 0; i < 20; ++i) {
		edit.Key(WheelKey(NKCODE_EXT_MOUSEWHEEL_UP));
	}
	EXPECT_EQ_INT(edit.FirstVisibleLine(), 0);
	for (int i = 0; i < 20; ++i) {
		edit.Key(WheelKey(NKCODE_EXT_MOUSEWHEEL_DOWN));
	}
	EXPECT_EQ_INT(edit.FirstVisibleLine(), 5);  // 15 lines minus the 10 visible ones.
	return true;
}

static bool TestEditDragScrollsOnlyForTouch() {
	UI::MultilineTextEdit edit("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15", "");
	ScopedFocus focus(&edit);
	edit.Move(Bounds(0.0f, 0.0f, 200.0f, 100.0f));

	TouchInput down{};
	down.x = 100.0f;
	down.y = 90.0f;
	down.flags = TouchInputFlags::DOWN;
	EXPECT_TRUE(edit.Touch(down));

	// Dragging a finger up scrolls the view down.
	TouchInput drag{};
	drag.x = 100.0f;
	drag.y = 30.0f;
	drag.flags = TouchInputFlags::MOVE;
	EXPECT_TRUE(edit.Touch(drag));
	EXPECT_TRUE(edit.FirstVisibleLine() > 0);
	EXPECT_EQ_INT(edit.Buffer().Caret(), 0);

	TouchInput up{};
	up.x = 100.0f;
	up.y = 30.0f;
	up.flags = TouchInputFlags::UP;
	EXPECT_TRUE(edit.Touch(up));

	// With a mouse the same gesture must leave the view alone: there is no selection yet, so
	// moving the text under the cursor would just be surprising.
	UI::MultilineTextEdit mouseEdit("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15", "");
	ScopedFocus mouseFocus(&mouseEdit);
	mouseEdit.Move(Bounds(0.0f, 0.0f, 200.0f, 100.0f));
	down.flags = TouchInputFlags::DOWN | TouchInputFlags::MOUSE;
	EXPECT_TRUE(mouseEdit.Touch(down));
	drag.flags = TouchInputFlags::MOVE | TouchInputFlags::MOUSE;
	EXPECT_FALSE(mouseEdit.Touch(drag));
	EXPECT_EQ_INT(mouseEdit.FirstVisibleLine(), 0);
	return true;
}

bool TestMultilineTextBuffer() {
	if (!TestBufferLineIndex())
		return false;
	if (!TestBufferCaretUtf8())
		return false;
	if (!TestBufferUpDownKeepsColumn())
		return false;
	if (!TestBufferHomeEndAndPages())
		return false;
	if (!TestBufferEditAndMaxLen())
		return false;
	if (!TestBufferColumnsWithTabs())
		return false;
	return true;
}

bool TestMultilineTextEdit() {
	if (!TestEditTypingAndEnter())
		return false;
	if (!TestEditBackspaceDeleteAndArrows())
		return false;
	if (!TestEditUndoAndClipboard())
		return false;
	if (!TestEditMaxLenAndSetText())
		return false;
	if (!TestEditWheelScrollsWithoutMovingTheCaret())
		return false;
	if (!TestEditDragScrollsOnlyForTouch())
		return false;
	return true;
}
