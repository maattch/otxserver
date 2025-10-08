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

using TalkFuncPtr = bool(*)(Creature* creature, const std::string& param);

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

	EventPtr getEvent(const std::string& nodeName) override;
	void registerEvent(EventPtr event, xmlNodePtr p) override;

	LuaInterface* getInterface() override { return m_interface.get(); }

	std::map<std::string, TalkAction> talkactions;
	LuaInterfacePtr m_interface;
	std::unique_ptr<TalkAction> defaultTalkAction;
};

extern TalkActions g_talkActions;

class TalkAction : public Event
{
public:
	TalkAction(LuaInterface* luaInterface) : Event(luaInterface) {}
	virtual ~TalkAction() = default;

	bool configureEvent(xmlNodePtr p) override;
	bool loadFunction(const std::string& functionName) override;

	bool executeSay(Creature* creature, const std::string& words, const std::string& param, uint16_t channel);

	const std::string& getFunctionName() const { return m_functionName; }
	const std::string& getWords() const { return m_words; }
	void setWords(const std::string& words) { m_words = words; }

	TalkActionFilter getFilter() const { return m_filter; }
	uint32_t getAccess() const { return m_access; }
	int32_t getChannel() const { return m_channel; }

	const auto& getExceptions() { return m_exceptions; }
	TalkFuncPtr getFunction() const { return m_function; }

	bool isLogged() const { return m_logged; }
	bool isHidden() const { return m_hidden; }
	bool isSensitive() const { return m_sensitive; }

	bool hasGroups() const { return !m_groups.empty(); }
	bool hasGroup(int32_t value) const { return std::find(m_groups.begin(), m_groups.end(), value) != m_groups.end(); }

	const auto& getGroups() const { return m_groups; }

private:
	std::string getScriptEventName() const override { return "onSay"; }

	static bool houseBuy(Creature* creature, const std::string& param);
	static bool houseSell(Creature* creature, const std::string& param);
	static bool houseKick(Creature* creature, const std::string& param);
	static bool houseDoorList(Creature* creature, const std::string& param);
	static bool houseGuestList(Creature* creature, const std::string& param);
	static bool houseSubOwnerList(Creature* creature, const std::string& param);
	static bool guildJoin(Creature* creature, const std::string& param);
	static bool guildCreate(Creature* creature, const std::string& param);
	static bool thingProporties(Creature* creature, const std::string& param);
	static bool banishmentInfo(Creature* creature, const std::string& param);
	static bool diagnostics(Creature* creature, const std::string& param);
	static bool ghost(Creature* creature, const std::string& param);
	static bool software(Creature* creature, const std::string& param);

	std::string m_words;
	std::string m_functionName;
	std::vector<int32_t> m_groups;
	std::vector<std::string> m_exceptions;
	TalkFuncPtr m_function = nullptr;
	uint32_t m_access = 0;
	int32_t m_channel = -1;
	TalkActionFilter m_filter = TALKFILTER_WORD;
	bool m_logged = false;
	bool m_hidden = false;
	bool m_sensitive = true;
};
