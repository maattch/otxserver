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

uint16_t otx::util::combat_index(CombatType_t combat)
{
	switch (combat) {
		case COMBAT_PHYSICALDAMAGE:  return COMBATINDEX_PHYSICALDAMAGE;
		case COMBAT_ENERGYDAMAGE:    return COMBATINDEX_ENERGYDAMAGE;
		case COMBAT_EARTHDAMAGE:     return COMBATINDEX_EARTHDAMAGE;
		case COMBAT_FIREDAMAGE:      return COMBATINDEX_FIREDAMAGE;
		case COMBAT_UNDEFINEDDAMAGE: return COMBATINDEX_UNDEFINEDDAMAGE;
		case COMBAT_LIFEDRAIN:       return COMBATINDEX_LIFEDRAIN;
		case COMBAT_MANADRAIN:       return COMBATINDEX_MANADRAIN;
		case COMBAT_HEALING:         return COMBATINDEX_HEALING;
		case COMBAT_DROWNDAMAGE:     return COMBATINDEX_DROWNDAMAGE;
		case COMBAT_ICEDAMAGE:       return COMBATINDEX_ICEDAMAGE;
		case COMBAT_HOLYDAMAGE:      return COMBATINDEX_HOLYDAMAGE;
		case COMBAT_DEATHDAMAGE:     return COMBATINDEX_DEATHDAMAGE;

		default:
			return COMBAT_NONE;
	}
}

CombatType_t otx::util::index_combat(uint16_t combat)
{
	switch (combat) {
		case COMBATINDEX_PHYSICALDAMAGE:  return COMBAT_PHYSICALDAMAGE;
		case COMBATINDEX_ENERGYDAMAGE:    return COMBAT_ENERGYDAMAGE;
		case COMBATINDEX_EARTHDAMAGE:     return COMBAT_EARTHDAMAGE;
		case COMBATINDEX_FIREDAMAGE:      return COMBAT_FIREDAMAGE;
		case COMBATINDEX_UNDEFINEDDAMAGE: return COMBAT_UNDEFINEDDAMAGE;
		case COMBATINDEX_LIFEDRAIN:       return COMBAT_LIFEDRAIN;
		case COMBATINDEX_MANADRAIN:       return COMBAT_MANADRAIN;
		case COMBATINDEX_HEALING:         return COMBAT_HEALING;
		case COMBATINDEX_DROWNDAMAGE:     return COMBAT_DROWNDAMAGE;
		case COMBATINDEX_ICEDAMAGE:       return COMBAT_ICEDAMAGE;
		case COMBATINDEX_HOLYDAMAGE:      return COMBAT_HOLYDAMAGE;
		case COMBATINDEX_DEATHDAMAGE:     return COMBAT_DEATHDAMAGE;

		default:
			return COMBAT_NONE;
	}
}
