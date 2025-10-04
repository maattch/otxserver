#pragma once

namespace otx::util
{
	// safe-cast
	template<class T> inline std::pair<T, bool> safe_cast(const char* str);

	template<> inline std::pair<double, bool> safe_cast(const char* str)
	{
		char* endp;
		auto v = std::strtod(str, &endp);
		return { v, (endp != str && *endp == '\0') };
	}
	template<> inline std::pair<float, bool> safe_cast(const char* str)
	{
		char* endp;
		auto v = std::strtof(str, &endp);
		return { v, (endp != str && *endp == '\0') };
	}
	template<> inline std::pair<long, bool> safe_cast(const char* str)
	{
		char* endp;
		auto v = std::strtol(str, &endp, 0);
		return { v, (endp != str && *endp == '\0') };
	}
	template<> inline std::pair<long long, bool> safe_cast(const char* str)
	{
		char* endp;
		auto v = std::strtoll(str, &endp, 0);
		return { v, (endp != str && *endp == '\0') };
	}
	template<> inline std::pair<unsigned long, bool> safe_cast(const char* str)
	{
		char* endp;
		unsigned long v = std::strtoul(str, &endp, 0);
		return { v, (endp != str && *endp == '\0') };
	}
	template<> inline std::pair<unsigned long long, bool> safe_cast(const char* str)
	{
		char* endp;
		auto v = std::strtoull(str, &endp, 0);
		return { v, (endp != str && *endp == '\0') };
	}

	template<> inline std::pair<char, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<long>(str);
		return { static_cast<char>(p.first), p.second };
	}
	template<> inline std::pair<signed char, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<long>(str);
		return { static_cast<signed char>(p.first), p.second };
	}
	template<> inline std::pair<short, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<long>(str);
		return { static_cast<short>(p.first), p.second };
	}
	template<> inline std::pair<int, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<long>(str);
		return { static_cast<int>(p.first), p.second };
	}
	template<> inline std::pair<unsigned char, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<unsigned long>(str);
		return { static_cast<unsigned char>(p.first), p.second };
	}
	template<> inline std::pair<unsigned short, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<unsigned long>(str);
		return { static_cast<unsigned short>(p.first), p.second };
	}
	template<> inline std::pair<unsigned int, bool> safe_cast(const char* str)
	{
		auto p = safe_cast<unsigned long>(str);
		return { static_cast<unsigned int>(p.first), p.second };
	}

	template<class T> T cast(const char* str) { return safe_cast<T>(str).first; }

} // otx::util
