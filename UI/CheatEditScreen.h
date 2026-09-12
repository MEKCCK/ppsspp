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

#pragma once

#include <string>

#include "Common/File/Path.h"
#include "Common/UI/Notice.h"
#include "Common/UI/View.h"
#include "UI/BaseScreens.h"

namespace UI {
class ViewGroup;
}

// Edits a cheat file as text, either the whole file or just the block of lines that makes up
// one cheat. Saving patches the edited text back into the rest of the file, so everything the
// editor does not show (other cheats, comments, _S/_G lines) is preserved byte for byte.
class CheatEditScreen : public UIBaseDialogScreen {
public:
	enum class Mode {
		WholeFile,  // The entire .ini.
		OneCheat,   // The block for one cheat, from its _C line to the next cheat or _S/_G.
		NewCheat,   // A new block, appended at the end of the file.
	};

	CheatEditScreen(const Path &gamePath, const Path &cheatFile, std::string_view gameID, Mode mode, int cheatLineNum = 0);

	const char *tag() const override { return "CheatEdit"; }

protected:
	void CreateViews() override;
	bool key(const KeyInput &key) override;
	void onFinish(DialogResult result) override;

private:
	void LoadInitialText();
	std::string_view Title() const;

	void OnSave(UI::EventParams &params);
	void OnCancel(UI::EventParams &params);
	void OnDelete(UI::EventParams &params);

	// Writes newText to disk and closes the screen. Reports a failure in place instead.
	void SaveText(const std::string &newText);

	// Turns the edited text into the full new file contents, or explains why it cannot.
	bool BuildNewFileText(std::string *out, std::string *error) const;
	void ShowNotice(NoticeLevel level, std::string_view text);
	void ConfirmDiscard();

	const Path cheatFile_;
	const std::string gameID_;
	const Mode mode_;
	const int cheatLineNum_;

	// The file as it was when this screen opened; the edit is applied on top of this.
	std::string originalText_;
	bool fileTrailingNewline_ = false;

	// Kept up to date so rebuilding the views (a notice, a rotation) does not lose edits.
	std::string currentText_;
	// What the editor started with, to tell whether anything was changed.
	std::string initialEditText_;

	UI::MultilineTextEdit *edit_ = nullptr;
	std::string noticeText_;
	NoticeLevel noticeLevel_ = NoticeLevel::INFO;
};
