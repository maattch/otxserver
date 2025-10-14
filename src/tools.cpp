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

#include "tools.h"

#include "configmanager.h"
#include "otx/cast.hpp"
#include "otx/util.hpp"
#include "vocation.h"

#include <iomanip>

namespace
{

	uint32_t circularShift(int bits, uint32_t value)
	{
		return (value << bits) | (value >> (32 - bits));
	}

	void processSHA1MessageBlock(const uint8_t* messageBlock, uint32_t* H)
	{
		uint32_t W[80];
		for (int i = 0; i < 16; ++i) {
			const size_t offset = i << 2;
			W[i] = messageBlock[offset] << 24 | messageBlock[offset + 1] << 16 | messageBlock[offset + 2] << 8 | messageBlock[offset + 3];
		}

		for (int i = 16; i < 80; ++i) {
			W[i] = circularShift(1, W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16]);
		}

		uint32_t A = H[0], B = H[1], C = H[2], D = H[3], E = H[4];

		for (int i = 0; i < 20; ++i) {
			const uint32_t tmp = circularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[i] + 0x5A827999;
			E = D;
			D = C;
			C = circularShift(30, B);
			B = A;
			A = tmp;
		}

		for (int i = 20; i < 40; ++i) {
			const uint32_t tmp = circularShift(5, A) + (B ^ C ^ D) + E + W[i] + 0x6ED9EBA1;
			E = D;
			D = C;
			C = circularShift(30, B);
			B = A;
			A = tmp;
		}

		for (int i = 40; i < 60; ++i) {
			const uint32_t tmp = circularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[i] + 0x8F1BBCDC;
			E = D;
			D = C;
			C = circularShift(30, B);
			B = A;
			A = tmp;
		}

		for (int i = 60; i < 80; ++i) {
			const uint32_t tmp = circularShift(5, A) + (B ^ C ^ D) + E + W[i] + 0xCA62C1D6;
			E = D;
			D = C;
			C = circularShift(30, B);
			B = A;
			A = tmp;
		}

		H[0] += A;
		H[1] += B;
		H[2] += C;
		H[3] += D;
		H[4] += E;
	}

} // namespace

bool caseInsensitiveEqual(std::string_view str1, std::string_view str2)
{
	return str1.size() == str2.size() && std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
		return tolower(a) == tolower(b);
	});
}

bool caseInsensitiveStartsWith(std::string_view str, std::string_view prefix)
{
	return str.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), str.begin(), [](char a, char b) {
		return tolower(a) == tolower(b);
	});
}

std::string transformToSHA1(const std::string& input)
{
	uint32_t H[] = {
		0x67452301,
		0xEFCDAB89,
		0x98BADCFE,
		0x10325476,
		0xC3D2E1F0
	};

	uint8_t messageBlock[64];
	size_t index = 0;

	uint32_t length_low = 0;
	uint32_t length_high = 0;
	for (char ch : input) {
		messageBlock[index++] = ch;

		length_low += 8;
		if (length_low == 0) {
			length_high++;
		}

		if (index == 64) {
			processSHA1MessageBlock(messageBlock, H);
			index = 0;
		}
	}

	messageBlock[index++] = 0x80;

	if (index > 56) {
		while (index < 64) {
			messageBlock[index++] = 0;
		}

		processSHA1MessageBlock(messageBlock, H);
		index = 0;
	}

	while (index < 56) {
		messageBlock[index++] = 0;
	}

	messageBlock[56] = length_high >> 24;
	messageBlock[57] = length_high >> 16;
	messageBlock[58] = length_high >> 8;
	messageBlock[59] = length_high;

	messageBlock[60] = length_low >> 24;
	messageBlock[61] = length_low >> 16;
	messageBlock[62] = length_low >> 8;
	messageBlock[63] = length_low;

	processSHA1MessageBlock(messageBlock, H);

	char hexstring[41];
	static constexpr char hexDigits[] = "0123456789abcdef";
	for (int hashByte = 20; --hashByte >= 0;) {
		const uint8_t byte = H[hashByte >> 2] >> (((3 - hashByte) & 3) << 3);
		index = hashByte << 1;
		hexstring[index] = hexDigits[byte >> 4];
		hexstring[index + 1] = hexDigits[byte & 15];
	}
	return std::string(hexstring, 40);
}

bool booleanString(const std::string& input)
{
	if (input.empty()) {
		return false;
	}

	const char c = input.front();
	return (c == 'y' || c == 'Y' || c == 't' || c == 'T' || c == '1');
}

std::string ucfirst(std::string source)
{
	const size_t index = source.find_first_not_of(' ');
	if (index != std::string::npos) {
		source[index] = static_cast<char>(std::toupper(static_cast<uint8_t>(source[index])));
	}
	return source;
}

std::string ucwords(std::string source)
{
	const size_t index = source.find_first_not_of(' ');
	if (index != std::string::npos) {
		source[index] = static_cast<char>(std::tolower(static_cast<uint8_t>(source[index])));
	}
	return source;
}

bool readXMLInteger(xmlNodePtr node, const char* tag, int32_t& value)
{
	char* nodeValue = reinterpret_cast<char*>(xmlGetProp(node, reinterpret_cast<const xmlChar*>(tag)));
	if (!nodeValue) {
		return false;
	}

	value = atoi(nodeValue);
	xmlFree(nodeValue);
	return true;
}

bool readXMLInteger64(xmlNodePtr node, const char* tag, int64_t& value)
{
	char* nodeValue = reinterpret_cast<char*>(xmlGetProp(node, reinterpret_cast<const xmlChar*>(tag)));
	if (!nodeValue) {
		return false;
	}

	value = atoll(nodeValue);
	xmlFree(nodeValue);
	return true;
}

bool readXMLFloat(xmlNodePtr node, const char* tag, float& value)
{
	char* nodeValue = reinterpret_cast<char*>(xmlGetProp(node, reinterpret_cast<const xmlChar*>(tag)));
	if (!nodeValue) {
		return false;
	}

	value = atof(nodeValue);
	xmlFree(nodeValue);
	return true;
}

