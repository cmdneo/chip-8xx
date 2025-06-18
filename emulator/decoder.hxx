#pragma once

#include <string>
#include "chip8.hxx"

struct DecodedIns {
	explicit DecodedIns(uint16_t ins);
	[[nodiscard]] auto to_string() const -> std::string;

	Instruction type = Instruction::ILLEGAL;
	uint16_t bincode{};
	uint16_t addr{};
	uint8_t vx{};
	uint8_t vy{};
	uint8_t byte{};
	uint8_t nibble{};
};
