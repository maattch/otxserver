////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#pragma once

#include "const.h"

enum AddonRequirement_t : uint8_t
{
	REQUIREMENT_NONE = 0,
	REQUIREMENT_FIRST,
	REQUIREMENT_SECOND,
	REQUIREMENT_BOTH,
	REQUIREMENT_ANY
};

struct Outfit
{
	std::string name;
	std::string storageId;
	std::string storageValue;

	std::vector<int32_t> groups;

	uint32_t id = 0;
	uint32_t conditionSuppressions = 0;

	int32_t speed = 0;
	int32_t attackSpeed = 0;
	int32_t healthGain = 0;
	int32_t healthTicks = 0;
	int32_t manaGain = 0;
	int32_t manaTicks = 0;
	int32_t skills[SKILL_LAST + 1] = {};
	int32_t skillsPercent[SKILL_LAST + 1] = {};
	int32_t stats[STAT_LAST + 1] = {};
	int32_t statsPercent[STAT_LAST + 1] = {};

	uint16_t lookType = 0;
	uint16_t accessLevel = 0;

	int16_t reflect[REFLECT_LAST + 1][COMBATINDEX_LAST + 1] = {};
	int16_t absorb[COMBATINDEX_LAST + 1] = {};

	uint8_t addons = 0;

	AddonRequirement_t requirement = REQUIREMENT_BOTH;

	bool isDefault = true;
	bool isPremium = false;
	bool manaShield = false;
	bool invisible = false;
	bool regeneration = false;
};

class OutfitsManager final
{
public:
	OutfitsManager() = default;

	// non-copyable
	OutfitsManager(const OutfitsManager&) = delete;
	OutfitsManager& operator=(const OutfitsManager&) = delete;

	bool reload();
	bool loadFromXml();

	const auto& getOutfits(uint8_t sex) { return m_outfitsByGender[sex]; }

	const Outfit* getOutfitById(uint32_t id, uint8_t sex);
	const Outfit* getOutfitByLookType(uint16_t lookType);
	uint32_t getOutfitIdByLookType(uint16_t lookType);

	bool addAttributes(Player* player, uint16_t lookType, uint8_t addons);
	void removeAttributes(Player* player, uint16_t lookType);

	int16_t getOutfitAbsorb(uint16_t lookType, uint8_t index);
	int16_t getOutfitReflect(uint16_t lookType, uint8_t index);

private:
	std::unordered_map<uint16_t, Outfit> m_outfits;
	std::map<uint8_t, std::map<uint32_t, Outfit*>> m_outfitsByGender;
};
