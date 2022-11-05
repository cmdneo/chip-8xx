#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "raylib/raylib.h"
#include "emulator.hxx"
#include "chip8.hxx"
#include "space_mono.bin.h"

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
	MONO_FONT_SIZE = 32,
	// Gap of (3/4 * font_size) looks fine
	MONO_FONT_HEIGHT = 3 * MONO_FONT_SIZE / 4,
};

int main(int argc, char const **argv)
{
	if (argc != 2) {
		auto name = argc > 0 ? argv[0] : "c8asm";
		std::clog << "Usage: " << name << " <rom-filename>\n";
		return 1;
	}
	std::ifstream rom_file(argv[1], std::ios::binary);
	if (!rom_file) {
		std::clog << "Cannot open file '" << argv[1] << "'\n";
		return 1;
	}

	// Only read upto 4K, that's all we need
	// 3.5K to be accurate
	char raw_rom[C8_RAM_SIZE];
	int bin_size = rom_file.readsome(raw_rom, sizeof(raw_rom));
	Emulator emu(std::vector<uint8_t>(raw_rom, raw_rom + bin_size));
	if (!emu) {
		std::clog << "Cannot initialize emulator\n";
		return 1;
	}

	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(SCREEN_W, SCREEN_H, "Chip-8 emulator");
	SetTargetFPS(60);

	Font mono_font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF, SPACE_MONO_REGULAR_TTF_LEN,
		MONO_FONT_SIZE, nullptr, 0
	);

	while (!WindowShouldClose()) {
		//--------------------------------------------------
		emu.step();
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
		DrawRectangle(0, 320, 640, 320, SKYBLUE);

		for (int i = 0; i < C8_REG_CNT; ++i) {
			auto txt
				= "V" + std::to_string(i) + " = " + std::to_string(emu.regs[i]);
			if (i < 10)
				txt = " " + txt;
			Vector2 pos = {650, 320.0f + i * MONO_FONT_HEIGHT};
			if (i >= 12) {
				pos.x += 320 / 2;
				pos.y = 320.0f + (i - 12) * MONO_FONT_HEIGHT;
			}
			DrawTextEx(
				mono_font, txt.c_str(), pos, mono_font.baseSize, 0, GREEN
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