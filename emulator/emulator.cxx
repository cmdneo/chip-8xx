#include <cstdint>
#include <iostream>
#include <algorithm>
#include <bitset>
#include <chrono>
#include <random>

#include "chip8.hxx"
#include "emulator.hxx"
#include "decoder.hxx"

using std::array;
using std::begin;
using std::copy;
using std::end;
using std::uint8_t;
using std::chrono::steady_clock;

static std::uniform_int_distribution<unsigned> random_byte_distbr(0, 255);

Emulator::Emulator(const uint8_t *rom_beg, const uint8_t *rom_end)
{
	constexpr auto rom_max = C8_RAM_SIZE - C8_PROG_START;
	if (rom_end - rom_beg > rom_max) {
		std::clog << "Emulator: ROM size too big! "
				  << "Maximum is " << rom_max << " bytes\n";
		error = true;
		return;
	}

	// Seed the random number generator
	std::random_device rdev;
	rand_gen.seed(rdev());

	// Start the clock now!
	reset_clock();

	// Copy fonts to the begining of ROM.
	auto fontp = reinterpret_cast<const uint8_t *>(FONT_SPRITES);
	copy(fontp, fontp + sizeof(FONT_SPRITES), ram);

	// Load program into the RAM from the ROM provided.
	copy(rom_beg, rom_end, ram + C8_PROG_START);
}

bool Emulator::step()
{
	using I = Instruction;
	std::chrono::duration<double> dt = steady_clock::now() - last_time;
	last_time = steady_clock::now();
	update_timers(dt.count());

	if (wait_for_key) {
		if (key != C8_KEY_NONE) {
			pc += C8_INS_LEN;
			regs[key_reg] = key;
			wait_for_key = false;
		} else {
			return true;
		}
	}

	// Wrap evey indexing variable around before accessing data from array
	// to prevent out of bounds access
	// Instructions are 2 bytes long(big-endian)
	uint16_t bincode = fetch_ins(pc % C8_RAM_SIZE);
	DecodedIns ins(bincode);

	// Reference to Values of registers
	auto &vvx = regs[ins.vx];
	auto &vvy = regs[ins.vy];

	switch (ins.type) {
	case I::CLS:
		std::fill(begin(screen), end(screen), 0);
		break;

	case I::RET:
		pc = stack[--sp % C8_STACK_SIZE];
		break;

	case I::SYS_a:
		// Ignored
		break;

	case I::JP_a:
		pc = ins.addr;
		break;

	case I::CALL_a:
		stack[sp++ % C8_STACK_SIZE] = pc + C8_INS_LEN;
		pc = ins.addr;
		break;

	case I::SE_v_b:
		if (vvx == ins.byte)
			pc += C8_INS_LEN;
		break;

	case I::SNE_v_b:
		if (vvx != ins.byte)
			pc += C8_INS_LEN;
		break;

	case I::SE_v_v:
		if (vvx == vvy)
			pc += C8_INS_LEN;
		break;

	case I::LD_v_b:
		vvx = ins.byte;
		break;

	case I::ADD_v_b:
		// Carry flag is not changed for this instruction
		vvx += ins.byte;
		break;

	case I::LD_v_v:
		vvx = vvy;
		break;

	case I::OR_v_v:
		vvx |= vvy;
		break;

	case I::AND_v_v:
		vvx &= vvy;
		break;

	case I::XOR_v_v:
		vvx ^= vvy;
		break;

	case I::ADD_v_v:
		vvx = add_with_ovf(vvx, vvy);
		break;

	case I::SUB_v_v:
		vvx = add_with_ovf(vvx, ~vvy + 1);
		break;

	case I::SHR_v:
		regs[C8_FLAG_REG] = vvx & 1;
		vvx >>= 1;
		break;

	case I::SUBN_v_v:
		vvx = add_with_ovf(vvy, ~vvx + 1);
		break;

	case I::SHL_v:
		regs[C8_FLAG_REG] = (vvx >> 7) & 1; // regs[x] is 8-bits
		vvx <<= 1;
		break;

	case I::SNE_v_v:
		if (vvx != vvy)
			pc += C8_INS_LEN;
		break;

	case I::LD_I_a:
		index = ins.addr;
		break;

	case I::JP_V0_a:
		pc = regs[0] + ins.addr;
		break;

	case I::RND_v_b:
		vvx = random_byte_distbr(rand_gen) & ins.byte;
		break;

	case I::DRW_v_v_n:
		draw_sprite(vvx, vvy, ins.nibble);
		break;

	case I::SKP_v:
		if (key != C8_KEY_NONE && vvx == key)
			pc += C8_INS_LEN;
		break;

	case I::SKNP_v:
		if (key == C8_KEY_NONE || vvx != key)
			pc += C8_INS_LEN;
		break;

	case I::LD_v_DT:
		vvx = delay_timer();
		break;

	case I::LD_v_K:
		key_reg = ins.vx;
		wait_for_key = true;
		break;

	case I::LD_DT_v:
		dtimer = vvx;
		break;

	case I::LD_ST_v:
		stimer = vvx;
		break;

	case I::ADD_I_v:
		index += vvx;
		break;

	case I::LD_F_v:
		index = sizeof(FONT_SPRITES[0]) * vvx;
		break;

	case I::LD_B_v:
		// Instead of overflowing wrap around
		ram[(index + 0) % C8_RAM_SIZE] = vvx / 100;
		ram[(index + 1) % C8_RAM_SIZE] = (vvx % 100) / 10;
		ram[(index + 2) % C8_RAM_SIZE] = vvx % 10;
		break;

	case I::LD_IM_v:
		for (unsigned i = 0; i <= ins.vx; ++i)
			ram[(index + i) % C8_RAM_SIZE] = regs[i];
		break;

	case I::LD_v_IM:
		for (unsigned i = 0; i <= ins.vx; ++i)
			regs[i] = ram[(index + i) % C8_RAM_SIZE];
		break;

	case I::ILLEGAL:
		return false;
		break;
	}

	switch (ins.type) {
	// Branche instructions(except skip instructions) set PC themselves.
	// Keypress instruction blocks until a keypress is detected and then
	// it increments the PC. So for these cases do not touch PC.
	case I::RET:
	case I::JP_a:
	case I::CALL_a:
	case I::JP_V0_a:
	case I::LD_v_K:
		break;

	default:
		pc += C8_INS_LEN;
		break;
	}

	return true;
}

