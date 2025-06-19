#pragma once

#include <bitset>
#include <string>
#include <vector>

#include "raylib.h"

#include "chip8.hxx"
#include "emulator.hxx"

class EmulatorUi
{
public:
	explicit EmulatorUi(const Emulator &emu);
	~EmulatorUi();

	void draw();
	void play_beep() { PlayAudioStream(beep_stream); }
	void pause_beep() { PauseAudioStream(beep_stream); }

	static auto get_height() -> int;
	static auto get_width() -> int;

	std::bitset<C8_KEY_CNT> keys_down{};
	int pressed_key{};
	unsigned frequency{};

private:
	void draw_instruction_box();
	void draw_register_box();
	void draw_keypress_box();
	void draw_screen_box();
	void draw_info_box();
	void draw_padded_font(const char *s, Vector2 pos, Color color);

	const Emulator &emulator;
	Font font{};
	Font large_font{};
	AudioStream beep_stream{};

	// Stores information to be displayed on the UI.
	std::vector<std::string> register_texts{};
};