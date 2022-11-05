#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "raylib/raylib.h"
#include "emulator.hxx"
#include "chip8.hxx"
#include "space_mono.bin.h"

using std::clog;
using std::string;
using std::to_string;
using std::uint8_t;
using std::vector;

/*
GUI-plan:

|------------ 640px -------------|---- 320px -----|
|--------------------------------|----------------|  <
|                                |                |  |
|      64x32 Emualator Screen    | Instrustion    |  |
|      10px Square blocks        | List (Dynamic) |  |
|                                |                |  |
|                                |                | 320px
|                                |                |  |
|                                |                |  |
|                                |                |  |
|--------------------------------|----------------|  <
|                                |                |  |
|                                |                |  |
|                                |                |  |
|                                |                |  |
|                                |                | 320px
|                                |                |  |
|                                |                |  |
|                                |                |  |
|--------------------------------|----------------|  <

Key Mapping:
Original C8 keys     Mapped to keys
|---|---|---|---|    |---|---|---|---|
| 1 | 2 | 3 | C |    | 1 | 2 | 3 | 4 |
| 4 | 5 | 6 | D |    | Q | W | E | R |
| 7 | 8 | 9 | E |    | A | S | D | F |
| A | 0 | B | F |    | Z | X | C | V |
|---|---|---|---|    |---|---|---|---|
*/

static constexpr int C8_KEY_MAP[C8_KEY_CNT] = {
	KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q,    KEY_W, KEY_E, KEY_A,
	KEY_S, KEY_D,   KEY_Z,   KEY_C,     KEY_FOUR, KEY_R, KEY_F, KEY_V,
};

enum UiConfig {
	BLOCK_SIZE = 10,
	SCREEN_W = 640 + 320,
	SCREEN_H = 640,
	SIDE_BOX_W = 320,
	TEXT_PADDING = 10,
	FONT_SIZE = 32,
	// Gap of (3/4 * font_size) looks fine
	FONT_LINE_HEIGHT = 3 * FONT_SIZE / 4,
};
constexpr static Vector2 REG_INFO_BOX = {640, 320};

static void fmt_registers(const Emulator &emu, vector<string> &reg_fmt)
{
	static const std::string REGISTER_TEMPLATE[] = {
		" V0 = ", " V1 = ", " V2 = ", " V3 = ", " V4 = ", " V5 = ", " V6 = ",
		" V7 = ", " V8 = ", " V9 = ", "V10 = ", "V11 = ", "V12 = ", "V13 = ",
		"V14 = ", "V15 = ", " PC = ", " SP = ", "  I = ", " DT = ", " ST = ",
	};

	reg_fmt.clear();
	// Registers are: V0-V15
	int i = 0;
	for (auto rval : emu.regs) {
		reg_fmt.push_back(REGISTER_TEMPLATE[i] + to_string(rval));
		i++;
	}
	// and internal ones: PC, SP, I, ST, DT;
	reg_fmt.push_back(REGISTER_TEMPLATE[i++] + to_string(emu.pc));
	reg_fmt.push_back(REGISTER_TEMPLATE[i++] + to_string(emu.sp));
	reg_fmt.push_back(REGISTER_TEMPLATE[i++] + to_string(emu.index));
	reg_fmt.push_back(REGISTER_TEMPLATE[i++] + to_string(emu.delay_timer()));
	reg_fmt.push_back(REGISTER_TEMPLATE[i++] + to_string(emu.sound_timer()));
}

int main(int argc, char const **argv)
{
	if (argc != 2) {
		auto name = argc > 0 ? argv[0] : "c8asm";
		clog << "Usage: " << name << " <rom-filename>\n";
		return 1;
	}
	std::ifstream rom_file(argv[1], std::ios::binary);
	if (!rom_file) {
		clog << "Cannot open file '" << argv[1] << "'\n";
		return 1;
	}

	// Only read upto 4K, that's max we need
	uint8_t rom[C8_RAM_SIZE]{};
	int bin_size
		= rom_file.readsome(reinterpret_cast<char *>(rom), sizeof(rom));
	Emulator emu(rom, rom + bin_size);
	if (!emu) {
		clog << "Cannot initialize emulator\n";
		return 1;
	}

	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(SCREEN_W, SCREEN_H, "Chip-8 emulator");
	SetTargetFPS(60);

	Font mono_font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF, SPACE_MONO_REGULAR_TTF_LEN, FONT_SIZE,
		nullptr, 0
	);
	vector<string> regs_as_txt;

	while (!WindowShouldClose()) {
		//--------------------------------------------------
		for (int i = 0; i < 10; ++i) {
			if (!emu.step())
				clog << "Illegal instruction!\n";
		}
		fmt_registers(emu, regs_as_txt);
		emu.key = C8_KEY_NONE;
		for (int i = 0; i < C8_KEY_CNT; ++i) {
			if (IsKeyDown(C8_KEY_MAP[i])) {
				emu.key = i;
				break;
			}
		}

		//--------------------------------------------------
		BeginDrawing();
		ClearBackground(BLACK);
		DrawRectangle(0, 0, 640, 320, DARKBLUE);
		DrawRectangle(640, 0, 320, 320, DARKGRAY);
		DrawRectangle(0, 320, 640, 320, DARKGRAY);

		// Debug registers
		for (unsigned i = 0; i < regs_as_txt.size(); ++i) {
			Vector2 pos = REG_INFO_BOX;
			auto ypos_idx = i < 12 ? i : (i - 12);
			pos.x += TEXT_PADDING + (i < 12 ? 0 : SIDE_BOX_W / 2);
			pos.y += TEXT_PADDING + ypos_idx * FONT_LINE_HEIGHT;
			DrawTextEx(
				mono_font, regs_as_txt[i].c_str(), pos, mono_font.baseSize, 0,
				GREEN
			);
		}

		// Draw the emulator screen
		for (int y = 0; y < C8_SCREEN_HEIGHT; ++y) {
			for (int x = 0; x < C8_SCREEN_WIDTH; ++x) {
				if (!emu.pixel(x, y))
					continue;
				DrawRectangle(
					BLOCK_SIZE * x, BLOCK_SIZE * y, BLOCK_SIZE, BLOCK_SIZE,
					WHITE
				);
			}
		}

		EndDrawing();
		//--------------------------------------------------
	}

	UnloadFont(mono_font);
	CloseWindow();

	return 0;
}