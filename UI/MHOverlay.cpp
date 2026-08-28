#include "UI/MHOverlay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "Common/Data/Text/I18n.h"
#include "Common/Render/DrawBuffer.h"
#include "Common/Render/Text/draw_text.h"
#include "Common/UI/Context.h"
#include "Common/UI/View.h"
#include "Core/Core.h"
#include "Core/ELF/ParamSFO.h"
#include "Core/MemMap.h"
#include "Core/System.h"

// ---------------------------------------------------------------------------
// Monster identity tables, generated from the PC overlay project:
// MH-HP-Overlay-For-PSP-Emulator (modules/monsters_*.py).
// ---------------------------------------------------------------------------

struct MHMonsterDef {
	const char *name;
	int16_t gold, silver, mini;  // crown size thresholds; -1 = none
};

#include "monster_tables.inc"

// ---------------------------------------------------------------------------
// Simple Chinese names (Mainland community standard). Used when PPSSPP's UI
// language is zh_CN/zh_TW. Keyed by the English name; missing entries fall
// back to English automatically. Edit freely / add more languages below.
// ---------------------------------------------------------------------------

static const std::map<std::string, const char *> kMonsterNamesZh = {
	{"Agnaktor", "炎戈龙"},
	{"Akantor", "霸龙"},
	{"Alatreon", "煌黑龙"},
	{"Altaroth", "甲虫"},
	{"Amatsu", "岚龙"},
	{"Anteka", "雪鹿"},
	{"Apceros", "草食龙"},
	{"Aptonoth", "食草龙"},
	{"Arzuros", "青熊兽"},
	{"Ash Lao-Shan Lung", "苍老山龙"},
	{"Azure Rathalos", "苍火龙"},
	{"Baggi", "眠狗龙"},
	{"Baleful Gigginox", "电怪龙"},
	{"Barioth", "冰牙龙"},
	{"Barroth", "土砂龙"},
	{"Basarios", "岩龙"},
	{"Black Diablos", "黑角龙"},
	{"Black Gravios", "黑铠龙"},
	{"Blango", "雪狮子"},
	{"Blangonga", "雪狮王"},
	{"Blue Yian Kut-Ku", "青怪鸟"},
	{"Bnahabra", "飞甲虫"},
	{"Brute Tigrex", "黑轰龙"},
	{"Bulldrome", "野猪王"},
	{"Bullfango", "野猪"},
	{"Cart Felyne", "推车艾露猫"},
	{"Ceanataur", "镰蟹"},
	{"Cephadrome", "沙龙王"},
	{"Cephalos", "沙龙"},
	{"Chameleos", "霞龙"},
	{"Conga", "桃毛兽"},
	{"Congalala", "桃毛兽王"},
	{"Copper Blangonga", "砂狮子"},
	{"Crimson Fatalis", "红黑龙"},
	{"Crimson Qurupeco", "红彩鸟"},
	{"Daimyo Hermitaur", "大名盾蟹"},
	{"Delex", "砂鱼"},
	{"Deviljho", "恐暴龙"},
	{"Diablos", "角龙"},
	{"Duramboros", "尾槌龙"},
	{"Emerald Congalala", "绿桃毛兽王"},
	{"Fatalis", "黑龙"},
	{"Felyne", "艾露猫"},
	{"Furious Rajang", "激昂金狮子"},
	{"Gargwa", "丸鸟"},
	{"Gendrome", "黄速龙王"},
	{"Genprey", "黄速龙"},
	{"Giadrome", "白速龙王"},
	{"Giaprey", "白速龙"},
	{"Giggi", "毒龙幼仔"},
	{"Giggi Sac", "毒囊"},
	{"Gigginox", "毒怪龙"},
	{"Glacial Agnaktor", "冻戈龙"},
	{"Gold Rathian", "金火龙"},
	{"Gravios", "铠龙"},
	{"Great Baggi", "眠狗龙王"},
	{"Great Jaggi", "狗龙王"},
	{"Great Thunderbug", "大雷光虫"},
	{"Great Wroggi", "毒狗龙王"},
	{"Green Nargacuga", "绿迅龙"},
	{"Green Plesioth", "翠水龙"},
	{"Gypceros", "毒怪鸟"},
	{"Hermitaur", "小盾蟹"},
	{"Hornetaur", "巨甲虫"},
	{"Hypnocatrice", "眠鸟"},
	{"Iodrome", "红速龙王"},
	{"Ioprey", "红速龙"},
	{"Jade Barroth", "冰碎龙"},
	{"Jaggi", "狗龙"},
	{"Jaggia", "雌狗龙"},
	{"Jhen Mohran", "峯山龙"},
	{"Kelbi", "精灵鹿"},
	{"Khezu", "电龙"},
	{"King Shakalaka", "奇面王"},
	{"Kirin", "麒麟"},
	{"Kushala Daora", "钢龙"},
	{"Lagombi", "白兔兽"},
	{"Lao-Shan Lung", "老山龙"},
	{"Lavasioth", "熔岩龙"},
	{"Ludroth", "水生兽"},
	{"Lunastra", "炎妃龙"},
	{"Melynx", "梅拉露"},
	{"Monoblos", "一角龙"},
	{"Mosswine", "菌猪"},
	{"NO DATA", "无数据"},
	{"Nargacuga", "迅龙"},
	{"Nibelsnarf", "潜口龙"},
	{"Nyanjiro", "喵次郎"},
	{"Pink Rathian", "樱火龙"},
	{"Plesioth", "水龙"},
	{"Plum D.Hermitaur", "紫盾蟹"},
	{"Popo", "波波"},
	{"Purple Gypceros", "紫毒怪鸟"},
	{"Purple Ludroth", "紫水兽"},
	{"Qurupeco", "彩鸟"},
	{"Rajang", "金狮子"},
	{"Rathalos", "雄火龙"},
	{"Rathian", "雌火龙"},
	{"Red Khezu", "红电龙"},
	{"Remobra", "翼蛇龙"},
	{"Rhenoplos", "硬甲龙"},
	{"Rock", "岩石"},
	{"Rock (Forest)", "岩石(森林)"},
	{"Rock (Peaks)", "岩石(山峰)"},
	{"Rock (Plains)", "岩石(平原)"},
	{"Rock (Tundra)", "岩石(冻土)"},
	{"Rock (Volcano)", "岩石(火山)"},
	{"Royal Ludroth", "水兽"},
	{"Rusted Kushala Daora", "锈钢龙"},
	{"Sand Barioth", "风牙龙"},
	{"Scarred Yian Garuga", "战痕黑狼鸟"},
	{"Shakalaka", "奇面族"},
	{"Shen Gaoren", "砦蟹"},
	{"Shogun Ceanataur", "将军镰蟹"},
	{"Silver Rathalos", "银火龙"},
	{"Slagtoth", "垂皮龙"},
	{"Steel Uragaan", "钢锤龙"},
	{"Teostra", "炎王龙"},
	{"Terra S.Ceanataur", "朱镰蟹"},
	{"Tigrex", "轰龙"},
	{"Ukanlos", "崩龙"},
	{"Uragaan", "爆锤龙"},
	{"Uroktor", "熔岩兽"},
	{"Veggie Elder", "山菜爷"},
	{"Velocidrome", "蓝速龙王"},
	{"Velociprey", "蓝速龙"},
	{"Vespoid", "巨蜂"},
	{"Vespoid Queen", "女王虫"},
	{"Volvidon", "赤甲兽"},
	{"White Fatalis", "祖龙"},
	{"White Monoblos", "白一角龙"},
	{"Wroggi", "毒狗龙"},
	{"Yama Tsukami", "浮岳龙"},
	{"Yian Garuga", "黑狼鸟"},
	{"Yian Kut-Ku", "大怪鸟"},
	{"Zinogre", "雷狼龙"},
	{"dummy", "占位"},
};

