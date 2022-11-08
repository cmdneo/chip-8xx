#include <cstdint>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>

#include "raylib/raylib.h"
#include "decoder.hxx"
#include "emulator.hxx"
#include "chip8.hxx"
#include "space_mono.bin.h"

using std::clog;
using std::int16_t;
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
|      10px Square blocks        | Debug          |  |
|                                |                |  |
|                                |                | 320px
|                                |                |  |
|                                |                |  |
|                                |                |  |
|--------------------------------|----------------|  <
|                                | Register       |  |
|                                | Debug          |  |
|                                |                |  |
|         Control Box            |                |  |
|                                |                | 320px
|                                |                |  |
|                                |                |  |
|                                |         xxxHz  |  |
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

enum AudioConfig {
	SAMPLE_RATE = 48000,
	SAMPLE_SIZE = 16,
};

enum UiConfig {
	BLOCK_SIZE = 10,
	SCREEN_W = 640 + 320,
	SCREEN_H = 640,
	INFO_BOX_W = 320,
	TEXT_PADDING = 10,
	INS_CONTEXT = 5,
	FONT_SIZE = 32,
	// Gap of (3/4 * font_size) looks fine
	FONT_LINE_HEIGHT = 3 * FONT_SIZE / 4,
};
constexpr static Vector2 REG_INFO_BOX = {640, 320};
constexpr static Vector2 INS_INFO_BOX = {640, 0};
constexpr static Vector2 CONTROL_BOX = {0, 320};

static void fmt_registers(const Emulator &emu, vector<string> &reg_txts)
{
	const std::pair<string, uint16_t> INTERNAL_REG_VALS[] = {
		{"PC", emu.pc},
		{"SP", emu.sp},
		{" I", emu.index},
		{"DT", emu.delay_timer()},
		{"ST", emu.sound_timer()},
	};

	auto fmt_reg = [](const string &reg_name, uint16_t val) {
		return reg_name + " = " + std::to_string(val);
	};
	reg_txts.clear();

	// Registers V0-VF
	for (int i = 0; i < C8_REG_CNT; ++i)
		reg_txts.push_back(fmt_reg(string(REGISTERS[i]), emu.regs[i]));
	// Internal Registers
	for (auto [name, val] : INTERNAL_REG_VALS)
		reg_txts.push_back(fmt_reg(name, val));
}

