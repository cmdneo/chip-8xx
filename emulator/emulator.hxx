#ifndef CHIP8_EMULATOR_HXX_INCLUDED
#define CHIP8_EMULATOR_HXX_INCLUDED

#include <cmath>
#include <cstdint>
#include <random>
#include <bitset>
#include <chrono>

#include "chip8.hxx"

using std::uint16_t;
using std::uint8_t;

class Emulator
{
public:
	Emulator(const uint8_t *rom_beg, const uint8_t *rom_end);
	operator bool() { return !error; }
	bool step();
	/// @brief Resets the internal clock used for timers
	void reset_clock() { last_time = std::chrono::steady_clock::now(); }
	bool pixel(int x, int y) { return screen[y][x]; }

	// Direct access is often needed
	uint8_t delay_timer() const { return std::lround(dtimer); }
	uint8_t sound_timer() const { return std::lround(stimer); }
	uint16_t fetch_ins(uint16_t n) const { return (ram[n] << 8) | ram[n + 1]; }
	uint16_t pc = C8_PROG_START;
	uint16_t index = 0;
	uint8_t sp = 0;
	uint8_t regs[C8_REG_CNT]{};
	uint8_t key = C8_KEY_NONE;

private:
	bool error = false;
	bool wait_for_key = false;
	// Smoothly count down, and convert to uint8_t for use
	float dtimer = 0;
	float stimer = 0;
	uint8_t key_reg = 0;
	uint16_t stack[C8_STACK_SIZE]{};
	uint8_t ram[C8_RAM_SIZE]{};
	std::bitset<C8_SCREEN_WIDTH> screen[C8_SCREEN_HEIGHT]{};
	std::default_random_engine rand_gen;
	std::chrono::steady_clock::time_point last_time;

	void draw_sprite(uint8_t x, uint8_t y, uint8_t height);
	void update_timers(double dt);
	/// Add and set the overflow flag if unsigned overflow occurs
	uint8_t add_with_ovf(uint8_t a, uint8_t b);
};

#endif // END emulator.hxx
