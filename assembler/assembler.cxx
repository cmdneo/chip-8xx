// Parse CHIP-8 assembly as described in chi8.md

#include <cctype>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include "chip8.hxx"
#include "scanner.hxx"

using std::begin;
using std::end;
using std::nullopt;
using std::optional;
using std::string;
using std::string_view;
using std::uint16_t;
using std::vector;

/// @brief Instructions with operand info encoded and opcodes map
/// @note JP_V0_a is an exception
static std::map<string_view, uint16_t> INS_OPCODE_MAP = {
	{"CLS", 0x00E0},     {"RET", 0x00EE},     {"SYSa", 0x0000},
	{"JPa", 0x1000},     {"CALLa", 0x2000},   {"SEv,b", 0x3000},
	{"SNEv,b", 0x4000},  {"SEv,v", 0x5000},   {"LDv,b", 0x6000},
	{"ADDv,b", 0x7000},  {"LDv,v", 0x8000},   {"ORv,v", 0x8001},
	{"ANDv,v", 0x8002},  {"XORv,v", 0x8003},  {"ADDv,v", 0x8004},
	{"SUBv,v", 0x8005},  {"SHR,v", 0x8006},   {"SUBNv,v", 0x8007},
	{"SHLv", 0x800E},    {"SNEv,v", 0x9000},  {"LDI,a", 0xA000},
	{"JPV0,a", 0xB000},  {"RNDv,b", 0xC000},  {"DRWv,v,n", 0xD000},
	{"SKPv", 0xE09E},    {"SKNPv", 0xE0A1},   {"LDv,DT", 0xF007},
	{"LDv,K", 0xF00A},   {"LDDT,v", 0xF015},  {"LDST,v", 0xF018},
	{"ADDI,v", 0xF01E},  {"LDF,v", 0xF029},   {"LDBv", 0xF033},
	{"LD[I],v", 0xF055}, {"LDv,[I]", 0xF065},
};
// static constexpr int INS_INFO_MAX_LEN = 8;

struct Statement {
	uint16_t opcode = 0;
	uint16_t imm = 0;
	uint8_t vx = 0;
	uint8_t vy = 0;
	string label;
	int line = 0;
	// For db(data byte) directive
	bool is_db = false;
};

enum class Tok {
	STOP,
	NUM,
	IDENT,
	REG,
	DIR,
	INS,
	CHAR,
};

// Parser state Globals
static Scanner scanner("");

static uint16_t number; // For immediate and register index
static string identifier;
static Directive directive;
static char character;
static int error_count = 0;
static std::map<string, string> define_map;

#define LOG_ERR_GET(err_msg, val) (log_err((err_msg)), (val));

static void log_err(string_view err_msg, int line = -1)
{
	error_count++;
	std::clog << "Parse ERROR: "
			  << "Line " << (line < 0 ? scanner.line() + 1 : line + 1) << ": "
			  << err_msg << "\n";
}

static inline bool is_ident_char(char c) { return std::isalnum(c) || c == '_'; }

// Caps the value at UINT16_MAX, prevents wrap-around
static inline uint16_t mul_add(uint16_t a, uint16_t b, uint16_t c)
{
	if (a && b > UINT16_MAX / a)
		return UINT16_MAX;
	a *= b;
	a += c;
	if (a < c)
		return UINT16_MAX;

	return a;
}

// For digits of base 2-36, if return > desired_base then invalid char
static uint16_t char_to_uint(char c)
{
	c = std::toupper(c);
	if ('0' <= c && c <= '9')
		return c - '0';
	else if ('A' <= c && c <= 'Z')
		return c - 'A' + 10;
	else
		return UINT16_MAX;
}

static Tok parse_int()
{
	uint16_t base = 10;
	number = 0;

	if (scanner.first() == '0') {
		char sec = scanner.second().value_or('0');
		if (sec == 'X')
			base = 16;
		else if (sec == 'B')
			base = 2;
		if (base != 10)
			scanner.skip(2);
	}

	while (auto c = scanner.first()) {
		if (!std::isalnum(*c))
			break;
		auto dig = char_to_uint(*c);
		if (dig > base)
			return LOG_ERR_GET("Integer overflow", Tok::STOP);
		number = mul_add(number, base, dig);
		if (number == UINT16_MAX)
			return LOG_ERR_GET("Integer overflow", Tok::STOP);

		scanner.skip();
	}

	return Tok::NUM;
}

static int
index_of_sv(const string_view *b, const string_view *e, string_view item)
{
	auto it = std::find(b, e, item);
	if (it != e)
		return it - b;
	else
		return -1;
}

