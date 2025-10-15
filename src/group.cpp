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

#include "group.h"

#include "configmanager.h"
#include "tools.h"

bool GroupsManager::reload()
{
	m_groups.clear();
	return loadFromXml();
}

bool GroupsManager::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "groups.xml").c_str());
	if (!doc) {
		std::clog << "[Warning - GroupsManager::loadFromXml] Cannot load groups file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("groups"))) {
		std::clog << "[Error - GroupsManager::loadFromXml] Malformed groups file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	int64_t int64Value;
	int32_t intValue;
	for (xmlNodePtr p = root->children; p; p = p->next) {
		if (xmlStrcmp(p->name, reinterpret_cast<const xmlChar*>("group"))) {
			continue;
		}

		if (!readXMLInteger(p, "id", intValue)) {
			std::clog << "[Warning - GroupsManager::loadFromXml] Missing group id." << std::endl;
			continue;
		}

		const auto groupId = static_cast<uint16_t>(intValue);
		if (groupId == 0) {
			std::clog << "[Warning - GroupsManager::loadFromXml] Cannot create group with id 0." << std::endl;
			continue;
		}

		Group group;
		group.id = groupId;

		if (readXMLString(p, "name", strValue)) {
			group.name = strValue;
		}
		if (readXMLInteger64(p, "flags", int64Value)) {
			group.flags = int64Value;
		}
		if (readXMLInteger64(p, "customFlags", int64Value)) {
			group.customFlags = int64Value;
		}
		if (readXMLInteger(p, "access", intValue)) {
			group.access = intValue;
		}
		if (readXMLInteger(p, "ghostAccess", intValue)) {
			group.ghostAccess = intValue;
		} else {
			group.ghostAccess = group.access;
		}

		if (readXMLInteger(p, "violationReasons", intValue)) {
			group.violationReasons = intValue;
		}
		if (readXMLInteger(p, "nameViolationFlags", intValue)) {
			group.nameViolationFlags = intValue;
		}
		if (readXMLInteger(p, "statementViolationFlags", intValue)) {
			group.statementViolationFlags = intValue;
		}
		if (readXMLInteger(p, "depotLimit", intValue)) {
			group.depotLimit = intValue;
		}
		if (readXMLInteger(p, "maxVips", intValue)) {
			group.maxVips = intValue;
		}
		if (readXMLInteger(p, "outfit", intValue) || readXMLInteger(p, "lookType", intValue)) {
			group.lookType = intValue;
		}

		if (!m_groups.emplace(groupId, std::move(group)).second) {
			std::clog << "[Error - GroupsManager::loadFromXml] Duplicate group with id " << groupId << std::endl;
		}
	}

	xmlFreeDoc(doc);
	return true;
}

Group* GroupsManager::getGroup(uint16_t groupId)
{
	auto it = m_groups.find(groupId);
	if (it != m_groups.end()) {
		return &it->second;
	}
	return nullptr;
}

uint32_t Group::getDepotLimit(bool premium) const
{
	if (depotLimit != 0) {
		return depotLimit;
	}
	return otx::config::getInteger(premium ? otx::config::DEFAULT_DEPOT_SIZE_PREMIUM
										   : otx::config::DEFAULT_DEPOT_SIZE);
}

uint32_t Group::getMaxVips(bool premium) const
{
	if (maxVips != 0) {
		return maxVips;
	}
	return otx::config::getInteger(premium ? otx::config::VIPLIST_DEFAULT_PREMIUM_LIMIT
										   : otx::config::VIPLIST_DEFAULT_LIMIT);
}
