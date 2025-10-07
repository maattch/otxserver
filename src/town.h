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

class Position;
class Town
{
public:
	Town(uint32_t townId) { id = townId; }
	virtual ~Town() {}

	const Position& getPosition() const { return position; }
	const std::string& getName() const { return name; }

	void setPosition(const Position& pos) { position = pos; }
	void setName(const std::string& townName) { name = townName; }

	uint32_t getID() const { return id; }

private:
	uint32_t id;
	std::string name;
	Position position;
};

class Towns
{
public:
	static Towns* getInstance()
	{
		static Towns instance;
		return &instance;
	}

	bool addTown(uint32_t townId, Town* town)
	{
		auto it = townMap.find(townId);
		if (it != townMap.end()) {
			return false;
		}

		townMap[townId] = town;
		return true;
	}

	Town* getTown(const std::string& townName)
	{
		for (const auto& it : townMap) {
			if (caseInsensitiveEqual(it.second->getName(), townName)) {
				return it.second;
			}
		}
		return nullptr;
	}

	Town* getTown(uint32_t townId)
	{
		auto it = townMap.find(townId);
		if (it != townMap.end()) {
			return it->second;
		}

		return nullptr;
	}

	const auto& getTowns() const { return townMap; }

private:
	std::map<uint32_t, Town*> townMap;
};