bool readXMLString(xmlNodePtr node, const char* tag, std::string& value)
{
	auto nodeValue = reinterpret_cast<char*>(xmlGetProp(node, reinterpret_cast<const xmlChar*>(tag)));
	if (!nodeValue) {
		return false;
	}

	if (!utf8ToLatin1(nodeValue, value)) {
		value = nodeValue;
	}

	xmlFree(nodeValue);
	return true;
}

bool readXMLContentString(xmlNodePtr node, std::string& value)
{
	auto nodeValue = reinterpret_cast<char*>(xmlNodeGetContent(node));
	if (!nodeValue) {
		return false;
	}

	if (!utf8ToLatin1(nodeValue, value)) {
		value = nodeValue;
	}

	xmlFree(nodeValue);
	return true;
}

bool parseXMLContentString(xmlNodePtr node, std::string& value)
{
	bool result = false;
	std::string compareValue;
	while (node) {
		if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("text")) && node->type != XML_CDATA_SECTION_NODE) {
			node = node->next;
			continue;
		}

		if (!readXMLContentString(node, compareValue)) {
			node = node->next;
			continue;
		}

		otx::util::trim_left_string(compareValue, '\r');
		otx::util::trim_left_string(compareValue, '\n');
		otx::util::trim_left_string(compareValue, ' ');
		if (compareValue.length() > value.length()) {
			value = compareValue;
			if (!result) {
				result = true;
			}
		}

		node = node->next;
	}

	return result;
}

std::string getLastXMLError()
{
	std::ostringstream ss;
	auto lastError = xmlGetLastError();
	if (lastError->line) {
		ss << "Line: " << lastError->line << ", ";
	}

	ss << "Info: " << lastError->message << std::endl;
	return ss.str();
}

bool utf8ToLatin1(char* inText, std::string& outText)
{
	outText = "";
	if (!inText) {
		return false;
	}

	int32_t inLen = strlen(inText);
	if (!inLen) {
		return false;
	}

	int32_t outLen = inLen << 1;
	uint8_t* outBuf = new uint8_t[outLen];

	int32_t res = UTF8Toisolat1(outBuf, &outLen, reinterpret_cast<uint8_t*>(inText), &inLen);
	if (res < 0) {
		delete[] outBuf;
		return false;
	}

	outBuf[outLen] = '\0';
	outText = reinterpret_cast<char*>(outBuf);

	delete[] outBuf;
	return true;
}

bool latin1ToUtf8(char* inText, std::string& outText)
{
	outText = "";
	if (!inText) {
		return false;
	}

	int32_t inLen = strlen(inText);
	if (!inLen) {
		return false;
	}

	int32_t outLen = inLen << 1;
	uint8_t* outBuf = new uint8_t[outLen];

	int32_t res = isolat1ToUTF8(outBuf, &outLen, reinterpret_cast<uint8_t*>(inText), &inLen);
	if (res < 0) {
		delete[] outBuf;
		return false;
	}

	outBuf[outLen] = '\0';
	outText = reinterpret_cast<char*>(outBuf);

	delete[] outBuf;
	return true;
}

StringVec explodeString(const std::string& string, const std::string& separator, bool trim /* = true*/, uint16_t limit /* = 0*/)
{
	StringVec returnVector;
	size_t start = 0, end = 0;

	uint16_t i = 1;
	while ((end = string.find(separator, start)) != std::string::npos) {
		std::string t = string.substr(start, end - start);
		if (trim) {
			otx::util::trim_string(t);
		}

		returnVector.push_back(t);
		start = end + separator.size();

		++i;
		if (limit > 0 && i > limit) {
			break;
		}
	}

	returnVector.push_back(string.substr(start));
	return returnVector;
}

std::vector<int32_t> vectorAtoi(const std::vector<std::string>& strvec)
{
	std::vector<int32_t> vec;
	for (const std::string& s : strvec) {
		auto result = otx::util::safe_cast<int32_t>(s.data());
		if (result.second) {
			vec.push_back(result.first);
		}
	}
	return vec;
}

uint32_t rand24b()
{
	return ((rand() << 12) ^ rand()) & 0xFFFFFF;
}

std::mt19937& getRandomGenerator()
{
	static std::random_device rd;
	static std::mt19937 generator(rd());
	return generator;
}

int32_t normal_random(int32_t minNumber, int32_t maxNumber)
{
	static std::normal_distribution<float> normalRand(0.45f, 0.22f);

	if (minNumber == maxNumber) {
		return minNumber;
	} else if (minNumber > maxNumber) {
		std::swap(minNumber, maxNumber);
	}

	int32_t increment;
	const int32_t diff = maxNumber - minNumber;
	const float v = normalRand(getRandomGenerator());
	if (v < 0.0) {
		increment = diff / 2;
	} else if (v > 1.0) {
		increment = (diff + 1) / 2;
	} else {
		increment = round(v * diff);
	}
	return minNumber + increment;
}

int32_t uniform_random(int32_t minNumber, int32_t maxNumber)
{
	static std::uniform_int_distribution<int32_t> uniformRand;
	if (minNumber == maxNumber) {
		return minNumber;
	} else if (minNumber > maxNumber) {
		std::swap(minNumber, maxNumber);
	}
	return uniformRand(getRandomGenerator(), std::uniform_int_distribution<int32_t>::param_type(minNumber, maxNumber));
}

int32_t random_range(int32_t lowestNumber, int32_t highestNumber, DistributionType_t type /*= DISTRO_UNIFORM*/)
{
	switch (type) {
		case DISTRO_UNIFORM:
			return uniform_random(lowestNumber, highestNumber);
		case DISTRO_NORMAL:
			return normal_random(lowestNumber, highestNumber);

		default:
			break;
	}

	const float randMax = 16777216;
	return lowestNumber + (highestNumber - lowestNumber) * (1.f - sqrt((1.f * rand24b()) / randMax));
}

char upchar(char character)
{
	if ((character >= 97 && character <= 122) || (character <= -1 && character >= -32)) {
		character -= 32;
	}

	return character;
}

bool isNumber(char character)
{
	return (character >= 48 && character <= 57);
}

bool isLowercaseLetter(char character)
{
	return (character >= 97 && character <= 122);
}

bool isUppercaseLetter(char character)
{
	return (character >= 65 && character <= 90);
}

bool isPasswordCharacter(char character)
{
	return ((character >= 33 && character <= 47) || (character >= 58 && character <= 64) || (character >= 91 && character <= 96) || (character >= 123 && character <= 126));
}

