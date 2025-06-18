#include <cctype>
#include <climits>
#include <optional>
#include <ranges>

#include "chip8.hxx"
#include "lexer.hxx"

Token Lexer::next()
{
	if (!next_token_as_line)
		return next_token();

	next_token_as_line = false;
	skip_blanks();

	Position pos{line, column};
	auto tok = make_token(TokenKind::Raw);
	start = at;
	for (;;) {
		auto c = peekc();
		if (is_at_end() || c == ';' || c == '\n')
			break;
		nextc();
	}

	tok.pos = pos;
	tok.lexeme = current_lexeme();
	return tok;
}

Token Lexer::next_token()
{
	skip_blanks();
	if (peekc() == ';') {
		// Discard comment line
		while (!is_at_end() && peekc() != '\n')
			nextc();
	}

	Token ret{};
	Position pos{.line = line, .column = column};
	char c = peekc();
	start = at;

	if (is_at_end())
		ret = make_token(TokenKind::Eof);
	else if (std::isdigit(c) || c == '+' || c == '-')
		ret = immediate();
	else if (std::isalpha(c) || c == '_')
		ret = identifier();
	else if (c == '%' && (std::isalpha(peekc(1)) || peekc(1) == '_'))
		ret = macro_token();
	else
		ret = make_token(TokenKind::Char, nextc());

	ret.pos = pos;
	ret.lexeme = current_lexeme();
	return ret;
}

static std::optional<int> char_to_idigit(char c)
{
	int v = std::tolower(c);
	if ('0' <= v && v <= '9')
		return v - '0';
	if ('a' <= v && v <= 'z')
		return v - 'a' + 10;
	return {};
}

static std::optional<int> mul_add_checked(int a, int b, int c)
{
	// Signed overflow is UB, so check for overflow before computing.
	if (((a > 0 && b > 0) || (a < 0 && b < 0)) && b > INT_MAX / a)
		return {};
	if (a < 0 && b > 0 && a < INT_MIN / b)
		return {};
	if (a > 0 && b < 0 && b < INT_MIN / a)
		return {};
	a *= b;

	if (a > 0 && c > 0 && a > INT_MAX - c)
		return {};
	if (a < 0 && c < 0 && a < INT_MIN - c)
		return {};
	return a + c;
}

Token Lexer::immediate()
{
	int base = 10;
	bool is_neg = false;
	int ret = 0;

	if (auto c = peekc(); c == '+' || c == '-') {
		is_neg = c == '-';
		nextc();
	}

	if (peekc() == '0') {
		char c = std::tolower(peekc(1));
		if (c == 'x')
			base = 16;
		else if (c == 'b')
			base = 2;
		else if (c == 'o')
			base = 8;
	}
	// Eat base prefix if present, and if nothing is after it then error.
	if (base != 10) {
		nextc();
		nextc();
		if (!std::isalnum(peekc()))
			return make_token(TokenKind::Invalid);
	}

	while (auto c = peekc()) {
		if (!std::isalnum(c))
			break;
		auto digit = char_to_idigit(c);
		if (!digit || *digit >= base)
			return make_token(TokenKind::Invalid);
		auto res = mul_add_checked(ret, base, *digit);
		if (!res)
			return make_token(TokenKind::Invalid);

		ret = *res;
		nextc();
	}

	if (is_neg)
		ret = -ret;
	return make_token(TokenKind::Immediate, ret);
}

inline bool is_ident_tail_char(char c) { return std::isalnum(c) || c == '_'; }

Token Lexer::identifier()
{
	while (is_ident_tail_char(peekc()))
		nextc();

	auto icase_match = [ident = current_lexeme()](std::string_view s) {
		return icase_equals(ident, s);
	};
	auto find_ident_in = [&](const auto &arr) -> int {
		if (auto r = std::ranges::find_if(arr, icase_match); r != std::end(arr))
			return r - arr;
		return -1;
	};

	if (icase_match("db"))
		return make_token(TokenKind::Db);
	if (find_ident_in(INSTRUCTIONS) >= 0)
		return make_token(TokenKind::Instruction);
	if (auto r = find_ident_in(REGISTERS); r >= 0)
		return make_token(TokenKind::Register, r);
	if (auto r = find_ident_in(SPECIAL_REGISTERS); r >= 0)
		return make_token(TokenKind::SpecialRegister);
	return make_token(TokenKind::Identifier);
}

Token Lexer::macro_token()
{
	nextc(); // eat '%'
	while (is_ident_tail_char(peekc()))
		nextc();
	auto ident = current_lexeme();

	if (icase_equals(ident, "%define"))
		return make_token(TokenKind::Define);
	return make_token(TokenKind::Invalid);
}

void Lexer::skip_blanks()
{
	while (std::isblank(peekc()))
		nextc();
}

char Lexer::peekc(unsigned adv) const
{
	return adv + at >= source.length() ? '\0' : source[at + adv];
}

char Lexer::nextc()
{
	if (is_at_end())
		return '\0';

	char ret = source[at++];
	if (ret == '\n') {
		line++;
		column = 1;
	} else {
		column++;
	}

	return ret;
}
