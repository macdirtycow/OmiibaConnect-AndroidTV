#include "BluetoothWrapper.h"

#include <chrono>

namespace {
constexpr size_t kMaxRecvStagingBytes = 1024;
constexpr int kMaxRecvLoopsPerCall = 24;
constexpr auto kQueryDeadline = std::chrono::milliseconds(4500);
constexpr auto kPostSendAckDeadline = std::chrono::milliseconds(3500);

int64_t steadyClockEpochMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch())
		.count();
}
}

BluetoothWrapper::BluetoothWrapper(std::unique_ptr<IBluetoothConnector> connector)
{
	this->_connector.swap(connector);
}

BluetoothWrapper::BluetoothWrapper(BluetoothWrapper&& other) noexcept
{
	this->_connector.swap(other._connector);
	this->_seqNumber = other._seqNumber;
	this->_recvStaging = std::move(other._recvStaging);
	this->_recvInMessage = other._recvInMessage;
}

BluetoothWrapper& BluetoothWrapper::operator=(BluetoothWrapper&& other) noexcept
{
	if (this == &other) return *this;

	this->_connector.swap(other._connector);
	this->_seqNumber = other._seqNumber;
	this->_recvStaging = std::move(other._recvStaging);
	this->_recvInMessage = other._recvInMessage;

	return *this;
}

unsigned char BluetoothWrapper::_nextSendSequence()
{
	const unsigned char out = this->_seqNumber;
	this->_seqNumber = static_cast<unsigned char>(1 - this->_seqNumber);
	return out;
}

void BluetoothWrapper::_onAckReceived(unsigned char ackSeq)
{
	this->_seqNumber = ackSeq;
}

void BluetoothWrapper::_sendAck(unsigned char deviceSeqNumber)
{
	const Buffer empty;
	const auto ackSeq = static_cast<unsigned char>(1 - deviceSeqNumber);
	const auto data = CommandSerializer::packageDataForBt(empty, DATA_TYPE::ACK, ackSeq);
	this->_connector->send(const_cast<char*>(data.data()), data.size());
}

int BluetoothWrapper::sendCommand(const std::vector<char>& bytes, DATA_TYPE dataType, int postDrainMs)
{
	std::lock_guard guard(this->_connectorMtx);
	this->_recvStaging.clear();
	this->_recvInMessage = false;
	this->_connector->discardPendingReceive();
	this->_connector->setBlockingRecv(true);

	auto data = CommandSerializer::packageDataForBt(bytes, dataType, _nextSendSequence());
	const auto bytesSent = this->_connector->send(data.data(), data.size());
	if (bytesSent <= 0) {
		this->_connector->setBlockingRecv(false);
		throw RecoverableException("Could not send command to headphones.", false);
	}

	try {
		this->_waitForAck();
	} catch (...) {
		this->_connector->setBlockingRecv(false);
		throw;
	}

	this->_connector->setBlockingRecv(false);
	if (postDrainMs > 0) {
		_drainIncomingMessages(std::chrono::milliseconds(postDrainMs));
	}
	this->noteCommandExchange();
	return bytesSent;
}

void BluetoothWrapper::_waitForAck()
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);

	for (int attempt = 0; attempt < 40; attempt++) {
		if (std::chrono::steady_clock::now() > deadline) {
			break;
		}

		auto msgOpt = this->_tryRecvFramedMessage();
		if (!msgOpt) {
			continue;
		}

		if (msgOpt->dataType == DATA_TYPE::ACK) {
			this->_onAckReceived(msgOpt->seqNumber);
			return;
		}

		if (msgOpt->dataType == DATA_TYPE::DATA_MDR || msgOpt->dataType == DATA_TYPE::DATA_MDR_NO2) {
			this->_sendAck(msgOpt->seqNumber);
		}
	}

	throw RecoverableException("Did not receive ACK from headphones", false);
}

void BluetoothWrapper::_drainIncomingMessages(std::chrono::milliseconds duration)
{
	const auto deadline = std::chrono::steady_clock::now() + duration;
	while (std::chrono::steady_clock::now() < deadline) {
		auto msgOpt = this->_tryRecvFramedMessage();
		if (!msgOpt) {
			continue;
		}
		if (msgOpt->dataType == DATA_TYPE::DATA_MDR || msgOpt->dataType == DATA_TYPE::DATA_MDR_NO2) {
			this->_sendAck(msgOpt->seqNumber);
		}
	}
}

std::optional<Buffer> BluetoothWrapper::sendQuery(
	const Buffer& payloadBytes,
	DATA_TYPE dataType,
	unsigned char expectedRetCode,
	int maxMessages)
{
	return sendQuery(payloadBytes, dataType, { expectedRetCode }, maxMessages);
}