bool isValidAccountName(std::string text)
{
	otx::util::to_lower_string(text);

	uint32_t textLength = text.length();
	for (uint32_t size = 0; size < textLength; size++) {
		if (!isLowercaseLetter(text[size]) && !isNumber(text[size])) {
			return false;
		}
	}
	return true;
}

bool isValidPassword(std::string text)
{
	otx::util::to_lower_string(text);

	uint32_t textLength = text.length();
	for (uint32_t size = 0; size < textLength; size++) {
		if (!isLowercaseLetter(text[size]) && !isNumber(text[size]) && !isPasswordCharacter(text[size])) {
			return false;
		}
	}
	return true;
}

bool isValidName(std::string text, bool forceUppercaseOnFirstLetter /* = true*/)
{
	uint32_t textLength = text.length(), lenBeforeSpace = 1, lenBeforeQuote = 1, lenBeforeDash = 1, repeatedCharacter = 0;
	char lastChar = 32;
	if (forceUppercaseOnFirstLetter) {
		if (!isUppercaseLetter(text[0])) {
			return false;
		}
	} else if (!isLowercaseLetter(text[0]) && !isUppercaseLetter(text[0])) {
		return false;
	}

	for (uint32_t size = 1; size < textLength; size++) {
		if (text[size] != 32) {
			lenBeforeSpace++;
			if (text[size] != 39) {
				lenBeforeQuote++;
			} else {
				if (lenBeforeQuote <= 1 || size == textLength - 1 || text[size + 1] == 32) {
					return false;
				}

				lenBeforeQuote = 0;
			}

			if (text[size] != 45) {
				lenBeforeDash++;
			} else {
				if (lenBeforeDash <= 1 || size == textLength - 1 || text[size + 1] == 32) {
					return false;
				}

				lenBeforeDash = 0;
			}

			if (text[size] == lastChar) {
				repeatedCharacter++;
				if (repeatedCharacter > 2) {
					return false;
				}
			} else {
				repeatedCharacter = 0;
			}

			lastChar = text[size];
		} else {
			if (lenBeforeSpace <= 1 || size == textLength - 1 || text[size + 1] == 32) {
				return false;
			}

			lenBeforeSpace = lenBeforeQuote = lenBeforeDash = 0;
		}

		if (!(isLowercaseLetter(text[size]) || text[size] == 32 || text[size] == 39 || text[size] == 45
				|| (isUppercaseLetter(text[size]) && text[size - 1] == 32))) {
			return false;
		}
	}

	return true;
}

std::string generateRecoveryKey(int32_t fieldCount, int32_t fieldLenght, bool mixCase /* = false*/)
{
	std::ostringstream key;
	int32_t i = 0, j = 0, lastNumber = 99, number = 0;

	char character = 0, lastCharacter = 0;
	bool madeNumber = false, madeCharacter = false;
	do {
		do {
			madeNumber = madeCharacter = false;
			if ((mixCase && !random_range(0, 2)) || (!mixCase && !random_range(0, 1))) {
				number = random_range(2, 9);
				if (number != lastNumber) {
					key << number;
					lastNumber = number;
					madeNumber = true;
				}
			} else {
				if (mixCase && !random_range(0, 1)) {
					character = static_cast<char>(random_range(97, 122));
				} else {
					character = static_cast<char>(random_range(65, 90));
				}

				if (character != lastCharacter) {
					key << character;
					lastCharacter = character;
					madeCharacter = true;
				}
			}
		} while ((!madeCharacter && !madeNumber) ? true : (++j && j < fieldLenght));
		lastCharacter = character = number = j = 0;

		lastNumber = 99;
		if (i < fieldCount - 1) {
			key << "-";
		}
	} while (++i && i < fieldCount);
	return key.str();
}

std::string parseParams(tokenizer::iterator& it, tokenizer::iterator end)
{
	if (it == end) {
		return "";
	}

	std::string tmp = (*it);
	++it;
	if (tmp[0] == '"') {
		tmp.erase(0, 1);
		while (it != end && tmp[tmp.length() - 1] != '"') {
			tmp += " " + (*it);
			++it;
		}

		if (tmp.length() > 0 && tmp[tmp.length() - 1] == '"') {
			tmp.erase(tmp.length() - 1);
		}
	}

	return tmp;
}

std::string formatDate(time_t _time /* = 0*/)
{
	if (!_time) {
		_time = time(nullptr);
	}

	const tm* tms = localtime(&_time);
	std::ostringstream s;
	if (tms) {
		s << tms->tm_mday << "/" << (tms->tm_mon + 1) << "/" << (tms->tm_year + 1900) << " " << tms->tm_hour << ":" << tms->tm_min << ":" << tms->tm_sec;
	} else {
		s << "UNIX Time: " << _time;
	}
	return s.str();
}

std::string formatDateEx(time_t _time /* = 0*/, std::string format /* = "%d %b %Y, %H:%M:%S"*/)
{
	if (!_time) {
		_time = time(nullptr);
	}

	const tm* tms = localtime(&_time);
	char buffer[100];
	if (tms) {
		strftime(buffer, 25, format.c_str(), tms);
	} else {
		sprintf(buffer, "UNIX Time: %d", static_cast<int32_t>(_time));
	}
	return buffer;
}

std::string formatTime()
{
	using namespace std::chrono;

	auto now = system_clock::now();
	auto millisec = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
	auto timer = system_clock::to_time_t(now);

	std::tm bt = *std::localtime(&timer);
	std::ostringstream oss;

	oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
	oss << '.' << std::setfill('0') << std::setw(3) << millisec.count();

	return oss.str();
}

std::string convertIPAddress(uint32_t ip)
{
	char buffer[17];
	sprintf(buffer, "%d.%d.%d.%d", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24));
	return buffer;
}

Skulls_t getSkulls(std::string strValue)
{
	std::string tmpStrValue = otx::util::as_lower_string(strValue);
	if (tmpStrValue == "black" || tmpStrValue == "5") {
		return SKULL_BLACK;
	}

	if (tmpStrValue == "red" || tmpStrValue == "4") {
		return SKULL_RED;
	}

	if (tmpStrValue == "white" || tmpStrValue == "3") {
		return SKULL_WHITE;
	}

	if (tmpStrValue == "green" || tmpStrValue == "2") {
		return SKULL_GREEN;
	}

	if (tmpStrValue == "yellow" || tmpStrValue == "1") {
		return SKULL_YELLOW;
	}

	return SKULL_NONE;
}

