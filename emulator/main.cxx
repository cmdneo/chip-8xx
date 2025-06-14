#include <cstdint>
#include <cmath>
#include <array>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "raylib.h"
#include "raymath.h"

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

#define ARRAY_SIZE(arr) ((sizeof(arr)) / (sizeof(arr[0])))

/*
GUI-plan:
|--------------------------------|----------------|  <
|                                |                |  |
|      64x32 Emulator Screen     | Instruction    |  |
|      10px Square blocks        | Debug          |  |
|                                |                |  |
|                                |                | 320px
|                                |                |  |
|                                |                |  |
|                                |                |  |
|---------------|----------------|----------------|  <
|               |                | Register       |  |
|   Key press   |   Info box     | Debug          |  |
|   Debug       |                |                |  |
|               |                |                |  |
|               |                |                | 320px
|               |                |                |  |
|               |                |                |  |
|               |                |         xxxHz  |  |
|---------------|----------------|----------------|  <
|---- 320px ----|---- 320px -----|---- 320px -----|
*/

constexpr int SCREEN_W = 640 + 320;
constexpr int SCREEN_H = 640;
constexpr int INSTR_DECODE_CONTEXT = 5;
constexpr int FONT_SIZE = 32;
constexpr int LARGE_FONT_SIZE = 60;
constexpr int TEXT_PADDING = 10;
constexpr int LARGE_TEXT_PADDING = 20;

// Block sizes for elements
constexpr int PIXEL_BLOCK_SIZE = 10;
constexpr int KEY_BLOCK_SIZE = 80;
// Gap of (3/4 * font_size) looks fine
constexpr int FONT_LINE_HEIGHT = 3 * FONT_SIZE / 4;

constexpr Rectangle SCREEN_BOX = {0, 0, 640, 320};
constexpr Rectangle REG_DEBUG_BOX = {640, 320, 320, 320};
constexpr Rectangle INSTR_DEBUG_BOX = {640, 0, 320, 320};
constexpr Rectangle INFO_BOX = {320, 320, 320, 320};
constexpr Rectangle KEY_PRESS_DEBUG_BOX = {0, 320, 320, 320};
constexpr Vector2 KEY_SUBSCRIPT_OFFSET = {
	2. * KEY_BLOCK_SIZE / 5., KEY_BLOCK_SIZE / 2.
};

/*
Key Mapping:
Original C8 keys     Mapped to keys
|---|---|---|---|    |---|---|---|---|
| 1 | 2 | 3 | C |    | 1 | 2 | 3 | 4 |
| 4 | 5 | 6 | D |    | Q | W | E | R |
| 7 | 8 | 9 | E |    | A | S | D | F |
| A | 0 | B | F |    | Z | X | C | V |
|---|---|---|---|    |---|---|---|---|
*/

struct C8Key {
	int c8_keycode;
	const char *c8_key_name;
	const char *key_name;
};

// Map C8-key characters to keyboard characters. Since these are in their
// layout order, we also store the c8-keycode for identifying keys.
constexpr C8Key C8_KEY_NAME_MAP[C8_KEY_CNT] = {
	{0x1, "1", "1"}, {0x2, "2", "2"}, {0x3, "3", "3"}, {0xc, "C", "4"}, // Row 1
	{0x4, "4", "Q"}, {0x5, "5", "W"}, {0x6, "6", "E"}, {0xd, "D", "R"}, // Row 2
	{0x7, "7", "A"}, {0x8, "8", "S"}, {0x9, "9", "D"}, {0xe, "E", "F"}, // Row 3
	{0xa, "A", "Z"}, {0x0, "0", "X"}, {0xb, "B", "C"}, {0xf, "F", "V"}, // Row 4

};

// C8-keycodes to keyboard keys mapping, indexed by C8 keys in order: 0 to F.
constexpr int C8_KEY_MAP[C8_KEY_CNT] = {
	KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q,    KEY_W, KEY_E, KEY_A,
	KEY_S, KEY_D,   KEY_Z,   KEY_C,     KEY_FOUR, KEY_R, KEY_F, KEY_V,
};

enum AudioConfig {
	SAMPLE_RATE = 48000,
	SAMPLE_SIZE = 16,
};

constexpr Color COLOR_SUPER_DARK = {32, 32, 32, 255};
constexpr Color COLOR_DIM_BLUE = {40, 85, 125, 255};

inline Vector2 get_rect_pos(Rectangle r) { return Vector2{r.x, r.y}; }

