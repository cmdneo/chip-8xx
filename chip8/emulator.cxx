#include <cstdint>
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <random>

#include "chip8.hxx"
#include "emulator.hxx"
#include "decoder.hxx"
#include "logger.hxx"

using std::array;
using std::begin;
using std::copy;
using std::end;
using std::uint8_t;
using std::chrono::steady_clock;

static std::uniform_int_distribution<unsigned> byte_dist(0, 255);

/// Returns digits at hundreds, tens and ones places respectively
static array<uint8_t, 3> bcd_encode(uint8_t x)
{
	uint8_t p100 = x / 100, p10 = (x % 100) / 10, p1 = x % 10;
	return {p100, p10, p1};
}

Logger Emulator::log("Emulator");

Emulator::Emulator(const std::vector<uint8_t> &rom)
{
	if (rom.size() > C8_RAM_SIZE - C8_PROG_START) {
		log("ROM size too big", "ERROR");
		error = true;
		return;
	}

	// Seed the random number generator
	std::random_device rdev;
	rand_gen.seed(rdev());
	// Copy fonts
	auto fontp = reinterpret_cast<const uint8_t *>(FONT_SPRITES);
	copy(fontp, fontp + sizeof(FONT_SPRITES), ram);
	// Load program
	copy(rom.begin(), rom.end(), ram + C8_PROG_START);
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

	// Instructions are 2 bytes long(big-endian)
	uint16_t bincode = (ram[pc] << 8) | ram[pc + 1];
	DecodedIns ins(bincode);
	// log("INS= " + ins.to_string());

	// Reference to Values of registers
	auto &vvx = regs[ins.vx];
	auto &vvy = regs[ins.vy];
	array<uint8_t, 3> bcd;

	switch (ins.type) {
	case I::CLS:
		std::fill(begin(screen), end(screen), 0);
		break;

	case I::RET:
		pc = stack[sp--];
		break;

	case I::SYS_a:
		// Ignored
		break;

	case I::JP_a:
		pc = ins.addr;
		break;

	case I::CALL_a:
		stack[sp++] = pc + C8_INS_LEN;
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
		regs[vvx] = byte_dist(rand_gen) & vvx;
		break;

	case I::DRW_v_v_n:
		draw_sprite(vvx, vvy, ins.nibble);
		break;

	case I::SKP_v:
		if (vvx == key)
			pc += C8_INS_LEN;
		break;

	case I::SKNP_v:
		if (vvx != key)
			pc += C8_INS_LEN;
		break;

	case I::LD_v_DT:
		vvx = dtimer;
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
		bcd = bcd_encode(vvx);
		copy(bcd.begin(), bcd.end(), ram + index);
		break;

	case I::LD_IM_v:
		copy(ram, ram + ins.vx + 1, regs);
		break;

	case I::LD_v_IM:
		copy(regs, regs + ins.vx + 1, ram);
		break;

	case I::ILLEGAL:
		return false;
		break;
	}

	switch (ins.type) {
	// For branches that are not skips and key_input instructions do nothing
	case I::RET:
	case I::JP_a:
	case I::CALL_a:
	case I::JP_V0_a:
	case I::LD_v_K:
		break;

	default:
		pc += C8_INS_LEN;
	}

	sp %= C8_STACK_HEIGHT;
	pc %= C8_RAM_SIZE;

	return true;
}

void Emulator::draw_sprite(uint8_t x, uint8_t y, uint8_t height)
{
	bool collision = false;
	for (unsigned i = 0; i != height; ++i) {
		auto yf = (y + i) % C8_SCREEN_HEIGHT;
		for (unsigned j = 0; j != 8; ++j) {
			auto xf = (x + j) % C8_SCREEN_WIDTH;
			// MSB to LSB - Left to right
			uint8_t tmp = screen[yf][xf] ^ ((ram[index + i] >> (7 - j)) & 1);
			collision = screen[yf][xf] && !tmp;
			screen[yf][xf] = tmp;
		}
	}

	if (collision)
		regs[C8_FLAG_REG] = 1;
}

void Emulator::update_timers(double dt)
{
	if (stimer > 0)
		stimer -= dt * C8_TIMER_FREQ;
	else
		stimer = 0;
	if (dtimer > 0)
		dtimer -= dt * C8_TIMER_FREQ;
	else
		dtimer = 0;
}
