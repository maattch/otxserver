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

#include "baseevents.h"

class TalkAction;

enum TalkActionFilter : uint8_t
{
	TALKFILTER_QUOTATION,
	TALKFILTER_WORD,
	TALKFILTER_WORD_SPACED,
	TALKFILTER_LAST
};

class TalkActions final : public BaseEvents
{
public:
	TalkActions() = default;

	// non-copyable
	TalkActions(const TalkActions&) = delete;
	TalkActions& operator=(const TalkActions&) = delete;

	void init();
	void terminate();

	bool onPlayerSay(Creature* creature, uint16_t channelId, const std::string& words, bool ignoreAccess);

	const auto& getTalkActions() const { return talkactions; }

private:
	std::string getScriptBaseName() const override { return "talkactions"; }
	void clear() override;

	Event* getEvent(const std::string& nodeName) override;
	bool registerEvent(Event* event, xmlNodePtr p) override;

	LuaInterface* getInterface() override { return m_interface.get(); }

	LuaInterfacePtr m_interface;

	std::map<std::string, TalkAction> talkactions;
	std::unique_ptr<TalkAction> defaultTalkAction;
};

extern TalkActions g_talkActions;

typedef bool(TalkFunction)(Creature* creature, const std::string& words, const std::string& param);
class TalkAction : public Event
{
public:
	TalkAction(LuaInterface* _interface);
	virtual ~TalkAction() {}

	virtual bool configureEvent(xmlNodePtr p);
	virtual bool loadFunction(const std::string& functionName);

	int32_t executeSay(Creature* creature, const std::string& words, std::string param, uint16_t channel);

	std::string getFunctionName() const { return m_functionName; }
	std::string getWords() const { return m_words; }
	void setWords(const std::string& words) { m_words = words; }

	TalkActionFilter getFilter() const { return m_filter; }
	uint32_t getAccess() const { return m_access; }
	int32_t getChannel() const { return m_channel; }

	const auto& getExceptions() { return m_exceptions; }
	TalkFunction* getFunction() { return m_function; }

	bool isLogged() const { return m_logged; }
	bool isHidden() const { return m_hidden; }
	bool isSensitive() const { return m_sensitive; }

	bool hasGroups() const { return !m_groups.empty(); }
	bool hasGroup(int32_t value) const { return std::find(m_groups.begin(), m_groups.end(), value) != m_groups.end(); }

	const auto& getGroups() const { return m_groups; }

protected:
	virtual std::string getScriptEventName() const { return "onSay"; }

	static TalkFunction houseBuy;
	static TalkFunction houseSell;
	static TalkFunction houseKick;
	static TalkFunction houseDoorList;
	static TalkFunction houseGuestList;
	static TalkFunction houseSubOwnerList;
	static TalkFunction guildJoin;
	static TalkFunction guildCreate;
	static TalkFunction thingProporties;
	static TalkFunction banishmentInfo;
	static TalkFunction diagnostics;
	static TalkFunction ghost;
	static TalkFunction software;

	std::string m_words, m_functionName;
	TalkFunction* m_function;
	TalkActionFilter m_filter;
	uint32_t m_access;
	int32_t m_channel;
	bool m_logged, m_hidden, m_sensitive;
	std::vector<std::string> m_exceptions;
	std::vector<int32_t> m_groups;
};
