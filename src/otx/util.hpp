#pragma once

#include "../enums.h"

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

} // otx::util
