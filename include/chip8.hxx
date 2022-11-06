#ifndef INCLUDE_COMMON_CHIP8_HXX_INCLUDED
#define INCLUDE_COMMON_CHIP8_HXX_INCLUDED

#include <cstdint>
#include <map>
#include <string_view>

enum Chip8Info {
	C8_TIMER_FREQ = 60,
	C8_KEY_NONE = 16, // Valid keys are [0-15]
	C8_KEY_CNT = 16,
	C8_FLAG_REG = 15,

	C8_SCREEN_WIDTH = 64,
	C8_SCREEN_HEIGHT = 32,

	// All immediates have zero offset
	C8_VX_OFFSET = 8,
	C8_VY_OFFSET = 4,

	C8_INS_LEN = 2,
	C8_REG_CNT = 16,
	C8_PROG_START = 0x200,
	C8_STACK_SIZE = 16,
	C8_RAM_SIZE = 4096,
	C8_ADDR_MAX = C8_RAM_SIZE - 1,
	C8_BYTE_MAX = 255,
	C8_NIBBLE_MAX = 15,

	C8_FONT_HEIGHT = 5,
	C8_FONT_CNT = 16,
};

static constexpr std::uint8_t FONT_SPRITES[C8_FONT_CNT][C8_FONT_HEIGHT] = {
	{0xF0, 0x90, 0x90, 0x90, 0xF0}, // 0
	{0x20, 0x60, 0x20, 0x20, 0x70}, // 1
	{0xF0, 0x10, 0xF0, 0x80, 0xF0}, // 2
	{0xF0, 0x10, 0xF0, 0x10, 0xF0}, // 3
	{0x90, 0x90, 0xF0, 0x10, 0x10}, // 4
	{0xF0, 0x80, 0xF0, 0x10, 0xF0}, // 5
	{0xF0, 0x80, 0xF0, 0x90, 0xF0}, // 6
	{0xF0, 0x10, 0x20, 0x40, 0x40}, // 7
	{0xF0, 0x90, 0xF0, 0x90, 0xF0}, // 8
	{0xF0, 0x90, 0xF0, 0x10, 0xF0}, // 9
	{0xF0, 0x90, 0xF0, 0x90, 0x90}, // A
	{0xE0, 0x90, 0xE0, 0x90, 0xE0}, // B
	{0xF0, 0x80, 0x80, 0x80, 0xF0}, // C
	{0xE0, 0x90, 0x90, 0x90, 0xE0}, // D
	{0xF0, 0x80, 0xF0, 0x80, 0xF0}, // E
	{0xF0, 0x80, 0xF0, 0x80, 0x80}, // F
};

/// @brief All instructions with operand info.
/// v - Any V register
/// b - Byte
/// h - Nibble(half-byte)
/// a - Address
enum class Instruction {
	CLS,
	RET,
	SYS_a,
	JP_a,
	CALL_a,
	SE_v_b,
	SNE_v_b,
	SE_v_v,
	LD_v_b,
	ADD_v_b,
	LD_v_v,
	OR_v_v,
	AND_v_v,
	XOR_v_v,
	ADD_v_v,
	SUB_v_v,
	SHR_v,
	SUBN_v_v,
	SHL_v,
	SNE_v_v,
	LD_I_a,
	JP_V0_a,
	RND_v_b,
	DRW_v_v_n,
	SKP_v,
	SKNP_v,
	LD_v_DT,
	LD_v_K,
	LD_DT_v,
	LD_ST_v,
	ADD_I_v,
	LD_F_v,
	LD_B_v,
	LD_IM_v,
	LD_v_IM,
	// IM = [I], I as memory location
	// Illegal instruction marker
	ILLEGAL,
};

/// @brief Convenience directives for assembly
enum class Directive {
	DB,
	DEFINE,
};

/// @brief Register Mnemonics
static constexpr std::string_view REGISTERS[] = {
	"V0", "V1", "V2",  "V3",  "V4",  "V5",  "V6",  "V7",
	"V8", "V9", "V10", "V11", "V12", "V13", "V14", "V15",
};

/// @brief Instruction Mnemonics, ordered according to Instruction enum.
/// Note that several instructions have same mnemonics,
/// such instructions are further identified by their operands
static constexpr std::string_view INSTRUCTIONS[] = {
	"CLS", "RET", "SYS", "JP",  "CALL", "SE",  "SNE", "SE",   "LD",
	"ADD", "LD",  "OR",  "AND", "XOR",  "ADD", "SUB", "SHR",  "SUBN",
	"SHL", "SNE", "LD",  "JP",  "RND",  "DRW", "SKP", "SKNP", "LD",
	"LD",  "LD",  "LD",  "ADD", "LD",   "LD",  "LD",  "LD",
};

/// @brief Masked opcodes, ordered according to Instruction enum.
/// All operand fields are zeroed out, so just OR opcode with the field
static constexpr std::uint16_t OPCODES[] = {
	0x00E0, 0x00EE, 0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000,
	0x7000, 0x8000, 0x8001, 0x8002, 0x8003, 0x8004, 0x8005, 0x8006, 0x8007,
	0x800E, 0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE09E, 0xE0A1, 0xF007,
	0xF00A, 0xF015, 0xF018, 0xF01E, 0xF029, 0xF033, 0xF055, 0xF065,
};

/// @brief Directive mnemonics, ordered according to Directive enum
static constexpr std::string_view DIRECTIVES[] = {
	"DB",
	"DEFINE",
};

#endif
