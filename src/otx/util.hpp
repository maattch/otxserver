#pragma once

#include "../const.h"

namespace otx::util
{
	int64_t mstime();

	void trim_left_string(std::string& str, char c = ' ');
	void trim_right_string(std::string& str, char c = ' ');
	void trim_string(std::string& str, char c = ' ');

	void to_lower_string(std::string& str);
	void to_upper_string(std::string& str);
	std::string as_lower_string(std::string str);
	std::string as_upper_string(std::string str);

	void replace_all(std::string& str, std::string_view search, std::string_view rep);

	uint16_t combat_index(CombatType_t combat);
	CombatType_t index_combat(uint16_t combat);


	// This is only here because C++17 std::abs is not constexpr
	template<class T>
	constexpr std::enable_if_t<std::is_signed_v<T> || std::is_floating_point_v<T>, T>
	abs(T value) noexcept {
		return value < 0 ? -value : value;
	}

} // otx::util
