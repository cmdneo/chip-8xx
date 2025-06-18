#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "chip8.hxx"
#include "lexer.hxx"
#include "parser.hxx"

#define PANIC(msg)               \
	do {                         \
		LOG("PANIC: " msg "\n"); \
		abort();                 \
	} while (0)

#define LOG(...) (std::clog << std::format(__VA_ARGS__))

#define TRY(result_expr)                  \
	do {                                  \
		if ((result_expr) == Result::Err) \
			return Result::Err;           \
	} while (0)

constexpr int ERROR_LIMIT = 10;

auto Parser::parse_and_assemble() -> std::optional<std::vector<u8>>
{
	std::vector<u8> bincode{};

	advance(); // Take in first token.
	while (current.kind != TokenKind::Eof) {
		if (error_count >= ERROR_LIMIT) {
			LOG("Too many errors, stopping.\n");
			return {};
		}
		if (parse_statement() == Result::Err) {
			recover();
		}
	}

	// Assign label address if label field is present.
	for (auto &stmt : statements) {
		if (error_count >= ERROR_LIMIT) {
			LOG("Too many errors, stopping.\n");
			return {};
		}
		auto lb = stmt.label.lexeme;
		if (lb.empty()) {
			continue;
		}
		if (auto t = label_targets.find(lb); t != label_targets.end()) {
			stmt.imm = t->second;
		} else {
			log_err("Label not found", stmt.label);
		}
	}

	if (error_count != 0) {
		return {};
	}

	for (auto stmt : statements) {
		if (stmt.is_data_byte) {
			bincode.push_back(stmt.imm);
			continue;
		}

		u16 code = stmt.opcode;
		code |= stmt.imm;
		code |= static_cast<u16>(stmt.vx) << C8_VX_OFFSET;
		code |= static_cast<u16>(stmt.vy) << C8_VY_OFFSET;
		// Chip8 is big-endian.
		bincode.push_back(code >> 8);
		bincode.push_back(code & 0xFF);
	}

	return bincode;
}

auto Parser::parse_statement() -> Result
{
	if (match_advance(TokenKind::Identifier)) {
		TRY(parse_label());
	}

	if (match_advance(TokenKind::Instruction)) {
		TRY(parse_instruction());
	} else if (match_advance(TokenKind::Db)) {
		TRY(parse_db());
	} else if (match_advance(TokenKind::Define)) {
		TRY(parse_define());
	}

	TRY(expect(TokenKind::Char, '\n'));
	return Result::Ok;
}

static auto limit_value(int value, unsigned bits) -> std::optional<u16>
{
	const unsigned umax = ~(~0 << bits);
	unsigned av = std::abs(value);

	if (value >= 0 && av <= umax) {
		return av;
	}
	if (value < 0 && av <= (umax + 1) / 2) {
		return ~av + 1;
	}
	return {};
}

auto Parser::parse_instruction() -> Result
{
	auto get_imm_bits = [](Matched match) {
		switch (match) {
		case Matched::Address:
			return 12;
		case Matched::Byte:
			return 8;
		case Matched::Nibble:
			return 4;
		default:
			PANIC("unreachable");
			return 0;
		}
	};

	Statement stmt{};
	int regs_matched = 0;
	matcher.start_new_match();

	// This must match, its the instruction mnemonic.
	if (matcher.try_next(previous) == Matched::None) {
		PANIC("not an instruction token");
	}

	for (;;) {
		if (auto ins = matcher.get_matched_rule()) {
			stmt.opcode = OPCODES[static_cast<int>(*ins)];
			add_statement(stmt);
			return Result::Ok;
		}

		switch (auto mc = matcher.try_next(current)) {
		case Matched::None:
			log_err("Unexpected token", current);
			return Result::Err;
		case Matched::Multiple:
			if (current.kind == TokenKind::Immediate) {
				PANIC("ambigious immediate token in rule");
			}
			break;

		case Matched::Register:
			if (regs_matched == 0) {
				stmt.vx = current.value;
			} else if (regs_matched == 1) {
				stmt.vy = current.value;
			} else {
				PANIC("too many registers in rule");
			}
			regs_matched++;
			break;

		case Matched::Label:
			stmt.label = current;
			break;

		case Matched::Address:
		case Matched::Byte:
		case Matched::Nibble:
			if (auto v = limit_value(current.value, get_imm_bits(mc))) {
				stmt.imm = *v;
			} else {
				log_err("Immediate out of range", current);
			}
			break;

		default:
			break;
		}
		advance();
	}

	PANIC("unreachable");
	return {};
}

auto Parser::parse_label() -> Result
{
	auto label = previous;
	TRY(expect(TokenKind::Char, ':'));

	if (label_targets.contains(label.lexeme)) {
		log_err("duplicate label name", label);
	} else {
		label_targets[label.lexeme] = next_stmt_addr;
	}
	return Result::Ok;
}

auto Parser::parse_define() -> Result
{
	// %define directive, allows any identifier as the macro name
	if (auto c = current.lexeme[0]; std::isalpha(c) || c == '_') {
		current.kind = TokenKind::Identifier;
	}
	lexer.set_next_token_as_line();
	TRY(expect(TokenKind::Identifier));

	auto name = previous.lexeme;
	auto subst = advance();
	macros[name] = Macro{
		.name = name,
		.subst = subst.lexeme,
		.pos = subst.pos,
	};
	return Result::Ok;
}

