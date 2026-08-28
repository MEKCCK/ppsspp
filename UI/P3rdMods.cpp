#include "UI/P3rdMods.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

#include "Common/Data/Format/IniFile.h"
#include "Common/File/DirListing.h"
#include "Common/File/FileUtil.h"
#include "Common/File/Path.h"
#include "Common/StringUtils.h"
#include "Common/Render/DrawBuffer.h"
#include "Common/UI/Context.h"
#include "Common/UI/ScrollView.h"
#include "Common/UI/UI.h"
#include "Common/UI/View.h"
#include "Common/UI/ViewGroup.h"
#include "Common/UI/ScreenManager.h"
#include "Core/Config.h"
#include "Core/ELF/ParamSFO.h"
#include "Core/System.h"

// ---------------------------------------------------------------------------
// Staging engine (mirrors code/main.lua + code/mods.lua of p3rdml_modman).
// The game-side mhp3reload loader consumes the staged files at game boot.
// ---------------------------------------------------------------------------

struct P3rdTargetCfg {
	const char *root;      // relative to memstick root
	const char *dataDir;   // DATA / DATA_HD / DATA_FU
	const char *userDir;   // user/nohd | user/hd | user/fuc
};

static const P3rdTargetCfg kP3rdTargets[3] = {
	{"P3RDML", "DATA", "user/nohd"},
	{"P3RDHDML", "DATA_HD", "user/hd"},
	{"PSP/SAVEDATA/FUCDAT", "DATA_FU", "user/fuc"},
};

int P3rdGuessTarget() {
	if (!PSP_IsInited()) {
		return -1;
	}
	const std::string &id = g_paramSFO.GetDiscID();
	if (id == "ULJM05800") {
		return 0;  // P3 original
	}
	if (id == "NPJB40001") {
		return 1;  // P3 HD
	}
	if (id == "ULES01213" || id == "ULUS10391" || id == "ULJM05500") {
		return 2;  // 2G/FUC family
	}
	return -1;
}

std::vector<P3rdModInfo> P3rdScanMods() {
	std::vector<P3rdModInfo> result;
	Path modsDir = g_Config.memStickDirectory / "MODS";
	if (!File::Exists(modsDir)) {
		return result;
	}
	std::vector<File::FileInfo> entries;
	if (!File::GetFilesInDir(modsDir, &entries)) {
		return result;
	}
	for (const File::FileInfo &e : entries) {
		if (!e.isDirectory) {
			continue;
		}
		Path iniPath = e.fullName / "mod.ini";
		if (!File::Exists(iniPath)) {
			continue;
		}
		IniFile ini;
		if (!ini.Load(iniPath)) {
			continue;
		}
		Section *sec = ini.GetOrCreateSection("MOD INFO");
		P3rdModInfo m;
		m.id = e.name;
		sec->Get("Name", &m.name);
		if (m.name.empty()) {
			m.name = m.id;
		}
		sec->Get("Type", &m.type);
		sec->Get("Priority", &m.priority);
		if (m.priority < 0) m.priority = 0;
		if (m.priority > 5) m.priority = 5;
		sec->Get("Files", &m.files);
		sec->Get("Target", &m.target);
		sec->Get("FilesHD", &m.filesHD);
		sec->Get("TargetHD", &m.targetHD);
		sec->Get("Dest", &m.dest);
		sec->Get("DestID", &m.destID);
		sec->Get("ModList", &m.modList);
		sec->Get("SubModList", &m.subModList);
		sec->Get("Audio", &m.audio);
		result.push_back(m);
	}
	std::stable_sort(result.begin(), result.end(), [](const P3rdModInfo &a, const P3rdModInfo &b) {
		return a.name < b.name;
	});
	return result;
}

static void SplitList(const std::string &str, std::vector<std::string> &out) {
	std::vector<std::string> parts;
	SplitString(str, ';', parts, true);
	for (std::string &p : parts) {
		if (!p.empty()) {
			out.push_back(p);
		}
	}
}

static bool StageCopy(const Path &src, const Path &destDir, const std::string &destName) {
	Path dest = destDir / destName;
	if (File::Exists(dest)) {
		File::Delete(dest);
	}
	return File::Copy(src, dest);
}

static bool StageCopy(const Path &src, const Path &dest) {
	if (File::Exists(dest)) {
		File::Delete(dest);
	}
	return File::Copy(src, dest);
}

static bool WriteFile(const Path &path, const std::string &data) {
	FILE *f = fopen(path.ToString().c_str(), "wb");
	if (!f) {
		return false;
	}
	bool ok = true;
	if (!data.empty()) {
		ok = fwrite(data.data(), 1, data.size(), f) == data.size();
	}
	fclose(f);
	return ok;
}

static void AppendU32LE(std::string &out, uint32_t v) {
	out.push_back((char)(v & 0xFF));
	out.push_back((char)((v >> 8) & 0xFF));
	out.push_back((char)((v >> 16) & 0xFF));
	out.push_back((char)((v >> 24) & 0xFF));
}

