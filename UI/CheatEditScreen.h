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

// A plain text editor for a game's cheat file: the whole .ini, nothing else. Opened from the
// cheat screen, saved back over the file, and picked up by the game on the next cheat refresh.
class CheatEditScreen : public UIBaseDialogScreen {
public:
	CheatEditScreen(const Path &gamePath, const Path &cheatFile, std::string_view gameID);

	const char *tag() const override { return "CheatEdit"; }

protected:
	void CreateViews() override;
	bool key(const KeyInput &key) override;
	void onFinish(DialogResult result) override;

private:
	void OnSave(UI::EventParams &params);
	void OnCancel(UI::EventParams &params);

	// Writes newText to disk and closes the screen. Reports a failure in place instead.
	void SaveText(const std::string &newText);
	void ShowNotice(NoticeLevel level, std::string_view text);
	void ConfirmDiscard();

	const Path cheatFile_;
	const std::string gameID_;

	// What was on disk when this screen opened, and what the editor holds right now.
	std::string fileText_;
	std::string currentText_;

	UI::MultilineTextEdit *edit_ = nullptr;
	std::string noticeText_;
	NoticeLevel noticeLevel_ = NoticeLevel::INFO;
};
