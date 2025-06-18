#pragma once

#include <cstdint>
#include <bitset>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "lexer.hxx"

using u8 = std::uint8_t;
using u16 = std::uint16_t;

struct Statement {
	Token label{};
	u16 opcode{};
	u16 imm{};
	u8 vx{};
	u8 vy{};
	bool is_data_byte{};
};

enum class [[nodiscard]] Result {
	Ok,
	Err,
};

enum class [[nodiscard]] Matched {
	None,
	Multiple,
	Register,
	Label,
	Address,
	Byte,
	Nibble,
	Exact,
};

/// @brief Checks which instruction-operand combination matches our
/// token stream for parsing instructions.
///
/// Instead of writing code for checking for each instruction format,
/// we just use the instruction format list and match directly from it.
class RuleMatcher
{
public:
	RuleMatcher();

	void start_new_match()
	{
		match_count = 0;
		matched = {};
		matching.set();
	}

	std::optional<Instruction> get_matched_rule() { return matched; }

	/// @brief Try matching the token and advance if matched.
	/// @param tok Token
	/// @return Match type.
	Matched try_next(Token tok);

private:
	unsigned match_count = 0;
	std::optional<Instruction> matched{};
	std::bitset<std::size(INSTRUCTION_FORMATS)> matching{};
	std::vector<std::vector<std::string_view>> rules{};
};

class Parser
{
public:
	explicit Parser(std::string_view src)
		: lexer(src)
	{
	}

	std::optional<std::vector<u8>> parse_and_assemble();

private:
	Result parse_statement();
	Result parse_instruction();
	Result parse_label();
	Result parse_define();
	Result parse_db();

	Result expect(TokenKind kind, std::optional<int> value = {});
	bool match_advance(TokenKind kind, std::optional<int> value = {});
	bool match(TokenKind kind, std::optional<int> value = {});
	Token advance();

	void add_statement(Statement stmt);
	void log_err(std::string_view msg, Token at);
	void recover();

	struct Macro {
		std::string_view name{};
		std::string_view subst{};
		Position pos{};
	};
	struct ActiveMacro {
		Macro *macro{};
		Position expand_pos{};
		Lexer lexer{};
	};

	Lexer lexer{};
	Token previous{};
	Token current{};
	RuleMatcher matcher{};

	std::optional<ActiveMacro> active_macro{};
	std::unordered_map<std::string_view, Macro> macros{};

	int error_count = 0;
	u16 next_stmt_addr = C8_PROG_START;
	std::vector<Statement> statements{};
	std::unordered_map<std::string_view, u16> label_targets{};
};
