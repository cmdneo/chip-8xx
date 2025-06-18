#include <fstream>
#include <format>
#include <iostream>

#include "parser.hxx"

#define LOG(...) (std::clog << std::format(__VA_ARGS__))

int main(int argc, char const **argv)
{
	if (argc != 3) {
		auto name = argc > 0 ? argv[0] : "c8asm";
		LOG("Usage: {} <infile> <outfile>\n", name);
		return 1;
	}

	std::ifstream infile(argv[1]);
	if (!infile) {
		LOG("Cannot open file '{}'\n", argv[1]);
		return 1;
	}

	std::string text(
		(std::istreambuf_iterator<char>(infile)),
		std::istreambuf_iterator<char>()
	);
	text += '\n';

	Parser parser(text);
	auto bincode = parser.parse_and_assemble();
	if (!bincode)
		return 1;

	std::ofstream outfile(argv[2], std::ios::binary);
	if (!outfile) {
		LOG("Cannot open/create output file '{}'\n", argv[2]);
		return 1;
	}

	outfile.write(
		reinterpret_cast<char *>(bincode->data()),
		static_cast<std::streamsize>(bincode->size() * sizeof(bincode->front()))
	);

	return 0;
}