PartyShields_t getShields(std::string strValue)
{
	std::string tmpStrValue = otx::util::as_lower_string(strValue);
	if (tmpStrValue == "whitenoshareoff" || tmpStrValue == "10") {
		return SHIELD_YELLOW_NOSHAREDEXP;
	}

	if (tmpStrValue == "blueshareoff" || tmpStrValue == "9") {
		return SHIELD_BLUE_NOSHAREDEXP;
	}

	if (tmpStrValue == "yellowshareblink" || tmpStrValue == "8") {
		return SHIELD_YELLOW_NOSHAREDEXP_BLINK;
	}

	if (tmpStrValue == "blueshareblink" || tmpStrValue == "7") {
		return SHIELD_BLUE_NOSHAREDEXP_BLINK;
	}

	if (tmpStrValue == "yellowshareon" || tmpStrValue == "6") {
		return SHIELD_YELLOW_SHAREDEXP;
	}

	if (tmpStrValue == "blueshareon" || tmpStrValue == "5") {
		return SHIELD_BLUE_SHAREDEXP;
	}

	if (tmpStrValue == "yellow" || tmpStrValue == "4") {
		return SHIELD_YELLOW;
	}

	if (tmpStrValue == "blue" || tmpStrValue == "3") {
		return SHIELD_BLUE;
	}

	if (tmpStrValue == "whiteyellow" || tmpStrValue == "2") {
		return SHIELD_WHITEYELLOW;
	}

	if (tmpStrValue == "whiteblue" || tmpStrValue == "1") {
		return SHIELD_WHITEBLUE;
	}

	return SHIELD_NONE;
}

GuildEmblems_t getEmblems(std::string strValue)
{
	std::string tmpStrValue = otx::util::as_lower_string(strValue);
	if (tmpStrValue == "blue" || tmpStrValue == "neutral" || tmpStrValue == "3") {
		return GUILDEMBLEM_NEUTRAL;
	}

	if (tmpStrValue == "red" || tmpStrValue == "enemy" || tmpStrValue == "2") {
		return GUILDEMBLEM_ENEMY;
	}

	if (tmpStrValue == "green" || tmpStrValue == "ally" || tmpStrValue == "1") {
		return GUILDEMBLEM_ALLY;
	}

	return GUILDEMBLEM_NONE;
}

Direction getDirection(std::string string)
{
	if (string == "north" || string == "n" || string == "0") {
		return NORTH;
	}

	if (string == "east" || string == "e" || string == "1") {
		return EAST;
	}

	if (string == "south" || string == "s" || string == "2") {
		return SOUTH;
	}

	if (string == "west" || string == "w" || string == "3") {
		return WEST;
	}

	if (string == "southwest" || string == "south west" || string == "south-west" || string == "sw" || string == "4") {
		return SOUTHWEST;
	}

	if (string == "southeast" || string == "south east" || string == "south-east" || string == "se" || string == "5") {
		return SOUTHEAST;
	}

	if (string == "northwest" || string == "north west" || string == "north-west" || string == "nw" || string == "6") {
		return NORTHWEST;
	}

	if (string == "northeast" || string == "north east" || string == "north-east" || string == "ne" || string == "7") {
		return NORTHEAST;
	}

	return SOUTH;
}

Direction getDirectionTo(Position pos1, Position pos2, bool extended /* = true*/)
{
	Direction direction = NORTH;
	if (pos1.x > pos2.x) {
		direction = WEST;
		if (extended) {
			if (pos1.y > pos2.y) {
				direction = NORTHWEST;
			} else if (pos1.y < pos2.y) {
				direction = SOUTHWEST;
			}
		}
	} else if (pos1.x < pos2.x) {
		direction = EAST;
		if (extended) {
			if (pos1.y > pos2.y) {
				direction = NORTHEAST;
			} else if (pos1.y < pos2.y) {
				direction = SOUTHEAST;
			}
		}
	} else {
		if (pos1.y > pos2.y) {
			direction = NORTH;
		} else if (pos1.y < pos2.y) {
			direction = SOUTH;
		}
	}

	return direction;
}

Position getNextPosition(Direction direction, Position pos)
{
	switch (direction) {
		case NORTH:
			pos.y--;
			break;
		case SOUTH:
			pos.y++;
			break;
		case WEST:
			pos.x--;
			break;
		case EAST:
			pos.x++;
			break;
		case SOUTHWEST:
			pos.x--;
			pos.y++;
			break;
		case NORTHWEST:
			pos.x--;
			pos.y--;
			break;
		case SOUTHEAST:
			pos.x++;
			pos.y++;
			break;
		case NORTHEAST:
			pos.x++;
			pos.y--;
			break;
		default:
			break;
	}

	return pos;
}

struct AmmoTypeNames
{
	const char* name;
	Ammo_t ammoType;
};

struct MagicEffectNames
{
	const char* name;
	MagicEffect_t magicEffect;
};

struct ShootTypeNames
{
	const char* name;
	ShootEffect_t shootType;
};

struct CombatTypeNames
{
	const char* name;
	CombatType_t combatType;
};

struct AmmoActionNames
{
	const char* name;
	AmmoAction_t ammoAction;
};

struct FluidTypeNames
{
	const char* name;
	FluidTypes_t fluidType;
};

struct WeaponTypeNames
{
	const char* name;
	WeaponType_t weaponType;
};

