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
#include <vector>

#include "Common/File/Path.h"
#include "Core/CwCheat.h"

#include "UnitTest.h"

// A path that is never opened: ParseText() works purely on a string.
// (CheatFileParser owns a FILE *, so it is constructed in place rather than returned.)
static Path NonexistentCheatPath() {
	return Path("/nonexistent/ppsspp-unittest-cheat.ini");
}

static bool TestCheatParseTextValid() {
	CheatFileParser parser(NonexistentCheatPath(), "ULUS10000");
	EXPECT_TRUE(parser.ParseText(
		"_S ULUS10000\n"
		"_G Test game\n"
		"_C1 Infinite health\n"
		"_L 0x20123456 0x000000FF\n"
		"_L 0x20123458 0x0000000A\n"));

	EXPECT_EQ_INT((int)parser.GetErrors().size(), 0);
	EXPECT_EQ_INT((int)parser.GetFileInfo().size(), 1);
	EXPECT_EQ_INT(parser.GetFileInfo()[0].lineNum, 3);
	EXPECT_EQ_STR(parser.GetFileInfo()[0].name, std::string("Infinite health"));
	EXPECT_TRUE(parser.GetFileInfo()[0].enabled);

	EXPECT_EQ_INT((int)parser.GetCheats().size(), 1);
	EXPECT_EQ_INT((int)parser.GetCheats()[0].lines.size(), 2);
	return true;
}

static bool TestCheatParseTextErrors() {
	CheatFileParser parser(NonexistentCheatPath(), "ULUS10000");
	// An unknown line type on line 2 and a _L line with only one value on line 4. The cheat has
	// to be enabled for its _L lines to be looked at at all, so this is a _C1.
	EXPECT_FALSE(parser.ParseText(
		"_S ULUS10000\n"
		"_X0 Broken line type\n"
		"_C1 Missing values\n"
		"_L 0x20123456\n"));
	EXPECT_EQ_INT((int)parser.GetErrors().size(), 2);
	return true;
}

static bool TestCheatParseBomAndComments() {
	CheatFileParser parser(NonexistentCheatPath(), "ULUS10000");
	EXPECT_TRUE(parser.ParseText(
		"\xEF\xBB\xBF_S ULUS10000\n"
		"// a comment\n"
		"# another comment\n"
		"\n"
		"_C1 Commented\n"
		"// comment inside the cheat\n"
		"_L 0x00000001 0x00000002\n"));

	EXPECT_EQ_INT((int)parser.GetFileInfo().size(), 1);
	EXPECT_EQ_INT(parser.GetFileInfo()[0].lineNum, 5);
	EXPECT_EQ_STR(parser.GetFileInfo()[0].name, std::string("Commented"));
	EXPECT_EQ_INT((int)parser.GetCheats().size(), 1);
	EXPECT_EQ_INT((int)parser.GetCheats()[0].lines.size(), 1);
	return true;
}

static bool TestCheatInfoDisabledAndEmpty() {
	CheatFileParser parser(NonexistentCheatPath(), "ULUS10000");
	EXPECT_TRUE(parser.ParseText(
		"_S ULUS10000\n"
		"_C0 Disabled with code\n"
		"_L 0x20123456 0x000000FF\n"
		"_L 0x20123458 0x0000000A\n"
		"_C0 Disabled without code\n"
		"_C1 Enabled without code\n"));

	// Every _C line is listed, whether or not it has code lines and whether or not it is enabled.
	const std::vector<CheatFileInfo> &info = parser.GetFileInfo();
	EXPECT_EQ_INT((int)info.size(), 3);
	EXPECT_EQ_INT(info[0].lineNum, 2);
	EXPECT_EQ_STR(info[0].name, std::string("Disabled with code"));
	EXPECT_FALSE(info[0].enabled);
	EXPECT_EQ_INT(info[1].lineNum, 5);
	EXPECT_EQ_STR(info[1].name, std::string("Disabled without code"));
	EXPECT_FALSE(info[1].enabled);
	EXPECT_EQ_INT(info[2].lineNum, 6);
	EXPECT_EQ_STR(info[2].name, std::string("Enabled without code"));
	EXPECT_TRUE(info[2].enabled);

	// None of them is executable: two are disabled and the third has no code lines.
	EXPECT_EQ_INT((int)parser.GetCheats().size(), 0);
	return true;
}

static bool TestCheatParseMultiGame() {
	CheatFileParser parser(NonexistentCheatPath(), "ULUS10000");
	EXPECT_TRUE(parser.ParseText(
		"_S ULUS99999\n"
		"_G Other game\n"
		"_C0 Other cheat\n"
		"_L 0x00000001 0x00000002\n"
		"_S ULUS10000\n"
		"_G Test game\n"
		"_C1 Our cheat\n"
		"_L 0x00000003 0x00000004\n"));

	// Only the block for the game we asked about is listed, and the other game's
	// cheat must not leak into the executable list.
	EXPECT_EQ_INT((int)parser.GetFileInfo().size(), 1);
	EXPECT_EQ_INT(parser.GetFileInfo()[0].lineNum, 7);
	EXPECT_EQ_STR(parser.GetFileInfo()[0].name, std::string("Our cheat"));

	EXPECT_EQ_INT((int)parser.GetCheats().size(), 1);
	EXPECT_EQ_STR(parser.GetCheats()[0].name, std::string("Our cheat"));
	EXPECT_EQ_INT((int)parser.GetCheats()[0].lines.size(), 1);
	return true;
}

bool TestCwCheat() {
	if (!TestCheatParseTextValid())
		return false;
	if (!TestCheatParseTextErrors())
		return false;
	if (!TestCheatParseBomAndComments())
		return false;
	if (!TestCheatInfoDisabledAndEmpty())
		return false;
	if (!TestCheatParseMultiGame())
		return false;
	return true;
}
