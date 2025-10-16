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

#include "quests.h"

#include "otx/cast.hpp"
#include "otx/util.hpp"

bool Mission::isStarted(Player* player)
{
	if (!player) {
		return false;
	}

	if (const std::string* value = player->getStorage(m_storageId)) {
		return otx::util::cast<int32_t>(value->data()) >= m_startValue;
	}
	return false;
}

bool Mission::isCompleted(Player* player)
{
	if (!player) {
		return false;
	}

	if (const std::string* value = player->getStorage(m_storageId)) {
		return otx::util::cast<int32_t>(value->data()) >= m_endValue;
	}
	return false;
}

std::string Mission::parseStorages(std::string state, std::string value, Player* player)
{
	std::string::size_type start = 0, end = 0, nStart = 0, length = 0;
	while ((start = state.find("|STORAGE:", end)) != std::string::npos) {
		nStart = start + 9;
		if ((end = state.find("|", nStart)) == std::string::npos) {
			break;
		}

		length = end - nStart;

		const std::string* svalue = player->getStorage(state.substr(nStart, length));
		state.replace(start, (end - start + 1), (svalue ? *svalue : std::string()));
	}

	otx::util::replace_all(state, "|STATE|", value);
	otx::util::replace_all(state, "\\n", "\n");
	return state;
}

std::string Mission::getDescription(Player* player)
{
	if (!player) {
		return "Couldn't retrieve any mission description, please report to a gamemaster.";
	}

	std::string value;
	if (const std::string* ret = player->getStorage(m_storageId)) {
		value = *ret;
	}

	if (!m_states.empty()) {
		int32_t cmp = atoi(value.c_str());
		if (cmp >= m_endValue) {
			return parseStorages(m_states.rbegin()->second, value, player);
		}

		for (int32_t i = (m_endValue - 1); i >= m_startValue; --i) {
			if (cmp != i) {
				continue;
			}

			std::string tmp = m_states[i];
			if (!tmp.empty()) {
				return parseStorages(tmp, value, player);
			}
		}
	}

	if (!m_state.empty()) {
		return parseStorages(m_state, value, player);
	}
	return "Couldn't retrieve any mission description, please report to a gamemaster.";
}

Quest::~Quest()
{
	for (MissionList::iterator it = m_missions.begin(); it != m_missions.end(); ++it) {
		delete (*it);
	}

	m_missions.clear();
}

bool Quest::isStarted(Player* player)
{
	if (!player) {
		return false;
	}

	for (MissionList::const_iterator it = m_missions.begin(); it != m_missions.end(); ++it) {
		if ((*it)->isStarted(player)) {
			return true;
		}
	}

	if (const std::string* value = player->getStorage(m_storageId)) {
		return otx::util::cast<int32_t>(value->data()) >= m_storageValue;
	}
	return false;
}

bool Quest::isCompleted(Player* player) const
{
	if (!player) {
		return false;
	}

	for (MissionList::const_iterator it = m_missions.begin(); it != m_missions.end(); ++it) {
		if (!(*it)->isCompleted(player)) {
			return false;
		}
	}

	return true;
}

uint16_t Quest::getMissionCount(Player* player)
{
	if (!player) {
		return 0;
	}

	uint16_t count = 0;
	for (MissionList::iterator it = m_missions.begin(); it != m_missions.end(); ++it) {
		if ((*it)->isStarted(player)) {
			count++;
		}
	}

	return count;
}

void Quests::clear()
{
	for (QuestList::iterator it = m_quests.begin(); it != m_quests.end(); ++it) {
		delete (*it);
	}

	m_quests.clear();
}

bool Quests::reload()
{
	clear();
	return loadFromXml();
}

bool Quests::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "quests.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - Quests::loadFromXml] Cannot load quests file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("quests"))) {
		std::clog << "[Error - Quests::loadFromXml] Malformed quests file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	for (xmlNodePtr p = root->children; p; p = p->next) {
		parseQuestNode(p, false);
	}

	xmlFreeDoc(doc);
	return true;
}