MagicEffectNames magicEffectNames[] = {
	{ "redspark", MAGIC_EFFECT_DRAW_BLOOD },
	{ "bluebubble", MAGIC_EFFECT_LOSE_ENERGY },
	{ "poff", MAGIC_EFFECT_POFF },
	{ "yellowspark", MAGIC_EFFECT_BLOCKHIT },
	{ "explosionarea", MAGIC_EFFECT_EXPLOSION_AREA },
	{ "explosion", MAGIC_EFFECT_EXPLOSION_DAMAGE },
	{ "firearea", MAGIC_EFFECT_FIRE_AREA },
	{ "yellowbubble", MAGIC_EFFECT_YELLOW_RINGS },
	{ "greenbubble", MAGIC_EFFECT_POISON_RINGS },
	{ "blackspark", MAGIC_EFFECT_HIT_AREA },
	{ "teleport", MAGIC_EFFECT_TELEPORT },
	{ "energy", MAGIC_EFFECT_ENERGY_DAMAGE },
	{ "blueshimmer", MAGIC_EFFECT_WRAPS_BLUE },
	{ "redshimmer", MAGIC_EFFECT_WRAPS_RED },
	{ "greenshimmer", MAGIC_EFFECT_WRAPS_GREEN },
	{ "fire", MAGIC_EFFECT_HITBY_FIRE },
	{ "greenspark", MAGIC_EFFECT_POISON },
	{ "mortarea", MAGIC_EFFECT_MORT_AREA },
	{ "greennote", MAGIC_EFFECT_SOUND_GREEN },
	{ "rednote", MAGIC_EFFECT_SOUND_RED },
	{ "poison", MAGIC_EFFECT_POISON_AREA },
	{ "yellownote", MAGIC_EFFECT_SOUND_YELLOW },
	{ "purplenote", MAGIC_EFFECT_SOUND_PURPLE },
	{ "bluenote", MAGIC_EFFECT_SOUND_BLUE },
	{ "whitenote", MAGIC_EFFECT_SOUND_WHITE },
	{ "bubbles", MAGIC_EFFECT_BUBBLES },
	{ "dice", MAGIC_EFFECT_CRAPS },
	{ "giftwraps", MAGIC_EFFECT_GIFT_WRAPS },
	{ "yellowfirework", MAGIC_EFFECT_FIREWORK_YELLOW },
	{ "redfirework", MAGIC_EFFECT_FIREWORK_RED },
	{ "bluefirework", MAGIC_EFFECT_FIREWORK_BLUE },
	{ "stun", MAGIC_EFFECT_STUN },
	{ "sleep", MAGIC_EFFECT_SLEEP },
	{ "watercreature", MAGIC_EFFECT_WATERCREATURE },
	{ "groundshaker", MAGIC_EFFECT_GROUNDSHAKER },
	{ "hearts", MAGIC_EFFECT_HEARTS },
	{ "fireattack", MAGIC_EFFECT_FIREATTACK },
	{ "energyarea", MAGIC_EFFECT_ENERGY_AREA },
	{ "smallclouds", MAGIC_EFFECT_SMALLCLOUDS },
	{ "holydamage", MAGIC_EFFECT_HOLYDAMAGE },
	{ "bigclouds", MAGIC_EFFECT_BIGCLOUDS },
	{ "icearea", MAGIC_EFFECT_ICEAREA },
	{ "icetornado", MAGIC_EFFECT_ICETORNADO },
	{ "iceattack", MAGIC_EFFECT_ICEATTACK },
	{ "stones", MAGIC_EFFECT_STONES },
	{ "smallplants", MAGIC_EFFECT_SMALLPLANTS },
	{ "carniphila", MAGIC_EFFECT_CARNIPHILA },
	{ "purpleenergy", MAGIC_EFFECT_PURPLEENERGY },
	{ "yellowenergy", MAGIC_EFFECT_YELLOWENERGY },
	{ "holyarea", MAGIC_EFFECT_HOLYAREA },
	{ "bigplants", MAGIC_EFFECT_BIGPLANTS },
	{ "cake", MAGIC_EFFECT_CAKE },
	{ "giantice", MAGIC_EFFECT_GIANTICE },
	{ "watersplash", MAGIC_EFFECT_WATERSPLASH },
	{ "plantattack", MAGIC_EFFECT_PLANTATTACK },
	{ "tutorialarrow", MAGIC_EFFECT_TUTORIALARROW },
	{ "tutorialsquare", MAGIC_EFFECT_TUTORIALSQUARE },
	{ "mirrorhorizontal", MAGIC_EFFECT_MIRRORHORIZONTAL },
	{ "mirrorvertical", MAGIC_EFFECT_MIRRORVERTICAL },
	{ "skullhorizontal", MAGIC_EFFECT_SKULLHORIZONTAL },
	{ "skullvertical", MAGIC_EFFECT_SKULLVERTICAL },
	{ "assassin", MAGIC_EFFECT_ASSASSIN },
	{ "stepshorizontal", MAGIC_EFFECT_STEPSHORIZONTAL },
	{ "bloodysteps", MAGIC_EFFECT_BLOODYSTEPS },
	{ "stepsvertical", MAGIC_EFFECT_STEPSVERTICAL },
	{ "yalaharighost", MAGIC_EFFECT_YALAHARIGHOST },
	{ "bats", MAGIC_EFFECT_BATS },
	{ "smoke", MAGIC_EFFECT_SMOKE },
	{ "insects", MAGIC_EFFECT_INSECTS },
	{ "dragonhead", MAGIC_EFFECT_DRAGONHEAD }
};