auto Parser::parse_db() -> Result
{
	TRY(expect(TokenKind::Immediate));
	auto v = limit_value(previous.value, 8);
	if (!v) {
		log_err("Immediate out of range", previous);
	}

	add_statement(
		Statement{
			.imm = v.value_or(0),
			.is_data_byte = true,
		}
	);
	return Result::Ok;
}

auto Parser::expect(TokenKind kind, std::optional<int> value) -> Result
{
	if (match_advance(kind, value)) {
		return Result::Ok;
	}

	auto expected_tok = make_token(kind, value.value_or(0));
	log_err(
		std::format(
			"Syntax error: expected {} found {}",
			expected_tok.as_debug(value.has_value()), current.as_debug()
		),
		current
	);
	return Result::Err;
}

auto Parser::match_advance(TokenKind kind, std::optional<int> value) -> bool
{
	if (match(kind, value)) {
		advance();
		return true;
	}
	return false;
}

auto Parser::match(TokenKind kind, std::optional<int> value) -> bool
{
	return current.kind == kind && (!value || *value == current.value);
}

auto Parser::advance() -> Token
{
	previous = current;

	// Generate tokens from macro expansion if a macro is present.
	// When a macro expansion ends we goto the next token.
	for (;;) {
		if (active_macro) {
			current = active_macro->lexer.next();
			current.origin = active_macro->macro;
			current.pos = active_macro->expand_pos;
			if (current.kind != TokenKind::Eof) {
				break;
			}

			active_macro = {};
		} else {
			current = lexer.next();
			auto m = macros.find(current.lexeme);
			if (m == macros.end()) {
				break;
			}

			active_macro = ActiveMacro{
				.macro = &m->second,
				.expand_pos = current.pos,
				.lexer = Lexer(m->second.subst),
			};
		}
	}

	return previous;
}

void Parser::add_statement(Statement stmt)
{
	if (error_count != 0) {
		return;
	}
	statements.push_back(stmt);
	next_stmt_addr += stmt.is_data_byte ? 1 : C8_INSTR_LEN;
}

void Parser::log_err(std::string_view msg, Token at)
{
	if (at.origin) {
		auto *origin = reinterpret_cast<Macro *>(at.origin);
		LOG("Line {}:{} (expanded from macro '{}' on line {})", at.pos.line,
			at.pos.column, origin->name, origin->pos.line);
	} else {
		LOG("Line {}:{}", at.pos.line, at.pos.column);
	}
	LOG(" ERROR on {}:\n\t{}.\n", at.as_debug(), msg);

	error_count++;
}

void Parser::recover()
{
	for (;;) {
		if (match_advance(TokenKind::Char, '\n')) {
			break;
		}
		advance();
	}
}

RuleMatcher::RuleMatcher()
{
	// Use the lexer to split instruction format into lexemes, then
	// use that token sequence as a rule for checking which instruction
	// matches with the supplied sequence of tokens.
	for (auto rule : INSTRUCTION_FORMATS) {
		Lexer lexer(rule);
		std::vector<std::string_view> tokens{};
		tokens.reserve(4); // most rules have 4 tokens.

		while (auto t = lexer.next()) {
			tokens.push_back(t.lexeme);
		}
		rules.push_back(std::move(tokens));
	}
}

static auto match_one(std::string_view rule_token, Token tok) -> Matched
{
	using M = Matched;

	// Match token with rule:
	//     Register matches 'v'.
	//     Label matches 'a'.
	//     Immediate matches 'a', 'b' or 'n'.
	if (rule_token.size() == 1) {
		switch (rule_token[0]) {
		case 'v':
			if (tok.kind == TokenKind::Register) {
				return M::Register;
			}
			return M::None;

		case 'a':
			if (tok.kind == TokenKind::Identifier) {
				return M::Label;
			}
			if (tok.kind == TokenKind::Immediate) {
				return M::Address;
			}
			return M::None;

		case 'b':
			if (tok.kind == TokenKind::Immediate) {
				return M::Byte;
			}
			return M::None;

		case 'n':
			if (tok.kind == TokenKind::Immediate) {
				return M::Nibble;
			}
			return M::None;

		default:
			break;
		}
	}
	if (icase_equals(rule_token, tok.lexeme)) {
		return M::Exact;
	}

	return M::None;
}

auto RuleMatcher::try_next(Token tok) -> Matched
{
	if (matched) {
		return Matched::None;
	}
	Matched code{};

	for (unsigned i = 0; i < matching.size(); ++i) {
		if (!matching[i]) {
			continue;
		}

		// When one of 'a', 'b' or 'n' match, then only one rule will
		// match, so the MatchCode::Multiple should never be produced when
		// matching Immediate tokens which need to be distinguished.
		auto c = match_one(rules[i][match_count], tok);
		if (c == Matched::None) {
			matching[i] = false;
		}
		if (code == Matched::None) {
			code = c;
		} else if (c != Matched::None && code != c) {
			code = Matched::Multiple;
		}

		if (c != Matched::None && match_count == rules[i].size() - 1) {
			matched = static_cast<Instruction>(i);
			break;
		}
	}

	if (code != Matched::None) {
		match_count++;
	}
	return code;
}
