#include <cstddef>
#include <string>

#include "chip8.hxx"
#include "decoder.hxx"

using std::string;

static inline uint16_t getbits(uint16_t word, uint8_t offset, uint8_t n)
{
	return (word >> offset) & ~(~0U << n);
}

DecodedIns::DecodedIns(uint16_t ins)
{
	using I = Instruction;
	// For ms_nib == 0x8, data
	constexpr static I ms_nib_x8_map[] = {
		I::LD_v_v,  I::OR_v_v,  I::AND_v_v, I::XOR_v_v,
		I::ADD_v_v, I::SUB_v_v, I::SHR_v,   I::SUBN_v_v,
	};

	// Refer chip8.md for info about instruction encoding
	// Encoding formats (MSB left):
	// oooo xxxx yyyy oooo
	// oooo aaaa aaaa aaaa
	// oooo xxxx oooo oooo
	// oooo xxxx bbbb bbbb
	// oooo xxxx yyyy nnnn
	// Symbols: x - Vx,   y - Vy,     o - opcode
	//          b - byte, n - nibble, a - address
	bincode = ins;
	vx = getbits(ins, C8_VX_OFFSET, 4);
	vy = getbits(ins, C8_VY_OFFSET, 4);
	addr = getbits(ins, 0, 12);
	nibble = getbits(ins, 0, 4);
	byte = getbits(ins, 0, 8);
	type = I::ILLEGAL;
	auto ms_nib = getbits(ins, 12, 4); // Most significant nibble

	switch (ms_nib) {
	case 0x0:
		if (ins == 0x00E0)
			type = I::CLS;
		else if (ins == 0x00EE)
			type = I::RET;
		else
			type = I::SYS_a;
		break;

	case 0x1:
		type = I::JP_a;
		break;
	case 0x2:
		type = I::CALL_a;
		break;
	case 0x3:
		type = I::SE_v_b;
		break;
	case 0x4:
		type = I::SNE_v_b;
		break;
	case 0x5:
		type = I::SE_v_v;
		break;
	case 0x6:
		type = I::LD_v_b;
		break;
	case 0x7:
		type = I::ADD_v_b;
		break;

	case 0x8:
		if (nibble <= 0x7)
			type = ms_nib_x8_map[nibble];
		else if (nibble == 0xE)
			type = I::SHL_v;
		break;

	case 0x9:
		type = I::SNE_v_v;
		break;
	case 0xa:
		type = I::LD_I_a;
		break;
	case 0xb:
		type = I::JP_V0_a;
		break;
	case 0xc:
		type = I::RND_v_b;
		break;
	case 0xd:
		type = I::DRW_v_v_n;
		break;

	case 0xe:
		if (byte == 0x9E)
			type = I::SKP_v;
		else if (byte == 0xA1)
			type = I::SKNP_v;
		break;

	case 0xf:
		switch (byte) {
		case 0x07:
			type = I::LD_v_DT;
			break;
		case 0x0A:
			type = I::LD_v_K;
			break;
		case 0x15:
			type = I::LD_DT_v;
			break;
		case 0x18:
			type = I::LD_ST_v;
			break;
		case 0x1E:
			type = I::ADD_I_v;
			break;
		case 0x29:
			type = I::LD_F_v;
			break;
		case 0x33:
			type = I::LD_B_v;
			break;
		case 0x55:
			type = I::LD_IM_v;
			break;
		case 0x65:
			type = I::LD_v_IM;
			break;
		}
		break;
	}
}

/// @brief Instruction format string
const static string INSTRUCTION_FMT[] = {
	"CLS",      "RET",      "SYS a",    "JP a",     "CALL a",    "SE x, b",
	"SNE x, b", "SE x, x",  "LD x, b",  "ADD x, b", "LD x, y",   "OR x, y",
	"AND x, y", "XOR x, y", "ADD x, y", "SUB x, y", "SHR x",     "SUBN x, y",
	"SHL x",    "SNE x, y", "LD I, a",  "JP V0,a",  "RND x, b",  "DRW x, y, n",
	"SKP x",    "SKNP x",   "LD x, DT", "LD x, K",  "LD DT, x",  "LD ST, x",
	"ADD I, x", "LD F, x",  "LD B, x",   "LD [I], x",   "LD x, [I]",
};

// Replaces once
static void replace_char(string &s, char old, const string &new_)
{
	if (auto at = s.find(old); at != string::npos) {
		s.replace(at, 1, new_);
	}
}

string DecodedIns::to_string()
{
	if (type == Instruction::ILLEGAL)
		return "<! DECODING ERROR !>";

	string ret = INSTRUCTION_FMT[static_cast<int>(type)];
	replace_char(ret, 'a', std::to_string(addr));
	replace_char(ret, 'b', std::to_string(byte));
	replace_char(ret, 'n', std::to_string(nibble));
	replace_char(ret, 'x', "V" + std::to_string(vx));
	replace_char(ret, 'y', "V" + std::to_string(vy));
	return ret;
}
