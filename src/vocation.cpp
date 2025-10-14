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

#include "otpch.h"

#include "vocation.h"

#include "tools.h"

Vocations g_vocations;

bool Vocations::reload()
{
	clear();
	return loadFromXml();
}

bool Vocations::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "vocations.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - Vocations::loadFromXml] Cannot load vocations file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("vocations"))) {
		std::clog << "[Error - Vocations::loadFromXml] Malformed vocations file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	int32_t intValue;
	float floatValue;
	for (xmlNodePtr p = root->children; p; p = p->next) {
		if (xmlStrcmp(p->name, reinterpret_cast<const xmlChar*>("vocation"))) {
			continue;
		}

		if (!readXMLInteger(p, "id", intValue)) {
			std::clog << "[Error - Vocations::loadFromXml] Missing vocation id." << std::endl;
			continue;
		}

		const auto vocationId = static_cast<uint32_t>(intValue);

		Vocation vocation;
		vocation.id = vocationId;

		if (readXMLString(p, "name", strValue)) {
			vocation.name = strValue;
		}
		if (readXMLString(p, "description", strValue)) {
			vocation.description = strValue;
		}
		if (readXMLString(p, "needpremium", strValue)) {
			vocation.needPremium = booleanString(strValue);
		}
		if (readXMLInteger(p, "gaincap", intValue) || readXMLInteger(p, "gaincapacity", intValue)) {
			vocation.capGain = intValue;
		}
		if (readXMLInteger(p, "gainhp", intValue) || readXMLInteger(p, "gainhealth", intValue)) {
			vocation.gain[GAIN_HEALTH] = intValue;
		}
		if (readXMLInteger(p, "gainmana", intValue)) {
			vocation.gain[GAIN_MANA] = intValue;
		}
		if (readXMLInteger(p, "gainhpticks", intValue) || readXMLInteger(p, "gainhealthticks", intValue)) {
			vocation.gainTicks[GAIN_HEALTH] = intValue;
		}
		if (readXMLInteger(p, "gainhpamount", intValue) || readXMLInteger(p, "gainhealthamount", intValue)) {
			vocation.gainAmount[GAIN_HEALTH] = intValue;
		}
		if (readXMLInteger(p, "gainmanaticks", intValue)) {
			vocation.gainTicks[GAIN_MANA] = intValue;
		}
		if (readXMLInteger(p, "gainmanaamount", intValue)) {
			vocation.gainAmount[GAIN_MANA] = intValue;
		}
		if (readXMLInteger(p, "attackspeed", intValue)) {
			vocation.attackSpeed = intValue;
		}
		if (readXMLInteger(p, "basespeed", intValue)) {
			vocation.baseSpeed = intValue;
		}
		if (readXMLInteger(p, "soulmax", intValue)) {
			vocation.gain[GAIN_SOUL] = intValue;
		}
		if (readXMLInteger(p, "gainsoulamount", intValue)) {
			vocation.gainAmount[GAIN_SOUL] = intValue;
		}
		if (readXMLInteger(p, "gainsoulticks", intValue)) {
			vocation.gainTicks[GAIN_SOUL] = intValue;
		}
		if (readXMLString(p, "attackable", strValue)) {
			vocation.attackable = booleanString(strValue);
		}
		if (readXMLInteger(p, "fromvoc", intValue) || readXMLInteger(p, "fromvocation", intValue)) {
			vocation.fromVocationId = intValue;
		}
		if (readXMLInteger(p, "lessloss", intValue)) {
			vocation.lessLoss = intValue;
		}
		if (readXMLString(p, "droploot", strValue) || readXMLString(p, "lootdrop", strValue)) {
			vocation.dropLoot = booleanString(strValue);
		}
		if (readXMLString(p, "skillloss", strValue) || readXMLString(p, "lossskill", strValue)) {
			vocation.skillLoss = booleanString(strValue);
		}
		if (readXMLFloat(p, "manamultiplier", floatValue)) {
			if (floatValue >= 1.0f) {
				vocation.formulaMultipliers[MULTIPLIER_MANA] = floatValue;
			} else {
				std::clog << "[Error - Vocations::loadFromXml] Mana multiplier must be equal or greater than 1." << std::endl;
			}
		}

		for (xmlNodePtr configNode = p->children; configNode; configNode = configNode->next) {
			if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("skill"))) {
				if (readXMLFloat(configNode, "fist", floatValue)) {
					vocation.skillMultipliers[SKILL_FIST] = floatValue;
				}
				if (readXMLInteger(configNode, "fistBase", intValue)) {
					vocation.skillBase[SKILL_FIST] = intValue;
				}
				if (readXMLFloat(configNode, "club", floatValue)) {
					vocation.skillMultipliers[SKILL_CLUB] = floatValue;
				}
				if (readXMLInteger(configNode, "clubBase", intValue)) {
					vocation.skillBase[SKILL_CLUB] = intValue;
				}
				if (readXMLFloat(configNode, "axe", floatValue)) {
					vocation.skillMultipliers[SKILL_AXE] = floatValue;
				}
				if (readXMLInteger(configNode, "axeBase", intValue)) {
					vocation.skillBase[SKILL_AXE] = intValue;
				}
				if (readXMLFloat(configNode, "sword", floatValue)) {
					vocation.skillMultipliers[SKILL_SWORD] = floatValue;
				}
				if (readXMLInteger(configNode, "swordBase", intValue)) {
					vocation.skillBase[SKILL_SWORD] = intValue;
				}
				if (readXMLFloat(configNode, "distance", floatValue) || readXMLFloat(configNode, "dist", floatValue)) {
					vocation.skillMultipliers[SKILL_DIST] = floatValue;
				}
				if (readXMLInteger(configNode, "distanceBase", intValue) || readXMLInteger(configNode, "distBase", intValue)) {
					vocation.skillBase[SKILL_DIST] = intValue;
				}
				if (readXMLFloat(configNode, "shielding", floatValue) || readXMLFloat(configNode, "shield", floatValue)) {
					vocation.skillMultipliers[SKILL_SHIELD] = floatValue;
				}
				if (readXMLInteger(configNode, "shieldingBase", intValue) || readXMLInteger(configNode, "shieldBase", intValue)) {
					vocation.skillBase[SKILL_SHIELD] = intValue;
				}
				if (readXMLFloat(configNode, "fishing", floatValue) || readXMLFloat(configNode, "fish", floatValue)) {
					vocation.skillMultipliers[SKILL_FISH] = floatValue;
				}
				if (readXMLInteger(configNode, "fishingBase", intValue) || readXMLInteger(configNode, "fishBase", intValue)) {
					vocation.skillBase[SKILL_FISH] = intValue;
				}
				if (readXMLFloat(configNode, "experience", floatValue) || readXMLFloat(configNode, "exp", floatValue)) {
					vocation.skillMultipliers[SKILL__LEVEL] = floatValue;
				}

				if (readXMLInteger(configNode, "id", intValue)) {
					const auto skill = static_cast<Skills_t>(intValue);
					if (skill < SKILL_FIRST || skill >= SKILL__LAST) {
						std::clog << "[Error - Vocations::loadFromXml] Invalid skill id (" << intValue << ")." << std::endl;
						continue;
					}

					if (readXMLInteger(configNode, "base", intValue)) {
						vocation.skillBase[skill] = intValue;
					}

					if (readXMLFloat(configNode, "multiplier", floatValue)) {
						vocation.skillMultipliers[skill] = floatValue;
					}
				}
			} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("formula"))) {
				if (readXMLFloat(configNode, "meleeDamage", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_MELEE] = floatValue;
				}
				if (readXMLFloat(configNode, "distDamage", floatValue) || readXMLFloat(configNode, "distanceDamage", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_DISTANCE] = floatValue;
				}
				if (readXMLFloat(configNode, "wandDamage", floatValue) || readXMLFloat(configNode, "rodDamage", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_WAND] = floatValue;
				}
				if (readXMLFloat(configNode, "magDamage", floatValue) || readXMLFloat(configNode, "magicDamage", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_MAGIC] = floatValue;
				}
				if (readXMLFloat(configNode, "magHealingDamage", floatValue) || readXMLFloat(configNode, "magicHealingDamage", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_HEALING] = floatValue;
				}
				if (readXMLFloat(configNode, "defense", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_DEFENSE] = floatValue;
				}
				if (readXMLFloat(configNode, "magDefense", floatValue) || readXMLFloat(configNode, "magicDefense", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_MAGICDEFENSE] = floatValue;
				}
				if (readXMLFloat(configNode, "armor", floatValue)) {
					vocation.formulaMultipliers[MULTIPLIER_ARMOR] = floatValue;
				}
			} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("absorb"))) {
				if (readXMLInteger(configNode, "percentAll", intValue)) {
					for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
						vocation.absorb[i] = intValue;
					}
				}

				if (readXMLInteger(configNode, "percentElements", intValue)) {
					vocation.absorb[COMBATINDEX_ENERGYDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_FIREDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_EARTHDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_ICEDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentMagic", intValue)) {
					vocation.absorb[COMBATINDEX_ENERGYDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_FIREDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_EARTHDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_ICEDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_HOLYDAMAGE] = intValue;
					vocation.absorb[COMBATINDEX_DEATHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentEnergy", intValue)) {
					vocation.absorb[COMBATINDEX_ENERGYDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentFire", intValue)) {
					vocation.absorb[COMBATINDEX_FIREDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentPoison", intValue) || readXMLInteger(configNode, "percentEarth", intValue)) {
					vocation.absorb[COMBATINDEX_EARTHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentIce", intValue)) {
					vocation.absorb[COMBATINDEX_ICEDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentHoly", intValue)) {
					vocation.absorb[COMBATINDEX_HOLYDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentDeath", intValue)) {
					vocation.absorb[COMBATINDEX_DEATHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentLifeDrain", intValue)) {
					vocation.absorb[COMBATINDEX_LIFEDRAIN] = intValue;
				}
				if (readXMLInteger(configNode, "percentManaDrain", intValue)) {
					vocation.absorb[COMBATINDEX_MANADRAIN] = intValue;
				}
				if (readXMLInteger(configNode, "percentDrown", intValue)) {
					vocation.absorb[COMBATINDEX_DROWNDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentPhysical", intValue)) {
					vocation.absorb[COMBATINDEX_PHYSICALDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentHealing", intValue)) {
					vocation.absorb[COMBATINDEX_HEALING] = intValue;
				}
				if (readXMLInteger(configNode, "percentUndefined", intValue)) {
					vocation.absorb[COMBATINDEX_UNDEFINEDDAMAGE] = intValue;
				}
			} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("reflect"))) {
				if (readXMLInteger(configNode, "percentAll", intValue)) {
					for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
						vocation.reflect[REFLECT_PERCENT][i] = intValue;
					}
				}

				if (readXMLInteger(configNode, "percentElements", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentMagic", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_HOLYDAMAGE] = intValue;
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_DEATHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentEnergy", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentFire", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentPoison", intValue) || readXMLInteger(configNode, "percentEarth", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentIce", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentHoly", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_HOLYDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentDeath", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_DEATHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentLifeDrain", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_LIFEDRAIN] = intValue;
				}
				if (readXMLInteger(configNode, "percentManaDrain", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_MANADRAIN] = intValue;
				}
				if (readXMLInteger(configNode, "percentDrown", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_DROWNDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentPhysical", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_PHYSICALDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "percentHealing", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_HEALING] = intValue;
				}
				if (readXMLInteger(configNode, "percentUndefined", intValue)) {
					vocation.reflect[REFLECT_PERCENT][COMBATINDEX_UNDEFINEDDAMAGE] = intValue;
				}

				if (readXMLInteger(configNode, "chanceAll", intValue)) {
					for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
						vocation.reflect[REFLECT_CHANCE][i] = intValue;
					}
				}

				if (readXMLInteger(configNode, "chanceElements", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceMagic", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_HOLYDAMAGE] = intValue;
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_DEATHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceEnergy", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceFire", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chancePoison", intValue) || readXMLInteger(configNode, "chanceEarth", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceIce", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceHoly", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_HOLYDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceDeath", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_DEATHDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceLifeDrain", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_LIFEDRAIN] = intValue;
				}
				if (readXMLInteger(configNode, "chanceManaDrain", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_MANADRAIN] = intValue;
				}
				if (readXMLInteger(configNode, "chanceDrown", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_DROWNDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chancePhysical", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_PHYSICALDAMAGE] = intValue;
				}
				if (readXMLInteger(configNode, "chanceHealing", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_HEALING] = intValue;
				}
				if (readXMLInteger(configNode, "chanceUndefined", intValue)) {
					vocation.reflect[REFLECT_CHANCE][COMBATINDEX_UNDEFINEDDAMAGE] = intValue;
				}
			}
		}

		if (!m_vocations.emplace(vocationId, std::move(vocation)).second) {
			std::clog << "[Error - Vocations::loadFromXml] Duplicate vocation with id " << vocationId << std::endl;
		}
	}

	xmlFreeDoc(doc);
	return true;
}

Vocation* Vocations::getVocation(uint32_t id)
{
	auto it = m_vocations.find(id);
	if (it != m_vocations.end()) {
		return &it->second;
	}
	return nullptr;
}

int32_t Vocations::getVocationId(const std::string& name)
{
	for (const auto& [id, vocation] : m_vocations) {
		if (caseInsensitiveEqual(vocation.name, name)) {
			return id;
		}
	}
	return -1;
}

int32_t Vocations::getPromotedVocation(uint32_t vocationId)
{
	for (const auto& [id, vocation] : m_vocations) {
		if (vocation.fromVocationId == vocationId && id != vocationId) {
			return id;
		}
	}
	return -1;
}

int16_t Vocation::getReflect(uint16_t index) const
{
	assert(index >= COMBATINDEX_FIRST && index <= COMBATINDEX_LAST);
	if (reflect[REFLECT_CHANCE][index] >= random_range(1, 100)) {
		return reflect[REFLECT_PERCENT][index];
	}
	return 0;
}

uint64_t Vocation::getReqSkillTries(uint8_t skill, int32_t level) const
{
	if (skill > SKILL_LAST || level <= 10) {
		return 0;
	}
	return skillBase[skill] * std::pow(skillMultipliers[skill], (level - 11));
}

uint64_t Vocation::getReqMana(uint32_t magLevel) const
{
	if (magLevel == 0) {
		return 0;
	}
	return 1600 * std::pow(formulaMultipliers[MULTIPLIER_MANA], magLevel - 1);
}
