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

#include "ppsspp_config.h"

#include "Common/Data/Text/I18n.h"
#include "Common/File/FileUtil.h"
#include "Common/StringUtils.h"
#include "Common/UI/Notice.h"
#include "Common/UI/PopupScreens.h"
#include "Common/UI/ScreenManager.h"
#include "Common/UI/ViewGroup.h"
#include "Core/Config.h"
#include "Core/CwCheat.h"
#include "Core/MIPS/JitCommon/JitCommon.h"
#include "UI/CheatEditScreen.h"
#include "UI/MiscViews.h"

using namespace UI;

CheatEditScreen::CheatEditScreen(const Path &gamePath, const Path &cheatFile, std::string_view gameID, Mode mode, int cheatLineNum)
	: UIBaseDialogScreen(gamePath), cheatFile_(cheatFile), gameID_(gameID), mode_(mode), cheatLineNum_(cheatLineNum) {
	LoadInitialText();
}

void CheatEditScreen::LoadInitialText() {
	std::string text;
	if (File::ReadTextFileToString(cheatFile_, &text)) {
		originalText_ = text;
	}
	fileTrailingNewline_ = CheatFileText::EndsWithNewline(originalText_);

	switch (mode_) {
	case Mode::WholeFile:
		currentText_ = originalText_;
		break;
	case Mode::OneCheat:
	{
		const std::vector<std::string> lines = CheatFileText::SplitLines(originalText_);
		currentText_ = CheatFileText::JoinLines(CheatFileText::GetCheatBlock(lines, cheatLineNum_), true);
		break;
	}
	case Mode::NewCheat:
	default:
		// A starting point that the parser will accept as-is.
		currentText_ = "_C0 New cheat\n_L 0x00000000 0x00000000\n";
		break;
	}
	initialEditText_ = currentText_;
}

std::string_view CheatEditScreen::Title() const {
	auto cw = GetI18NCategory(I18NCat::CWCHEATS);
	switch (mode_) {
	case Mode::WholeFile:
		return cw->T("Edit Cheat File");
	case Mode::NewCheat:
		return cw->T("Add Cheat");
	case Mode::OneCheat:
	default:
		return cw->T("Edit Cheat");
	}
}

void CheatEditScreen::CreateViews() {
	auto di = GetI18NCategory(I18NCat::DIALOG);
	auto cw = GetI18NCategory(I18NCat::CWCHEATS);

	const bool portrait = GetDeviceOrientation() == DeviceOrientation::Portrait;

	root_ = new LinearLayout(ORIENT_VERTICAL, new LayoutParams(FILL_PARENT, FILL_PARENT));
	root_->SetSpacing(0.0f);

	// Deliberately no back button: Cancel is the way out, and it asks before throwing edits
	// away. The system back key goes through key() and does the same.
	TopBarFlags topBarFlags = portrait ? TopBarFlags::Portrait : TopBarFlags::Default;
	topBarFlags |= TopBarFlags::NoBackButton;
	root_->Add(new TopBar(*screenManager()->getUIContext(), topBarFlags, Title()));

	LinearLayout *buttons = root_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT, Margins(8, 8, 8, 4))));
	buttons->SetSpacing(8.0f);
	buttons->Add(new Button(di->T("Save"), ImageID("I_FILE_SAVE"), new LinearLayoutParams(1.0f)))->OnClick.Handle(this, &CheatEditScreen::OnSave);
	buttons->Add(new Button(di->T("Cancel"), new LinearLayoutParams(1.0f)))->OnClick.Handle(this, &CheatEditScreen::OnCancel);

	if (!noticeText_.empty()) {
		root_->Add(new NoticeView(noticeLevel_, noticeText_, "", new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT, Margins(8, 0, 8, 4))));
	}

	edit_ = root_->Add(new MultilineTextEdit(currentText_, "", new LinearLayoutParams(FILL_PARENT, FILL_PARENT, 1.0f, Margins(8, 0, 8, 8))));
	edit_->OnTextChange.Add([this](UI::EventParams &) {
		if (edit_) {
			currentText_ = edit_->GetText();
		}
	});

	if (mode_ == Mode::OneCheat) {
		root_->Add(new Choice(cw->T("Delete this cheat"), ImageID("I_TRASHCAN"), new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT, Margins(8, 0, 8, 8))))->OnClick.Handle(this, &CheatEditScreen::OnDelete);
	}
}

bool CheatEditScreen::key(const KeyInput &key) {
	if ((key.flags & KeyInputFlags::DOWN) && (key.keyCode == NKCODE_BACK || key.keyCode == NKCODE_ESCAPE)) {
		ConfirmDiscard();
		return true;
	}
	return UIBaseDialogScreen::key(key);
}

void CheatEditScreen::onFinish(DialogResult result) {
	// Cheats write straight into emulated memory, so drop translated code on the way out.
	if (MIPSComp::jit) {
		MIPSComp::jit->ClearCache();
	}
}

void CheatEditScreen::ShowNotice(NoticeLevel level, std::string_view text) {
	noticeLevel_ = level;
	noticeText_ = text;
	// Rebuilding the views drops the notice; currentText_ keeps the edits.
	currentText_ = edit_ ? edit_->GetText() : currentText_;
	RecreateViews();
}