ShootTypeNames shootTypeNames[] = {
	{ "spear", SHOOT_EFFECT_SPEAR },
	{ "bolt", SHOOT_EFFECT_BOLT },
	{ "arrow", SHOOT_EFFECT_ARROW },
	{ "fire", SHOOT_EFFECT_FIRE },
	{ "energy", SHOOT_EFFECT_ENERGY },
	{ "poisonarrow", SHOOT_EFFECT_POISONARROW },
	{ "burstarrow", SHOOT_EFFECT_BURSTARROW },
	{ "throwingstar", SHOOT_EFFECT_THROWINGSTAR },
	{ "throwingknife", SHOOT_EFFECT_THROWINGKNIFE },
	{ "smallstone", SHOOT_EFFECT_SMALLSTONE },
	{ "death", SHOOT_EFFECT_DEATH },
	{ "largerock", SHOOT_EFFECT_LARGEROCK },
	{ "snowball", SHOOT_EFFECT_SNOWBALL },
	{ "powerbolt", SHOOT_EFFECT_POWERBOLT },
	{ "poison", SHOOT_EFFECT_POISONFIELD },
	{ "infernalbolt", SHOOT_EFFECT_INFERNALBOLT },
	{ "huntingspear", SHOOT_EFFECT_HUNTINGSPEAR },
	{ "enchantedspear", SHOOT_EFFECT_ENCHANTEDSPEAR },
	{ "redstar", SHOOT_EFFECT_REDSTAR },
	{ "greenstar", SHOOT_EFFECT_GREENSTAR },
	{ "royalspear", SHOOT_EFFECT_ROYALSPEAR },
	{ "sniperarrow", SHOOT_EFFECT_SNIPERARROW },
	{ "onyxarrow", SHOOT_EFFECT_ONYXARROW },
	{ "piercingbolt", SHOOT_EFFECT_PIERCINGBOLT },
	{ "whirlwindsword", SHOOT_EFFECT_WHIRLWINDSWORD },
	{ "whirlwindaxe", SHOOT_EFFECT_WHIRLWINDAXE },
	{ "whirlwindclub", SHOOT_EFFECT_WHIRLWINDCLUB },
	{ "etherealspear", SHOOT_EFFECT_ETHEREALSPEAR },
	{ "ice", SHOOT_EFFECT_ICE },
	{ "earth", SHOOT_EFFECT_EARTH },
	{ "holy", SHOOT_EFFECT_HOLY },
	{ "suddendeath", SHOOT_EFFECT_SUDDENDEATH },
	{ "flasharrow", SHOOT_EFFECT_FLASHARROW },
	{ "flammingarrow", SHOOT_EFFECT_FLAMMINGARROW },
	{ "flamingarrow", SHOOT_EFFECT_FLAMMINGARROW },
	{ "shiverarrow", SHOOT_EFFECT_SHIVERARROW },
	{ "energyball", SHOOT_EFFECT_ENERGYBALL },
	{ "smallice", SHOOT_EFFECT_SMALLICE },
	{ "smallholy", SHOOT_EFFECT_SMALLHOLY },
	{ "smallearth", SHOOT_EFFECT_SMALLEARTH },
	{ "eartharrow", SHOOT_EFFECT_EARTHARROW },
	{ "explosion", SHOOT_EFFECT_EXPLOSION },
	{ "cake", SHOOT_EFFECT_CAKE }
};

CombatTypeNames combatTypeNames[] = {
	{ "physical", COMBAT_PHYSICALDAMAGE },
	{ "energy", COMBAT_ENERGYDAMAGE },
	{ "earth", COMBAT_EARTHDAMAGE },
	{ "fire", COMBAT_FIREDAMAGE },
	{ "undefined", COMBAT_UNDEFINEDDAMAGE },
	{ "lifedrain", COMBAT_LIFEDRAIN },
	{ "life drain", COMBAT_LIFEDRAIN },
	{ "manadrain", COMBAT_MANADRAIN },
	{ "mana drain", COMBAT_MANADRAIN },
	{ "healing", COMBAT_HEALING },
	{ "drown", COMBAT_DROWNDAMAGE },
	{ "ice", COMBAT_ICEDAMAGE },
	{ "holy", COMBAT_HOLYDAMAGE },
	{ "death", COMBAT_DEATHDAMAGE }
};

AmmoTypeNames ammoTypeNames[] = {
	{ "spear", AMMO_SPEAR },
	{ "arrow", AMMO_ARROW },
	{ "poisonarrow", AMMO_ARROW },
	{ "burstarrow", AMMO_ARROW },
	{ "bolt", AMMO_BOLT },
	{ "powerbolt", AMMO_BOLT },
	{ "smallstone", AMMO_STONE },
	{ "largerock", AMMO_STONE },
	{ "throwingstar", AMMO_THROWINGSTAR },
	{ "throwingknife", AMMO_THROWINGKNIFE },
	{ "snowball", AMMO_SNOWBALL },
	{ "huntingspear", AMMO_SPEAR },
	{ "royalspear", AMMO_SPEAR },
	{ "enchantedspear", AMMO_SPEAR },
	{ "sniperarrow", AMMO_ARROW },
	{ "onyxarrow", AMMO_ARROW },
	{ "piercingbolt", AMMO_BOLT },
	{ "infernalbolt", AMMO_BOLT },
	{ "flasharrow", AMMO_ARROW },
	{ "flammingarrow", AMMO_ARROW },
	{ "flamingarrow", AMMO_ARROW },
	{ "shiverarrow", AMMO_ARROW },
	{ "eartharrow", AMMO_ARROW },
	{ "etherealspear", AMMO_SPEAR }
};

AmmoActionNames ammoActionNames[] = {
	{ "move", AMMOACTION_MOVE },
	{ "moveback", AMMOACTION_MOVEBACK },
	{ "move back", AMMOACTION_MOVEBACK },
	{ "removecharge", AMMOACTION_REMOVECHARGE },
	{ "remove charge", AMMOACTION_REMOVECHARGE },
	{ "removecount", AMMOACTION_REMOVECOUNT },
	{ "remove count", AMMOACTION_REMOVECOUNT }
};

FluidTypeNames fluidTypeNames[] = {
	{ "none", FLUID_NONE },
	{ "water", FLUID_WATER },
	{ "blood", FLUID_BLOOD },
	{ "beer", FLUID_BEER },
	{ "slime", FLUID_SLIME },
	{ "lemonade", FLUID_LEMONADE },
	{ "milk", FLUID_MILK },
	{ "mana", FLUID_MANA },
	{ "life", FLUID_LIFE },
	{ "oil", FLUID_OIL },
	{ "urine", FLUID_URINE },
	{ "coconutmilk", FLUID_COCONUTMILK },
	{ "coconut milk", FLUID_COCONUTMILK },
	{ "wine", FLUID_WINE },
	{ "mud", FLUID_MUD },
	{ "fruitjuice", FLUID_FRUITJUICE },
	{ "fruit juice", FLUID_FRUITJUICE },
	{ "lava", FLUID_LAVA },
	{ "rum", FLUID_RUM },
	{ "swamp", FLUID_SWAMP },
	{ "tea", FLUID_TEA },
	{ "mead", FLUID_MEAD }
};

WeaponTypeNames weaponTypeNames[] = {
	{ "", WEAPON_NONE },
	{ "sword", WEAPON_SWORD },
	{ "club", WEAPON_CLUB },
	{ "axe", WEAPON_AXE },
	{ "distance", WEAPON_DIST },
	{ "shield", WEAPON_SHIELD },
	{ "fist", WEAPON_FIST },
	{ "wand", WEAPON_WAND },
	{ "ammunition", WEAPON_AMMO }
};