static void fill_audio_buffer_cb(void *raw_data, unsigned frames)
{
	static double t = 0.0;
	auto data = reinterpret_cast<int16_t *>(raw_data);
	for (unsigned i = 0; i < frames; ++i) {
		// Generate a tone by combining some frequencies
		double wt = 2 * 3.14159 * t;
		double amp = std::sin(wt * 600) / 2;
		amp += std::sin(wt * 800) / 4;
		amp += std::sin(wt * 300) / 4;
		data[i] = amp * (INT16_MAX - 1);
		t += 1.0 / SAMPLE_RATE;
	}
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

	// Init Raylib and configure it
	InitAudioDevice();
	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	InitWindow(SCREEN_W, SCREEN_H, "Chip-8 emulator");
	SetTargetFPS(60);

	// 48kHz, 16-bit, mono-audio, generate audio
	AudioStream beep_stream = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, 1);
	SetAudioStreamCallback(beep_stream, fill_audio_buffer_cb);

	// Load Google Space-mono font
	Font mono_font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF, SPACE_MONO_REGULAR_TTF_LEN, FONT_SIZE,
		nullptr, 0
	);
	auto draw_padded_font = [mono_font](const char *s, Vector2 pos, Color col) {
		pos.x += 2 * TEXT_PADDING;
		pos.y += TEXT_PADDING;
		DrawTextEx(mono_font, s, pos, FONT_SIZE, 0, col);
	};

	vector<string> regs_as_txt;
	// State control
	int ins_per_frame = 5;
	bool paused = false;

	while (!WindowShouldClose()) {
		// Handle key UI presses
		//--------------------------------------------------
		// Only change speed if not paused
		if (!paused && IsKeyPressed(KEY_LEFT) && ins_per_frame > 1)
			ins_per_frame--;
		else if (!paused && IsKeyPressed(KEY_RIGHT))
			ins_per_frame++;
		if (IsKeyPressed(KEY_SPACE))
			paused = !paused;
		else if (IsKeyPressed(KEY_R))
			emu = Emulator(rom, rom + bin_size);

		// Start drawing things
		//--------------------------------------------------
		BeginDrawing();
		ClearBackground(BLACK);

		// Magic numbers!? See GUI-plan diagram above ^^^
		// Register debug box, BLACK (Already painted background)
		// Instruction debug box, Daark
		DrawRectangle(640, 0, 320, 320, Color{32, 32, 32, 255});
		// Emulator screen, Dim blue
		DrawRectangle(0, 0, 640, 320, Color{40, 85, 125, 255});
		// Control box
		DrawRectangle(0, 320, 640, 320, DARKGRAY);

		// Debug frequency, Lower left corner
		auto hz_str = to_string(GetFPS() * ins_per_frame) + "Hz";
		if (paused)
			hz_str = "PAUSED";
		draw_padded_font(
			hz_str.c_str(), {SCREEN_W - 120, SCREEN_H - 60}, RAYWHITE
		);

		// Debug decoded instructions
		for (int i = -INS_CONTEXT; i <= INS_CONTEXT; ++i) {
			string ins_str;
			auto new_pc = static_cast<long>(emu.pc) + C8_INS_LEN * i;
			// If at RAM boundary, then just print '~'
			if (new_pc < 0 || new_pc + C8_INS_LEN > C8_RAM_SIZE)
				ins_str = "~";
			else
				ins_str = DecodedIns(emu.fetch_ins(new_pc)).to_string();
			Vector2 pos = INS_INFO_BOX;
			pos.y += FONT_LINE_HEIGHT * (i + INS_CONTEXT);
			// Highlight the current instruction GOLD
			draw_padded_font(ins_str.c_str(), pos, (i == 0 ? GOLD : RED));
		}

		// Debug registers
		fmt_registers(emu, regs_as_txt);
		for (unsigned i = 0; i < regs_as_txt.size(); ++i) {
			Vector2 pos = REG_INFO_BOX;
			// Put registers V0-VB(10) in the first column, rest in the second
			auto ypos_idx = i < 12 ? i : (i - 12);
			pos.x += (i < 12 ? 0 : INFO_BOX_W / 2);
			pos.y += ypos_idx * FONT_LINE_HEIGHT;
			draw_padded_font(regs_as_txt[i].c_str(), pos, GREEN);
		}

		// Draw the emulator screen
		for (int y = 0; y < C8_SCREEN_HEIGHT; ++y) {
			for (int x = 0; x < C8_SCREEN_WIDTH; ++x) {
				if (!emu.pixel(x, y))
					continue;
				auto sz = BLOCK_SIZE;
				DrawRectangle(sz * x, sz * y, sz, sz, WHITE);
			}
		}

		// Draw Help text
		Vector2 pos = CONTROL_BOX;
		draw_padded_font("Left/Right Arrow    : Speed(-/+)", pos, RAYWHITE);
		pos.y += FONT_LINE_HEIGHT;
		draw_padded_font("Space               : Play/Pause", pos, RAYWHITE);
		pos.y += FONT_LINE_HEIGHT;
		draw_padded_font("R                   : Reset", pos, RAYWHITE);

		EndDrawing();
		//--------------------------------------------------

		// Update emulator at last
		//--------------------------------------------------
		if (paused) {
			PauseAudioStream(beep_stream);
			emu.reset_clock(); // Stops timers while paused
			continue;
		}
		for (int i = 0; i < ins_per_frame; ++i) {
			if (!emu.step())
				clog << "Illegal instruction!\n";
		}
		emu.key = C8_KEY_NONE;
		for (int i = 0; i < C8_KEY_CNT; ++i) {
			if (IsKeyDown(C8_KEY_MAP[i])) {
				emu.key = i;
				break;
			}
		}
		if (emu.sound_timer() > 0)
			PlayAudioStream(beep_stream);
		else
			PauseAudioStream(beep_stream);
	}

	// Cleanup
	// -----------------------------------------------------
	StopAudioStream(beep_stream);
	UnloadAudioStream(beep_stream);
	CloseAudioDevice();
	UnloadFont(mono_font);
	CloseWindow();

	return 0;
}