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
#include "enums.h"

struct Vocation
{
	uint64_t getReqSkillTries(uint8_t skill, int32_t level) const;
	uint64_t getReqMana(uint32_t magLevel) const;
	int16_t getReflect(CombatType_t combat) const;
	double getExperienceMultiplier() const { return skillMultipliers[SKILL__LEVEL]; }

	std::string name;
	std::string description;

	uint32_t id = 0;
	uint32_t fromVocationId = 0;
	uint32_t baseSpeed = 220;
	uint32_t attackSpeed = 1500;
	uint32_t gain[GAIN_LAST + 1] = { 5, 5, 100 };
	uint32_t gainTicks[GAIN_LAST + 1] = { 6, 6, 120 };
	uint32_t gainAmount[GAIN_LAST + 1] = { 1, 1, 1 };
	uint32_t skillBase[SKILL_LAST + 1] = { 50, 50, 50, 50, 30, 100, 20 };

	int32_t lessLoss = 0;
	int32_t capGain = 5;

	float skillMultipliers[SKILL__LAST + 1] = { 1.5f, 2.f, 2.f, 2.f, 2.f, 2.f, 1.1f, 2.f, 1.0f, 2.f };
	float formulaMultipliers[MULTIPLIER_LAST + 1] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 4.f };

	int16_t absorb[COMBAT_LAST + 1] = {};
	int16_t reflect[REFLECT_LAST + 1][COMBAT_LAST + 1] = {};

	bool attackable = true;
	bool needPremium = false;
	bool dropLoot = true;
	bool skillLoss = true;
};

class Vocations final
{
public:
	Vocations() = default;

	// non-copyable
	Vocations(const Vocations&) = delete;
	Vocations& operator=(const Vocations&) = delete;

	// non-moveable
	Vocations(Vocations&&) = delete;
	Vocations& operator=(Vocations&&) = delete;

	void clear() { m_vocations.clear(); }

	bool reload();
	bool loadFromXml();

	Vocation* getVocation(uint32_t id);
	int32_t getVocationId(const std::string& name);
	int32_t getPromotedVocation(uint32_t id);

	const auto& getVocations() { return m_vocations; }

private:
	std::map<uint32_t, Vocation> m_vocations;
};

extern Vocations g_vocations;
