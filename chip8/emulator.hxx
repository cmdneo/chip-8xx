#ifndef CHIP8_EMULATOR_HXX_INCLUDED
#define CHIP8_EMULATOR_HXX_INCLUDED

#include <cstdint>
#include <vector>
#include <random>
#include <bitset>
#include <chrono>

#include "chip8.hxx"
#include "logger.hxx"

using std::uint16_t;
using std::uint8_t;

class Emulator
{
public:
	Emulator(const std::vector<uint8_t> &rom);
	operator bool() { return !error; }
	bool step();
	bool pixel(int x, int y) { return screen[y][x]; }

	// Direct access is often needed
	uint8_t delay_timer() { return dtimer; }
	uint8_t sound_timer() { return stimer; }
	uint16_t pc = C8_PROG_START;
	uint16_t index = 0;
	uint8_t sp = 0;
	uint8_t regs[C8_REG_CNT]{};
	uint8_t key = C8_KEY_NONE;

private:
	static Logger log;
	std::default_random_engine rand_gen;
	std::chrono::time_point<std::chrono::steady_clock> last_time;
	bool error = false;
	bool wait_for_key = false;
	// Smoothly count down, and convert to uint8_t for use
	float dtimer = 0;
	float stimer = 0;
	uint8_t key_reg = 0;
	uint16_t stack[C8_STACK_HEIGHT]{};
	uint8_t ram[C8_RAM_SIZE]{};
	std::bitset<C8_SCREEN_WIDTH> screen[C8_SCREEN_HEIGHT]{};
	void draw_sprite(uint8_t x, uint8_t y, uint8_t height);
	void update_timers(double dt);

	/// Add and set the overflow flag if unsigned overflow occurs
	uint8_t add_with_ovf(uint8_t a, uint8_t b)
	{
		// Usigned overflow is well-defined in C++
		auto s = a + b;
		if (s < a || s < b)
			regs[C8_FLAG_REG] = 1;
		return s;
	}
};

#endif // END emulator.hxx