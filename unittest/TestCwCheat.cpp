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

// --- CheatFileText ---

static bool TestCheatTextSplitJoin() {
	// Anything that round-trips must come back byte for byte, with the trailing
	// newline state preserved.
	const char *cases[] = {
		"",
		"\n",
		"\n\n",
		"a",
		"a\n",
		"a\nb",
		"a\nb\n",
		"a\n\nb\n",
		"  \n\t\n",
		"a\r\nb\r\n",
		"_S ULUS10000\n_G Game\n_C0 Name\n_L 0x00000001 0x00000002\n",
	};
	for (const char *c : cases) {
		const std::string original(c);
		const std::vector<std::string> lines = CheatFileText::SplitLines(original);
		const std::string rebuilt = CheatFileText::JoinLines(lines, CheatFileText::EndsWithNewline(original));
		EXPECT_EQ_STR(rebuilt, original);
	}

	// Concrete shapes too, so a wrong implementation can't pass just by being symmetric.
	std::vector<std::string> lines = CheatFileText::SplitLines("a\nb\n");
	EXPECT_EQ_INT((int)lines.size(), 2);
	EXPECT_EQ_STR(lines[0], std::string("a"));
	EXPECT_EQ_STR(lines[1], std::string("b"));

	EXPECT_EQ_INT((int)CheatFileText::SplitLines("").size(), 0);

	lines = CheatFileText::SplitLines("\n");
	EXPECT_EQ_INT((int)lines.size(), 1);
	EXPECT_EQ_STR(lines[0], std::string(""));

	// CRLF: the '\r' stays part of the line, so writing back does not normalize it away.
	lines = CheatFileText::SplitLines("a\r\nb\r\n");
	EXPECT_EQ_INT((int)lines.size(), 2);
	EXPECT_EQ_STR(lines[0], std::string("a\r"));
	EXPECT_EQ_STR(lines[1], std::string("b\r"));
	return true;
}

static bool TestCheatTextBlockExtent() {
	const std::vector<std::string> lines = CheatFileText::SplitLines(
		"_S ULUS10000\n"                 // line 1
		"_G Test game\n"                 // line 2
		"_C0 First\n"                    // line 3
		"_L 0x00000001 0x00000002\n"     // line 4
		"// note in the middle\n"        // line 5
		"\n"                             // line 6
		"_L 0x00000003 0x00000004\n"     // line 7
		"_C0 Second\n"                   // line 8
		"_L 0x00000005 0x00000006\n"     // line 9
		"_S ULUS99999\n"                 // line 10
		"_C0 Other game\n"               // line 11
		"_L 0x00000007 0x00000008\n");   // line 12
	EXPECT_EQ_INT((int)lines.size(), 12);

	// The first cheat runs up to the next _C, including the comment and the blank line.
	EXPECT_EQ_INT(CheatFileText::BlockEndLine(lines, 3), 7);
	const std::vector<std::string> first = CheatFileText::GetCheatBlock(lines, 3);
	EXPECT_EQ_INT((int)first.size(), 4);
	EXPECT_EQ_STR(first[0], std::string("_C0 First"));
	EXPECT_EQ_STR(first[1], std::string("_L 0x00000001 0x00000002"));
	EXPECT_EQ_STR(first[2], std::string("// note in the middle"));
	EXPECT_EQ_STR(first[3], std::string(""));

	// The second cheat ends at the _S of the next game.
	EXPECT_EQ_INT(CheatFileText::BlockEndLine(lines, 8), 9);

	// The last cheat in the file runs to the end.
	EXPECT_EQ_INT(CheatFileText::BlockEndLine(lines, 11), (int)lines.size());

	// Out of range line numbers are not clamped into the file.
	EXPECT_EQ_INT((int)CheatFileText::GetCheatBlock(lines, 0).size(), 0);
	EXPECT_EQ_INT((int)CheatFileText::GetCheatBlock(lines, 13).size(), 0);
	return true;
}