// ---------------------------------------------------------------------------
// Per-game memory layout. Offsets are PSP virtual addresses relative to the
// monster pointer, exactly as in the PC overlay (mhf.py / mhf2.py / mhfu.py /
// mhp3rd.py). A status pair {cur, max}; {0, 0} means the game has no such
// status (only MHF lacks Dizzy).
// ---------------------------------------------------------------------------

struct MHStatusOffsets {
	uint32_t cur, max;
};

struct MHGame {
	const char *discId;
	uint32_t initial;                        // offset of the monster pointer list from PSP user RAM (0x08800000)
	uint32_t nameOff;                        // monster name byte
	uint32_t hpOff, maxHpOff, sizeOff;       // u32 hp/max, u16 size
	MHStatusOffsets poison, sleep, para, dizzy;
	uint32_t rageOff;                        // u16, /60 -> seconds
	const std::map<uint8_t, MHMonsterDef> *large;
	const std::map<uint8_t, const char *> *small;
};

static const MHGame kMHFGames[] = {
	// MHF (also MHP JPN)
	{"ULES00318", 0x1254D70, 0x210, 0x312, 0x43C, 0x2A4, {0x3A8, 0x46C}, {0x462, 0x460}, {0x474, 0x472}, {0, 0}, 0x580, &kMHFLargeMonsters, &kMHFSmallMonsters},
	{"ULUS10084", 0x1253F70, 0x210, 0x312, 0x43C, 0x2A4, {0x3A8, 0x46C}, {0x462, 0x460}, {0x474, 0x472}, {0, 0}, 0x580, &kMHFLargeMonsters, &kMHFSmallMonsters},
	{"ULJM05066", 0x1253570, 0x210, 0x312, 0x43C, 0x2A4, {0x3A8, 0x46C}, {0x462, 0x460}, {0x474, 0x472}, {0, 0}, 0x580, &kMHFLargeMonsters, &kMHFSmallMonsters},
};

