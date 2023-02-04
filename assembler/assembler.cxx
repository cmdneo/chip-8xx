// Parse CHIP-8 assembly as described in chi8.md
// PARSING
// Each line is a statement, parse line by line

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
using std::clog;
using std::end;
using std::map;
using std::nullopt;
using std::optional;
using std::string;
using std::string_view;
using std::uint16_t;
using std::vector;

/// @brief Instructions with operand info encoded and opcodes map
/// @note JP_V0_a is an exception
static map<string_view, uint16_t> INS_OPCODE_MAP = {
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
	{"ADDI,v", 0xF01E},  {"LDF,v", 0xF029},   {"LDB,v", 0xF033},
	{"LD[I],v", 0xF055}, {"LDv,[I]", 0xF065},
};
// static constexpr int INS_INFO_MAX_LEN = 8;

#define LOG_ERR_GET(err_msg, val) (log_err((err_msg), (this->tok_span)), (val));

struct Span {
	unsigned line = 0;
	unsigned column = 0;
	unsigned length = 0;
};

struct Statement {
	uint16_t opcode = 0;
	uint16_t immediate = 0;
	uint8_t vx = 0;
	uint8_t vy = 0;
	string label;
	// Debugging info
	Span label_span;
	// Is db(data byte) directive
	bool is_db = false;
};

enum class Tok {
	STOP,
	DIREC_DB,
	DIREC_DEFINE,
	INSTRUCTION,
	REGISTER,
	IDENTIFIER,
	IMMEDIATE,
	CHAR,
};

// -----------------------------------------------------------------------
// Chip-8 Assembly parser, parser line by line, case-insesetive
class Parser
{
public:
	Parser(const std::vector<string> &asm_lines)
	{
		for (auto &line : asm_lines)
			lines.push_back(line);
	}

	optional<vector<uint8_t>> parse_and_assemble();

private:
	Tok parse_immediate();
	Tok parse_identifier();
	Tok next_token();
	Tok next_token_impl();
	optional<int> parse_define();
	optional<Statement> parse_instruction();
	optional<Statement> parse_line(string_view line);
	void log_err(string_view msg, Span span);

	// The stream for reading input
	std::vector<string_view> lines;
	// Scanner for the current line
	Scanner scn;
	/// Last token position
	Span tok_span;
	// Define substitution map
	map<string, string> define_map;
	// Label and their associated addresses
	map<string, uint16_t> label_map;
	// Current label address (if present)
	uint16_t label_addr = C8_PROG_START;

	/// Immediate has negative sign
	bool is_negative = false;
	// Absolute immediate value or register index)
	uint16_t number = 0;
	string identifier;
	char character = '\0';

	int line_num = 0;
	int error_cnt = 0;
	const int error_max = 10;
};

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

static inline bool is_reserved_name(string_view s)
{
	// Name of internal registers(non V's) used by assembly
	return s == "F" || s == "B" || s == "I" || s == "K" || s == "DT"
		   || s == "ST";
}

Tok Parser::parse_immediate()
{
	uint16_t base = 10;
	is_negative = false;
	number = 0;

	if (auto c = scn.first(); c == '+' || c == '-') {
		is_negative = c == '-';
		scn.skip();
		scn.skip_while([](char c) { return !!std::isblank(c); });
	}

	if (scn.first() == '0') {
		char sec = scn.second().value_or('0');
		if (sec == 'X')
			base = 16;
		else if (sec == 'B')
			base = 2;
		if (base != 10)
			scn.skip(2);
	}

	while (auto c = scn.first()) {
		if (!std::isalnum(*c))
			break;
		auto dig = char_to_uint(*c);
		if (dig > base)
			return LOG_ERR_GET("Illegal character in immediate", Tok::STOP);
		number = mul_add(number, base, dig);
		if (number == UINT16_MAX)
			return LOG_ERR_GET("Integer overflow", Tok::STOP);

		scn.skip();
	}

	return Tok::IMMEDIATE;
}

Tok Parser::parse_identifier()
{
	identifier.clear();
	identifier = string(scn.skip_while(is_ident_char));
	if (auto alias = define_map.find(identifier); alias != define_map.end())
		identifier = alias->second;
	if (identifier.empty())
		return LOG_ERR_GET("Empty substitution", Tok::STOP);

	// If the substitution result is a number(maybe), then parse that
	if (auto c = identifier.front(); c == '-' || c == '+' || std::isdigit(c)) {
		auto old_scn = scn;
		scn = Scanner(identifier);
		auto ret = parse_immediate();
		scn = old_scn;
		return ret;
	}

	// Check if: Directive
	if (identifier == "DB")
		return Tok::DIREC_DB;
	if (identifier == "DEFINE")
		return Tok::DIREC_DEFINE;

	// Check if: Instruction
	auto ins = std::find(begin(INSTRUCTIONS), end(INSTRUCTIONS), identifier);
	if (ins != end(INSTRUCTIONS))
		return Tok::INSTRUCTION;

	// Check if: Register
	auto reg = std::find(begin(REGISTERS), end(REGISTERS), identifier);
	if (reg != end(REGISTERS)) {
		number = reg - begin(REGISTERS);
		return Tok::REGISTER;
	}

	return Tok::IDENTIFIER;
}