bool Quests::parseQuestNode(xmlNodePtr p, bool checkDuplicate)
{
	if (xmlStrcmp(p->name, reinterpret_cast<const xmlChar*>("quest"))) {
		return false;
	}

	int32_t intValue;
	std::string strValue;

	uint32_t id = m_lastId;
	if (readXMLInteger(p, "id", intValue) && id > 0) {
		id = intValue;
		if (id > m_lastId) {
			m_lastId = id;
		}
	}

	std::string name;
	if (readXMLString(p, "name", strValue)) {
		name = strValue;
	}

	std::string startStorageId;
	if (readXMLString(p, "startstorageid", strValue) || readXMLString(p, "storageId", strValue)) {
		startStorageId = strValue;
	}

	int32_t startStorageValue = 0;
	if (readXMLInteger(p, "startstoragevalue", intValue) || readXMLInteger(p, "storageValue", intValue)) {
		startStorageValue = intValue;
	}

	Quest* quest = new Quest(name, id, startStorageId, startStorageValue);
	if (!quest) {
		return false;
	}

	for (xmlNodePtr missionNode = p->children; missionNode; missionNode = missionNode->next) {
		if (xmlStrcmp(missionNode->name, reinterpret_cast<const xmlChar*>("mission"))) {
			continue;
		}

		std::string missionName, missionState, storageId;
		if (readXMLString(missionNode, "name", strValue)) {
			missionName = strValue;
		}

		if (readXMLString(missionNode, "state", strValue) || readXMLString(missionNode, "description", strValue)) {
			missionState = strValue;
		}

		if (readXMLString(missionNode, "storageid", strValue) || readXMLString(missionNode, "storageId", strValue)) {
			storageId = strValue;
		}

		int32_t startValue = 0, endValue = 0;
		if (readXMLInteger(missionNode, "startvalue", intValue) || readXMLInteger(missionNode, "startValue", intValue)) {
			startValue = intValue;
		}

		if (readXMLInteger(missionNode, "endvalue", intValue) || readXMLInteger(missionNode, "endValue", intValue)) {
			endValue = intValue;
		}

		bool notify = true;
		if (readXMLString(missionNode, "notify", strValue)) {
			notify = booleanString(strValue);
		}

		if (Mission* mission = new Mission(missionName, missionState, storageId, startValue, endValue, notify)) {
			// parse sub-states only if main is not set
			for (xmlNodePtr stateNode = missionNode->children; stateNode; stateNode = stateNode->next) {
				if (xmlStrcmp(stateNode->name, reinterpret_cast<const xmlChar*>("missionstate"))) {
					continue;
				}

				if (!readXMLString(stateNode, "id", strValue)) {
					std::clog << "[Warning - Quests::parseQuestNode] Missing missionId for mission state" << std::endl;
					continue;
				}

				std::string description, strDesc;
				if (readXMLString(stateNode, "description", strDesc)) {
					description = strDesc;
				}

				StringVec strVector = explodeString(strValue, "-");
				if (strVector.size() > 1) {
					IntegerVec intVector = vectorAtoi(strVector);
					if (intVector[0] && intVector[1]) {
						for (int32_t i = intVector[0]; i <= intVector[1]; i++) {
							mission->newState(i, description);
						}
					} else {
						std::clog << "Invalid mission state id '" << strValue << "' for mission '" << mission->getName(nullptr) << "'" << std::endl;
					}

					continue;
				} else {
					mission->newState(atoi(strValue.c_str()), description);
				}
			}

			quest->newMission(mission);
		}
	}

	if (checkDuplicate) {
		for (QuestList::iterator it = m_quests.begin(); it != m_quests.end(); ++it) {
			if ((*it)->getName() == name) {
				delete *it;
			}
		}
	}

	m_lastId++;
	m_quests.push_back(quest);
	return true;
}

bool Quests::isQuestStorage(const std::string& key, const std::string& value, bool notification) const
{
	int32_t _value = atoi(value.c_str());
	for (QuestList::const_iterator it = m_quests.begin(); it != m_quests.end(); ++it) {
		if (value.empty()) {
			if ((*it)->getStorageId() == key) {
				return true;
			}
		} else if ((*it)->getStorageId() == key && (_value && _value == (*it)->getStorageValue())) {
			return true;
		}

		for (MissionList::const_iterator mit = (*it)->getFirstMission(); mit != (*it)->getLastMission(); ++mit) {
			if (notification && !(*mit)->isNotifying()) {
				continue;
			}

			if (value.empty()) {
				if ((*mit)->getStorageId() == key) {
					return true;
				}
			} else if ((*mit)->getStorageId() == key && (_value && _value >= (*mit)->getStartValue() && _value <= (*mit)->getEndValue())) {
				return true;
			}
		}
	}

	return false;
}

uint16_t Quests::getQuestCount(Player* player)
{
	if (!player) {
		return 0;
	}

	uint16_t count = 0;
	for (QuestList::iterator it = m_quests.begin(); it != m_quests.end(); ++it) {
		if ((*it)->isStarted(player)) {
			count++;
		}
	}

	return count;
}

Quest* Quests::getQuestById(uint16_t id) const
{
	for (QuestList::const_iterator it = m_quests.begin(); it != m_quests.end(); ++it) {
		if ((*it)->getId() == id) {
			return (*it);
		}
	}

	return nullptr;
}