MagicEffect_t getMagicEffect(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(magicEffectNames) / sizeof(MagicEffectNames); ++i) {
		if (caseInsensitiveEqual(strValue, magicEffectNames[i].name)) {
			return magicEffectNames[i].magicEffect;
		}
	}
	return MAGIC_EFFECT_NONE;
}

ShootEffect_t getShootType(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(shootTypeNames) / sizeof(ShootTypeNames); ++i) {
		if (caseInsensitiveEqual(strValue, shootTypeNames[i].name)) {
			return shootTypeNames[i].shootType;
		}
	}
	return SHOOT_EFFECT_NONE;
}

CombatType_t getCombatType(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(combatTypeNames) / sizeof(CombatTypeNames); ++i) {
		if (caseInsensitiveEqual(strValue, combatTypeNames[i].name)) {
			return combatTypeNames[i].combatType;
		}
	}

	return COMBAT_NONE;
}

Ammo_t getAmmoType(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(ammoTypeNames) / sizeof(AmmoTypeNames); ++i) {
		if (caseInsensitiveEqual(strValue, ammoTypeNames[i].name)) {
			return ammoTypeNames[i].ammoType;
		}
	}

	return AMMO_NONE;
}

AmmoAction_t getAmmoAction(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(ammoActionNames) / sizeof(AmmoActionNames); ++i) {
		if (caseInsensitiveEqual(strValue, ammoActionNames[i].name)) {
			return ammoActionNames[i].ammoAction;
		}
	}

	return AMMOACTION_NONE;
}

FluidTypes_t getFluidType(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(fluidTypeNames) / sizeof(FluidTypeNames); ++i) {
		if (caseInsensitiveEqual(strValue, fluidTypeNames[i].name)) {
			return fluidTypeNames[i].fluidType;
		}
	}

	return FLUID_NONE;
}

WeaponType_t getWeaponType(const std::string& strValue)
{
	for (uint32_t i = 0; i < sizeof(weaponTypeNames) / sizeof(WeaponTypeNames); ++i) {
		if (caseInsensitiveEqual(strValue, weaponTypeNames[i].name)) {
			return weaponTypeNames[i].weaponType;
		}
	}

	return WEAPON_NONE;
}

std::string getCombatName(CombatType_t combatType)
{
	switch (combatType) {
		case COMBAT_PHYSICALDAMAGE:
			return "physical";
		case COMBAT_ENERGYDAMAGE:
			return "energy";
		case COMBAT_EARTHDAMAGE:
			return "earth";
		case COMBAT_FIREDAMAGE:
			return "fire";
		case COMBAT_UNDEFINEDDAMAGE:
			return "undefined";
		case COMBAT_LIFEDRAIN:
			return "life drain";
		case COMBAT_MANADRAIN:
			return "mana drain";
		case COMBAT_HEALING:
			return "healing";
		case COMBAT_DROWNDAMAGE:
			return "drown";
		case COMBAT_ICEDAMAGE:
			return "ice";
		case COMBAT_HOLYDAMAGE:
			return "holy";
		case COMBAT_DEATHDAMAGE:
			return "death";
		default:
			break;
	}

	return "unknown";
}

std::string getSkillName(uint16_t skillId, bool suffix /* = true*/)
{
	switch (skillId) {
		case SKILL_FIST: {
			std::string tmp = "fist";
			if (suffix) {
				tmp += " fighting";
			}

			return tmp;
		}
		case SKILL_CLUB: {
			std::string tmp = "club";
			if (suffix) {
				tmp += " fighting";
			}

			return tmp;
		}
		case SKILL_SWORD: {
			std::string tmp = "sword";
			if (suffix) {
				tmp += " fighting";
			}

			return tmp;
		}
		case SKILL_AXE: {
			std::string tmp = "axe";
			if (suffix) {
				tmp += " fighting";
			}

			return tmp;
		}
		case SKILL_DIST: {
			std::string tmp = "distance";
			if (suffix) {
				tmp += " fighting";
			}

			return tmp;
		}
		case SKILL_SHIELD:
			return "shielding";
		case SKILL_FISH:
			return "fishing";
		case SKILL__MAGLEVEL:
			return "magic level";
		case SKILL__LEVEL:
			return "level";
		default:
			break;
	}

	return "unknown";
}

std::string getReason(int32_t reasonId)
{
	switch (reasonId) {
		case 0:
			return "Offensive Name";
		case 1:
			return "Invalid Name Format";
		case 2:
			return "Unsuitable Name";
		case 3:
			return "Name Inciting Rule Violation";
		case 4:
			return "Offensive Statement";
		case 5:
			return "Spamming";
		case 6:
			return "Illegal Advertising";
		case 7:
			return "Off-Topic Public Statement";
		case 8:
			return "Non-English Public Statement";
		case 9:
			return "Inciting Rule Violation";
		case 10:
			return "Bug Abuse";
		case 11:
			return "Game Weakness Abuse";
		case 12:
			return "Using Unofficial Software to Play";
		case 13:
			return "Hacking";
		case 14:
			return "Multi-Clienting";
		case 15:
			return "Account Trading or Sharing";
		case 16:
			return "Threatening Gamemaster";
		case 17:
			return "Pretending to Have Influence on Rule Enforcement";
		case 18:
			return "False Report to Gamemaster";
		case 19:
			return "Destructive Behaviour";
		case 20:
			return "Excessive Unjustified Player Killing";
		default:
			break;
	}

	return "Unknown Reason";
}

std::string getAction(ViolationAction_t actionId, bool ipBanishment)
{
	std::string action = "Unknown";
	switch (actionId) {
		case ACTION_NOTATION:
			action = "Notation";
			break;
		case ACTION_NAMEREPORT:
			action = "Name Report";
			break;
		case ACTION_BANISHMENT:
			action = "Banishment";
			break;
		case ACTION_BANREPORT:
			action = "Name Report + Banishment";
			break;
		case ACTION_BANFINAL:
			action = "Banishment + Final Warning";
			break;
		case ACTION_BANREPORTFINAL:
			action = "Name Report + Banishment + Final Warning";
			break;
		case ACTION_STATEMENT:
			action = "Statement Report";
			break;
		// internal use
		case ACTION_DELETION:
			action = "Deletion";
			break;
		case ACTION_NAMELOCK:
			action = "Name Lock";
			break;
		case ACTION_BANLOCK:
			action = "Name Lock + Banishment";
			break;
		case ACTION_BANLOCKFINAL:
			action = "Name Lock + Banishment + Final Warning";
			break;
		default:
			break;
	}

	if (ipBanishment) {
		action += " + IP Banishment";
	}

	return action;
}

