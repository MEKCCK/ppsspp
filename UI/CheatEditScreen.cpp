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
#include "Common/UI/Root.h"
#include "Common/UI/ScreenManager.h"
#include "Common/UI/ViewGroup.h"
#include "Core/Config.h"
#include "Core/CwCheat.h"
#include "Core/MIPS/JitCommon/JitCommon.h"
#include "UI/CheatEditScreen.h"
#include "UI/MiscViews.h"

using namespace UI;

CheatEditScreen::CheatEditScreen(const Path &gamePath, const Path &cheatFile, std::string_view gameID)
	: UIBaseDialogScreen(gamePath), cheatFile_(cheatFile), gameID_(gameID) {
	std::string text;
	if (File::ReadTextFileToString(cheatFile_, &text)) {
		fileText_ = text;
	}
	currentText_ = fileText_;
}

void CheatEditScreen::CreateViews() {
	auto di = GetI18NCategory(I18NCat::DIALOG);
	auto cw = GetI18NCategory(I18NCat::CWCHEATS);

	const bool portrait = GetDeviceOrientation() == DeviceOrientation::Portrait;

	// SetSpacing lives on LinearLayout, not ViewGroup, so keep the concrete type around.
	LinearLayout *root = new LinearLayout(ORIENT_VERTICAL, new LayoutParams(FILL_PARENT, FILL_PARENT));
	root->SetSpacing(0.0f);
	root_ = root;

	// Deliberately no back button: Cancel is the way out, and it asks before throwing edits
	// away. The system back key goes through key() and does the same.
	TopBarFlags topBarFlags = portrait ? TopBarFlags::Portrait : TopBarFlags::Default;
	topBarFlags |= TopBarFlags::NoBackButton;
	root->Add(new TopBar(*screenManager()->getUIContext(), topBarFlags, cw->T("Edit Cheat File")));

	LinearLayout *buttons = root->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT, Margins(8, 8, 8, 4))));
	buttons->SetSpacing(8.0f);
	buttons->Add(new Button(di->T("Save"), ImageID("I_FILE_SAVE"), new LinearLayoutParams(1.0f)))->OnClick.Handle(this, &CheatEditScreen::OnSave);
	buttons->Add(new Button(di->T("Cancel"), new LinearLayoutParams(1.0f)))->OnClick.Handle(this, &CheatEditScreen::OnCancel);

	if (!noticeText_.empty()) {
		root->Add(new NoticeView(noticeLevel_, noticeText_, "", new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT, Margins(8, 0, 8, 4))));
	}

	edit_ = root->Add(new MultilineTextEdit(currentText_, "", new LinearLayoutParams(FILL_PARENT, FILL_PARENT, 1.0f, Margins(8, 0, 8, 8))));
	edit_->OnTextChange.Add([this](UI::EventParams &) {
		if (edit_) {
			currentText_ = edit_->GetText();
		}
	});

	// Focus the editor right away: this screen exists to edit text, and on Android the focus
	// notification is what asks the platform for its on-screen keyboard.
	SetFocusedView(edit_, FocusFlags::CAUSE_SCREEN_CHANGE);
}

bool CheatEditScreen::key(const KeyInput &key) {
	if ((key.flags & KeyInputFlags::DOWN) && (key.keyCode == NKCODE_BACK || key.keyCode == NKCODE_ESCAPE)) {
		ConfirmDiscard();
		return true;
	}
	return UIBaseDialogScreen::key(key);
}

void CheatEditScreen::onFinish(DialogResult result) {
	// Let the platform put its on-screen keyboard away.
	System_NotifyUIEvent(UIEventNotification::DIALOG_CLOSED);
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

void CheatEditScreen::OnSave(UI::EventParams &params) {
	const std::string edited = edit_ ? edit_->GetText() : currentText_;

	// Check the result with the same parser the game uses.
	CheatFileParser parser(cheatFile_, gameID_);
	parser.ParseText(edited);
	const std::vector<std::string> &errors = parser.GetErrors();
	if (!errors.empty()) {
		// The file may well have had problems before this edit, so offer to save anyway.
		std::string message = "The cheat file has problems:";
		for (size_t i = 0; i < errors.size() && i < 3; ++i) {
			message += "\n";
			message += errors[i];
		}
		if (errors.size() > 3) {
			message += StringFromFormat("\n(+%d more)", (int)errors.size() - 3);
		}
		auto di = GetI18NCategory(I18NCat::DIALOG);
		screenManager()->push(new MessagePopupScreen(di->T("Save anyway?"), message, di->T("Save"), di->T("Cancel"), [this, edited](bool result) {
			if (result) {
				SaveText(edited);
			}
		}));
		return;
	}

	SaveText(edited);
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
	if (edited == fileText_) {
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
