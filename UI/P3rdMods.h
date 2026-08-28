#pragma once

#include <string>
#include <vector>

#include "Common/UI/Screen.h"

// P3rd ML Mod Manager — native port of
// https://github.com/Kurogami2134/p3rdml_modman  (Lua PSP homebrew)
//
// Instead of booting the separate homebrew, PPSSPP lists the mods found in
// <memstick>/MODS and stages them exactly like the Lua tool does:
//   - file replacement mods  -> <root>/FILES/<target>
//   - code mods              -> <root>/MODS.BIN (path table) + <root>/MODS/
//   - patch mods             -> <root>/FILES/<target>P  ("0.01" + data + FFFFFFFF)
//   - PRELOAD.BIN            -> <root>/PRELOAD.BIN
//   - user/<ver>/*.ini       -> enabled / replaced / dest ids (Lua-compatible)
// where <root> is P3RDML (P3 original), P3RDHDML (P3HD) or PSP/SAVEDATA/FUCDAT (FUC).
// The game-side mhp3reload loader consumes these files at game boot.
//
// Phase 1 covers Code/File/Patch/Pack + Dest-based replacement. Not ported yet:
// EquipSET/EquipCATSET, animation compilation, audio header/binary staging.

struct P3rdModInfo {
	std::string id;       // mod folder name
	std::string name;
	std::string type;     // Code / File / Patch / Pack / EquipSET / AudioXXX / ...
	int priority = 3;     // 0..5, used for mods.bin ordering
	std::string files;    // ';' separated
	std::string target;   // ';' separated
	std::string filesHD;  // HD variant of Files
	std::string targetHD; // HD variant of Target
	std::string dest;     // direct destination file id (Dest= field)
	std::string destID;   // DestID field
	std::string modList;  // Pack: mods included
	std::string subModList;   // PseudoPack
	std::string audio;    // Audio field
	bool enabled = false;
};

// Returns the target matching the currently running game (P3/HD/FUC), or NONE.
int P3rdGuessTarget();

// Scans <memstick>/MODS for mod folders containing mod.ini.
std::vector<P3rdModInfo> P3rdScanMods();

// Stages the enabled mods for the given target (0=P3, 1=P3HD, 2=FUC).
// Returns an empty string on success, or an error message.
std::string P3rdApplyMods(int target, const std::vector<P3rdModInfo> &mods);

// UI screen (pause menu -> "P3rd Mods").
class P3rdModsScreen : public UIScreen {
public:
	P3rdModsScreen() {}

	void CreateViews() override;

protected:
	void Refresh();
	void ApplyNow();
	void UpdateHeader();

	std::vector<P3rdModInfo> mods_;
	int target_ = 0;
	std::string resultText_;
};