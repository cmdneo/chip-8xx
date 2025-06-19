#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>
#include <limits>
#include <numbers>
#include <string>
#include <utility>

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

#include "space_mono.bin.h"
#include "chip8.hxx"
#include "decoder.hxx"
#include "emulator_ui.hxx"

using std::pair;
using std::string;

/*
UI-plan:
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
constexpr int PIXEL_BLOCK_SIZE = 10;
constexpr int KEY_BLOCK_SIZE = 80;

constexpr int FONT_SIZE = 32;
constexpr int LARGE_FONT_SIZE = 60;
constexpr int TEXT_PADDING = 10;
constexpr int LARGE_TEXT_PADDING = 20;
constexpr int FONT_LINE_HEIGHT = 3 * FONT_SIZE / 4;
constexpr int INSTR_DECODE_CONTEXT = 5;

constexpr Rectangle SCREEN_BOX = {0, 0, 640, 320};
constexpr Rectangle REGISTER_BOX = {640, 320, 320, 320};
constexpr Rectangle INSTRUCTION_BOX = {640, 0, 320, 320};
constexpr Rectangle INFO_BOX = {320, 320, 320, 320};
constexpr Rectangle KEY_PRESSES_BOX = {0, 320, 320, 320};
constexpr Vector2 KEY_SUBSCRIPT_OFFSET = {
	2. * KEY_BLOCK_SIZE / 5., KEY_BLOCK_SIZE / 2.
};

constexpr Color COLOR_SUPER_DARK = {32, 32, 32, 255};
constexpr Color COLOR_DIM_BLUE = {40, 85, 125, 255};

constexpr unsigned SAMPLE_RATE = 44100;
constexpr unsigned SAMPLE_SIZE = 16;

struct C8Key {
	int c8_keycode;
	char c8_key_name;
	char key_name;
};

consteval inline auto c8k(int code, char c8name, char name)
{
	return C8Key{
		.c8_keycode = code,
		.c8_key_name = c8name,
		.key_name = name,
	};
}

// Map C8-key characters to keyboard characters. Since these are in their
// layout order, we also store the c8-keycode for identifying keys.
constexpr std::array<C8Key, C8_KEY_CNT> C8_KEY_NAME_MAP = {
	c8k(0x1, '1', '1'), c8k(0x2, '2', '2'),
	c8k(0x3, '3', '3'), c8k(0xc, 'C', '4'), // Row 1
	c8k(0x4, '4', 'Q'), c8k(0x5, '5', 'W'),
	c8k(0x6, '6', 'E'), c8k(0xd, 'D', 'R'), // Row 2
	c8k(0x7, '7', 'A'), c8k(0x8, '8', 'S'),
	c8k(0x9, '9', 'D'), c8k(0xe, 'E', 'F'), // Row 3
	c8k(0xa, 'A', 'Z'), c8k(0x0, '0', 'X'),
	c8k(0xb, 'B', 'C'), c8k(0xf, 'F', 'V'), // Row 4

};

class ColoredAreaOffseter
{
public:
	ColoredAreaOffseter(Rectangle area, Color area_color)
		: offx(area.x)
		, offy(area.y)
	{
		DrawRectangleRec(area, area_color);
		rlPushMatrix();
		rlTranslatef(offx, offy, 0);
	}

	~ColoredAreaOffseter() { rlPopMatrix(); }

private:
	int offx{};
	int offy{};
};

static void fill_audio_buffer_cb(void *raw_data, unsigned frames)
{
	static double t = 0.0;
	auto *data = static_cast<std::int16_t *>(raw_data);

	for (unsigned i = 0; i < frames; ++i) {
		// Generate a tone by combining some frequencies, it sounds ok.
		const double wt = 2 * std::numbers::pi * t;
		double amp = std::sin(wt * 600) / 2;
		amp += std::sin(wt * 800) / 4;
		amp += std::sin(wt * 300) / 4;
		amp = std::clamp(amp, -1.0, 1.0);

		data[i] = amp * (std::numeric_limits<std::int16_t>::max() - 1);
		t += 1.0 / SAMPLE_RATE;
	}
}

EmulatorUi::EmulatorUi(const Emulator &emu)
	: emulator(emu)
{
	font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF,
		static_cast<int>(SPACE_MONO_REGULAR_TTF_LEN), FONT_SIZE, nullptr, 0
	);
	large_font = LoadFontFromMemory(
		".ttf", SPACE_MONO_REGULAR_TTF,
		static_cast<int>(SPACE_MONO_REGULAR_TTF_LEN), LARGE_FONT_SIZE, nullptr,
		0
	);

	// 44kHz, 16-bit, mono-audio. Generate audio when requested.
	beep_stream = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, 1);
	SetAudioStreamCallback(beep_stream, fill_audio_buffer_cb);
}

EmulatorUi::~EmulatorUi()
{
	StopAudioStream(beep_stream);

	UnloadFont(font);
	UnloadFont(large_font);
	UnloadAudioStream(beep_stream);
}

auto EmulatorUi::get_height() -> int { return SCREEN_H; }
auto EmulatorUi::get_width() -> int { return SCREEN_W; }

void EmulatorUi::draw()
{
	draw_instruction_box();
	draw_register_box();
	draw_keypress_box();
	draw_screen_box();
	draw_info_box();

	// Draw chip frequency(instructions per second), lower left corner.
	auto hz_str = frequency == 0 ? "PAUSED" : std::to_string(frequency) + "Hz";
	draw_padded_font(hz_str.c_str(), {SCREEN_W - 120, SCREEN_H - 60}, RAYWHITE);
}

void EmulatorUi::draw_instruction_box()
{
	auto t = ColoredAreaOffseter(INSTRUCTION_BOX, COLOR_SUPER_DARK);

	// Draw decoded instructions
	for (int i = -INSTR_DECODE_CONTEXT; i <= INSTR_DECODE_CONTEXT; ++i) {
		string ins_str{};
		auto pc = static_cast<std::int32_t>(emulator.pc) + C8_INSTR_LEN * i;

		// If out of bounds, then just print '~'
		if (pc < 0 || pc + C8_INSTR_LEN > C8_RAM_SIZE) {
			ins_str = "~";
		} else {
			ins_str = DecodedIns(emulator.fetch_ins(pc)).to_string();
		}

		// Highlight the current instruction in golden color.
		Color color = i == 0 ? GOLD : RED;
		Vector2 pos{
			.x = 0,
			.y = static_cast<float>(
				FONT_LINE_HEIGHT * (i + INSTR_DECODE_CONTEXT)
			),
		};
		draw_padded_font(ins_str.c_str(), pos, color);
	}
}

static void fmt_registers(const Emulator &emu, std::vector<string> &reg_texts)
{
	const std::array<pair<string, std::uint16_t>, 5> SPECIAL_REG_VALS = {
		pair{"PC", emu.pc},
		pair{"SP", emu.sp},
		pair{" I", emu.index},
		pair{"DT", emu.delay_timer()},
		pair{"ST", emu.sound_timer()},
	};

	auto fmt_reg = [](const auto &reg_name, std::uint16_t val) {
		return reg_name + " = " + std::to_string(val);
	};
	reg_texts.clear();

	// Registers V0-VF
	for (int i = 0; i < C8_REG_CNT; ++i) {
		reg_texts.push_back(fmt_reg(string(REGISTERS[i]), emu.regs[i]));
	}
	// Special Registers
	for (auto &[name, val] : SPECIAL_REG_VALS) {
		reg_texts.push_back(fmt_reg(name, val));
	}
}

void EmulatorUi::draw_register_box()
{
	auto t = ColoredAreaOffseter(REGISTER_BOX, BLACK);

	fmt_registers(emulator, register_texts);
	for (unsigned i = 0; i < register_texts.size(); ++i) {
		// Put registers V0-VB(12) in the first column, rest in the second.
		auto row = static_cast<float>(i < 12 ? i : (i - 12));
		Vector2 pos{
			.x = i < 12 ? 0.0f : (REGISTER_BOX.width / 2),
			.y = row * FONT_LINE_HEIGHT,
		};
		draw_padded_font(register_texts[i].c_str(), pos, GREEN);
	}
}

void EmulatorUi::draw_keypress_box()
{
	auto t = ColoredAreaOffseter(KEY_PRESSES_BOX, BLACK);
	std::array<char, 2> buf{};

	for (unsigned i = 0; i < keys_down.size(); ++i) {
		// We need a 4x4 grid for the 16 keys present.
		Vector2 pos{
			.x = static_cast<float>(KEY_BLOCK_SIZE * (i % 4)),
			.y = static_cast<float>(KEY_BLOCK_SIZE * (i / 4)),
		};

		auto [keycode, c8_key_name, key_name] = C8_KEY_NAME_MAP[i];
		auto color = GRAY; // For keys not currently down.
		if (keys_down[keycode]) {
			color = MAROON; // For keys which are currently down.
		}
		if (pressed_key == keycode) {
			color = GOLD; // Key press which will be registered.
		}

		Rectangle border{pos.x, pos.y, KEY_BLOCK_SIZE, KEY_BLOCK_SIZE};
		DrawRectangleLinesEx(border, 1., DARKGREEN);

		// Draw c8-key.
		pos.x += LARGE_TEXT_PADDING;
		buf[0] = c8_key_name;
		DrawTextEx(large_font, buf.data(), pos, LARGE_FONT_SIZE, 0, color);
		// Draw the key c8-key is mapped to as a subscript.
		pos = Vector2Add(pos, KEY_SUBSCRIPT_OFFSET);
		buf[0] = key_name;
		DrawTextEx(font, buf.data(), pos, FONT_SIZE, 0, color);
	}

	// Make outer border also 2px thick.
	auto rect = KEY_PRESSES_BOX;
	rect.x = 0;
	rect.y = 0;
	DrawRectangleLinesEx(rect, 2., DARKGREEN);
}

void EmulatorUi::draw_screen_box()
{
	auto t = ColoredAreaOffseter(SCREEN_BOX, COLOR_DIM_BLUE);

	// Draw the emulator screen
	for (int y = 0; y < C8_SCREEN_HEIGHT; ++y) {
		for (int x = 0; x < C8_SCREEN_WIDTH; ++x) {
			if (!emulator.screen[y][x]) {
				continue;
			}
			auto sz = PIXEL_BLOCK_SIZE;
			DrawRectangle(sz * x, sz * y, sz, sz, WHITE);
		}
	}
}

void EmulatorUi::draw_info_box()
{
	auto t = ColoredAreaOffseter(INFO_BOX, DARKGRAY);

	Vector2 pos{};
	draw_padded_font("Left/Right: Speed(-/+)", pos, RAYWHITE);
	pos.y += static_cast<float>(FONT_LINE_HEIGHT);
	draw_padded_font("Space     : Play/Pause", pos, RAYWHITE);
	pos.y += static_cast<float>(FONT_LINE_HEIGHT);
	draw_padded_font("Enter     : Reset", pos, RAYWHITE);
}

void EmulatorUi::draw_padded_font(const char *s, Vector2 pos, Color color)
{
	pos.x += 2 * TEXT_PADDING;
	pos.y += TEXT_PADDING;
	DrawTextEx(font, s, pos, FONT_SIZE, 0, color);
}