std::optional<Buffer> BluetoothWrapper::sendQuery(
	const Buffer& payloadBytes,
	DATA_TYPE dataType,
	std::initializer_list<unsigned char> expectedRetCodes,
	int maxMessages)
{
	const auto deadline = std::chrono::steady_clock::now() + kQueryDeadline;

	std::lock_guard guard(this->_connectorMtx);
	this->_recvStaging.clear();
	this->_recvInMessage = false;
	this->_connector->discardPendingReceive();

	auto data = CommandSerializer::packageDataForBt(payloadBytes, dataType, _nextSendSequence());
	if (this->_connector->send(data.data(), data.size()) <= 0) {
		return std::nullopt;
	}
	this->_tryWaitForAckAfterSend();

	int consecutiveTimeouts = 0;
	int unmatchedMessages = 0;

	for (int i = 0; i < maxMessages; i++) {
		if (std::chrono::steady_clock::now() > deadline) {
			return std::nullopt;
		}

		auto msgOpt = this->_tryRecvFramedMessage();
		if (!msgOpt) {
			if (++consecutiveTimeouts >= 4) {
				return std::nullopt;
			}
			continue;
		}
		consecutiveTimeouts = 0;

		const auto& msg = *msgOpt;
		if (msg.dataType == DATA_TYPE::ACK) {
			this->_onAckReceived(msg.seqNumber);
			continue;
		}
		if (msg.dataType == DATA_TYPE::DATA_MDR || msg.dataType == DATA_TYPE::DATA_MDR_NO2) {
			this->_sendAck(msg.seqNumber);
		} else if (msg.dataType != dataType) {
			if (++unmatchedMessages > 16) {
				return std::nullopt;
			}
			continue;
		}
		if (msg.dataType != dataType || msg.payload.empty()) {
			if (++unmatchedMessages > 16) {
				return std::nullopt;
			}
			continue;
		}

		const auto first = static_cast<unsigned char>(msg.payload[0]);
		for (unsigned char code : expectedRetCodes) {
			if (first == code) {
				return msg.payload;
			}
		}

		if (++unmatchedMessages > 16) {
			return std::nullopt;
		}
	}

	return std::nullopt;
}

bool BluetoothWrapper::isConnected() noexcept
{
	return this->_connector->isConnected();
}

void BluetoothWrapper::connect(const std::string& addr)
{
	std::lock_guard guard(this->_connectorMtx);
	this->_recvStaging.clear();
	this->_recvInMessage = false;
	this->_seqNumber = 0;
	this->_connector->connect(addr);
	this->noteCommandExchange();
}

void BluetoothWrapper::disconnect() noexcept
{
	std::lock_guard guard(this->_connectorMtx);
	this->_seqNumber = 0;
	this->_recvStaging.clear();
	this->_recvInMessage = false;
	this->_connector->disconnect();
}

void BluetoothWrapper::serviceTransport() noexcept
{
	this->_connector->serviceTransport();
}

void BluetoothWrapper::prepareTransportForCommands() noexcept
{
	std::lock_guard guard(this->_connectorMtx);
	this->_seqNumber = 0;
	this->_recvStaging.clear();
	this->_recvInMessage = false;
	this->_connector->discardPendingReceive();
	this->_connector->serviceTransport();
}

void BluetoothWrapper::noteCommandExchange() noexcept
{
	this->_lastCommandExchangeEpochMs.store(steadyClockEpochMs());
}

bool BluetoothWrapper::isCommandChannelStale(std::chrono::seconds idle) const noexcept
{
	const int64_t last = this->_lastCommandExchangeEpochMs.load();
	if (last <= 0) {
		return true;
	}
	return steadyClockEpochMs() - last > idle.count() * 1000;
}

std::vector<BluetoothDevice> BluetoothWrapper::getConnectedDevices()
{
	return this->_connector->getConnectedDevices();
}

std::optional<CommandSerializer::Message> BluetoothWrapper::_tryRecvFramedMessage()
{
	int loops = 0;
	while (loops++ < kMaxRecvLoopsPerCall) {
		char buf[MAX_BLUETOOTH_MESSAGE_SIZE] = { 0 };
		const auto numRecvd = this->_connector->recv(buf, sizeof(buf));
		if (numRecvd <= 0) {
			return std::nullopt;
		}

		for (size_t i = 0; i < static_cast<size_t>(numRecvd); i++) {
			const char byte = buf[i];
			if (byte == START_MARKER) {
				if (this->_recvInMessage) {
					this->_recvStaging.clear();
					this->_recvInMessage = false;
				}
				this->_recvStaging.clear();
				this->_recvInMessage = true;
				continue;
			}

			if (!this->_recvInMessage) {
				continue;
			}

			if (byte == END_MARKER) {
				try {
					auto msg = CommandSerializer::unpackBtMessage(this->_recvStaging);
					this->_recvStaging.clear();
					this->_recvInMessage = false;
					return msg;
				} catch (const RecoverableException&) {
					this->_recvStaging.clear();
					this->_recvInMessage = false;
					return std::nullopt;
				} catch (const std::exception&) {
					this->_recvStaging.clear();
					this->_recvInMessage = false;
					return std::nullopt;
				}
			}

			this->_recvStaging.push_back(byte);
			if (this->_recvStaging.size() > kMaxRecvStagingBytes) {
				this->_recvStaging.clear();
				this->_recvInMessage = false;
				return std::nullopt;
			}
		}
	}

	return std::nullopt;
}

bool BluetoothWrapper::_tryWaitForAckAfterSend()
{
	const auto deadline = std::chrono::steady_clock::now() + kPostSendAckDeadline;

	for (int i = 0; i < 10; i++) {
		if (std::chrono::steady_clock::now() > deadline) {
			return false;
		}
		auto msgOpt = this->_tryRecvFramedMessage();
		if (!msgOpt) {
			continue;
		}
		if (msgOpt->dataType == DATA_TYPE::ACK) {
			this->_onAckReceived(msgOpt->seqNumber);
			return true;
		}
		if (msgOpt->dataType == DATA_TYPE::DATA_MDR || msgOpt->dataType == DATA_TYPE::DATA_MDR_NO2) {
			this->_sendAck(msgOpt->seqNumber);
		}
	}
	return false;
}