static Tok parse_ident()
{
	identifier.clear();
	while (auto c = scanner.first()) {
		if (!is_ident_char(*c))
			break;
		identifier += *c;
		scanner.skip();
	}
	auto search = define_map.find(identifier);
	if (search != define_map.end())
		identifier = search->second;

	int idx = 0;
	idx = index_of_sv(begin(INSTRUCTIONS), end(INSTRUCTIONS), identifier);
	if (idx != -1) {
		return Tok::INS;
	}
	idx = index_of_sv(begin(DIRECTIVES), end(DIRECTIVES), identifier);
	if (idx != -1) {
		directive = Directive{idx};
		return Tok::DIR;
	}
	idx = index_of_sv(begin(REGISTERS), end(REGISTERS), identifier);
	if (idx != -1) {
		number = idx;
		return Tok::REG;
	}

	return Tok::IDENT;
}

static Tok next_token()
{
	while (auto tmp = scanner.first()) {
		char c = *tmp;
		if (std::isdigit(c)) {
			return parse_int();
		} else if (is_ident_char(c)) {
			return parse_ident();
		} else if (std::isblank(c)) {
			scanner.skip();
		} else if (c == ';') {
			scanner.skip_while([](char c) { return c != '\n'; });
		} else {
			scanner.skip();
			character = c;
			return Tok::CHAR;
		}
	}

	return Tok::STOP;
}

static bool is_ins_info_valid(string_view ins_info)
{

	for (const auto &kv : INS_OPCODE_MAP) {
		if (kv.first.substr(0, ins_info.size()) == ins_info)
			return true;
	}
	return false;
}

static optional<Statement> parse_ins()
{
	Statement ret{};
	uint16_t imm_lim = 0;
	string ins_info = identifier;
	bool is_reg_vx = true;
	auto insop_map_has = [](string_view s) -> bool {
		return INS_OPCODE_MAP.find(s) != INS_OPCODE_MAP.end();
	};

	// Data driven parsing
	// Construct the Instruction-Operand info string and match
	while (1) {
		auto search = INS_OPCODE_MAP.find(ins_info);
		if (search != INS_OPCODE_MAP.end()) {
			if (next_token() != Tok::CHAR && character != '\n')
				return LOG_ERR_GET("Newline expected", nullopt);
			ret.line = scanner.line();
			ret.opcode = search->second;
			return ret;
		}
		if (!is_ins_info_valid(ins_info)) {
			// std::clog << ins_info << "\n";
			return LOG_ERR_GET("Illegal Token", nullopt);
		}

		switch (next_token()) {
		// Terminal token
		case Tok::NUM:
			//	Ony DRW can have nibble as its last argument
			if (ins_info == "DRWv,v,") {
				imm_lim = C8_NIBBLE_MAX;
				ins_info += 'n';
			} else if (insop_map_has(ins_info + 'a')) {
				imm_lim = C8_ADDR_MAX;
				ins_info += 'a';
			} else if (insop_map_has(ins_info + 'b')) {
				imm_lim = C8_BYTE_MAX;
				ins_info += 'b';
			} else {
				return LOG_ERR_GET("Unexpected immediate", nullopt);
			}
			if (number > imm_lim)
				return LOG_ERR_GET("Immediate out of range", nullopt);
			ret.imm = number;
			break;

		case Tok::IDENT:
			// JP addr; JP V0, addr; CALL addr; LD I, addr; can have labels
			if (ins_info == "JP" || ins_info == "JPV0," || ins_info == "CALL"
				|| ins_info == "LDI,") {
				ins_info += 'a';
				ret.label = identifier;
			} else {
				ins_info += identifier;
			}
			break;

		case Tok::REG:
			// Exception: JP V0, addr
			if (ins_info == "JP" && number == 0) {
				ins_info += "V0";
				break;
			}
			if (is_reg_vx) {
				ret.vx = number;
				is_reg_vx = false;
			} else {
				ret.vy = number;
			}
			ins_info += 'v';
			break;

		case Tok::CHAR:
			ins_info += character;
			break;

		case Tok::STOP:
			return LOG_ERR_GET("Unexpected end of token input", nullopt);
			break;

		default:
			return LOG_ERR_GET("Unexpected token", nullopt);
			break;
		}
	}

	return LOG_ERR_GET("This should not happen", nullopt);
}

