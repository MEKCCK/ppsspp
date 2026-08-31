#pragma once

#include "Common/UI/Context.h"

// Monster Hunter HP overlay — in-emulator port of
// https://github.com/Alexander-Lancellott/MH-HP-Overlay-For-PSP-Emulator
//
// Reads monster HP directly from PSP RAM (same pointer chains as the PC overlay)
// and draws it on top of the game window while a supported Monster Hunter game
// is running. Also respects PPSSPP's in-game language for monster names
// (zh* -> Chinese, anything else -> English).
//
// Wired into the "Debug overlay" developer setting (DebugOverlay::MH_HP).
void DrawMHOverlay(UIContext *ctx, const Bounds &bounds);

// True if the currently booted game is supported by the MH HP overlay.
// Used by UI code to show a quick toggle only for supported games.
bool MHOverlay_SupportsCurrentGame();