#pragma once

#include <cctype>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <ostream>
#include <string>
#include <string_view>

#include "chip8.hxx"

enum class TokenKind : int {
	Invalid,
	Db,
	Define,
	Instruction,
	Register,
	SpecialRegister,
	Identifier,
	Immediate,
	Char,
	Raw,
	Eof,
};

struct Position {
	unsigned line;
	unsigned column;
};

struct Token {
	std::string_view lexeme{};
	TokenKind kind{};
	Position pos{};
	int value{};
	void *origin{}; // For storing other tracking data.

	explicit operator bool() const
	{
		return kind != TokenKind::Eof && kind != TokenKind::Invalid;
	}

	[[nodiscard]] auto as_debug(bool use_value = true) const -> std::string
	{
		constexpr std::array<std::string_view, 11> MNEMONICS = {
			"Invalid",     "Db",        "Define",
			"Instruction", "Register",  "SpecialRegister",
			"Identifier",  "Immediate", "Char",
			"Raw",         "Eof",
		};
		const auto name = MNEMONICS[static_cast<int>(kind)];
		if (!use_value) {
			return std::format("[{}]", name);
		}

		switch (kind) {
		case TokenKind::Char:
			if (std::isprint(value)) {
				auto c = static_cast<char>(value);
				return std::format("[{}, '{}']", name, c);
			}
			[[fallthrough]];
		case TokenKind::Immediate:
		case TokenKind::Register:
		case TokenKind::SpecialRegister:
			return std::format("[{}, {}]", name, value);

		case TokenKind::Identifier:
			return std::format("[{}, '{}']", name, lexeme);

		default:
			return std::format("[{}]", name);
		}
	}
};

inline auto make_token(TokenKind kind, int value = 0) -> Token
{
	return Token{
		.lexeme = "",
		.kind = kind,
		.pos = Position{},
		.value = value,
	};
}

class Lexer
{
public:
	Lexer() = default;

	explicit Lexer(std::string_view src)
		: source(src)
	{
	}

	auto next() -> Token;
	void set_next_token_as_line() { next_token_as_line = true; }

private:
	auto next_token() -> Token;
	auto immediate() -> Token;
	auto identifier() -> Token;
	auto macro_token() -> Token;
	void skip_blanks();

	[[nodiscard]] auto peekc(unsigned adv = 0) const -> char;
	auto nextc() -> char;

	[[nodiscard]] auto is_at_end() const -> bool
	{
		return at == source.length();
	}
	[[nodiscard]] auto current_lexeme() const -> std::string_view
	{
		return source.substr(start, at - start);
	}

	std::string_view source{};
	unsigned start = 0;
	unsigned at = 0;
	bool next_token_as_line = false;

	unsigned line = 1;
	unsigned column = 1;
};

inline auto icase_equals(std::string_view s, std::string_view t) -> bool
{
	return std::ranges::equal(s, t, [](auto c, auto d) {
		return std::tolower(c) == std::tolower(d);
	});
}
