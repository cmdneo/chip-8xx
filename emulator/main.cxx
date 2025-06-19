#include <array>
#include <bitset>
#include <iostream>
#include <iterator>
#include <fstream>
#include <string>

#include "raylib.h"

#include "chip8.hxx"
#include "decoder.hxx"
#include "emulator.hxx"
#include "emulator_ui.hxx"

using std::clog;

/*
Key Mapping:
Original C8 keys  -> Mapped to keys
-----------------    -----------------
| 1 | 2 | 3 | C |    | 1 | 2 | 3 | 4 |
| 4 | 5 | 6 | D |    | Q | W | E | R |
| 7 | 8 | 9 | E |    | A | S | D | F |
| A | 0 | B | F |    | Z | X | C | V |
-----------------    -----------------
*/

// C8-keycodes to keyboard keys mapping, indexed by C8 keys in order: 0 to F.
constexpr std::array<int, C8_KEY_CNT> C8_KEY_MAP = {
	KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q,    KEY_W, KEY_E, KEY_A,
	KEY_S, KEY_D,   KEY_Z,   KEY_C,     KEY_FOUR, KEY_R, KEY_F, KEY_V,
};

auto main(int argc, char const **argv) -> int
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
	auto screen_w = EmulatorUi::get_width();
	auto screen_h = EmulatorUi::get_height();

	// Initialization:
	// Initialize Raylib, configure it and load resources.
	//------------------------------------------------------
	InitAudioDevice();
	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	InitWindow(screen_w, screen_h, "Chip-8 emulator");
	SetTargetFPS(60);

	// State control and UI.
	int instr_per_frame = 5;
	int pressed_key = C8_KEY_NONE;
	bool paused = false;
	std::bitset<C8_KEY_CNT> keys_down{};
	EmulatorUi emu_ui(emu);

	while (!WindowShouldClose()) {
		// Handle key presses
		//--------------------------------------------------
		// Only change speed if not paused
		if (!paused && IsKeyPressed(KEY_LEFT) && instr_per_frame > 1) {
			instr_per_frame--;
		} else if (!paused && IsKeyPressed(KEY_RIGHT)) {
			instr_per_frame++;
		}
		if (IsKeyPressed(KEY_SPACE)) {
			paused = !paused;
		} else if (IsKeyPressed(KEY_ENTER)) {
			emu = Emulator(rom_begin, rom_end);
		}

		// If multiple keys are pressed for the emulator we register
		// the key which was pressed earliest.
		// Therefore, if the same key is still pressed then maintain it.
		if (pressed_key != C8_KEY_NONE && IsKeyUp(C8_KEY_MAP[pressed_key])) {
			pressed_key = C8_KEY_NONE;
		}
		for (unsigned i = 0; i < keys_down.size(); ++i) {
			keys_down[i] = IsKeyDown(C8_KEY_MAP[i]);
			if (keys_down[i] && pressed_key == C8_KEY_NONE) {
				pressed_key = i;
			}
		}

		// Update the UI state
		emu_ui.keys_down = keys_down;
		emu_ui.pressed_key = pressed_key;
		emu_ui.frequency = paused ? 0 : GetFPS() * instr_per_frame;

		// Start drawing things
		//--------------------------------------------------
		BeginDrawing();
		{
			ClearBackground(BLACK);
			emu_ui.draw();
		}
		EndDrawing();

		// Update emulator at last.
		//--------------------------------------------------
		if (paused) {
			emu.reset_clock(); // effectively halts timers while paused.
			emu_ui.pause_beep();
			continue;
		}

		// Run code.
		for (int i = 0; i < instr_per_frame; ++i) {
			emu.key = static_cast<uint8_t>(pressed_key);
			emu.step();
		}

		// Beep play/pause as per sound timer.
		if (emu.sound_timer() > 0) {
			emu_ui.play_beep();
		} else {
			emu_ui.pause_beep();
		}
	}

	// Cleanup
	// -----------------------------------------------------
	CloseAudioDevice();
	CloseWindow();

	return 0;
}
