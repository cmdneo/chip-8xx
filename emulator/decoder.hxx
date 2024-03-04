#ifndef CHIP8_DECODER_HXX_INCLUDED
#define CHIP8_DECODER_HXX_INCLUDED

#include <string>
#include "chip8.hxx"

struct DecodedIns {
	DecodedIns(uint16_t ins);
	std::string to_string();

	Instruction type = Instruction::ILLEGAL;
	uint16_t bincode = 0;
	uint16_t addr = 0;
	uint8_t vx = 0;
	uint8_t vy = 0;
	uint8_t byte = 0;
	uint8_t nibble = 0;
};

#endif