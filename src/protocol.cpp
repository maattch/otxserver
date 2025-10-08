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

#include "protocol.h"

#include "outputmessage.h"
#include "rsa.h"

extern RSA g_RSA;

namespace
{
	constexpr size_t RSA_BUFFER_LENGTH = 128;

	void XTEA_encrypt(OutputMessage& msg, const otx::xtea::round_keys& key)
	{
		// The message must be a multiple of 8
		size_t paddingBytes = msg.getLength() % 8u;
		if (paddingBytes != 0) {
			msg.addPaddingBytes(8 - paddingBytes);
		}

		uint8_t* buffer = msg.getOutputBuffer();
		otx::xtea::encrypt(buffer, msg.getLength(), key);
	}

	bool XTEA_decrypt(NetworkMessage& msg, const otx::xtea::round_keys& key)
	{
		if (((msg.getLength() - 6) & 7) != 0) {
			return false;
		}

		uint8_t* buffer = msg.getRemainingBuffer();
		otx::xtea::decrypt(buffer, msg.getLength() - 6, key);

		const auto innerLength = msg.get<uint16_t>();
		if (innerLength + 8 > msg.getLength()) {
			return false;
		}

		msg.setLength(innerLength);
		return true;
	}

} // namespace

void Protocol::onSendMessage(const OutputMessage_ptr& msg)
{
	if (!m_rawMessages) {
		msg->writeMessageLength();

		if (m_encryptionEnabled) {
			XTEA_encrypt(*msg, m_key);
			msg->addCryptoHeader(m_checksumEnabled);
		}
	}
}

void Protocol::onRecvMessage(NetworkMessage& msg)
{
	if (m_encryptionEnabled && !XTEA_decrypt(msg, m_key)) {
		return;
	}

	parsePacket(msg);
}

OutputMessage_ptr Protocol::getOutputBuffer()
{
	if (!m_outputBuffer) {
		m_outputBuffer = OutputMessagePool::getOutputMessage();
	}
	return m_outputBuffer;
}

OutputMessage_ptr Protocol::getOutputBuffer(int32_t size)
{
	// dispatcher thread
	if (!m_outputBuffer) {
		m_outputBuffer = OutputMessagePool::getOutputMessage();
	} else if ((m_outputBuffer->getLength() + size) > NetworkMessage::MAX_PROTOCOL_BODY_LENGTH) {
		send(m_outputBuffer);
		m_outputBuffer = OutputMessagePool::getOutputMessage();
	}
	return m_outputBuffer;
}

bool Protocol::RSA_decrypt(NetworkMessage& msg)
{
	if (msg.getRemainingBufferLength() < RSA_BUFFER_LENGTH) {
		return false;
	}

	g_RSA.decrypt(reinterpret_cast<char*>(msg.getBuffer()) + msg.getBufferPosition()); //does not break strict aliasing
	return msg.getByte() == 0;
}

uint32_t Protocol::getIP() const
{
	if (auto connection = getConnection()) {
		return connection->getIP();
	}
	return 0;
}
