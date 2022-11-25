#ifndef ASSEMBLER_SCANNER_HXX_INCLUDED
#define ASSEMBLER_SCANNER_HXX_INCLUDED

#include <optional>
#include <string_view>
#include <functional>

class Scanner
{
public:
	Scanner(std::string_view s = "")
		: txt(s)
		, original(s)
	{
		if (!txt.empty())
			cur = txt[0];
	}

	int cursor() { return cursor_at; }
	std::string_view view() { return txt; }
	std::optional<char> prev() { return pre; }
	std::optional<char> first() { return cur; }
	std::optional<char> second()
	{
		if (txt.size() > 1)
			return txt[1];
		else
			return {};
	}
	void skip(int n = 1)
	{
		while (n--) {
			txt = txt.substr(1);
			if (txt.empty()) {
				cur = {};
				break;
			}
			cursor_at++;
			pre = cur;
			cur = txt.front();
		}
	}
	std::string_view skip_while(std::function<bool(char)> unary_pred)
	{
		auto ret = txt;
		int cnt = 0;
		while (auto c = first()) {
			if (!unary_pred(*c))
				break;
			cnt++;
			skip();
		}
		return ret.substr(0, cnt);
	}

private:
	std::string_view txt;
	std::string_view original;
	std::optional<char> cur = {};
	std::optional<char> pre = {};
	std::string_view::size_type cursor_at = 0;
};

#endif // END scanner.hxx
