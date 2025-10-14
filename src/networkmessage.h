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

#include "const.h"

class Item;
class Creature;
class Player;
class Position;
class RSA;

class NetworkMessage
{
public:
	using MsgSize_t = uint16_t;

	// Headers:
	// 2 bytes for unencrypted message size
	// 4 bytes for checksum
	// 2 bytes for encrypted message size
	static constexpr MsgSize_t INITIAL_BUFFER_POSITION = 8;

	static constexpr uint8_t HEADER_LENGTH = 2;
	static constexpr uint8_t CHECKSUM_LENGTH = 4;
	static constexpr uint8_t XTEA_MULTIPLE = 8;
	static constexpr uint16_t MAX_BODY_LENGTH = NETWORKMESSAGE_MAXSIZE - HEADER_LENGTH - CHECKSUM_LENGTH - XTEA_MULTIPLE;
	static constexpr uint16_t MAX_PROTOCOL_BODY_LENGTH = MAX_BODY_LENGTH - 10;

	NetworkMessage() noexcept = default;

	void reset() noexcept { info = {}; }

	// simply read functions for incoming message
	uint8_t getByte() noexcept {
		if (!canRead(1)) {
			return 0;
		}
		return buffer[info.position++];
	}

	uint8_t getPreviousByte() noexcept { return buffer[--info.position]; }

	// workaround missing "is_trivially_copyable" and "is_trivially_constructible" in g++ < 5.0
	template<typename T>
#if defined(__GNUC__) && __GNUC__ < 5
	std::enable_if_t<std::has_trivial_copy_constructor_v<T>, T> get() noexcept {
#else
	std::enable_if_t<std::is_trivially_copyable_v<T>, T> get() noexcept {
		static_assert(std::is_trivially_constructible<T>::value, "Destination type must be trivially constructible");
#endif
		if (!canRead(sizeof(T))) {
			return 0;
		}

		T value;
		std::memcpy(&value, buffer + info.position, sizeof(T));
		info.position += sizeof(T);
		return value;
	}

	std::string getString(uint16_t stringLen = 0);
	Position getPosition();

	// skips count unknown/unused bytes in an incoming message
	void skipBytes(int16_t count) noexcept { info.position += count; }

	// simply write functions for outgoing message
	void addByte(uint8_t value) noexcept {
		if (canAdd(1)) {
			buffer[info.position++] = value;
			++info.length;
		}
	}

	template<typename T>
	void add(T value) noexcept {
		if (canAdd(sizeof(T))) {
			std::memcpy(buffer + info.position, &value, sizeof(T));
			info.position += sizeof(T);
			info.length += sizeof(T);
		}
	}

	void addBytes(const char* bytes, size_t size);
	void addPaddingBytes(size_t n);

	void addDouble(double value, uint8_t precision);
	void addString(std::string_view value);

	// write functions for complex types
	void addPosition(const Position& pos);
	void addItem(const Item* item);
	void addItemId(uint16_t itemId);

	MsgSize_t getLength() const noexcept { return info.length; }
	void setLength(MsgSize_t newLength) noexcept { info.length = newLength; }

	MsgSize_t getRemainingBufferLength() const noexcept { return info.length - info.position; }

	MsgSize_t getBufferPosition() const noexcept { return info.position; }
	bool setBufferPosition(MsgSize_t pos) noexcept {
		if (pos < NETWORKMESSAGE_MAXSIZE - INITIAL_BUFFER_POSITION) {
			info.position = pos + INITIAL_BUFFER_POSITION;
			return true;
		}
		return false;
	}

	uint16_t getLengthHeader() const noexcept { return static_cast<uint16_t>(buffer[0] | buffer[1] << 8); }

	bool isOverrun() const noexcept { return info.overrun; }

	uint8_t* getBuffer() noexcept { return buffer; }
	const uint8_t* getBuffer() const noexcept { return buffer; }

	uint8_t* getRemainingBuffer() noexcept { return buffer + info.position; }

	uint8_t* getBodyBuffer() noexcept {
		info.position = HEADER_LENGTH;
		return buffer + HEADER_LENGTH;
	}

private:
	bool canAdd(size_t size) const noexcept { return (size + info.position) < MAX_BODY_LENGTH; }

	constexpr bool canRead(int32_t size) noexcept {
		if ((info.position + size) > (info.length + INITIAL_BUFFER_POSITION) || size >= (NETWORKMESSAGE_MAXSIZE - info.position)) {
			info.overrun = true;
			return false;
		}
		return true;
	}

protected:
	uint8_t buffer[NETWORKMESSAGE_MAXSIZE];

	struct {
		MsgSize_t length{ 0 };
		MsgSize_t position{ INITIAL_BUFFER_POSITION };
		bool overrun{ false };
	} info;
};
