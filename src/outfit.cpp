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

#include "outfit.h"

#include "condition.h"
#include "game.h"
#include "player.h"

#include "otx/util.hpp"

bool OutfitsManager::reload()
{
	m_outfitsByGender.clear();
	m_outfits.clear();
	return loadFromXml();
}

bool OutfitsManager::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "outfits.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - OutfitsManager::loadFromXml] Cannot load outfits file, using defaults." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("outfits"))) {
		std::clog << "[Error - OutfitsManager::loadFromXml] Malformed outfits file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	int32_t intValue;
	for (xmlNodePtr p = root->children; p; p = p->next) {
		if (xmlStrcmp(p->name, reinterpret_cast<const xmlChar*>("outfit"))) {
			continue;
		}

		if (!readXMLInteger(p, "id", intValue)) {
			std::clog << "[Error - OutfitsManager::loadFromXml] Missing outfit id, skipping" << std::endl;
			continue;
		}

		Outfit outfit;
		outfit.id = intValue;

		if (readXMLString(p, "default", strValue)) {
			outfit.isDefault = booleanString(strValue);
		}

		if (!readXMLString(p, "name", strValue)) {
			outfit.name = "Outfit #" + std::to_string(outfit.id);
		} else {
			outfit.name = strValue;
		}

		if (readXMLInteger(p, "access", intValue)) {
			outfit.accessLevel = intValue;
		}

		if (readXMLString(p, "group", strValue) || readXMLString(p, "groups", strValue)) {
			outfit.groups = parseStringInts(strValue);
			if (outfit.groups.empty()) {
				std::clog << "[Warning - OutfitsManager::loadFromXml] Invalid group(s) for an outfit with id " << outfit.id << std::endl;
			}
		}

		if (readXMLString(p, "quest", strValue)) {
			outfit.storageId = strValue;
			outfit.storageValue = "1";
		} else {
			if (readXMLString(p, "storageId", strValue)) {
				outfit.storageId = strValue;
			}

			if (readXMLString(p, "storageValue", strValue)) {
				outfit.storageValue = strValue;
			}
		}

		if (readXMLString(p, "premium", strValue)) {
			outfit.isPremium = booleanString(strValue);
		}

		for (xmlNodePtr listNode = p->children; listNode != nullptr; listNode = listNode->next) {
			if (xmlStrcmp(listNode->name, reinterpret_cast<const xmlChar*>("list"))) {
				continue;
			}

			if (!readXMLInteger(listNode, "looktype", intValue) && !readXMLInteger(listNode, "lookType", intValue)) {
				std::clog << "[Error - OutfitsManager::loadFromXml] Missing looktype for an outfit with id " << outfit.id << std::endl;
				continue;
			}

			outfit.lookType = intValue;
			if (!readXMLString(listNode, "gender", strValue) && !readXMLString(listNode, "type", strValue) && !readXMLString(listNode, "sex", strValue)) {
				std::clog << "[Error - OutfitsManager::loadFromXml] Missing gender(s) for an outfit with id " << outfit.id
					<< " and looktype " << outfit.lookType << std::endl;
				continue;
			}

			auto intVector = parseStringInts(strValue);
			if (intVector.empty()) {
				std::clog << "[Error - OutfitsManager::loadFromXml] Invalid gender(s) for an outfit with id " << outfit.id << " and looktype " << outfit.lookType << std::endl;
				continue;
			}

			if (readXMLInteger(listNode, "addons", intValue)) {
				outfit.addons = intValue;
			}

			if (readXMLString(listNode, "name", strValue)) {
				outfit.name = strValue;
			}

			if (readXMLString(listNode, "premium", strValue)) {
				outfit.isPremium = booleanString(strValue);
			}

			if (readXMLString(listNode, "requirement", strValue)) {
				std::string tmpStrValue = otx::util::as_lower_string(strValue);
				if (tmpStrValue == "none") {
					outfit.requirement = REQUIREMENT_NONE;
				} else if (tmpStrValue == "first") {
					outfit.requirement = REQUIREMENT_FIRST;
				} else if (tmpStrValue == "second") {
					outfit.requirement = REQUIREMENT_SECOND;
				} else if (tmpStrValue == "any") {
					outfit.requirement = REQUIREMENT_ANY;
				} else if (tmpStrValue != "both") {
					std::clog << "[Warning - OutfitsManager::loadFromXml] Unknown requirement tag value, using default (both)" << std::endl;
				}
			}

			if (readXMLString(listNode, "manaShield", strValue)) {
				outfit.manaShield = booleanString(strValue);
			}

			if (readXMLString(listNode, "invisible", strValue)) {
				outfit.invisible = booleanString(strValue);
			}

			if (readXMLInteger(listNode, "healthGain", intValue)) {
				outfit.healthGain = intValue;
				outfit.regeneration = true;
			}

			if (readXMLInteger(listNode, "healthTicks", intValue)) {
				outfit.healthTicks = intValue;
				outfit.regeneration = true;
			}

			if (readXMLInteger(listNode, "manaGain", intValue)) {
				outfit.manaGain = intValue;
				outfit.regeneration = true;
			}

			if (readXMLInteger(listNode, "manaTicks", intValue)) {
				outfit.manaTicks = intValue;
				outfit.regeneration = true;
			}

			if (readXMLInteger(listNode, "speed", intValue)) {
				outfit.speed = intValue;
			}

			if (readXMLInteger(listNode, "attackspeed", intValue) || readXMLInteger(listNode, "attackSpeed", intValue)) {
				outfit.attackSpeed = intValue;
			}

			for (xmlNodePtr configNode = listNode->children; configNode != nullptr; configNode = configNode->next) {
				if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("reflect"))) {
					if (readXMLInteger(configNode, "percentAll", intValue)) {
						for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
							outfit.reflect[REFLECT_PERCENT][i] += intValue;
						}
					}
					if (readXMLInteger(configNode, "percentElements", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentMagic", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_HOLYDAMAGE] += intValue;
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_DEATHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentEnergy", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_ENERGYDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentFire", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_FIREDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentPoison", intValue) || readXMLInteger(configNode, "percentEarth", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_EARTHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentIce", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_ICEDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentHoly", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_HOLYDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentDeath", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_DEATHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentLifeDrain", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_LIFEDRAIN] += intValue;
					}
					if (readXMLInteger(configNode, "percentManaDrain", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_MANADRAIN] += intValue;
					}
					if (readXMLInteger(configNode, "percentDrown", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_DROWNDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentPhysical", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_PHYSICALDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentHealing", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_HEALING] += intValue;
					}
					if (readXMLInteger(configNode, "percentUndefined", intValue)) {
						outfit.reflect[REFLECT_PERCENT][COMBATINDEX_UNDEFINEDDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceAll", intValue)) {
						for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
							outfit.reflect[REFLECT_CHANCE][i] += intValue;
						}
					}
					if (readXMLInteger(configNode, "chanceElements", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceMagic", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_HOLYDAMAGE] += intValue;
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_DEATHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceEnergy", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_ENERGYDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceFire", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_FIREDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chancePoison", intValue) || readXMLInteger(configNode, "chanceEarth", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_EARTHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceIce", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_ICEDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceHoly", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_HOLYDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceDeath", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_DEATHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceLifeDrain", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_LIFEDRAIN] += intValue;
					}
					if (readXMLInteger(configNode, "chanceManaDrain", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_MANADRAIN] += intValue;
					}
					if (readXMLInteger(configNode, "chanceDrown", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_DROWNDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chancePhysical", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_PHYSICALDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "chanceHealing", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_HEALING] += intValue;
					}
					if (readXMLInteger(configNode, "chanceUndefined", intValue)) {
						outfit.reflect[REFLECT_CHANCE][COMBATINDEX_UNDEFINEDDAMAGE] += intValue;
					}
				} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("absorb"))) {
					if (readXMLInteger(configNode, "percentAll", intValue)) {
						for (uint16_t i = COMBATINDEX_FIRST; i <= COMBATINDEX_LAST; ++i) {
							outfit.absorb[i] += intValue;
						}
					}
					if (readXMLInteger(configNode, "percentElements", intValue)) {
						outfit.absorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_FIREDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_EARTHDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_ICEDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentMagic", intValue)) {
						outfit.absorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_FIREDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_EARTHDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_ICEDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_HOLYDAMAGE] += intValue;
						outfit.absorb[COMBATINDEX_DEATHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentEnergy", intValue)) {
						outfit.absorb[COMBATINDEX_ENERGYDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentFire", intValue)) {
						outfit.absorb[COMBATINDEX_FIREDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentPoison", intValue) || readXMLInteger(configNode, "percentEarth", intValue)) {
						outfit.absorb[COMBATINDEX_EARTHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentIce", intValue)) {
						outfit.absorb[COMBATINDEX_ICEDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentHoly", intValue)) {
						outfit.absorb[COMBATINDEX_HOLYDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentDeath", intValue)) {
						outfit.absorb[COMBATINDEX_DEATHDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentLifeDrain", intValue)) {
						outfit.absorb[COMBATINDEX_LIFEDRAIN] += intValue;
					}
					if (readXMLInteger(configNode, "percentManaDrain", intValue)) {
						outfit.absorb[COMBATINDEX_MANADRAIN] += intValue;
					}
					if (readXMLInteger(configNode, "percentDrown", intValue)) {
						outfit.absorb[COMBATINDEX_DROWNDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentPhysical", intValue)) {
						outfit.absorb[COMBATINDEX_PHYSICALDAMAGE] += intValue;
					}
					if (readXMLInteger(configNode, "percentHealing", intValue)) {
						outfit.absorb[COMBATINDEX_HEALING] += intValue;
					}
					if (readXMLInteger(configNode, "percentUndefined", intValue)) {
						outfit.absorb[COMBATINDEX_UNDEFINEDDAMAGE] += intValue;
					}
				} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("skills"))) {
					if (readXMLInteger(configNode, "fist", intValue)) {
						outfit.skills[SKILL_FIST] += intValue;
					}
					if (readXMLInteger(configNode, "club", intValue)) {
						outfit.skills[SKILL_CLUB] += intValue;
					}
					if (readXMLInteger(configNode, "axe", intValue)) {
						outfit.skills[SKILL_AXE] += intValue;
					}
					if (readXMLInteger(configNode, "sword", intValue)) {
						outfit.skills[SKILL_SWORD] += intValue;
					}
					if (readXMLInteger(configNode, "distance", intValue) || readXMLInteger(configNode, "dist", intValue)) {
						outfit.skills[SKILL_DIST] += intValue;
					}
					if (readXMLInteger(configNode, "shielding", intValue) || readXMLInteger(configNode, "shield", intValue)) {
						outfit.skills[SKILL_SHIELD] = intValue;
					}
					if (readXMLInteger(configNode, "fishing", intValue) || readXMLInteger(configNode, "fish", intValue)) {
						outfit.skills[SKILL_FISH] = intValue;
					}
					if (readXMLInteger(configNode, "melee", intValue)) {
						outfit.skills[SKILL_FIST] += intValue;
						outfit.skills[SKILL_CLUB] += intValue;
						outfit.skills[SKILL_SWORD] += intValue;
						outfit.skills[SKILL_AXE] += intValue;
					}
					if (readXMLInteger(configNode, "weapon", intValue) || readXMLInteger(configNode, "weapons", intValue)) {
						outfit.skills[SKILL_CLUB] += intValue;
						outfit.skills[SKILL_SWORD] += intValue;
						outfit.skills[SKILL_AXE] += intValue;
						outfit.skills[SKILL_DIST] += intValue;
					}
					if (readXMLInteger(configNode, "fistPercent", intValue)) {
						outfit.skillsPercent[SKILL_FIST] += intValue;
					}
					if (readXMLInteger(configNode, "clubPercent", intValue)) {
						outfit.skillsPercent[SKILL_CLUB] += intValue;
					}
					if (readXMLInteger(configNode, "swordPercent", intValue)) {
						outfit.skillsPercent[SKILL_SWORD] += intValue;
					}
					if (readXMLInteger(configNode, "axePercent", intValue)) {
						outfit.skillsPercent[SKILL_AXE] += intValue;
					}
					if (readXMLInteger(configNode, "distancePercent", intValue) || readXMLInteger(configNode, "distPercent", intValue)) {
						outfit.skillsPercent[SKILL_DIST] += intValue;
					}
					if (readXMLInteger(configNode, "shieldingPercent", intValue) || readXMLInteger(configNode, "shieldPercent", intValue)) {
						outfit.skillsPercent[SKILL_SHIELD] = intValue;
					}
					if (readXMLInteger(configNode, "fishingPercent", intValue) || readXMLInteger(configNode, "fishPercent", intValue)) {
						outfit.skillsPercent[SKILL_FISH] = intValue;
					}
					if (readXMLInteger(configNode, "meleePercent", intValue)) {
						outfit.skillsPercent[SKILL_FIST] += intValue;
						outfit.skillsPercent[SKILL_CLUB] += intValue;
						outfit.skillsPercent[SKILL_SWORD] += intValue;
						outfit.skillsPercent[SKILL_AXE] += intValue;
					}
					if (readXMLInteger(configNode, "weaponPercent", intValue) || readXMLInteger(configNode, "weaponsPercent", intValue)) {
						outfit.skillsPercent[SKILL_CLUB] += intValue;
						outfit.skillsPercent[SKILL_SWORD] += intValue;
						outfit.skillsPercent[SKILL_AXE] += intValue;
						outfit.skillsPercent[SKILL_DIST] += intValue;
					}
				} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("stats"))) {
					if (readXMLInteger(configNode, "maxHealth", intValue)) {
						outfit.stats[STAT_MAXHEALTH] = intValue;
					}
					if (readXMLInteger(configNode, "maxMana", intValue)) {
						outfit.stats[STAT_MAXMANA] = intValue;
					}
					if (readXMLInteger(configNode, "soul", intValue)) {
						outfit.stats[STAT_SOUL] = intValue;
					}
					if (readXMLInteger(configNode, "level", intValue)) {
						outfit.stats[STAT_LEVEL] = intValue;
					}
					if (readXMLInteger(configNode, "magLevel", intValue) || readXMLInteger(configNode, "magicLevel", intValue)) {
						outfit.stats[STAT_MAGICLEVEL] = intValue;
					}
					if (readXMLInteger(configNode, "maxHealthPercent", intValue)) {
						outfit.statsPercent[STAT_MAXHEALTH] = intValue;
					}
					if (readXMLInteger(configNode, "maxManaPercent", intValue)) {
						outfit.statsPercent[STAT_MAXMANA] = intValue;
					}
					if (readXMLInteger(configNode, "soulPercent", intValue)) {
						outfit.statsPercent[STAT_SOUL] = intValue;
					}
					if (readXMLInteger(configNode, "levelPercent", intValue)) {
						outfit.statsPercent[STAT_LEVEL] = intValue;
					}
					if (readXMLInteger(configNode, "magLevelPercent", intValue) || readXMLInteger(configNode, "magicLevelPercent", intValue)) {
						outfit.statsPercent[STAT_MAGICLEVEL] = intValue;
					}
				} else if (!xmlStrcmp(configNode->name, reinterpret_cast<const xmlChar*>("suppress"))) {
					if (readXMLString(configNode, "poison", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_POISON;
					}
					if (readXMLString(configNode, "fire", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_FIRE;
					}
					if (readXMLString(configNode, "energy", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_ENERGY;
					}
					if (readXMLString(configNode, "physical", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_BLEEDING;
					}
					if (readXMLString(configNode, "haste", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_HASTE;
					}
					if (readXMLString(configNode, "paralyze", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_PARALYZE;
					}
					if (readXMLString(configNode, "outfit", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_OUTFIT;
					}
					if (readXMLString(configNode, "invisible", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_INVISIBLE;
					}
					if (readXMLString(configNode, "light", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_LIGHT;
					}
					if (readXMLString(configNode, "manaShield", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_MANASHIELD;
					}
					if (readXMLString(configNode, "infight", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_INFIGHT;
					}
					if (readXMLString(configNode, "drunk", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_DRUNK;
					}
					if (readXMLString(configNode, "exhaust", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_EXHAUST;
					}
					if (readXMLString(configNode, "regeneration", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_REGENERATION;
					}
					if (readXMLString(configNode, "soul", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_SOUL;
					}
					if (readXMLString(configNode, "drown", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_DROWN;
					}
					if (readXMLString(configNode, "muted", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_MUTED;
					}
					if (readXMLString(configNode, "attributes", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_ATTRIBUTES;
					}
					if (readXMLString(configNode, "freezing", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_FREEZING;
					}
					if (readXMLString(configNode, "dazzled", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_DAZZLED;
					}
					if (readXMLString(configNode, "cursed", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_CURSED;
					}
					if (readXMLString(configNode, "pacified", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_PACIFIED;
					}
					if (readXMLString(configNode, "gamemaster", strValue) && booleanString(strValue)) {
						outfit.conditionSuppressions |= CONDITION_GAMEMASTER;
					}
				}
			}

			auto ret = m_outfits.emplace(outfit.lookType, outfit);
			if (!ret.second) {
				std::clog << "[Warning - OutfitsManager::loadFromXml] Duplicate outfit with lookType " << outfit.lookType << ", with id " << outfit.id << std::endl;
				continue;
			}

			for (int32_t gender : intVector) {
				auto& map = m_outfitsByGender[gender];
				if (!map.emplace(outfit.id, &ret.first->second).second) {
					std::clog << "[Warning - OutfitsManager::loadFromXml] Duplicate outfit with id " << outfit.id << ", for gender " << gender << ", with lookType " << outfit.lookType << std::endl;
				}
			}
		}
	}

	xmlFreeDoc(doc);
	return true;
}

const Outfit* OutfitsManager::getOutfitById(uint32_t id, uint8_t sex)
{
	auto it = m_outfitsByGender.find(sex);
	if (it == m_outfitsByGender.end()) {
		return nullptr;
	}

	auto iit = it->second.find(id);
	if (iit == it->second.end()) {
		return nullptr;
	}
	return iit->second;
}

const Outfit* OutfitsManager::getOutfitByLookType(uint16_t lookType)
{
	auto it = m_outfits.find(lookType);
	if (it == m_outfits.end()) {
		return nullptr;
	}
	return &it->second;
}

uint32_t OutfitsManager::getOutfitIdByLookType(uint16_t lookType)
{
	if (const Outfit* outfit = getOutfitByLookType(lookType)) {
		return outfit->id;
	}
	return 0;
}

bool OutfitsManager::addAttributes(Player* player, uint16_t lookType, uint8_t addons)
{
	auto it = m_outfits.find(lookType);
	if (it == m_outfits.end()) {
		return false;
	}

	Outfit& outfit = it->second;
	if (outfit.requirement != addons && (outfit.requirement != REQUIREMENT_ANY || addons == 0)) {
		return false;
	}

	if (outfit.invisible) {
		Condition* condition = Condition::createCondition(CONDITIONID_OUTFIT, CONDITION_INVISIBLE, -1, 0);
		player->addCondition(condition);
	}

	if (outfit.manaShield) {
		Condition* condition = Condition::createCondition(CONDITIONID_OUTFIT, CONDITION_MANASHIELD, -1, 0);
		player->addCondition(condition);
	}

	if (outfit.speed != 0) {
		g_game.changeSpeed(player, outfit.speed);
	}

	if (outfit.conditionSuppressions != 0) {
		player->setConditionSuppressions(outfit.conditionSuppressions, false);
		player->sendIcons();
	}

	if (outfit.regeneration) {
		Condition* condition = Condition::createCondition(CONDITIONID_OUTFIT, CONDITION_REGENERATION, -1, 0);
		if (outfit.healthGain != 0) {
			condition->setParam(CONDITIONPARAM_HEALTHGAIN, outfit.healthGain);
		}

		if (outfit.healthTicks != 0) {
			condition->setParam(CONDITIONPARAM_HEALTHTICKS, outfit.healthTicks);
		}

		if (outfit.manaGain != 0) {
			condition->setParam(CONDITIONPARAM_MANAGAIN, outfit.manaGain);
		}

		if (outfit.manaTicks != 0) {
			condition->setParam(CONDITIONPARAM_MANATICKS, outfit.manaTicks);
		}

		player->addCondition(condition);
	}

	bool needUpdateSkills = false;
	for (uint8_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (outfit.skills[i] != 0) {
			needUpdateSkills = true;
			player->setVarSkill(i, outfit.skills[i]);
		}

		if (outfit.skillsPercent[i] != 0) {
			needUpdateSkills = true;
			player->setVarSkill(i, (player->getSkillLevel(i) * ((outfit.skillsPercent[i] - 100) / 100.f)));
		}
	}

	bool needUpdateStats = false;
	for (uint8_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (outfit.stats[s] != 0) {
			needUpdateStats = true;
			player->setVarStats(static_cast<Stats_t>(s), outfit.stats[s]);
		}

		if (outfit.statsPercent[s] != 0) {
			needUpdateStats = true;
			player->setVarStats(static_cast<Stats_t>(s), (player->getDefaultStats(static_cast<Stats_t>(s)) * ((outfit.statsPercent[s] - 100) / 100.f)));
		}
	}

	if (needUpdateSkills) {
		player->sendSkills();
	}
	if (needUpdateStats) {
		player->sendStats();
	}
	return true;
}

void OutfitsManager::removeAttributes(Player* player, uint16_t lookType)
{
	auto it = m_outfits.find(lookType);
	if (it == m_outfits.end()) {
		return;
	}

	Outfit& outfit = it->second;
	if (outfit.invisible) {
		player->removeCondition(CONDITION_INVISIBLE, CONDITIONID_OUTFIT);
	}

	if (outfit.manaShield) {
		player->removeCondition(CONDITION_MANASHIELD, CONDITIONID_OUTFIT);
	}

	if (outfit.speed != 0) {
		g_game.changeSpeed(player, -outfit.speed);
	}

	if (outfit.conditionSuppressions != 0) {
		player->setConditionSuppressions(outfit.conditionSuppressions, true);
		player->sendIcons();
	}

	if (outfit.regeneration) {
		player->removeCondition(CONDITION_REGENERATION, CONDITIONID_OUTFIT);
	}

	bool needUpdateSkills = false;
	for (uint8_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (outfit.skills[i] != 0) {
			needUpdateSkills = true;
			player->setVarSkill(static_cast<Skills_t>(i), -outfit.skills[i]);
		}

		if (outfit.skillsPercent[i] != 0) {
			needUpdateSkills = true;
			player->setVarSkill(static_cast<Skills_t>(i), -(player->getSkillLevel(i) * ((outfit.skillsPercent[i] - 100) / 100.f)));
		}
	}

	bool needUpdateStats = false;
	for (uint8_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (outfit.stats[s] != 0) {
			needUpdateStats = true;
			player->setVarStats(static_cast<Stats_t>(s), -outfit.stats[s]);
		}

		if (outfit.statsPercent[s] != 0) {
			needUpdateStats = true;
			player->setVarStats(static_cast<Stats_t>(s), -(player->getDefaultStats(static_cast<Stats_t>(s)) * ((outfit.statsPercent[s] - 100) / 100.f)));
		}
	}

	if (needUpdateSkills) {
		player->sendSkills();
	}

	if (needUpdateStats) {
		player->sendStats();
	}
}

int16_t OutfitsManager::getOutfitAbsorb(uint16_t lookType, uint8_t index)
{
	assert(index >= COMBATINDEX_FIRST && index <= COMBATINDEX_LAST);

	auto it = m_outfits.find(lookType);
	if (it == m_outfits.end()) {
		return 0;
	}

	if (it->second.lookType == lookType) {
		return it->second.absorb[index];
	}
	return 0;
}

int16_t OutfitsManager::getOutfitReflect(uint16_t lookType, uint8_t index)
{
	assert(index >= COMBATINDEX_FIRST && index <= COMBATINDEX_LAST);

	auto it = m_outfits.find(lookType);
	if (it == m_outfits.end()) {
		return 0;
	}

	if (it->second.reflect[REFLECT_PERCENT][index] != 0 && it->second.reflect[REFLECT_CHANCE][index] >= random_range(1, 100)) {
		return it->second.reflect[REFLECT_PERCENT][index];
	}
	return 0;
}
