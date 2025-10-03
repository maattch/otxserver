#include "../otpch.h"

#include "util.hpp"

int64_t otx::util::mstime()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void otx::util::trim_left_string(std::string& str, char c/* = ' '*/)
{
	str.erase(0, str.find_first_not_of(c));
}

void otx::util::trim_right_string(std::string& str, char c/* = ' '*/)
{
	str.erase(str.find_last_not_of(c) + 1);
}

void otx::util::trim_string(std::string& str, char c/* = ' '*/)
{
	trim_left_string(str, c);
	trim_right_string(str, c);
}

void otx::util::to_lower_string(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](uint8_t c) { return std::tolower(c); });
}

void otx::util::to_upper_string(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](uint8_t c) { return std::toupper(c); });
}

std::string otx::util::as_lower_string(std::string str)
{
	to_lower_string(str);
	return str;
}

std::string otx::util::as_upper_string(std::string str)
{
	to_upper_string(str);
	return str;
}

void otx::util::replace_all(std::string& str, std::string_view search, std::string_view rep)
{
	size_t pos = str.find(search);
	while (pos != std::string::npos) {
		str.replace(pos, search.size(), rep);
		pos = str.find(search, pos + rep.size());
	}
}