std::string getWeaponName(WeaponType_t weaponType)
{
	switch (weaponType) {
		case WEAPON_SWORD:
			return "sword";
		case WEAPON_CLUB:
			return "club";
		case WEAPON_AXE:
			return "axe";
		case WEAPON_DIST:
			return "distance";
		case WEAPON_SHIELD:
			return "shield";
		case WEAPON_WAND:
			return "wand";
		case WEAPON_FIST:
			return "fist";
		case WEAPON_AMMO:
			return "ammunition";
		default:
			break;
	}

	return "";
}

std::string parseVocationString(StringVec vocStringVec)
{
	std::string str = "";
	if (!vocStringVec.empty()) {
		for (StringVec::iterator it = vocStringVec.begin(); it != vocStringVec.end(); ++it) {
			if ((*it) != vocStringVec.front()) {
				if ((*it) != vocStringVec.back()) {
					str += ", ";
				} else {
					str += " and ";
				}
			}

			str += (*it);
			str += "s";
		}
	}

	return str;
}

bool parseVocationNode(xmlNodePtr vocationNode, VocationMap& vocationMap, StringVec& vocStringVec, std::string& errorStr)
{
	if (xmlStrcmp(vocationNode->name, reinterpret_cast<const xmlChar*>("vocation"))) {
		return true;
	}

	int32_t vocationId = -1;
	std::string strValue, tmpStrValue;
	if (readXMLString(vocationNode, "name", strValue)) {
		vocationId = g_vocations.getVocationId(strValue);
		if (vocationId != -1) {
			vocationMap[vocationId] = true;
			int32_t promotedVocation = g_vocations.getPromotedVocation(vocationId);
			if (promotedVocation != -1) {
				vocationMap[promotedVocation] = true;
			}
		} else {
			errorStr = "Wrong vocation name: " + strValue;
			return false;
		}
	} else if (readXMLString(vocationNode, "id", strValue)) {
		auto intVector = parseStringInts(strValue);
		if (intVector.empty()) {
			errorStr = "Invalid vocation id - '" + strValue + "'";
			return false;
		}

		size_t size = intVector.size();
		for (size_t i = 0; i < size; ++i) {
			Vocation* vocation = g_vocations.getVocation(intVector[i]);
			if (vocation && !vocation->name.empty()) {
				vocationId = vocation->id;
				strValue = vocation->name;

				vocationMap[vocationId] = true;
				int32_t promotedVocation = g_vocations.getPromotedVocation(vocationId);
				if (promotedVocation != -1) {
					vocationMap[promotedVocation] = true;
				}
			} else {
				std::ostringstream ss;
				ss << "Wrong vocation id: " << intVector[i];

				errorStr = ss.str();
				return false;
			}
		}
	}

	if (vocationId != -1 && (!readXMLString(vocationNode, "showInDescription", tmpStrValue) || booleanString(tmpStrValue))) {
		vocStringVec.push_back(otx::util::as_lower_string(strValue));
	}

	return true;
}

std::vector<int32_t> parseStringInts(const std::string& str)
{
	std::vector<int32_t> vec;
	for (const std::string& s : explodeString(str, ";")) {
		auto tmpIntVector = vectorAtoi(explodeString(s, "-"));
		if (tmpIntVector.empty()) {
			continue;
		}

		vec.push_back(tmpIntVector[0]);
		if (tmpIntVector.size() > 1) {
			while (tmpIntVector[0] < tmpIntVector[1]) {
				vec.push_back(++tmpIntVector[0]);
			}
		}
	}
	return vec;
}

bool fileExists(const std::string& filename)
{
	return std::filesystem::exists(filename);
}

uint32_t adlerChecksum(uint8_t* data, size_t length)
{
	if (length > NETWORKMESSAGE_MAXSIZE || !length) {
		return 0;
	}

	const uint16_t adler = 65521;
	uint32_t a = 1, b = 0;
	while (length > 0) {
		size_t tmp = length > 5552 ? 5552 : length;
		length -= tmp;
		do {
			a += *data++;
			b += a;
		} while (--tmp);
		a %= adler;
		b %= adler;
	}

	return (b << 16) | a;
}

std::string getFilePath(FileType_t type, std::string name /* = ""*/)
{
	std::string path = otx::config::getString(otx::config::DATA_DIRECTORY);
	switch (type) {
		case FILE_TYPE_OTHER:
			path += name;
			break;
		case FILE_TYPE_XML:
			path += "XML/" + name;
			break;
		case FILE_TYPE_LOG:
			path = otx::config::getString(otx::config::LOGS_DIRECTORY) + name;
			break;
		case FILE_TYPE_CONFIG:
			path = name;
			break;

		default:
			std::clog << "> ERROR: Wrong file type!" << std::endl;
			break;
	}
	return path;
}

uint8_t serverFluidToClient(uint8_t serverFluid)
{
	uint8_t size = sizeof(clientToServerFluidMap) / sizeof(int8_t);
	for (uint8_t i = 0; i < size; ++i) {
		if (clientToServerFluidMap[i] == serverFluid) {
			return i;
		}
	}
	return 0;
}

uint8_t clientFluidToServer(uint8_t clientFluid)
{
	uint8_t size = sizeof(clientToServerFluidMap) / sizeof(int8_t);
	if (clientFluid >= size) {
		return 0;
	}
	return clientToServerFluidMap[clientFluid];
}

// TODO: remove the serial thing on items or make it better
std::string generateSerial()
{
	static constexpr char serial_chars[37] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

	std::string serial;
	serial.reserve(23);

	for (uint8_t part = 0; part < 4; ++part) {
		if (!serial.empty()) {
			serial.push_back('-');
		}

		for (uint8_t i = 0; i < 5; ++i) {
			serial.push_back(serial_chars[random_range(1, 37)]);
		}
	}
	return serial;
}
