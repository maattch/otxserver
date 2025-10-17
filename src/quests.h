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

#include "player.h"

class Mission final
{
public:
	Mission(const std::string& name, const std::string& state,
			const std::string& storageId, int32_t startValue, int32_t endValue, bool notify) :
		m_name(name),
		m_state(state),
		m_storageId(storageId),
		m_startValue(startValue),
		m_endValue(endValue),
		m_notify(notify) {}

	void addState(uint32_t id, const std::string& description) { m_states[id] = description; }

	bool isStarted(Player* player) const;
	bool isCompleted(Player* player) const;
	bool isNotifying() const { return m_notify; }

	std::string getName(Player* player) const { return (isCompleted(player) ? (m_name + " (completed)") : m_name); }
	std::string getDescription(Player* player) const;

	int32_t getStartValue() const { return m_startValue; }
	int32_t getEndValue() const { return m_endValue; }

	const std::string& getStorageId() const { return m_storageId; }

private:
	std::string m_name;
	std::string m_state;
	std::string m_storageId;
	std::map<uint32_t, std::string> m_states;
	int32_t m_startValue;
	int32_t m_endValue;
	bool m_notify;
};

class Quest final
{
public:
	Quest(const std::string& name, uint16_t id, const std::string& storageId, int32_t storageValue) :
		m_name(name),
		m_storageId(storageId),
		m_storageValue(storageValue),
		m_id(id) {}

	void addMission(Mission&& mission) { m_missions.push_back(std::move(mission)); }

	bool isStarted(Player* player) const;
	bool isCompleted(Player* player) const;

	uint16_t getId() const { return m_id; }
	const std::string& getName() const { return m_name; }
	const std::string& getStorageId() const { return m_storageId; }
	int32_t getStorageValue() const { return m_storageValue; }
	uint16_t getMissionCount(Player* player) const;

	const auto& getMissions() const { return m_missions; }

private:
	std::string m_name;
	std::string m_storageId;
	std::vector<Mission> m_missions;
	int32_t m_storageValue;
	uint16_t m_id;
};

class QuestsManager final
{
public:
	QuestsManager() = default;

	bool loadFromXml();
	bool reload();

	const Quest* getQuestById(uint16_t id) const;

	bool isQuestStorage(const std::string& key, const std::string& value, bool notification) const;
	std::vector<const Quest*> getStartedQuests(Player* player);

	const auto& getQuests() const noexcept { return m_quests; }

private:
	std::vector<Quest> m_quests;
	uint16_t m_lastId = 0;
};