Tok Parser::next_token_impl()
{
	tok_span.line = line_num;
	tok_span.column = scn.cursor();

	while (auto tmp = scn.first()) {
		char c = *tmp;
		if (std::isdigit(c) || c == '-' || c == '+') {
			return parse_immediate();
		} else if (is_ident_char(c)) {
			return parse_identifier();
		} else if (std::isblank(c)) {
			scn.skip();
			tok_span.column = scn.cursor();
		} else if (c == ';') {
			scn.skip_while([](char c) { return c != '\n'; });
		} else {
			scn.skip();
			character = c;
			return Tok::CHAR;
		}
	}

	return Tok::STOP;
}

Tok Parser::next_token()
{
	auto ret = next_token_impl();
	tok_span.length = scn.cursor() - tok_span.column;
	return ret;
}

optional<int> Parser::parse_define()
{
	scn.skip_while([](char c) { return !!std::isblank(c); });
	auto alias = scn.skip_while(is_ident_char);
	if (alias.empty() || std::isdigit(alias.front()))
		return LOG_ERR_GET("Expected alias for define", nullopt);

	scn.skip_while([](char c) { return !!std::isblank(c); });
	auto subst = scn.skip_while(is_ident_char);
	if (subst.empty())
		return LOG_ERR_GET("Expected substituion for define", nullopt);

	define_map[string(alias)] = string(subst);
	return alias.size();
}

static bool is_ins_info_valid(string_view ins_info)
{

	for (const auto &kv : INS_OPCODE_MAP) {
		if (kv.first.substr(0, ins_info.size()) == ins_info)
			return true;
	}
	return false;
}

optional<Statement> Parser::parse_instruction()
{
	Statement ret{};
	uint16_t imm_lim = 0;
	string ins_info = identifier;
	bool is_reg_vx = true;
	auto insop_map_has = [](string_view s) -> bool {
		return INS_OPCODE_MAP.find(s) != INS_OPCODE_MAP.end();
	};

	// Construct the Instruction-Operand info string and match
	while (1) {
		auto valid = INS_OPCODE_MAP.find(ins_info);
		if (valid != INS_OPCODE_MAP.end()) {
			ret.label_span = tok_span;
			ret.opcode = valid->second;
			return ret;
		}
		// clog << ins_info << "\n";
		if (!is_ins_info_valid(ins_info))
			return LOG_ERR_GET("Illegal Token", nullopt);

		switch (next_token()) {
		// Immediate is a terminal token
		case Tok::IMMEDIATE:
			// Ony DRW has nibble as its last argument
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

			// Negative sign applicable only for BYTE, make a 2's complement
			if (!is_negative)
				ret.immediate = number;
			else if (imm_lim == C8_BYTE_MAX)
				ret.immediate = (~number + 1) & C8_BYTE_MAX;
			else
				return LOG_ERR_GET("Negative sign not allowed here", nullopt);

			if (number > imm_lim || (is_negative && number > abs(INT8_MIN)))
				return LOG_ERR_GET("Immediate out of range", nullopt);
			break;

		case Tok::IDENTIFIER:
			// JP addr; JP V0, addr; CALL addr; LD I, addr; can have labels
			// The labels are later replaced with their corresponding addresses
			if (ins_info == "JP" || ins_info == "JPV0," || ins_info == "CALL"
				|| ins_info == "LDI,") {
				ins_info += 'a';
				ret.label = identifier;
			} else {
				ins_info += identifier;
			}
			break;

		case Tok::REGISTER:
			// Exception: JP V0, addr; Has V0 as a fixed operand
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

		default:
			return LOG_ERR_GET("Unexpected token", nullopt);
		}
	}

	return LOG_ERR_GET("This should not happen", nullopt);
}