static void fmt_registers(const Emulator &emu, vector<string> &reg_texts)
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
	reg_texts.clear();

	// Registers V0-VF
	for (int i = 0; i < C8_REG_CNT; ++i)
		reg_texts.push_back(fmt_reg(string(REGISTERS[i]), emu.regs[i]));
	// Internal Registers
	for (auto &[name, val] : INTERNAL_REG_VALS)
		reg_texts.push_back(fmt_reg(name, val));
}

static void fill_audio_buffer_cb(void *raw_data, unsigned frames)
{
	static double t = 0.0;
	const auto data = static_cast<int16_t *>(raw_data);
	for (unsigned i = 0; i < frames; ++i) {
		// Generate a tone by combining some frequencies
		const double wt = 2 * 3.14159 * t;
		double amp = std::sin(wt * 600) / 2;
		amp += std::sin(wt * 800) / 4;
		amp += std::sin(wt * 300) / 4;

		data[i] = static_cast<int16_t>(amp * (INT16_MAX - 1));
		t += 1.0 / static_cast<double>(SAMPLE_RATE);
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

	const std::string rom(
		(std::istreambuf_iterator<char>(rom_file)),
		std::istreambuf_iterator<char>()
	);
	auto rom_begin = reinterpret_cast<const uint8_t *>(rom.data());
	auto rom_end = reinterpret_cast<const uint8_t *>(rom.data() + rom.length());

	Emulator emu(rom_begin, rom_end);
	if (!emu) {
		clog << "Cannot initialize emulator.\n";
		return 1;
	}

	// Initialization:
	// Initialize Raylib, configure it and load resources.
	//------------------------------------------------------
	InitAudioDevice();
	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	InitWindow(SCREEN_W, SCREEN_H, "Chip-8 emulator");
	SetTargetFPS(60);

	// 48kHz, 16-bit, mono-audio. Generate audio when requested.
	AudioStream beep_stream = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, 1);
	SetAudioStreamCallback(beep_stream, fill_audio_buffer_cb);

	// Load Google Space-mono font.
	const Font mono_font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF,
		static_cast<int>(SPACE_MONO_REGULAR_TTF_LEN), FONT_SIZE, nullptr, 0
	);
	const Font large_mono_font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF,
		static_cast<int>(SPACE_MONO_REGULAR_TTF_LEN), LARGE_FONT_SIZE, nullptr,
		0
	);

	auto draw_padded_font = [mono_font](const char *s, Vector2 pos, Color col) {
		pos.x += static_cast<float>(2 * TEXT_PADDING);
		pos.y += static_cast<float>(TEXT_PADDING);
		DrawTextEx(mono_font, s, pos, FONT_SIZE, 0, col);
	};

	// Stores each register in format: <register_name> = <value>.
	vector<string> registers_debug_text;
	// Stores which keys are pressed for debug.
	bool keys_down[C8_KEY_CNT]{};
	// Earliest key in the list which was pressed.
	int pressed_key = C8_KEY_NONE;

	// State control
	int instr_per_frame = 5;
	bool paused = false;

	while (!WindowShouldClose()) {
		// Handle key UI presses
		//--------------------------------------------------
		// Only change speed if not paused
		if (!paused && IsKeyPressed(KEY_LEFT) && instr_per_frame > 1)
			instr_per_frame--;
		else if (!paused && IsKeyPressed(KEY_RIGHT))
			instr_per_frame++;
		if (IsKeyPressed(KEY_SPACE))
			paused = !paused;
		else if (IsKeyPressed(KEY_ENTER))
			emu = Emulator(rom_begin, rom_end);

		// If multiple keys are pressed then for the emulator we register
		// the key which was pressed earliest.
		// Therefore, if the same key is still pressed then maintain it.
		if (pressed_key != C8_KEY_NONE && IsKeyUp(C8_KEY_MAP[pressed_key]))
			pressed_key = C8_KEY_NONE;

		for (int i = 0; i < static_cast<int>(ARRAY_SIZE(keys_down)); ++i) {
			keys_down[i] = IsKeyDown(C8_KEY_MAP[i]);

			if (keys_down[i] && pressed_key == C8_KEY_NONE)
				pressed_key = i;
		}

		// Start drawing things
		//--------------------------------------------------
		BeginDrawing();
		ClearBackground(BLACK);

		DrawRectangleRec(REG_DEBUG_BOX, BLACK);
		DrawRectangleRec(INSTR_DEBUG_BOX, COLOR_SUPER_DARK);
		DrawRectangleRec(SCREEN_BOX, COLOR_DIM_BLUE);
		DrawRectangleRec(INFO_BOX, DARKGRAY);
		DrawRectangleRec(KEY_PRESS_DEBUG_BOX, BLACK);

		// Debug chip frequency(instructions per second), lower left corner.
		auto hz_str = to_string(GetFPS() * instr_per_frame) + "Hz";
		if (paused)
			hz_str = "PAUSED";
		draw_padded_font(
			hz_str.c_str(), {SCREEN_W - 120, SCREEN_H - 60}, RAYWHITE
		);

		// Draw decoded instructions
		for (int i = -INSTR_DECODE_CONTEXT; i <= INSTR_DECODE_CONTEXT; ++i) {
			string ins_str;
			auto new_pc = static_cast<long>(emu.pc) + C8_INS_LEN * i;

			// If at RAM boundary, then just print '~'
			if (new_pc < 0 || new_pc + C8_INS_LEN > C8_RAM_SIZE)
				ins_str = "~";
			else
				ins_str = DecodedIns(emu.fetch_ins(new_pc)).to_string();

			Vector2 pos = get_rect_pos(INSTR_DEBUG_BOX);
			pos.y += static_cast<float>(
				FONT_LINE_HEIGHT * (i + INSTR_DECODE_CONTEXT)
			);

			// Highlight the current instruction in golden color.
			Color color = i == 0 ? GOLD : RED;
			draw_padded_font(ins_str.c_str(), pos, color);
		}

		// Draw registers values
		fmt_registers(emu, registers_debug_text);
		for (unsigned i = 0; i < registers_debug_text.size(); ++i) {
			Vector2 pos = get_rect_pos(REG_DEBUG_BOX);

			// Put registers V0-VB(12) in the first column, rest in the second.
			auto row = static_cast<float>(i < 12 ? i : (i - 12));
			pos.x += i < 12 ? 0 : (INFO_BOX.width / 2);
			pos.y += row * FONT_LINE_HEIGHT;
			draw_padded_font(registers_debug_text[i].c_str(), pos, GREEN);
		}

		// Draw key presses
		for (unsigned i = 0; i < ARRAY_SIZE(keys_down); ++i) {
			// We need a 4x4 grid.
			Vector2 pos = get_rect_pos(KEY_PRESS_DEBUG_BOX);
			pos.x += static_cast<float>(KEY_BLOCK_SIZE * (i % 4));
			pos.y += static_cast<float>(KEY_BLOCK_SIZE * (i / 4));

			auto [keycode, c8_key_name, key_name] = C8_KEY_NAME_MAP[i];
			auto color = GRAY; // For keys not currently down.
			if (keys_down[keycode])
				color = MAROON; // For keys which are currently down.
			if (pressed_key == keycode)
				color = GOLD; // Key press which will be registered.

			Rectangle border{pos.x, pos.y, KEY_BLOCK_SIZE, KEY_BLOCK_SIZE};
			DrawRectangleLinesEx(border, 1., DARKGREEN);

			// C8-keyname along with its mapped keyname as subscript.
			pos.x += LARGE_TEXT_PADDING;
			DrawTextEx(
				large_mono_font, c8_key_name, pos, LARGE_FONT_SIZE, 0, color
			);
			pos = Vector2Add(pos, KEY_SUBSCRIPT_OFFSET);
			DrawTextEx(mono_font, key_name, pos, FONT_SIZE, 0, color);
		}
		// So that outer border is also 2px thick.
		DrawRectangleLinesEx(KEY_PRESS_DEBUG_BOX, 2., DARKGREEN);

		// Draw the emulator screen
		for (int y = 0; y < C8_SCREEN_HEIGHT; ++y) {
			for (int x = 0; x < C8_SCREEN_WIDTH; ++x) {
				if (!emu.screen[y][x])
					continue;

				auto sz = PIXEL_BLOCK_SIZE;
				DrawRectangle(sz * x, sz * y, sz, sz, WHITE);
			}
		}

		// Draw help text
		Vector2 pos = get_rect_pos(INFO_BOX);
		draw_padded_font("Left/Right: Speed(-/+)", pos, RAYWHITE);
		pos.y += static_cast<float>(FONT_LINE_HEIGHT);
		draw_padded_font("Space     : Play/Pause", pos, RAYWHITE);
		pos.y += static_cast<float>(FONT_LINE_HEIGHT);
		draw_padded_font("Enter     : Reset", pos, RAYWHITE);

		EndDrawing();
		//--------------------------------------------------

		// Update emulator at last.
		//--------------------------------------------------
		if (paused) {
			PauseAudioStream(beep_stream);
			emu.reset_clock(); // Stops timers while paused.
			continue;
		}

		// Run code...
		for (int i = 0; i < instr_per_frame; ++i) {
			emu.key = static_cast<uint8_t>(pressed_key);
			if (!emu.step())
				clog << "Emulator: Illegal instruction!\n";
		}

		// Beep play/pause as per sound timer.
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
	UnloadFont(large_mono_font);
	CloseWindow();

	return 0;
}