bool CheatEditScreen::BuildNewFileText(std::string *out, std::string *error) const {
	const std::string edited = edit_ ? edit_->GetText() : currentText_;

	if (mode_ == Mode::WholeFile) {
		*out = edited;
		return true;
	}

	// A single cheat may not restructure the file, so _S and _G are not allowed here.
	const std::vector<std::string> editedLines = CheatFileText::SplitLines(edited);
	for (const std::string &line : editedLines) {
		if (CheatFileText::IsDirectiveLine(line, 'S') || CheatFileText::IsDirectiveLine(line, 'G')) {
			*error = "A cheat cannot contain _S or _G lines. Edit the whole cheat file for that.";
			return false;
		}
	}

	if (mode_ == Mode::NewCheat) {
		bool hasNameLine = false;
		for (const std::string &line : editedLines) {
			if (CheatFileText::IsCheatNameLine(line)) {
				hasNameLine = true;
				break;
			}
		}
		if (!hasNameLine) {
			*error = "A cheat needs a name line such as \"_C0 Infinite health\".";
			return false;
		}
	}

	std::vector<std::string> lines = CheatFileText::SplitLines(originalText_);
	if (mode_ == Mode::OneCheat) {
		if (!CheatFileText::ReplaceCheatBlock(lines, cheatLineNum_, editedLines)) {
			*error = "The cheat is no longer where it was. Reopen the cheat file and try again.";
			return false;
		}
	} else {
		CheatFileText::AppendCheatBlock(lines, editedLines);
	}
	*out = CheatFileText::JoinLines(lines, fileTrailingNewline_);
	return true;
}

void CheatEditScreen::OnSave(UI::EventParams &params) {
	std::string newText;
	std::string error;
	if (!BuildNewFileText(&newText, &error)) {
		ShowNotice(NoticeLevel::ERROR, error);
		return;
	}

	// Check the result with the same parser the game uses.
	CheatFileParser parser(cheatFile_, gameID_);
	parser.ParseText(newText);
	const std::vector<std::string> &errors = parser.GetErrors();
	if (!errors.empty()) {
		// The file may well have had problems before this edit, so offer to save anyway.
		std::string message = "The cheat file has problems after this edit:";
		for (size_t i = 0; i < errors.size() && i < 3; ++i) {
			message += "\n";
			message += errors[i];
		}
		if (errors.size() > 3) {
			message += StringFromFormat("\n(+%d more)", (int)errors.size() - 3);
		}
		auto di = GetI18NCategory(I18NCat::DIALOG);
		screenManager()->push(new MessagePopupScreen(di->T("Save anyway?"), message, di->T("Save"), di->T("Cancel"), [this, newText](bool result) {
			if (result) {
				SaveText(newText);
			}
		}));
		return;
	}

	SaveText(newText);
}

void CheatEditScreen::SaveText(const std::string &newText) {
	// Text mode, like the enable/disable toggle in the cheat list has always used.
	if (!File::WriteStringToFile(true, newText, cheatFile_)) {
		ShowNotice(NoticeLevel::ERROR, "Unable to save the cheat file.");
		return;
	}
	// Makes hleCheat() reparse the file, applying the change within one refresh interval.
	g_Config.bReloadCheats = true;
	TriggerFinish(DR_OK);
}

void CheatEditScreen::OnCancel(UI::EventParams &params) {
	ConfirmDiscard();
}

void CheatEditScreen::ConfirmDiscard() {
	const std::string edited = edit_ ? edit_->GetText() : currentText_;
	if (edited == initialEditText_) {
		TriggerFinish(DR_CANCEL);
		return;
	}

	auto di = GetI18NCategory(I18NCat::DIALOG);
	screenManager()->push(new MessagePopupScreen(di->T("Discard changes?"), di->T("Your edits will be lost."), di->T("Discard"), di->T("Cancel"), [this](bool result) {
		if (result) {
			TriggerFinish(DR_CANCEL);
		}
	}));
}

void CheatEditScreen::OnDelete(UI::EventParams &params) {
	auto cw = GetI18NCategory(I18NCat::CWCHEATS);
	auto di = GetI18NCategory(I18NCat::DIALOG);
	screenManager()->push(new MessagePopupScreen(cw->T("Delete this cheat"), cw->T("The cheat will be removed from the cheat file."), di->T("Delete"), di->T("Cancel"), [this](bool result) {
		if (!result) {
			return;
		}
		// Delete uses the file as it was on disk, not the edited text: the button is about
		// removing the cheat, and mixing it with unsaved edits would be confusing.
		std::vector<std::string> lines = CheatFileText::SplitLines(originalText_);
		if (!CheatFileText::RemoveCheatBlock(lines, cheatLineNum_)) {
			ShowNotice(NoticeLevel::ERROR, "The cheat is no longer where it was. Reopen the cheat file and try again.");
			return;
		}
		SaveText(CheatFileText::JoinLines(lines, fileTrailingNewline_));
	}));
}
