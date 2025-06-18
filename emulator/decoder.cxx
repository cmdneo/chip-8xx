#include <string>
#include <string_view>

#include "chip8.hxx"
#include "decoder.hxx"

using std::string;
using std::string_view;

static inline uint16_t get_bits(uint16_t word, uint8_t offset, uint8_t n)
{
	return (word >> offset) & ~(~0U << n);
}

DecodedIns::DecodedIns(uint16_t ins)
{
	using I = Instruction;
	// For ms_opcode == 0x8, table driven classification.
	constexpr I ms_opcode_x8_map[] = {
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
	vx = get_bits(ins, C8_VX_OFFSET, 4);
	vy = get_bits(ins, C8_VY_OFFSET, 4);
	addr = get_bits(ins, 0, 12);
	nibble = get_bits(ins, 0, 4);
	byte = get_bits(ins, 0, 8);
	type = I::ILLEGAL;
	// Most significant opcode nibble

	switch (get_bits(ins, 12, 4)) {
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
			type = ms_opcode_x8_map[nibble];
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
		default:
			type = I::ILLEGAL;
			break;
		}
		break;

	default:
		type = I::ILLEGAL;
		break;
	}
}

/// Replaces the first occuring char.
static void replace_char(string &s, char old, string_view replacement)
{
	if (auto at = s.find(old); at != string::npos) {
		s.replace(at, 1, replacement);
	}
}

string DecodedIns::to_string() const
{
	if (type == Instruction::ILLEGAL)
		return "<! DECODING ERROR !>";

	auto ret = string(INSTRUCTION_FORMATS[static_cast<int>(type)]);
	replace_char(ret, 'a', std::to_string(addr));
	replace_char(ret, 'b', std::to_string(byte));
	replace_char(ret, 'n', std::to_string(nibble));
	replace_char(ret, 'v', REGISTERS[static_cast<int>(vx)]);
	replace_char(ret, 'v', REGISTERS[static_cast<int>(vy)]);
	return ret;
}
