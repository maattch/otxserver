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

#define CLIENT_VERSION_MIN 860
#define CLIENT_VERSION_MAX 860
#define CLIENT_VERSION_ITEMS 20
#define CLIENT_VERSION_STRING "8.60"

#define SOFTWARE_NAME "OTXServer Fe Edition (modified by Wtver)"
#define SOFTWARE_VERSION "1"
#define MINOR_VERSION "15"
#define SOFTWARE_DEVELOPERS "https://github.com/FeTads/otxserver"

#define MAX_RAND_RANGE 10000000

#ifndef __FUNCTION__
	#define __FUNCTION__ __func__
#endif

#define BOOST_ASIO_ENABLE_CANCELIO 1
#define BOOST_BIND_NO_PLACEHOLDERS
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _MSC_VER
	#define __PRETTY_FUNCTION__ __FUNCDNAME__
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#ifdef NDEBUG
		#define _SECURE_SCL 0
		#define HAS_ITERATOR_DEBUGGING 0
	#endif

	#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
	#define VC_EXTRALEAN

	#include <cstring>
	#define atoll _atoi64

	#define strncasecmp _strnicmp
	#define strcasecmp _stricmp

	// enable warnings
	#pragma warning(default : 4800) // implicit conversion from 'type' to bool. Possible information loss
	#pragma warning(default : 5038) // data member 'member1' will be initialized after data member 'member2'

	// disable warnings
	#pragma warning(disable : 4250) // 'class1' : inherits 'class2::member' via dominance
	#pragma warning(disable : 4706) // assignment within conditional expression
	#pragma warning(disable : 4267) // 'return' : conversion from 'X' to 'Y', possible loss of data
	#pragma warning(disable : 4244) // 'return' : conversion from 'X' to 'Y', possible loss of data
	#pragma warning(disable : 26812) // The enum type is unscoped. Prefer 'enum class' over 'enum'
	#pragma warning(disable : 26813) // Use 'bitwise and' to check if flag is set
#endif

#ifdef _WIN32
	#ifdef _WIN32_WINNT
		#undef _WIN32_WINNT
	#endif

	// Windows 2000	 0x0500
	// Windows XP	 0x0501
	// Windows 2003	 0x0502
	// Windows Vista 0x0600
	// Windows Seven 0x0601
	#define _WIN32_WINNT 0x0601
#endif
