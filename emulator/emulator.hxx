#pragma once

#include <cmath>
#include <cstdint>
#include <array>
#include <random>
#include <bitset>
#include <chrono>

#include "chip8.hxx"

class Emulator
{
public:
	Emulator(const std::uint8_t *rom_beg, const std::uint8_t *rom_end);
	explicit operator bool() const { return !error; }
	auto step() -> bool;
	/// Resets the internal clock used for timers
	void reset_clock() { last_time = std::chrono::steady_clock::now(); }

	[[nodiscard]] auto delay_timer() const -> std::uint8_t
	{
		return std::lround(dtimer);
	}
	[[nodiscard]] auto sound_timer() const -> std::uint8_t
	{
		return std::lround(stimer);
	}
	[[nodiscard]] auto fetch_ins(std::uint16_t n) const -> std::uint16_t
	{
		return (ram[n % C8_RAM_SIZE] << 8) | ram[(n + 1) % C8_RAM_SIZE];
	}

	// Direct access is needed for displaying info
	std::uint16_t pc = C8_PROG_START;
	std::uint16_t index = 0;
	std::uint8_t sp = 0;
	std::uint8_t key = C8_KEY_NONE;
	std::array<std::uint8_t, C8_REG_CNT> regs{};
	std::array<std::bitset<C8_SCREEN_WIDTH>, C8_SCREEN_HEIGHT> screen{};

private:
	bool error = false;
	bool wait_for_key = false;
	// Smoothly count down, and convert to std::uint8_t for use
	double dtimer = 0;
	double stimer = 0;
	std::uint8_t key_reg = 0;
	std::array<std::uint16_t, C8_STACK_SIZE> stack{};
	std::array<std::uint8_t, C8_RAM_SIZE> ram{};
	std::default_random_engine rand_gen;
	std::chrono::steady_clock::time_point last_time;

	void draw_sprite(std::uint8_t x, std::uint8_t y, std::uint8_t height);
	void update_timers(double dt);
	/// Add and set the overflow flag if unsigned overflow occurs
	auto add_with_ovf(std::uint8_t a, std::uint8_t b) -> std::uint8_t;
};
