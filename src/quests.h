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

typedef std::map<uint32_t, std::string> StateMap;
class Mission
{
public:
	Mission(std::string _name, std::string _state, std::string _storageId, int32_t _startValue, int32_t _endValue, bool _notify)
	{
		m_name = _name;
		m_state = _state;
		m_startValue = _startValue;
		m_endValue = _endValue;
		m_storageId = _storageId;
		m_notify = _notify;
	}
	virtual ~Mission() { m_states.clear(); }

	void newState(uint32_t id, const std::string& description) { m_states[id] = description; }

	bool isStarted(Player* player);
	bool isCompleted(Player* player);
	bool isNotifying() const { return m_notify; }

	std::string getName(Player* player) { return (isCompleted(player) ? (m_name + " (completed)") : m_name); }
	std::string getDescription(Player* player);

	int32_t getStartValue() { return m_startValue; }
	int32_t getEndValue() { return m_endValue; }

	const std::string& getStorageId() const { return m_storageId; }

private:
	std::string parseStorages(std::string state, std::string value, Player* player);

	std::string m_name, m_state;
	StateMap m_states;

	bool m_notify;
	int32_t m_startValue, m_endValue;
	std::string m_storageId;
};

typedef std::list<Mission*> MissionList;
class Quest
{
public:
	Quest(std::string _name, uint16_t _id, std::string _storageId, int32_t _storageValue)
	{
		m_name = _name;
		m_id = _id;
		m_storageValue = _storageValue;
		m_storageId = _storageId;
	}
	virtual ~Quest();

	void newMission(Mission* mission) { m_missions.push_back(mission); }

	bool isStarted(Player* player);
	bool isCompleted(Player* player) const;

	uint16_t getId() const { return m_id; }
	const std::string& getName() const { return m_name; }
	const std::string& getStorageId() const { return m_storageId; }
	int32_t getStorageValue() const { return m_storageValue; }
	uint16_t getMissionCount(Player* player);

	inline MissionList::const_iterator getFirstMission() const { return m_missions.begin(); }
	inline MissionList::const_iterator getLastMission() const { return m_missions.end(); }

private:
	std::string m_name;
	MissionList m_missions;

	uint16_t m_id;
	int32_t m_storageValue;
	std::string m_storageId;
};

typedef std::list<Quest*> QuestList;
class Quests
{
public:
	virtual ~Quests() { clear(); }
	static Quests* getInstance()
	{
		static Quests instance;
		return &instance;
	}

	void clear();
	bool reload();

	bool loadFromXml();
	bool parseQuestNode(xmlNodePtr p, bool checkDuplicate);

	bool isQuestStorage(const std::string& key, const std::string& value, bool notification) const;
	uint16_t getQuestCount(Player* player);

	inline QuestList::const_iterator getFirstQuest() const { return m_quests.begin(); }
	inline QuestList::const_iterator getLastQuest() const { return m_quests.end(); }

	Quest* getQuestById(uint16_t id) const;

private:
	Quests() { m_lastId = 1; }

	QuestList m_quests;
	uint16_t m_lastId;
};
