#pragma once

#include <string>
#include "chip8.hxx"

struct DecodedIns {
	explicit DecodedIns(uint16_t ins);
	[[nodiscard]] std::string to_string() const;

	Instruction type = Instruction::ILLEGAL;
	uint16_t bincode = 0;
	uint16_t addr = 0;
	uint8_t vx = 0;
	uint8_t vy = 0;
	uint8_t byte = 0;
	uint8_t nibble = 0;
};