static bool TestCheatTextReplaceRemoveAppend() {
	const std::string original =
		"_S ULUS10000\n"
		"_G Test game\n"
		"_C0 First\n"
		"_L 0x00000001 0x00000002\n"
		"\n"
		"_C0 Second\n"
		"_L 0x00000003 0x00000004\n";
	std::vector<std::string> lines = CheatFileText::SplitLines(original);
	EXPECT_EQ_INT((int)lines.size(), 7);

	// Replace the first cheat (starts on line 3) with a shorter block.
	const std::vector<std::string> replacement = CheatFileText::SplitLines("_C1 Renamed\n_L 0x00000009 0x0000000A\n");
	EXPECT_TRUE(CheatFileText::ReplaceCheatBlock(lines, 3, replacement));
	EXPECT_EQ_STR(CheatFileText::JoinLines(lines, true),
		std::string("_S ULUS10000\n_G Test game\n_C1 Renamed\n_L 0x00000009 0x0000000A\n\n_C0 Second\n_L 0x00000003 0x00000004\n"));

	// Remove the second cheat by its line number in the current text (line 6).
	EXPECT_TRUE(CheatFileText::RemoveCheatBlock(lines, 6));
	EXPECT_EQ_STR(CheatFileText::JoinLines(lines, true),
		std::string("_S ULUS10000\n_G Test game\n_C1 Renamed\n_L 0x00000009 0x0000000A\n\n"));

	// Append a new cheat: the blank separator is already there, so none is added.
	CheatFileText::AppendCheatBlock(lines, CheatFileText::SplitLines("_C0 Third\n_L 0x00000007 0x00000008\n"));
	EXPECT_EQ_STR(CheatFileText::JoinLines(lines, true),
		std::string("_S ULUS10000\n_G Test game\n_C1 Renamed\n_L 0x00000009 0x0000000A\n\n_C0 Third\n_L 0x00000007 0x00000008\n"));

	// Appending to a file that does not end with a blank line inserts one.
	std::vector<std::string> tight = CheatFileText::SplitLines("_S ULUS10000\n_C0 First\n_L 0x1 0x2\n");
	CheatFileText::AppendCheatBlock(tight, CheatFileText::SplitLines("_C0 Second\n_L 0x3 0x4\n"));
	EXPECT_EQ_STR(CheatFileText::JoinLines(tight, true),
		std::string("_S ULUS10000\n_C0 First\n_L 0x1 0x2\n\n_C0 Second\n_L 0x3 0x4\n"));

	// Out of range line numbers are rejected, not clamped.
	EXPECT_FALSE(CheatFileText::ReplaceCheatBlock(lines, 0, replacement));
	EXPECT_FALSE(CheatFileText::RemoveCheatBlock(lines, 99));
	return true;
}

static bool TestCheatTextRoundTrip() {
	// A file with comments, several cheats and mixed line endings: extracting a block and
	// writing it back unchanged must reproduce the original bytes exactly.
	const std::string original =
		"\xEF\xBB\xBF_S ULUS10000\n"
		"_G Test game\n"
		"// a comment before any cheat\n"
		"_C0 First\n"
		"_L 0x00000001 0x00000002\n"
		"// comment inside the block\n"
		"_L 0x00000003 0x00000004\n"
		"\n"
		"_C1 Second\n"
		"_L 0x00000005 0x00000006\n";

	std::vector<std::string> lines = CheatFileText::SplitLines(original);
	const bool trailing = CheatFileText::EndsWithNewline(original);
	EXPECT_TRUE(trailing);

	for (int lineNum : { 4, 9 }) {
		const std::vector<std::string> block = CheatFileText::GetCheatBlock(lines, lineNum);
		EXPECT_TRUE(CheatFileText::ReplaceCheatBlock(lines, lineNum, block));
	}
	EXPECT_EQ_STR(CheatFileText::JoinLines(lines, trailing), original);
	return true;
}

// --- CheatFileParser ---

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
	// An unknown line type on line 2 and a _L line with only one value on line 4.
	EXPECT_FALSE(parser.ParseText(
		"_S ULUS10000\n"
		"_X0 Broken line type\n"
		"_C0 Missing values\n"
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
		"_C0 Commented\n"
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
	if (!TestCheatTextSplitJoin())
		return false;
	if (!TestCheatTextBlockExtent())
		return false;
	if (!TestCheatTextReplaceRemoveAppend())
		return false;
	if (!TestCheatTextRoundTrip())
		return false;
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