optional<Statement> Parser::parse_line(string_view line)
{
	Statement ret;
	scn = Scanner(line);

	while (true) {
		auto tok = next_token();

		switch (tok) {
		case Tok::STOP:
			return nullopt;

		case Tok::DIREC_DB:
			if (next_token() != Tok::IMMEDIATE)
				return LOG_ERR_GET("Expected immediate after db", nullopt);
			ret.is_db = true;
			ret.immediate = number;
			label_addr++;
			return ret;

		case Tok::DIREC_DEFINE:
			parse_define();
			// Define directive produces no code, so nullopt
			return nullopt;

		case Tok::INSTRUCTION:
			label_addr += C8_INS_LEN;
			return parse_instruction();

		case Tok::REGISTER:
			return LOG_ERR_GET("Unexpected register name", nullopt);

		case Tok::IDENTIFIER:
			if (is_reserved_name(identifier))
				return LOG_ERR_GET("Label cannot be a reserved name", nullopt);
			if (!(next_token() == Tok::CHAR && character == ':'))
				return LOG_ERR_GET("Expected colon after label name", nullopt);
			if (!label_map.insert({identifier, label_addr}).second)
				return LOG_ERR_GET("Redefined label name", nullopt);
			break;

		case Tok::IMMEDIATE:
			return LOG_ERR_GET("Unexpected immediate name", nullopt);

		case Tok::CHAR:
			return LOG_ERR_GET(
				"Unexpected character '" + string({character}) + "' ("
					+ std::to_string(character) + ")",
				nullopt
			);
		}
	}
}

optional<vector<uint8_t>> Parser::parse_and_assemble()
{
	vector<Statement> statements;

	for (const auto &line : lines) {
		auto prev_errs = error_cnt;
		auto stmt = parse_line(line);

		// TODO Code cleanup!
		// There should be stray tokens at end given no errors in this line
		if (prev_errs == error_cnt && next_token() != Tok::STOP)
			log_err("Unexpected stray token(s) after statement", tok_span);
		if (error_cnt >= error_max) {
			clog << "Stopping after max errors: " << error_max << "\n";
			return nullopt;
		}

		if (stmt)
			statements.push_back(*stmt);
		line_num++;
	}

	// Generate code even if some statemenets are invalid to report further errors
	vector<uint8_t> bincode;
	for (auto &stmt : statements) {
		if (stmt.is_db) {
			bincode.push_back(stmt.immediate & 0xFF);
			continue;
		}
		if (!stmt.label.empty()) {
			auto addr = label_map.find(stmt.label);
			if (addr == label_map.end()) {
				log_err("Label not found: " + stmt.label, stmt.label_span);
				if (error_cnt >= error_max) {
					clog << "Stopping after max errors: " << error_max << "\n";
					return nullopt;
				}
				continue;
			}
			stmt.immediate = addr->second;
		}

		uint16_t encoded = stmt.opcode | stmt.immediate
						   | (stmt.vx << C8_VX_OFFSET)
						   | (stmt.vy << C8_VY_OFFSET);
		// 2-bytes big endian
		bincode.push_back(encoded >> 8);
		bincode.push_back(encoded & 0xFF);
	}

	if (error_cnt != 0)
		return nullopt; // Discard all code if errors
	return bincode;
}

void Parser::log_err(string_view err_msg, Span span)
{
	error_cnt++;
	string token(lines[span.line].substr(span.column, span.length));
	bool is_macro = define_map.find(token) != define_map.end();

	// Print the message
	clog << "Parse ERROR: " << (span.line + 1) << ":" << (span.column + 1)
		 << " " << err_msg << "\n";
	// Print line and marke the region of invalid token
	clog << lines[span.line] << "\n";
	clog << string(span.column, '~') << string(span.length, '^');
	// Print replacement if is a macro
	if (is_macro)
		clog << "(macro for '" << define_map[token] << "')";
	clog << "\n\n";
}

vector<string> read_lines_uppercase(std::ifstream &is)
{
	vector<string> lines;
	string line;
	while (std::getline(is, line)) {
		std::for_each(line.begin(), line.end(), [](char &c) {
			c = std::toupper(c);
			if (c == '\t')
				c = ' ';
		});
		lines.push_back(std::move(line));
	}

	return lines;
}

int main(int argc, char const **argv)
{
	if (argc != 3) {
		auto name = argc > 0 ? argv[0] : "c8asm";
		clog << "Usage: " << name << " <infile> <outfile>\n";
		return 1;
	}

	std::ifstream infile(argv[1]);
	if (!infile) {
		clog << "Cannot open file '" << argv[1] << "'\n";
		return 1;
	}

	auto lines = read_lines_uppercase(infile);
	Parser c8_parser(lines);
	auto bincode = c8_parser.parse_and_assemble();
	if (!bincode) {
		clog << "Parsing failed\n";
		return 1;
	}

	std::ofstream outfile(argv[2], std::ios::binary);
	if (!outfile) {
		clog << "Cannot open file '" << argv[2] << "'\n";
		return 1;
	}

	outfile.write(
		reinterpret_cast<char *>(bincode->data()),
		bincode->size() * sizeof(bincode->front())
	);

	return 0;
}