static bool parse_define()
{
	string alias, subst;
	if (next_token() != Tok::IDENT)
		return LOG_ERR_GET("Identifier expected after define", false);
	alias = identifier;

	scanner.skip_while([](char c) { return !!std::isblank(c); });
	while (auto c = scanner.first()) {
		if (std::isspace(*c) || c == ';')
			break;
		subst += *c;
		scanner.skip();
	}

	if (subst.empty())
		return LOG_ERR_GET("Alias expected after name", false);
	define_map[alias] = subst;

	return true;
}

static inline bool is_reserved_name(string_view s)
{
	// Name of internal registers(non V's) used assembly
	return s == "F" || s == "B" || s == "I" || s == "K" || s == "DT"
		   || s == "ST";
}

static optional<vector<uint8_t>> parse_and_assemble()
{
	std::map<string, unsigned> label_map;
	vector<Statement> statements;
	vector<uint8_t> bincode;
	unsigned label_addr = C8_PROG_START;
	bool run = true;

	while (run) {
		auto tok = next_token();

		switch (tok) {
		case Tok::STOP:
			if (error_count)
				return nullopt;
			run = false;
			break;

		case Tok::NUM:
			return LOG_ERR_GET("Unexpected integer", nullopt);
			break;

		case Tok::IDENT:
			if (is_reserved_name(identifier)) {
				return LOG_ERR_GET(
					"Reserved name '" + identifier + "' cannot be a label",
					nullopt
				);
			}
			tok = next_token();
			if (!(tok == Tok::CHAR && character == ':'))
				return LOG_ERR_GET("Colon expected after label name", nullopt);
			if (!label_map.insert({identifier, label_addr}).second)
				return LOG_ERR_GET("Duplicate label defined", nullopt);
			break;

		case Tok::REG:
			return LOG_ERR_GET("Unexpected Register name", nullopt);
			break;

		case Tok::DIR:
			using D = Directive;
			if (directive == D::DB) {
				if (next_token() != Tok::NUM)
					return LOG_ERR_GET("Number expected after DB", nullopt);
				if (number > C8_BYTE_MAX)
					return LOG_ERR_GET("Number too big(>255) ", nullopt);
				if (!(next_token() == Tok::CHAR && character == '\n'))
					return LOG_ERR_GET("Newline expected", nullopt);
				Statement stmt{};
				stmt.is_db = true;
				stmt.imm = number;
				statements.push_back(stmt);
				label_addr++;
			} else if (directive == D::DEFINE) {
				if (!parse_define())
					return nullopt;
			}
			break;

		case Tok::INS:
			if (auto stmt = parse_ins()) {
				statements.push_back(*stmt);
				label_addr += C8_INS_LEN;
			} else {
				return nullopt;
			}
			break;

		case Tok::CHAR:
			// Ignore extra white space
			if (!std::isspace(character))
				return LOG_ERR_GET("Illegal character", nullopt);
			break;
		}
	}

	for (auto &stmt : statements) {
		if (stmt.is_db) {
			bincode.push_back(stmt.imm & 0xFF);
			continue;
		}
		if (!stmt.label.empty()) {
			auto search = label_map.find(stmt.label);
			if (search == label_map.end()) {
				log_err("Label not found: " + stmt.label, stmt.line);
				return nullopt;
			}
			stmt.imm = search->second;
		}
		uint16_t encoded = stmt.opcode | stmt.imm | (stmt.vx << C8_VX_OFFSET)
						   | (stmt.vy << C8_VY_OFFSET);
		// 2-bytes big endian
		bincode.push_back(encoded >> 8);
		bincode.push_back(encoded & 0xFF);
	}

	return bincode;
}

static string read_to_uppercase_string(std::ifstream &file)
{
	string ret;
	char c;
	while (file.get(c))
		ret += std::toupper(c);

	return ret;
}

int main(int argc, char const **argv)
{
	if (argc != 3) {
		auto name = argc > 0 ? argv[0] : "c8asm";
		std::clog << "Usage: " << name << " <infile> <outfile>\n";
		return 1;
	}

	std::ifstream infile(argv[1]);
	if (!infile) {
		std::clog << "Cannot open file '" << argv[1] << "'\n";
		return 1;
	}

	const string code = read_to_uppercase_string(infile); // Case-insesetive
	scanner = Scanner(code);
	auto bincode = parse_and_assemble();
	if (!bincode) {
		std::clog << "Parsing failed\n";
		return 1;
	}

	std::ofstream outfile(argv[2], std::ios::binary);
	if (!outfile) {
		std::clog << "Cannot open file '" << argv[2] << "'\n";
		return 1;
	}

	outfile.write(
		reinterpret_cast<char *>(bincode->data()),
		bincode->size() * sizeof(bincode->front())
	);

	return 0;
}
