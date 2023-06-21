#ifndef ASSEMBLER_SCANNER_HXX_INCLUDED
#define ASSEMBLER_SCANNER_HXX_INCLUDED

#include <cassert>
#include <string_view>
#include <functional>

class Scanner
{
public:
	Scanner(std::string_view s = "")
		: remaining(s)
		, original(s)
	{
		if (!remaining.empty())
			cur = remaining[0];
	}

	int cursor() const { return cursor_at; }

	bool is_at_end() const { return remaining.empty(); }

	char prev() const { return pre; }

	char first() const { return cur; }

	char second() const
	{
		if (remaining.size() > 1)
			return remaining[1];
		else
			return EOF_CHAR;
	}

	void skip(int n = 1)
	{
		assert(!remaining.empty());
		while (n--) {
			remaining = remaining.substr(1);
			cursor_at++;
			if (is_at_end()) {
				cur = EOF_CHAR;
				break;
			}
			pre = cur;
			cur = remaining.front();
		}
	}

	std::string_view skip_while(std::function<bool(char)> unary_pred)
	{
		auto ret = remaining;
		int cnt = 0;
		while (auto c = first()) {
			if (!unary_pred(c))
				break;
			cnt++;
			skip();
		}
		return ret.substr(0, cnt);
	}

private:
	static constexpr char EOF_CHAR = '\0';
	std::string_view remaining;
	std::string_view original;
	char cur = EOF_CHAR;
	char pre = EOF_CHAR;
	std::string_view::size_type cursor_at = 0;
};

#endif // END scanner.hxx
