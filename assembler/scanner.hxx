#ifndef ASSEMBLER_SCANNER_HXX_INCLUDED
#define ASSEMBLER_SCANNER_HXX_INCLUDED

#include <optional>
#include <string_view>

class Scanner
{
public:
	Scanner(std::string_view s)
		: txt(s)
		, original(s)
	{
		if (!txt.empty())
			cur = txt[0];
	}

	int line() { return line_num; }
	int column() { return column_num; }
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
			if (cur == '\n') {
				line_num++;
				column_num = 0;
			}
			if (txt.empty()) {
				cur = {};
				break;
			}
			column_num++;
			pre = cur;
			cur = txt.front();
		}
	}
	int skip_while(auto(*pred)(char ch)->bool)
	{
		int cnt = 0;
		while (auto c = first()) {
			if (!pred(*c))
				break;
			cnt++;
			skip();
		}
		return cnt;
	}
	// void reset()
	// {
	// 	txt = original;
	// 	line_num = 0;
	// 	pre = {};
	// 	if (!txt.empty())
	// 		cur = txt[0];
	// }

private:
	std::string_view txt;
	std::string_view original;
	std::optional<char> cur = {};
	std::optional<char> pre = {};
	int line_num = 0;
	int column_num = 0;
};

#endif // END scanner.hxx
