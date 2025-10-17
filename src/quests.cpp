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

namespace
{
	std::string parseStorages(std::string state, const std::string& value, Player* player)
	{
		std::string::size_type start = 0, end = 0, nStart = 0, length = 0;
		while ((start = state.find("|STORAGE:", end)) != std::string::npos) {
			nStart = start + 9;
			if ((end = state.find('|', nStart)) == std::string::npos) {
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

} // namespace

bool Mission::isStarted(Player* player) const
{
	if (!player) {
		return false;
	}

	if (const std::string* value = player->getStorage(m_storageId)) {
		return otx::util::cast<int32_t>(value->data()) >= m_startValue;
	}
	return false;
}

bool Mission::isCompleted(Player* player) const
{
	if (const std::string* value = player->getStorage(m_storageId)) {
		return otx::util::cast<int32_t>(value->data()) >= m_endValue;
	}
	return false;
}

std::string Mission::getDescription(Player* player) const
{
	std::string value;
	if (const std::string* ret = player->getStorage(m_storageId)) {
		value = *ret;
	}

	if (!m_states.empty()) {
		const auto value_num = otx::util::cast<int32_t>(value.data());
		if (value_num >= m_endValue) {
			return parseStorages(m_states.rbegin()->second, value, player);
		}

		for (int32_t i = (m_endValue - 1); i >= m_startValue; --i) {
			if (value_num != i) {
				continue;
			}

			auto it = m_states.find(i);
			if (it != m_states.end() && !it->second.empty()) {
				return parseStorages(it->second, value, player);
			}
		}
	}

	if (!m_state.empty()) {
		return parseStorages(m_state, value, player);
	}
	return "Couldn't retrieve any mission description, please report to a gamemaster.";
}

bool Quest::isStarted(Player* player) const
{
	for (const auto& mission : m_missions) {
		if (mission.isStarted(player)) {
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
	for (const auto& mission : m_missions) {
		if (!mission.isCompleted(player)) {
			return false;
		}
	}
	return true;
}

uint16_t Quest::getMissionCount(Player* player) const
{
	uint16_t count = 0;
	for (const auto& mission : m_missions) {
		if (mission.isStarted(player)) {
			++count;
		}
	}
	return count;
}

bool QuestsManager::reload()
{
	m_quests.clear();
	return loadFromXml();
}

bool QuestsManager::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "quests.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - QuestsManager::loadFromXml] Cannot load quests file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("quests"))) {
		std::clog << "[Error - QuestsManager::loadFromXml] Malformed quests file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	int32_t intValue;
	for (xmlNodePtr p = root->children; p; p = p->next) {
		if (xmlStrcmp(p->name, reinterpret_cast<const xmlChar*>("quest"))) {
			continue;
		}

		uint16_t id;
		if (readXMLInteger(p, "id", intValue)) {
			id = intValue;
			if (id > m_lastId) {
				m_lastId = id;
			}
		} else {
			id = ++m_lastId;
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

		auto quest = Quest(name, id, startStorageId, startStorageValue);

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

			auto mission = Mission(missionName, missionState, storageId, startValue, endValue, notify);

			// parse sub-states only if main is not set
			for (xmlNodePtr stateNode = missionNode->children; stateNode; stateNode = stateNode->next) {
				if (xmlStrcmp(stateNode->name, reinterpret_cast<const xmlChar*>("missionstate"))) {
					continue;
				}

				if (!readXMLString(stateNode, "id", strValue)) {
					std::clog << "[Warning - QuestsManager::loadFromXml] Missing missionId for mission state" << std::endl;
					continue;
				}

				const auto strVector = explodeString(strValue, "-");

				std::string description;
				if (readXMLString(stateNode, "description", strValue)) {
					description = strValue;
				}

				if (strVector.size() > 1) {
					const auto intVector = vectorAtoi(strVector);
					if (intVector.size() > 1) {
						for (int32_t i = intVector[0]; i <= intVector[1]; ++i) {
							mission.addState(i, description);
						}
					} else {
						std::clog << "Invalid mission state id '" << strValue << "' for mission '" << mission.getName(nullptr) << "'" << std::endl;
					}

					continue;
				} else {
					mission.addState(otx::util::cast<uint32_t>(strValue.data()), description);
				}
			}

			quest.addMission(std::move(mission));
		}

		m_quests.push_back(std::move(quest));
	}

	xmlFreeDoc(doc);
	return true;
}

bool QuestsManager::isQuestStorage(const std::string& key, const std::string& value, bool notification) const
{
	const auto value_num = otx::util::cast<int32_t>(value.data());
	for (const Quest& quest : m_quests) {
		if (quest.getStorageId() == key) {
			if (value.empty() || (value_num != 0 && value_num == quest.getStorageValue())) {
				return true;
			}
		}

		for (const Mission& mission : quest.getMissions()) {
			if (notification && !mission.isNotifying()) {
				continue;
			}

			if (quest.getStorageId() == key) {
				if (value.empty() || (value_num != 0 && value_num >= mission.getStartValue() && value_num <= mission.getEndValue())) {
					return true;
				}
			}
		}
	}
	return false;
}

std::vector<const Quest*> QuestsManager::getStartedQuests(Player* player)
{
	std::vector<const Quest*> quests;
	for (const Quest& quest : m_quests) {
		if (quest.isStarted(player)) {
			quests.push_back(&quest);
		}
	}
	return quests;
}

const Quest* QuestsManager::getQuestById(uint16_t id) const
{
	for (const Quest& quest : m_quests) {
		if (quest.getId() == id) {
			return &quest;
		}
	}
	return nullptr;
}
