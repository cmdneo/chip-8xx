#pragma once

#include <cassert>
#include <string_view>
#include <functional>

class Scanner
{
public:
	explicit Scanner(const std::string_view s = "")
		: remaining(s)
		, original(s)
	{
		if (!remaining.empty())
			cur = remaining[0];
	}

	[[nodiscard]] int cursor() const { return cursor_at; }

	[[nodiscard]] bool is_at_end() const { return remaining.empty(); }

	[[nodiscard]] char prev() const { return pre; }

	[[nodiscard]] char first() const { return cur; }

	[[nodiscard]] char second() const
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

	std::string_view skip_while(const std::function<bool(char)> &unary_pred)
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
	int cursor_at = 0;
};