static const MHGame kMHF2Games[] = {
	// MHF2 (also MHP2 JPN). Monster tables shared with MHFU.
	{"ULES00851", 0x127AD70, 0x1E8, 0x2E2, 0x41E, 0x274, {0x388, 0x450}, {0x446, 0x444}, {0x458, 0x456}, {0x440, 0x55E}, 0x564, &kMHFULargeMonsters, &kMHFUSmallMonsters},
	{"ULUS10266", 0x12799F0, 0x1E8, 0x2E2, 0x41E, 0x274, {0x388, 0x450}, {0x446, 0x444}, {0x458, 0x456}, {0x440, 0x55E}, 0x564, &kMHFULargeMonsters, &kMHFUSmallMonsters},
	{"ULJM05156", 0x1278E70, 0x1E8, 0x2E2, 0x41E, 0x274, {0x388, 0x450}, {0x446, 0x444}, {0x458, 0x456}, {0x440, 0x55E}, 0x564, &kMHFULargeMonsters, &kMHFUSmallMonsters},
};

static const MHGame kMHFUGames[] = {
	// MHFU (also MHP2G JPN)
	{"ULES01213", 0x1412140, 0x1E8, 0x2E4, 0x41E, 0x274, {0x388, 0x450}, {0x446, 0x444}, {0x458, 0x456}, {0x440, 0x566}, 0x56C, &kMHFULargeMonsters, &kMHFUSmallMonsters},
	{"ULUS10391", 0x1412240, 0x1E8, 0x2E4, 0x41E, 0x274, {0x388, 0x450}, {0x446, 0x444}, {0x458, 0x456}, {0x440, 0x566}, 0x56C, &kMHFULargeMonsters, &kMHFUSmallMonsters},
	{"ULJM05500", 0x140D3C0, 0x1E8, 0x2E4, 0x41E, 0x274, {0x388, 0x450}, {0x446, 0x444}, {0x458, 0x456}, {0x440, 0x566}, 0x56C, &kMHFULargeMonsters, &kMHFUSmallMonsters},
};

