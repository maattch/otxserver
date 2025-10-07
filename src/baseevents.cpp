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

#include "baseevents.h"

#include "configmanager.h"
#include "tools.h"

#include "otx/util.hpp"

bool BaseEvents::loadFromXml()
{
	const std::string scriptsName = getScriptBaseName();
	if (m_loaded) {
		std::clog << "[Error - BaseEvents::loadFromXml] " << scriptsName << " interface already loaded!" << std::endl;
		return false;
	}

	std::string path = getFilePath(FILE_TYPE_OTHER, scriptsName + "/lib/");
	if (!getInterface()->loadDirectory(path, false, true)) {
		std::clog << "[Warning - BaseEvents::loadFromXml] Cannot load " << path << std::endl;
	}

	path = getFilePath(FILE_TYPE_OTHER, scriptsName + "/" + scriptsName + ".xml");
	xmlDocPtr doc = xmlParseFile(path.c_str());
	if (!doc) {
		std::clog << "[Warning - BaseEvents::loadFromXml] Cannot open " << path << " file." << std::endl;
		std::clog << getLastXMLError() << std::endl;
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (xmlStrcmp(root->name, (const xmlChar*)scriptsName.c_str())) {
		std::clog << "[Error - BaseEvents::loadFromXml] Malformed " << path << " file." << std::endl;
		xmlFreeDoc(doc);
		return false;
	}

	path = getFilePath(FILE_TYPE_OTHER, scriptsName + "/scripts/");
	for (xmlNodePtr p = root->children; p; p = p->next) {
		parseEventNode(p, path);
	}

	xmlFreeDoc(doc);
	m_loaded = true;
	return m_loaded;
}

bool BaseEvents::parseEventNode(xmlNodePtr p, const std::string& scriptsPath)
{
	Event* event = getEvent((const char*)p->name);
	if (!event) {
		return false;
	}

	if (!event->configureEvent(p)) {
		std::clog << "[Warning - BaseEvents::loadFromXml] Cannot configure an event" << std::endl;
		delete event;
		return false;
	}

	bool success = false;
	std::string strValue;
	if (readXMLString(p, "event", strValue)) {
		const std::string tmpStrValue = otx::util::as_lower_string(strValue);
		if (tmpStrValue == "script") {
			if (readXMLString(p, "value", strValue)) {
				success = event->loadScript(scriptsPath + strValue);
			}
		} else if (tmpStrValue == "function") {
			if (readXMLString(p, "value", strValue)) {
				success = event->loadFunction(strValue);
			}
		}
	} else if (readXMLString(p, "script", strValue)) {
		success = event->loadScript(scriptsPath + strValue);
	} else if (readXMLString(p, "function", strValue)) {
		success = event->loadFunction(strValue);
	}

	if (!success) {
		delete event;
		return false;
	}

	if (registerEvent(event, p)) {
		return true;
	}

	if (!event) {
		return false;
	}

	delete event;
	return false;
}

bool BaseEvents::reload()
{
	m_loaded = false;
	clear();
	return loadFromXml();
}

bool Event::loadScript(const std::string& script)
{
	if (!m_interface) {
		std::clog << "[Error - Event::loadScript] m_interface = nullptr" << std::endl;
		return false;
	}

	if (m_scriptId != 0) {
		std::clog << "[Error - Event::loadScript] scriptId = " << m_scriptId << std::endl;
		return false;
	}

	if (!m_interface->loadFile(script)) {
		std::clog << "[Warning - Event::loadScript] Cannot load script (" << script << ')' << std::endl << m_interface->getLastError() << std::endl;
		return false;
	}

	const int id = m_interface->getEvent(getScriptEventName());
	if (id == -1) {
		std::clog << "[Warning - Event::loadScript] Event " << getScriptEventName() << " not found (" << script << ')' << std::endl;
		return false;
	}

	m_scripted = true;
	m_scriptId = id;
	return true;
}

bool CallBack::loadCallBack(LuaInterface* scriptInterface, const std::string& name)
{
	if (!scriptInterface) {
		std::clog << "[Error - CallBack::loadCallBack] scriptInterface = nullptr" << std::endl;
		return false;
	}

	m_interface = scriptInterface;

	const int id = m_interface->getEvent(name);
	if (id == -1) {
		std::clog << "[Warning - CallBack::loadCallBack] Event " << name << " not found." << std::endl;
		return false;
	}

	m_callbackName = name;
	m_scriptId = id;
	m_loaded = true;
	return true;
}
