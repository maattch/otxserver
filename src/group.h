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

struct Group
{
	uint32_t getDepotLimit(bool premium = false) const;
	uint32_t getMaxVips(bool premium = false) const;

	constexpr bool hasFlag(uint64_t value) const noexcept { return (flags & (static_cast<uint64_t>(1) << value)); }
	constexpr bool hasCustomFlag(uint64_t value) const noexcept { return (customFlags & (static_cast<uint64_t>(1) << value)); }

	std::string name;
	uint64_t flags = 0;
	uint64_t customFlags = 0;
	uint32_t depotLimit = 0;
	uint32_t maxVips = 0;
	uint16_t id = 0;
	uint16_t access = 0;
	uint16_t ghostAccess = 0;
	uint16_t lookType = 0;
	uint16_t statementViolationFlags = 0;
	uint16_t nameViolationFlags = 0;
	uint8_t violationReasons = 0;
};

class GroupsManager final
{
public:
	GroupsManager() = default;

	// non-copyable
	GroupsManager(const GroupsManager&) = delete;
	GroupsManager& operator=(const GroupsManager&) = delete;

	bool loadFromXml();
	bool reload();

	Group* getGroup(uint16_t groupId);

	const auto& getGroups() const { return m_groups; }

private:
	std::map<uint16_t, Group> m_groups;
};