static const MHGame kMHP3RDGames[] = {
	// MHP3RD (also MHP3RD HD)
	{"ULJM05800", 0x15A9860, 0x62, 0x246, 0x288, 0xD4, {0x23C, 0x252}, {0x24E, 0x24C}, {0x25A, 0x258}, {0xC5C, 0xC5E}, 0xBC8, &kMHP3RDLargeMonsters, &kMHP3RDSmallMonsters},
	{"NPJB40001", 0x19B0AE0, 0x62, 0x246, 0x288, 0xD4, {0x23C, 0x252}, {0x24E, 0x24C}, {0x25A, 0x258}, {0xC5C, 0xC5E}, 0xBC8, &kMHP3RDLargeMonsters, &kMHP3RDSmallMonsters},
};

static const MHGame *FindGame() {
	const std::string &discId = g_paramSFO.GetDiscID();
	if (discId.empty()) {
		return nullptr;
	}
	// Simple linear scan over all tables above.
	static const MHGame *all[] = {
		&kMHFGames[0], &kMHFGames[1], &kMHFGames[2],
		&kMHF2Games[0], &kMHF2Games[1], &kMHF2Games[2],
		&kMHFUGames[0], &kMHFUGames[1], &kMHFUGames[2],
		&kMHP3RDGames[0], &kMHP3RDGames[1],
	};
	for (const MHGame *g : all) {
		if (discId == g->discId) {
			return g;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Reading monsters (PSP virtual addresses, no host offset needed - unlike the
// PC overlay which had to map them through the emulator process base).
// ---------------------------------------------------------------------------

static constexpr uint32_t kMaxMonsters = 10;

struct MHMonster {
	bool large;
	uint32_t nameId;
	const char *englishName;
	uint32_t hp, maxHp;
	uint16_t size;
	uint32_t poisonCur, poisonMax, sleepCur, sleepMax, paraCur, paraMax, dizzyCur, dizzyMax;
	bool hasDizzy;
	uint32_t rage;
};

static void ReadMonsters(const MHGame &game, std::vector<MHMonster> &out) {
	const uint32_t base = PSP_GetUserMemoryBase();  // 0x08800000

	for (uint32_t i = 0; i < kMaxMonsters; i++) {
		const uint32_t ptrAddr = base + game.initial + i * 4;
		if (!Memory::IsValidAddress(ptrAddr)) {
			continue;
		}
		const uint32_t p0 = Memory::ReadUnchecked_U32(ptrAddr);
		if (p0 == 0 || !Memory::IsValidAddress(p0)) {
			continue;
		}

		const uint8_t nameId = Memory::ReadUnchecked_U8(p0 + game.nameOff);

		MHMonster m{};
		auto largeIt = game.large->find(nameId);
		auto smallIt = game.small->find(nameId);

		MHMonster *item = nullptr;
		if (largeIt != game.large->end()) {
			m.large = true;
			m.nameId = nameId;
			m.englishName = largeIt->second.name;
			item = &m;
		} else if (smallIt != game.small->end()) {
			m.large = false;
			m.nameId = nameId;
			m.englishName = smallIt->second;
			item = &m;
		}
		if (!item) {
			continue;
		}

		item->hp = Memory::ReadUnchecked_U32(p0 + game.hpOff);
		item->maxHp = Memory::ReadUnchecked_U32(p0 + game.maxHpOff);
		item->size = Memory::ReadUnchecked_U16(p0 + game.sizeOff);

		if (item->large) {
			item->poisonCur = Memory::ReadUnchecked_U16(p0 + game.poison.cur);
			item->poisonMax = Memory::ReadUnchecked_U16(p0 + game.poison.max);
			item->sleepCur = Memory::ReadUnchecked_U16(p0 + game.sleep.cur);
			item->sleepMax = Memory::ReadUnchecked_U16(p0 + game.sleep.max);
			item->paraCur = Memory::ReadUnchecked_U16(p0 + game.para.cur);
			item->paraMax = Memory::ReadUnchecked_U16(p0 + game.para.max);
			if (game.dizzy.cur != 0) {
				item->hasDizzy = true;
				item->dizzyCur = Memory::ReadUnchecked_U16(p0 + game.dizzy.cur);
				item->dizzyMax = Memory::ReadUnchecked_U16(p0 + game.dizzy.max);
			}
			item->rage = Memory::ReadUnchecked_U16(p0 + game.rageOff);
		}

		out.push_back(*item);
	}
}

// ---------------------------------------------------------------------------
// Formatting & rendering
// ---------------------------------------------------------------------------

// Visual/config knobs, mirroring the PC overlay's config.ini defaults.
static constexpr bool kShowInitialHp = true;
static constexpr bool kShowHpPercentage = true;
static constexpr bool kShowSmallMonsters = true;
static constexpr bool kShowSizeMultiplier = true;
static constexpr bool kShowCrown = true;
static constexpr bool kShowAbnormalStatus = true;

static constexpr float kFontScale = 0.9f;
static constexpr float kMarginRight = 8.0f;
static constexpr float kMarginTop = 8.0f;
static constexpr float kPadX = 6.0f;
static constexpr float kPadY = 3.0f;
static constexpr float kGapY = 3.0f;

// Colors are 0xAABBGGRR.
static constexpr uint32_t kTextColor = 0xFFD4FF7F;         // aquamarine
static constexpr uint32_t kBackgroundColor = 0x994F4F2F;   // darkslategray @ 60%
static constexpr uint32_t kStatusTextColor = 0xFF00FFFF;   // yellow
static constexpr uint32_t kStatusBackgroundColor = 0x80000080;  // green @ 50%

static const char *LocalizedName(const char *english, bool useChinese) {
	if (!useChinese) {
		return english;
	}
	auto it = kMonsterNamesZh.find(english);
	return it != kMonsterNamesZh.end() ? it->second : english;
}

static const char *Crown(uint16_t size, const MHMonsterDef &def) {
	if (def.gold != -1 && size >= (uint16_t)def.gold) {
		return " Gold";
	}
	if (def.silver != -1 && size >= (uint16_t)def.silver) {
		return " Silver";
	}
	if (def.mini != -1 && size <= (uint16_t)def.mini) {
		return " Mini";
	}
	return "";
}

static std::string FormatHp(uint32_t hp, uint32_t maxHp, bool withPercent, bool withInitial) {
	std::string text;
	if (withPercent && maxHp > 0) {
		int pct = (int)std::ceil((double)hp * 100.0 / (double)maxHp);
		char buf[32];
		snprintf(buf, sizeof(buf), " %d%% |", pct);
		text += buf;
	}
	text += " " + std::to_string(hp);
	if (withInitial) {
		text += " | " + std::to_string(maxHp);
	}
	return text;
}

// Builds every line of the overlay (monster label + abnormal status lines).
// Returns the lines and, for each line, whether it's an abnormal-status line.
static void BuildLines(const MHGame &game, const std::vector<MHMonster> &monsters, bool useChinese,
	std::vector<std::string> &lines, std::vector<bool> &isStatus, std::vector<uint32_t> &colors) {
	for (const MHMonster &m : monsters) {
		const char *name = LocalizedName(m.englishName, useChinese);

		if (m.large) {
			// Large monsters only appear during a hunt; filter placeholder data.
			if (m.maxHp <= 5 || m.hp >= 45000) {
				continue;
			}
			std::string text;
			if (kShowSizeMultiplier) {
				text += "(" + std::to_string(m.size) + ") ";
			}
			text += name;
			if (kShowCrown) {
				text += Crown(m.size, game.large->at(m.nameId));
			}
			text += ":";
			text += FormatHp(m.hp, m.maxHp, kShowHpPercentage, kShowInitialHp);
			lines.push_back(text);
			isStatus.push_back(false);
			colors.push_back(kTextColor);

			if (kShowAbnormalStatus) {
				const auto addStatus = [&](const char *key, uint32_t cur, uint32_t max) {
					if (max == 0xFFFF) {
						return;
					}
					char buf[64];
					snprintf(buf, sizeof(buf), " %s: %u/%u", key, cur, max);
					lines.push_back(buf);
					isStatus.push_back(true);
					colors.push_back(kStatusTextColor);
				};
				addStatus("Poison", m.poisonCur, m.poisonMax);
				addStatus("Sleep", m.sleepCur, m.sleepMax);
				addStatus("Paralysis", m.paraCur, m.paraMax);
				if (m.hasDizzy) {
					addStatus("Dizzy", m.dizzyCur, m.dizzyMax);
				}
				{
					char buf[64];
					unsigned m_ = m.rage / 60;
					unsigned s_ = m.rage % 60;
					snprintf(buf, sizeof(buf), " Rage: %u:%02u", m_, s_);
					lines.push_back(buf);
					isStatus.push_back(true);
					colors.push_back(kStatusTextColor);
				}
			}
		} else {
			if (!kShowSmallMonsters) {
				continue;
			}
			// Placeholder filter for small monsters, as in the PC overlay.
			if (m.hp >= 20000) {
				continue;
			}
			std::string text;
			text += name;
			text += ":";
			text += FormatHp(m.hp, m.maxHp, kShowHpPercentage, kShowInitialHp);
			lines.push_back(text);
			isStatus.push_back(false);
			colors.push_back(kTextColor);
		}
	}
}

void DrawMHOverlay(UIContext *ctx, const Bounds &bounds) {
	if (GetUIState() != UISTATE_INGAME) {
		return;
	}
	if (!PSP_IsInited() || !Memory::IsActive()) {
		return;
	}

	const MHGame *game = FindGame();
	if (!game) {
		return;
	}

	std::vector<MHMonster> monsters;
	ReadMonsters(*game, monsters);
	if (monsters.empty()) {
		return;
	}

	std::vector<std::string> lines;
	std::vector<bool> isStatus;
	std::vector<uint32_t> colors;
	const std::string lang = g_i18nrepo.LanguageID();
	const bool useChinese = lang.size() >= 2 && lang[0] == 'z' && lang[1] == 'h';
	BuildLines(*game, monsters, useChinese, lines, isStatus, colors);
	if (lines.empty()) {
		return;
	}

	TextDrawer *td = ctx->Text();
	td->SetOrCreateFont(ctx->GetTheme().uiFont);
	td->SetFontScale(kFontScale, kFontScale);

	// Measure all lines so backgrounds can share one width (like align=true).
	std::vector<float> widths(lines.size(), 0.0f);
	std::vector<float> heights(lines.size(), 0.0f);
	float maxW = 0.0f;
	for (size_t i = 0; i < lines.size(); i++) {
		td->MeasureString(lines[i], &widths[i], &heights[i]);
		maxW = std::max(maxW, widths[i]);
	}

	const float right = bounds.x2() - kMarginRight;
	const float bgW = maxW + kPadX * 2.0f;
	float y = bounds.y + kMarginTop;

	// Background boxes.
	ctx->Flush();
	ctx->BeginNoTex();
	for (size_t i = 0; i < lines.size(); i++) {
		const float bgH = heights[i] + kPadY * 2.0f;
		ctx->Draw()->Rect(right - bgW, y, bgW, bgH, isStatus[i] ? kStatusBackgroundColor : kBackgroundColor);
		y += bgH + kGapY;
	}
	ctx->Flush();
	ctx->Begin();
	ctx->RebindTexture();

	// Text (TextDrawer renders CJK; the debug bitmap font does not).
	y = bounds.y + kMarginTop;
	for (size_t i = 0; i < lines.size(); i++) {
		const float bgH = heights[i] + kPadY * 2.0f;
		td->DrawString(*ctx->Draw(), lines[i], right, y + kPadY, colors[i], ALIGN_TOPRIGHT);
		y += bgH + kGapY;
	}
	ctx->Flush();
}