static std::string UpperASCII(const std::string &s) {
	std::string r = s;
	for (char &c : r) {
		if (c >= 'a' && c <= 'z') {
			c -= 32;
		}
	}
	return r;
}

static bool ReadWholeFile(const Path &path, std::string &out) {
	FILE *f = fopen(path.ToString().c_str(), "rb");
	if (!f) {
		return false;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	out.assign((size_t)size, '\0');
	if (size > 0 && fread(&out[0], 1, (size_t)size, f) != (size_t)size) {
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

std::string P3rdApplyMods(int target, const std::vector<P3rdModInfo> &mods) {
	if (target < 0 || target > 2) {
		return "invalid target";
	}
	const P3rdTargetCfg &cfg = kP3rdTargets[target];
	const bool hd = (target == 1);
	const Path ms = g_Config.memStickDirectory;
	const Path root = ms / cfg.root;
	const Path filesDir = root / "FILES";
	const Path modsDir = root / "MODS";
	const Path modsSource = ms / "MODS";

	File::CreateFullPath(filesDir);
	File::CreateFullPath(modsDir);
	File::CreateFullPath(ms / cfg.userDir);

	const auto filesOf = [&](const P3rdModInfo &m) -> std::string {
		return (hd && !m.filesHD.empty()) ? m.filesHD : m.files;
	};
	const auto targetOf = [&](const P3rdModInfo &m) -> std::string {
		return (hd && !m.targetHD.empty()) ? m.targetHD : m.target;
	};

	// Code mods bucketed by priority (mods.bin is ordered by priority 0..5).
	std::vector<std::pair<const P3rdModInfo *, std::string>> codeFiles[6];
	// Patch mods: target id -> concatenated raw bytes.
	std::map<std::string, std::string> patchData;

	std::string replaced, destIds;

	for (const P3rdModInfo &m : mods) {
		if (!m.enabled) {
			continue;
		}
		const std::string &type = m.type;

		if (type == "Pack" || type == "PseudoPack") {
			// Pack only toggles other mods; PseudoPack needs per-section parsing (phase 2).
			continue;
		}
		if (type == "Code") {
			std::vector<std::string> fl;
			SplitList(filesOf(m), fl);
			for (const std::string &f : fl) {
				codeFiles[m.priority].push_back({&m, f});
			}
			continue;
		}
		if (type == "Patch") {
			std::vector<std::string> fl, tg;
			SplitList(filesOf(m), fl);
			SplitList(targetOf(m), tg);
			size_t n = std::min(fl.size(), tg.size());
			for (size_t i = 0; i < n; i++) {
				std::string data;
				Path src = modsSource / m.id / fl[i];
				if (!ReadWholeFile(src, data)) {
					return "missing patch file: " + src.ToString();
				}
				// Also skip the trailing FFFFFFFF00000000 marker like the Lua tool does.
				if (data.size() >= 8 && data.compare(data.size() - 8, 8, std::string("\xFF\xFF\xFF\xFF\x00\x00\x00\x00", 8)) == 0) {
					data.resize(data.size() - 8);
				}
				patchData[tg[i]] += data;
			}
			continue;
		}
		// Dest-based replacement (any type with a Dest= field).
		if (!m.dest.empty()) {
			std::vector<std::string> fl;
			SplitList(filesOf(m), fl);
			if (fl.empty()) {
				continue;
			}
			Path src = modsSource / m.id / fl[0];
			if (!StageCopy(src, filesDir, m.dest)) {
				return "copy failed: " + m.id + "/" + fl[0] + " -> " + m.dest;
			}
			replaced += m.id + m.dest + ";";
			if (!m.destID.empty()) {
				destIds += m.id + ":" + m.destID + ";";
			}
			continue;
		}
		// Plain File replacement (Files/Target pairs, Lua replace_files).
		if (type == "File" || !m.files.empty()) {
			std::vector<std::string> fl, tg;
			SplitList(filesOf(m), fl);
			SplitList(targetOf(m), tg);
			size_t n = std::min(fl.size(), tg.size());
			for (size_t i = 0; i < n; i++) {
				Path src = modsSource / m.id / fl[i];
				if (!StageCopy(src, filesDir, tg[i])) {
					return "copy failed: " + m.id + "/" + fl[i] + " -> " + tg[i];
				}
			}
			continue;
		}
		// EquipSET / EquipCATSET / animation compilation: phase 2.
	}

	// Patch targets: "<target>P" files, format "0.01" + bytes + -1 (Lua build_patches).
	for (const auto &it : patchData) {
		std::string out = "0.01";
		out += it.second;
		AppendU32LE(out, 0xFFFFFFFF);
		if (!WriteFile(filesDir / (it.first + "P"), out)) {
			return "cannot write FILES/" + it.first + "P";
		}
	}

	// MODS.BIN — path table of code mods, priority 0→5 (Lua build_mods_bin).
	std::string modsBin;
	for (int p = 0; p <= 5; p++) {
		for (const auto &entry : codeFiles[p]) {
			const P3rdModInfo *m = entry.first;
			Path src = modsSource / m->id / entry.second;
			if (!File::Exists(src)) {
				return "missing code file: " + src.ToString();
			}
			std::string name = UpperASCII(entry.second);
			modsBin.push_back((char)(name.size() + 2));
			modsBin.push_back('/');
			modsBin += name;
			modsBin.push_back('\0');
			if (!StageCopy(src, modsDir, entry.second)) {
				return "copy failed: " + m->id + "/" + entry.second;
			}
		}
	}
	if (!modsBin.empty()) {
		modsBin.push_back((char)255);  // Lua writes a single 0xFF terminator.
		if (!WriteFile(root / "MODS.BIN", modsBin)) {
			return "cannot write MODS.BIN";
		}
	}

	// PRELOAD.BIN from the DATA folder if present.
	Path preload = ms / cfg.dataDir / "PRELOAD.BIN";
	if (File::Exists(preload)) {
		StageCopy(preload, root / "PRELOAD.BIN");
	}

	// Lua-compatible state files under user/<version>/.
	auto writeIni = [&](const char *fileName, const char *sectionName, const std::string &value) {
		Path path = ms / cfg.userDir / fileName;
		IniFile ini;
		if (File::Exists(path)) {
			ini.Load(path);
		}
		Section *sec = ini.GetOrCreateSection(sectionName);
		sec->Set("enabled", value);
		ini.Save(path);
	};
	std::string enabled;
	for (const P3rdModInfo &m : mods) {
		if (m.enabled) {
			enabled += m.id + ";";
		}
	}
	// NOTE: ini section/key names mirror the Lua tool's ini.write calls.
	writeIni("enabled.ini", "enabled", enabled);
	writeIni("replaced_files.ini", "files", replaced);
	writeIni("dest_ids.ini", "ids", destIds);

	return "";
}

// ---------------------------------------------------------------------------
// UI screen
// ---------------------------------------------------------------------------

static const char *kTargetNames[3] = {"P3", "P3HD", "FUC"};

void P3rdModsScreen::CreateViews() {
	using namespace UI;

	if (mods_.empty()) {
		mods_ = P3rdScanMods();
		int guess = P3rdGuessTarget();
		if (guess >= 0) {
			target_ = guess;
		}
	}

	root_ = new LinearLayout(ORIENT_VERTICAL);
	auto left = root_->Add(new LinearLayout(ORIENT_VERTICAL, new LinearLayoutParams(FILL_PARENT, FILL_PARENT)));

	// Version picker + actions.
	auto header = left->Add(new LinearLayout(ORIENT_HORIZONTAL, new LayoutParams(FILL_PARENT, WRAP_CONTENT)));
	for (int i = 0; i < 3; i++) {
		std::string label = i == target_ ? std::string("[") + kTargetNames[i] + "]" : kTargetNames[i];
		Button *b = header->Add(new Button(label, new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)));
		b->OnClick.Add([this, i](UI::EventParams &) {
			target_ = i;
			RecreateViews();
		});
	}

	auto actions = left->Add(new LinearLayout(ORIENT_HORIZONTAL, new LayoutParams(FILL_PARENT, WRAP_CONTENT)));
	actions->Add(new Button("Refresh", new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Add([this](UI::EventParams &) {
		mods_ = P3rdScanMods();
		RecreateViews();
	});
	actions->Add(new Button("Apply", new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Add([this](UI::EventParams &) {
		ApplyNow();
	});

	std::string resultText = resultText_.empty() ? "Mods from <memstick>/MODS. Apply stages Code/File/Patch mods." : resultText_;
	left->Add(new TextView(resultText, ALIGN_LEFT | FLAG_WRAP_TEXT, false, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));

	if (mods_.empty()) {
		left->Add(new TextView("No mods found. Put mod folders containing mod.ini into <memstick>/MODS", ALIGN_LEFT | FLAG_WRAP_TEXT, true, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
	}

	auto scroll = left->Add(new ScrollView(ORIENT_VERTICAL, new LinearLayoutParams(FILL_PARENT, FILL_PARENT)));
	auto list = new LinearLayout(ORIENT_VERTICAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT));
	scroll->Add(list);

	int index = 0;
	for (P3rdModInfo &m : mods_) {
		int idx = index++;
		std::string label = std::string(m.enabled ? "[x] " : "[  ] ") + m.name + " (" + m.type + ")";
		Choice *c = list->Add(new Choice(label, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		c->OnClick.Add([this, idx, c](UI::EventParams &) {
			mods_[idx].enabled = !mods_[idx].enabled;
			c->SetText(std::string(mods_[idx].enabled ? "[x] " : "[  ] ") + mods_[idx].name + " (" + mods_[idx].type + ")");
		});
	}
}

void P3rdModsScreen::Refresh() {
	mods_ = P3rdScanMods();
	RecreateViews();
}

void P3rdModsScreen::ApplyNow() {
	resultText_ = P3rdApplyMods(target_, mods_);
	if (resultText_.empty()) {
		resultText_ = std::string("Applied to ") + kTargetNames[target_] + " (" + kP3rdTargets[target_].root + ") - restart the game";
	}
	RecreateViews();
}