void Emulator::draw_sprite(uint8_t x, uint8_t y, uint8_t height)
{
	bool collision = false;
	for (unsigned i = 0; i != height; ++i) {
		auto yf = (y + i) % C8_SCREEN_HEIGHT;
		for (unsigned j = 0; j != 8; ++j) {
			auto xf = (x + j) % C8_SCREEN_WIDTH;

			// We XOR the current pixels with the sprite
			// If an ON pixel goes OFF then we call that a collision
			// MSB to LSB - Left to right
			auto img_px = (ram[(index + i) % C8_RAM_SIZE] >> (7 - j)) & 1;
			uint8_t new_px = screen[yf][xf] ^ img_px;

			if (screen[yf][xf] && !new_px)
				collision = true;
			screen[yf][xf] = new_px;
		}
	}

	regs[C8_FLAG_REG] = collision;
}

void Emulator::update_timers(double dt)
{
	stimer -= dt * C8_TIMER_FREQ;
	dtimer -= dt * C8_TIMER_FREQ;
	if (stimer < 0)
		stimer = 0;
	if (dtimer < 0)
		dtimer = 0;
}

uint8_t Emulator::add_with_ovf(uint8_t a, uint8_t b)
{
	// Usigned overflow is well-defined in C++
	uint8_t s = a + b;
	regs[C8_FLAG_REG] = s < a || s < b;
	return s;
}
