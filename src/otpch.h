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

// definitions must be called first
#include "definitions.h"

#if defined WINDOWS
	#include <winerror.h>
#endif

// STD
#include <algorithm>
#include <bitset>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <vector>

// LibXML2
#include <libxml/parser.h>
#include <libxml/threads.h>
#include <libxml/xmlmemory.h>

// Boost
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/config.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION < 104400
	#define BOOST_DIR_ITER_FILENAME(iterator) (iterator)->path().filename()
#else
	#define BOOST_DIR_ITER_FILENAME(iterator) (iterator)->path().filename().string()
#endif

// OTX
#include "configmanager.h"
#include "thing.h"
