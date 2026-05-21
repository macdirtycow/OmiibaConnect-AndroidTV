#include "Headphones.h"
#include "CommandSerializer.h"
#include "ProtocolParser.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>

Headphones::Headphones(BluetoothWrapper& conn) : _conn(conn)
{
}

void Headphones::setAmbientSoundControl(bool val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_ambientSoundControl.desired = val;
}

bool Headphones::getAmbientSoundControl()
{
	return this->_ambientSoundControl.current;
}

bool Headphones::isFocusOnVoiceAvailable()
{
	return this->_ambientSoundControl.current && this->_asmLevel.current > MINIMUM_VOICE_FOCUS_STEP;
}

void Headphones::setFocusOnVoice(bool val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_focusOnVoice.desired = val;
}

bool Headphones::getFocusOnVoice()
{
	return this->_focusOnVoice.current;
}

bool Headphones::isSetAsmLevelAvailable()
{
	return this->_ambientSoundControl.current;
}

void Headphones::setAsmLevel(int val)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_asmLevel.desired = val;
}

int Headphones::getAsmLevel()
{
	return this->_asmLevel.current;
}

int Headphones::getDisplayAsmLevel() const
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_asmLevel.isFulfilled() ? this->_asmLevel.current : this->_asmLevel.desired;
}

bool Headphones::hasPendingAmbientChanges() const
{
	std::lock_guard guard(this->_propertyMtx);
	return !this->_ambientSoundControl.isFulfilled()
		|| !this->_asmLevel.isFulfilled()
		|| !this->_focusOnVoice.isFulfilled();
}

void Headphones::ensureTransportReady()
{
	this->prepareForControl();
}

void Headphones::prepareForControl()
{
	this->_conn.serviceTransport();

	const bool commandStale = this->_conn.isCommandChannelStale(std::chrono::seconds(20));
	const bool handshakeExpired = this->_handshakeComplete
		&& (std::chrono::steady_clock::now() - this->_lastHandshakeAt) > std::chrono::seconds(90);

	if (!this->_handshakeComplete || commandStale || handshakeExpired) {
		if (commandStale || !this->_handshakeComplete) {
			this->_conn.prepareTransportForCommands();
			this->invalidateConnectHandshake();
		}
		this->performConnectHandshake();
	}
}

void Headphones::setSurroundPosition(SOUND_POSITION_PRESET val)
{
	std::lock_guard guard(this->_propertyMtx);
	if (val != SOUND_POSITION_PRESET::OFF) {
		this->_vptType.desired = 0;
	}
	this->_surroundPosition.desired = val;
	this->_virtualSoundDirty = true;
}

SOUND_POSITION_PRESET Headphones::getSurroundPosition()
{
	return this->_surroundPosition.current;
}

void Headphones::setVptType(int val)
{
	std::lock_guard guard(this->_propertyMtx);
	if (val != 0) {
		this->_surroundPosition.desired = SOUND_POSITION_PRESET::OFF;
	}
	this->_vptType.desired = val;
	this->_virtualSoundDirty = true;
}

int Headphones::getVptType()
{
	return this->_vptType.current;
}

int Headphones::getDisplayVptType() const
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_vptType.isFulfilled() ? this->_vptType.current : this->_vptType.desired;
}

SOUND_POSITION_PRESET Headphones::getDisplaySurroundPosition() const
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_surroundPosition.isFulfilled() ? this->_surroundPosition.current : this->_surroundPosition.desired;
}

bool Headphones::hasPendingVirtualSoundChanges() const
{
	std::lock_guard guard(this->_propertyMtx);
	return this->_virtualSoundDirty
		|| !this->_vptType.isFulfilled()
		|| !this->_surroundPosition.isFulfilled();
}

void Headphones::setEqualizerPreset(EQ_PRESET preset)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_eqPreset.desired = preset;
	// Preset popup uses the short EQ_SET packet only; custom bands are a separate write path.
	this->_eqUseBandPayload = false;
	this->_eqBass.desired = this->_eqBass.current;
	this->_eqBands.desired = this->_eqBands.current;
	this->_eqApplyPending = true;
}

void Headphones::setEqualizerWithBands(EQ_PRESET preset, int clearBass, const std::array<int, EQ_BAND_COUNT>& bands)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_eqPreset.desired = preset;
	this->_eqBass.desired = clearBass;
	this->_eqBands.desired = bands;
	this->_eqUseBandPayload = true;
	this->_eqApplyPending = true;
}

EQ_PRESET Headphones::getEqualizerPreset() const
{
	return this->_eqPreset.current;
}

EQ_PRESET Headphones::getDisplayEqPreset() const
{
	std::lock_guard guard(this->_propertyMtx);
	if (this->_eqPreset.desired == EQ_PRESET::MANUAL) {
		return EQ_PRESET::MANUAL;
	}
	return this->_eqPreset.isFulfilled() ? this->_eqPreset.current : this->_eqPreset.desired;
}

bool Headphones::hasPendingEqChanges() const
{
	std::lock_guard guard(this->_propertyMtx);
	if (this->_eqApplyPending) {
		return true;
	}
	if (!this->_eqPreset.isFulfilled()) {
		return true;
	}
	if (this->_eqUseBandPayload && (!this->_eqBass.isFulfilled() || !this->_eqBands.isFulfilled())) {
		return true;
	}
	return false;
}

int Headphones::getDisplayEqBass() const
{
	std::lock_guard guard(this->_propertyMtx);
	const EQ_PRESET displayPreset = this->_eqPreset.isFulfilled()
		? this->_eqPreset.current
		: this->_eqPreset.desired;
	// Manual UI always follows desired — device EQ_GET can briefly report 0 for mid bands.
	if (displayPreset == EQ_PRESET::MANUAL) {
		return this->_eqBass.desired;
	}
	if (this->_eqUseBandPayload) {
		return this->_eqBass.isFulfilled() ? this->_eqBass.current : this->_eqBass.desired;
	}
	return this->_deviceStatus.eqBass;
}

std::array<int, EQ_BAND_COUNT> Headphones::getDisplayEqBands() const
{
	std::lock_guard guard(this->_propertyMtx);
	const EQ_PRESET displayPreset = this->_eqPreset.isFulfilled()
		? this->_eqPreset.current
		: this->_eqPreset.desired;
	if (displayPreset == EQ_PRESET::MANUAL) {
		return this->_eqBands.desired;
	}
	if (this->_eqUseBandPayload) {
		return this->_eqBands.isFulfilled() ? this->_eqBands.current : this->_eqBands.desired;
	}
	return this->_deviceStatus.eqBands;
}

void Headphones::setTouchSensorEnabled(bool enabled)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_touchSensorEnabled.desired = enabled;
}

bool Headphones::getTouchSensorEnabled() const
{
	return this->_touchSensorEnabled.current;
}

void Headphones::setVoiceGuidanceEnabled(bool enabled)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_voiceGuidanceEnabled.desired = enabled;
}

bool Headphones::getVoiceGuidanceEnabled() const
{
	return this->_voiceGuidanceEnabled.current;
}

const DeviceStatus& Headphones::getDeviceStatus() const
{
	return this->_deviceStatus;
}

const DeviceCapabilities& Headphones::getCapabilities() const
{
	return this->_capabilities;
}

void Headphones::configureForDevice(std::string_view deviceName)
{
	this->_deviceName.assign(deviceName);
	this->_capabilities = buildDeviceProfile(this->_deviceName, std::nullopt);
	this->_deviceStatus.modelName = this->_capabilities.model == SonyHeadphoneModel::Unknown
		? this->_deviceName
		: modelDisplayName(this->_capabilities.model);
	this->_deviceStatus.protocolLabel = this->_capabilities.usesV2AmbientSound ? "MDR v2" : "MDR v1";

	if (!this->_capabilities.supportsTouchSensor) {
		this->_touchSensorEnabled.fulfill();
	}
	if (!this->_capabilities.supportsEqualizer) {
		this->_eqPreset.fulfill();
		this->_eqBass.fulfill();
		this->_eqBands.fulfill();
	}
	if (!this->_capabilities.supportsVoiceGuidance) {
		this->_voiceGuidanceEnabled.fulfill();
	}
	if (!this->_capabilities.supportsVirtualSound) {
		this->_vptType.fulfill();
		this->_surroundPosition.fulfill();
	}
}

void Headphones::applyDeviceState(bool ambientEnabled, bool focusOnVoice, int asmLevel, bool syncDesired)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_ambientSoundControl.current = ambientEnabled;
	this->_focusOnVoice.current = focusOnVoice;
	this->_asmLevel.current = asmLevel;
	if (syncDesired) {
		this->_ambientSoundControl.desired = ambientEnabled;
		this->_focusOnVoice.desired = focusOnVoice;
		this->_asmLevel.desired = asmLevel;
		this->_ambientSoundControl.fulfill();
		this->_focusOnVoice.fulfill();
		this->_asmLevel.fulfill();
	}
}

void Headphones::updateAmbientCurrentFromDevice()
{
	if (this->_capabilities.usesV2AmbientSound) {
		return;
	}

	const Buffer ncQuery = {
		static_cast<char>(COMMAND_TYPE::NCASM_GET_PARAM),
		static_cast<char>(NC_ASM_INQUIRED_TYPE::NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE)
	};
	if (auto payload = this->_conn.sendQuery(ncQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::NCASM_RET))) {
		ProtocolParser::applyAmbientSoundControlV1(*this, *payload);
	}
}

void Headphones::applyDeviceVpt(int vptType, bool syncDesired)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_vptType.current = vptType;
	if (syncDesired) {
		this->_vptType.desired = vptType;
	}
}

void Headphones::applyDeviceSurroundPosition(SOUND_POSITION_PRESET preset, bool syncDesired)
{
	std::lock_guard guard(this->_propertyMtx);
	this->_surroundPosition.current = preset;
	if (syncDesired) {
		this->_surroundPosition.desired = preset;
	}
}

void Headphones::snapshotVirtualSoundFromDevice()
{
	if (!this->_capabilities.supportsVirtualSound) {
		return;
	}

	int vptFromDevice = 0;
	bool haveVpt = false;
	SOUND_POSITION_PRESET posFromDevice = SOUND_POSITION_PRESET::OFF;
	bool havePos = false;

	const Buffer soundQuery = {
		static_cast<char>(COMMAND_TYPE::VPT_GET_PARAM),
		0x01
	};
	if (auto payload = this->_conn.sendQuery(soundQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::SOUND_RET))) {
		if (payload->size() >= 3 && static_cast<unsigned char>((*payload)[1]) == 0x01) {
			vptFromDevice = static_cast<int>(static_cast<unsigned char>((*payload)[2]));
			haveVpt = true;
		}
	}

	const Buffer soundPosQuery = {
		static_cast<char>(COMMAND_TYPE::VPT_GET_PARAM),
		0x02
	};
	if (auto payload = this->_conn.sendQuery(soundPosQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::SOUND_RET))) {
		if (payload->size() >= 3 && static_cast<unsigned char>((*payload)[1]) == 0x02) {
			posFromDevice = static_cast<SOUND_POSITION_PRESET>((*payload)[2]);
			havePos = true;
		}
	}

	std::lock_guard guard(this->_propertyMtx);
	if (haveVpt && vptFromDevice != 0) {
		this->_vptType.current = vptFromDevice;
		this->_surroundPosition.current = SOUND_POSITION_PRESET::OFF;
	} else {
		this->_vptType.current = 0;
		this->_surroundPosition.current = havePos ? posFromDevice : SOUND_POSITION_PRESET::OFF;
	}
}

void Headphones::updateVirtualSoundCurrentFromDevice(bool syncDesired)
{
	if (!this->_capabilities.supportsVirtualSound) {
		return;
	}

	int vptFromDevice = 0;
	bool haveVpt = false;
	SOUND_POSITION_PRESET posFromDevice = SOUND_POSITION_PRESET::OFF;
	bool havePos = false;

	const Buffer soundQuery = {
		static_cast<char>(COMMAND_TYPE::VPT_GET_PARAM),
		0x01
	};
	if (auto payload = this->_conn.sendQuery(soundQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::SOUND_RET))) {
		if (payload->size() >= 3 && static_cast<unsigned char>((*payload)[1]) == 0x01) {
			vptFromDevice = static_cast<int>(static_cast<unsigned char>((*payload)[2]));
			haveVpt = true;
		}
	}

	const Buffer soundPosQuery = {
		static_cast<char>(COMMAND_TYPE::VPT_GET_PARAM),
		0x02
	};
	if (auto payload = this->_conn.sendQuery(soundPosQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::SOUND_RET))) {
		if (payload->size() >= 3 && static_cast<unsigned char>((*payload)[1]) == 0x02) {
			posFromDevice = static_cast<SOUND_POSITION_PRESET>((*payload)[2]);
			havePos = true;
		}
	}

	std::lock_guard guard(this->_propertyMtx);
	const int desiredVpt = this->_vptType.desired;
	const SOUND_POSITION_PRESET desiredPos = this->_surroundPosition.desired;
	const bool userWantsSurround = desiredVpt != 0;
	const bool userWantsPosition = desiredVpt == 0 && desiredPos != SOUND_POSITION_PRESET::OFF;
	const bool deviceSurround = haveVpt && vptFromDevice != 0;

	if (userWantsSurround && deviceSurround) {
		this->_vptType.current = vptFromDevice;
		this->_surroundPosition.current = SOUND_POSITION_PRESET::OFF;
	} else if (userWantsPosition) {
		this->_vptType.current = 0;
		this->_surroundPosition.current = havePos ? posFromDevice : desiredPos;
	} else {
		this->_vptType.current = haveVpt ? vptFromDevice : 0;
		this->_surroundPosition.current = SOUND_POSITION_PRESET::OFF;
	}

	if (syncDesired) {
		this->_vptType.desired = this->_vptType.current;
		this->_surroundPosition.desired = this->_surroundPosition.current;
	}
}

void Headphones::invalidateConnectHandshake()
{
	this->_handshakeComplete = false;
}

bool Headphones::ensureConnectHandshake()
{
	if (this->_handshakeComplete) {
		const auto elapsed = std::chrono::steady_clock::now() - this->_lastHandshakeAt;
		if (elapsed < std::chrono::seconds(120)) {
			return true;
		}
	}
	return this->performConnectHandshake();
}

bool Headphones::performConnectHandshake()
{
	// Sony | Sound Connect always sends CONNECT_GET_PROTOCOL_INFO first; without it
	// many GET commands return nothing (mdr-protocol / APK: tandemfamily.message.mdr).
	// Gadgetbridge init: { 0x00, 0x00 } — required before other queries respond.
	const Buffer protocolInfo = {
		static_cast<char>(PAYLOAD_CMD::CONNECT_GET_PROTOCOL_INFO),
		0x00
	};
	std::optional<size_t> initPayloadLength;
	if (auto payload = this->_conn.sendQuery(
		protocolInfo,
		DATA_TYPE::DATA_MDR,
		static_cast<unsigned char>(PAYLOAD_CMD::CONNECT_RET_PROTOCOL_INFO))) {
		initPayloadLength = payload->size();
	}

	// APK 9.5: CONNECT_GET_CAPABILITY_INFO + CommonCapabilityInquiredType::FIXED_VALUE (0x00).
	const Buffer capabilityInfo = {
		static_cast<char>(PAYLOAD_CMD::CONNECT_GET_CAPABILITY_INFO),
		0x00
	};
	this->_conn.sendQuery(
		capabilityInfo,
		DATA_TYPE::DATA_MDR,
		static_cast<unsigned char>(PAYLOAD_CMD::CONNECT_RET_CAPABILITY_INFO));

	this->_capabilities = buildDeviceProfile(this->_deviceName, initPayloadLength);
	this->_deviceStatus.modelName = this->_capabilities.model == SonyHeadphoneModel::Unknown
		? this->_deviceName
		: modelDisplayName(this->_capabilities.model);
	this->_deviceStatus.protocolLabel = this->_capabilities.usesV2AmbientSound ? "MDR v2" : "MDR v1";

	const Buffer supportFn = { static_cast<char>(PAYLOAD_CMD::CONNECT_GET_SUPPORT_FUNCTION) };
	this->_conn.sendQuery(
		supportFn,
		DATA_TYPE::DATA_MDR,
		static_cast<unsigned char>(PAYLOAD_CMD::CONNECT_RET_SUPPORT_FUNCTION)
	);

	this->_handshakeComplete = true;
	this->_lastHandshakeAt = std::chrono::steady_clock::now();
	return true;
}

bool Headphones::probeConnection()
{
	if (!this->_conn.isConnected()) {
		return false;
	}

	const Buffer batteryQuery = {
		static_cast<char>(PAYLOAD_CMD::BATTERY_REQUEST),
		0x00
	};
	const auto payload = this->_conn.sendQuery(
		batteryQuery,
		DATA_TYPE::DATA_MDR,
		{
			static_cast<unsigned char>(PAYLOAD_CMD::BATTERY_RET),
			static_cast<unsigned char>(PAYLOAD_CMD::BATTERY_NTFY)
		},
		8);
	return payload.has_value();
}

bool Headphones::refreshFromDevice(bool includeExtendedSettings)
{
	if (!this->_conn.isConnected()) {
		return false;
	}

	performConnectHandshake();

	DeviceStatus nextStatus = this->_deviceStatus;

	if (this->_capabilities.usesV2AmbientSound) {
		const unsigned char variant = this->_capabilities.supportsWindNoiseMode ? 0x17 : 0x15;
		const Buffer ncQuery = {
			static_cast<char>(PAYLOAD_CMD::NCASM_GET),
			static_cast<char>(variant)
		};
		if (auto payload = this->_conn.sendQuery(ncQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::NCASM_RET))) {
			if (!ProtocolParser::applyAmbientSoundControlV2(*this, *payload, true)) {
				ProtocolParser::applyAmbientSoundControl(*this, *payload, true);
			}
		}
	} else {
		const Buffer ncQuery = {
			static_cast<char>(COMMAND_TYPE::NCASM_GET_PARAM),
			static_cast<char>(NC_ASM_INQUIRED_TYPE::NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE)
		};
		if (auto payload = this->_conn.sendQuery(ncQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::NCASM_RET))) {
			if (!ProtocolParser::applyAmbientSoundControlV1(*this, *payload, true)) {
				ProtocolParser::applyAmbientSoundControl(*this, *payload, true);
			}
		}
	}

	const Buffer batteryQuery = {
		static_cast<char>(PAYLOAD_CMD::BATTERY_REQUEST),
		0x00
	};
	if (auto payload = this->_conn.sendQuery(
		batteryQuery,
		DATA_TYPE::DATA_MDR,
		{
			static_cast<unsigned char>(PAYLOAD_CMD::BATTERY_RET),
			static_cast<unsigned char>(PAYLOAD_CMD::BATTERY_NTFY)
		})) {
		if (auto level = ProtocolParser::parseBatteryPercent(*payload)) {
			nextStatus.batteryPercent = *level;
			nextStatus.hasBattery = true;
		}
	}

	const Buffer codecQuery = {
		static_cast<char>(PAYLOAD_CMD::AUDIO_CODEC_REQUEST),
		0x00
	};
	if (auto payload = this->_conn.sendQuery(
		codecQuery,
		DATA_TYPE::DATA_MDR,
		{
			static_cast<unsigned char>(PAYLOAD_CMD::AUDIO_CODEC_RET),
			static_cast<unsigned char>(PAYLOAD_CMD::AUDIO_CODEC_NTFY)
		})) {
		if (auto codec = ProtocolParser::parseAudioCodec(*payload)) {
			nextStatus.audioCodec = *codec;
			nextStatus.hasCodec = true;
		}
	}

	const Buffer fwQuery = {
		static_cast<char>(PAYLOAD_CMD::CONNECT_GET_DEVICE_INFO),
		static_cast<char>(DEVICE_INFO_TYPE::FW_VERSION)
	};
	if (auto payload = this->_conn.sendQuery(fwQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::FW_VERSION_RET))) {
		if (auto fw = ProtocolParser::parseFirmwareVersion(*payload)) {
			nextStatus.firmwareVersion = *fw;
			nextStatus.hasFirmware = true;
		}
	}

	if (!includeExtendedSettings) {
		nextStatus.modelName = this->_deviceStatus.modelName;
		nextStatus.protocolLabel = this->_deviceStatus.protocolLabel;
		this->_deviceStatus = nextStatus;
		return nextStatus.hasBattery || nextStatus.hasCodec || nextStatus.hasFirmware;
	}

	if (this->_capabilities.supportsEqualizer) {
		const Buffer eqQuery = {
			static_cast<char>(PAYLOAD_CMD::EQ_GET),
			0x01
		};
		if (auto payload = this->_conn.sendQuery(eqQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::EQ_RET))) {
			ProtocolParser::applyEqualizer(nextStatus, *payload);
			if (nextStatus.hasEqualizer) {
				DeviceStatus eqStatus = nextStatus;
				this->applyEqReadback(eqStatus);
			}
		}
	}

	if (this->_capabilities.supportsVirtualSound) {
		this->updateVirtualSoundCurrentFromDevice(true);
		std::lock_guard guard(this->_propertyMtx);
		this->_vptType.fulfill();
		this->_surroundPosition.fulfill();
		this->_virtualSoundDirty = false;
	}

	if (this->_capabilities.supportsTouchSensor) {
		const Buffer touchQuery = {
			static_cast<char>(PAYLOAD_CMD::TOUCH_GET),
			static_cast<char>(GS_INQUIRED_TYPE::GENERAL_SETTING2)
		};
		if (auto payload = this->_conn.sendQuery(touchQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::TOUCH_RET))) {
			ProtocolParser::applyTouchSensor(nextStatus, *payload);
			if (nextStatus.hasTouchSensor) {
				std::lock_guard guard(this->_propertyMtx);
				this->_touchSensorEnabled.current = nextStatus.touchSensorEnabled;
				this->_touchSensorEnabled.desired = this->_touchSensorEnabled.current;
			}
		}
	}

	if (this->_capabilities.supportsVoiceGuidance) {
		const Buffer voiceQuery = {
			static_cast<char>(VOICE_GUIDANCE_CMD::GET_PARAM),
			static_cast<char>(VOICE_GUIDANCE_INQUIRED::VOICE_GUIDANCE_SETTING),
			0x01
		};
		if (auto payload = this->_conn.sendQuery(voiceQuery, DATA_TYPE::DATA_MDR_NO2, static_cast<unsigned char>(VOICE_GUIDANCE_CMD::RET_PARAM))) {
			ProtocolParser::applyVoiceGuidance(nextStatus, *payload);
			if (nextStatus.hasVoiceGuidance) {
				std::lock_guard guard(this->_propertyMtx);
				this->_voiceGuidanceEnabled.current = nextStatus.voiceGuidanceEnabled;
				this->_voiceGuidanceEnabled.desired = this->_voiceGuidanceEnabled.current;
			}
		}
	}

	nextStatus.modelName = this->_deviceStatus.modelName;
	nextStatus.protocolLabel = this->_deviceStatus.protocolLabel;
	this->_deviceStatus = nextStatus;
	return nextStatus.hasBattery || nextStatus.hasCodec || nextStatus.hasEqualizer;
}

bool Headphones::hasAnyPendingChanges() const
{
	if (this->hasPendingAmbientChanges() || this->hasPendingVirtualSoundChanges() || this->hasPendingEqChanges()) {
		return true;
	}
	if (this->_capabilities.supportsTouchSensor && !this->_touchSensorEnabled.isFulfilled()) {
		return true;
	}
	if (this->_capabilities.supportsVoiceGuidance && !this->_voiceGuidanceEnabled.isFulfilled()) {
		return true;
	}
	return false;
}

bool Headphones::isChanged()
{
	return this->hasAnyPendingChanges();
}

void Headphones::applyEqReadback(const DeviceStatus& eqStatus)
{
	if (!eqStatus.hasEqualizer) {
		return;
	}

	std::lock_guard guard(this->_propertyMtx);
	const bool manualMode = this->_eqPreset.desired == EQ_PRESET::MANUAL;
	this->_eqPreset.current = static_cast<EQ_PRESET>(eqStatus.eqPresetCode);
	this->_deviceStatus.eqBass = eqStatus.eqBass;
	this->_deviceStatus.eqBands = eqStatus.eqBands;
	this->_deviceStatus.eqPresetCode = eqStatus.eqPresetCode;
	this->_deviceStatus.hasEqualizer = true;

	if (!manualMode) {
		this->_eqPreset.desired = this->_eqPreset.current;
		this->_eqBass.current = eqStatus.eqBass;
		this->_eqBass.desired = eqStatus.eqBass;
		this->_eqBands.current = eqStatus.eqBands;
		this->_eqBands.desired = eqStatus.eqBands;
		this->_eqUseBandPayload = false;
	} else {
		this->_eqUseBandPayload = true;
	}
}

bool Headphones::primeManualEqFromDevice()
{
	if (!this->_capabilities.supportsEqualizer) {
		return false;
	}

	const Buffer eqQuery = {
		static_cast<char>(PAYLOAD_CMD::EQ_GET),
		0x01
	};
	DeviceStatus status = this->_deviceStatus;
	const auto payload = this->_conn.sendQuery(eqQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::EQ_RET));
	if (!payload || !ProtocolParser::applyEqualizer(status, *payload)) {
		return false;
	}

	{
		std::lock_guard guard(this->_propertyMtx);
		this->_eqPreset.desired = EQ_PRESET::MANUAL;
		this->_eqPreset.current = EQ_PRESET::MANUAL;
		this->_eqBass.current = status.eqBass;
		this->_eqBass.desired = status.eqBass;
		this->_eqBands.current = status.eqBands;
		this->_eqBands.desired = status.eqBands;
		this->_eqUseBandPayload = true;
		this->_eqApplyPending = false;
		this->_eqPreset.fulfill();
		this->_eqBass.fulfill();
		this->_eqBands.fulfill();
		this->_deviceStatus.eqBass = status.eqBass;
		this->_deviceStatus.eqBands = status.eqBands;
		this->_deviceStatus.eqPresetCode = status.eqPresetCode;
		this->_deviceStatus.hasEqualizer = true;
	}
	return true;
}

void Headphones::updateEqCurrentFromDevice()
{
	if (!this->_capabilities.supportsEqualizer) {
		return;
	}

	const Buffer eqQuery = {
		static_cast<char>(PAYLOAD_CMD::EQ_GET),
		0x01
	};
	DeviceStatus status = this->_deviceStatus;
	if (auto payload = this->_conn.sendQuery(eqQuery, DATA_TYPE::DATA_MDR, static_cast<unsigned char>(PAYLOAD_CMD::EQ_RET))) {
		if (ProtocolParser::applyEqualizer(status, *payload)) {
			this->applyEqReadback(status);
		}
	}
}

void Headphones::sendEqChanges()
{
	EQ_PRESET preset = EQ_PRESET::OFF;
	int bass = 0;
	std::array<int, EQ_BAND_COUNT> bands{};
	bool useBands = false;
	{
		std::lock_guard guard(this->_propertyMtx);
		preset = this->_eqPreset.desired;
		bass = this->_eqBass.desired;
		bands = this->_eqBands.desired;
		useBands = this->_eqUseBandPayload;
	}

	constexpr int kEqPostDrainMs = 50;

	if (useBands) {
		// XM3: enter Manual once, then push band steps (UNSPECIFIED = band-only update on wire).
		EQ_PRESET currentPreset = EQ_PRESET::OFF;
		{
			std::lock_guard guard(this->_propertyMtx);
			currentPreset = this->_eqPreset.current;
		}
		if (currentPreset != EQ_PRESET::MANUAL) {
			this->_conn.sendCommand(CommandSerializer::serializeEqualizerPreset(EQ_PRESET::MANUAL), DATA_TYPE::DATA_MDR, kEqPostDrainMs);
			std::this_thread::sleep_for(std::chrono::milliseconds(35));
		}
		this->_conn.sendCommand(
			CommandSerializer::serializeEqualizerWithBands(EQ_PRESET::UNSPECIFIED, bass, bands),
			DATA_TYPE::DATA_MDR,
			kEqPostDrainMs);
		{
			std::lock_guard guard(this->_propertyMtx);
			this->_eqPreset.desired = EQ_PRESET::MANUAL;
			this->_eqPreset.current = EQ_PRESET::MANUAL;
			this->_eqBass.current = bass;
			this->_eqBass.desired = bass;
			this->_eqBands.current = bands;
			this->_eqBands.desired = bands;
			this->_deviceStatus.eqBass = bass;
			this->_deviceStatus.eqBands = bands;
			this->_deviceStatus.eqPresetCode = static_cast<int>(EQ_PRESET::MANUAL);
			this->_deviceStatus.hasEqualizer = true;
			this->_eqPreset.fulfill();
			this->_eqBass.fulfill();
			this->_eqBands.fulfill();
			this->_eqApplyPending = false;
		}
		// Do not EQ_GET immediately after band write — XM3 read-back often zeros mid bands and flickers the UI.
	} else {
		this->_conn.sendCommand(CommandSerializer::serializeEqualizerPreset(preset), DATA_TYPE::DATA_MDR, kEqPostDrainMs);
		std::lock_guard guard(this->_propertyMtx);
		this->_eqPreset.current = preset;
		this->_eqBass.fulfill();
		this->_eqBands.fulfill();
		this->_eqPreset.fulfill();
		this->_eqApplyPending = false;
		this->_deviceStatus.eqPresetCode = static_cast<int>(preset);
		this->_deviceStatus.hasEqualizer = true;
	}
}

void Headphones::sendAmbientV1Changes()
{
	bool ambientDesired = false;
	bool focusDesired = false;
	int levelDesired = 0;
	{
		std::lock_guard guard(this->_propertyMtx);
		ambientDesired = this->_ambientSoundControl.desired;
		focusDesired = this->_focusOnVoice.desired;
		levelDesired = ambientDesired
			? std::max(0, std::min(this->_asmLevel.desired, this->_capabilities.asmMaxLevel))
			: 0;
	}

	const ASM_ID asmId = focusDesired ? ASM_ID::VOICE : ASM_ID::NORMAL;
	const int level = levelDesired;

	auto sendAmbientV1 = [&](NC_ASM_EFFECT effect) {
		this->_conn.sendCommand(CommandSerializer::serializeNcAndAsmAmbientLevelV1(effect, asmId, level));
	};

	if (ambientDesired) {
		sendAmbientV1(NC_ASM_EFFECT::ADJUSTMENT_IN_PROGRESS);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		sendAmbientV1(NC_ASM_EFFECT::ADJUSTMENT_COMPLETION);
	} else {
		sendAmbientV1(NC_ASM_EFFECT::OFF);
	}

	this->updateAmbientCurrentFromDevice();
	std::lock_guard guard(this->_propertyMtx);
	const bool ambientOk = this->_ambientSoundControl.current == this->_ambientSoundControl.desired;
	const bool levelOk = !this->_ambientSoundControl.desired
		|| this->_asmLevel.current == this->_asmLevel.desired;
	const bool voiceOk = this->_focusOnVoice.current == this->_focusOnVoice.desired;
	if (ambientOk && levelOk && voiceOk) {
		this->_ambientSoundControl.fulfill();
		this->_asmLevel.fulfill();
		this->_focusOnVoice.fulfill();
	}
}

void Headphones::setAmbientChangesIfNeeded()
{
	if (this->_ambientSoundControl.isFulfilled() && this->_focusOnVoice.isFulfilled() && this->_asmLevel.isFulfilled()) {
		return;
	}

	const char asmLevel = this->_ambientSoundControl.desired
		? static_cast<char>(std::min(this->_asmLevel.desired, this->_capabilities.asmMaxLevel))
		: static_cast<char>(ASM_LEVEL_DISABLED);

	if (this->_capabilities.usesV2AmbientSound) {
		this->_conn.sendCommand(CommandSerializer::serializeAmbientSoundControlV2(
			this->_ambientSoundControl.desired,
			this->_focusOnVoice.desired,
			asmLevel,
			this->_capabilities.supportsWindNoiseMode
		));
		std::lock_guard guard(this->_propertyMtx);
		this->_ambientSoundControl.fulfill();
		this->_asmLevel.fulfill();
		this->_focusOnVoice.fulfill();
	} else {
		sendAmbientV1Changes();
	}
}

void Headphones::resyncAmbientAfterVirtualSoundIfNeeded()
{
	bool ambientOn = false;
	{
		std::lock_guard guard(this->_propertyMtx);
		ambientOn = this->_ambientSoundControl.desired;
	}
	if (this->_capabilities.usesV2AmbientSound || !ambientOn) {
		return;
	}

	this->updateAmbientCurrentFromDevice();
	if (!this->_asmLevel.isFulfilled() || !this->_ambientSoundControl.isFulfilled()) {
		sendAmbientV1Changes();
	}
}

void Headphones::setEqChangesIfNeeded()
{
	if (this->_capabilities.supportsEqualizer && this->hasPendingEqChanges()) {
		this->sendEqChanges();
	}
}

void Headphones::setVirtualSoundChangesIfNeeded()
{
	if (!this->_capabilities.supportsVirtualSound) {
		return;
	}

	{
		std::lock_guard guard(this->_propertyMtx);
		if (!this->_virtualSoundDirty
			&& this->_vptType.isFulfilled()
			&& this->_surroundPosition.isFulfilled()) {
			return;
		}
	}

	int desiredVpt = 0;
	int currentVpt = 0;
	SOUND_POSITION_PRESET desiredPosition = SOUND_POSITION_PRESET::OFF;
	SOUND_POSITION_PRESET currentPosition = SOUND_POSITION_PRESET::OFF;
	{
		std::lock_guard guard(this->_propertyMtx);
		desiredVpt = this->_vptType.desired;
		currentVpt = this->_vptType.current;
		desiredPosition = this->_surroundPosition.desired;
		currentPosition = this->_surroundPosition.current;
	}

	const bool wantVpt = desiredVpt != 0;
	const bool wantPosition = desiredPosition != SOUND_POSITION_PRESET::OFF;
	bool appliedSurround = false;
	bool appliedPosition = false;

	constexpr int kFastDrainMs = 40;
	constexpr int kSlowDrainMs = 120;

	auto sendVpt = [&](unsigned char preset, int drainMs) {
		this->_conn.sendCommand(
			CommandSerializer::serializeVPTSetting(VPT_INQUIRED_TYPE::VPT, preset),
			DATA_TYPE::DATA_MDR,
			drainMs);
	};
	auto sendSoundPosition = [&](SOUND_POSITION_PRESET preset, int drainMs) {
		this->_conn.sendCommand(
			CommandSerializer::serializeVPTSetting(
				VPT_INQUIRED_TYPE::SOUND_POSITION,
				static_cast<unsigned char>(preset)),
			DATA_TYPE::DATA_MDR,
			drainMs);
	};
	const auto pauseMs = [](int ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	};
	auto commitVirtualSound = [&](bool surround, bool position) {
		std::lock_guard guard(this->_propertyMtx);
		if (surround) {
			this->_vptType.current = desiredVpt;
			this->_surroundPosition.current = SOUND_POSITION_PRESET::OFF;
		} else if (position) {
			this->_vptType.current = 0;
			this->_surroundPosition.current = desiredPosition;
		} else if (!wantVpt && !wantPosition) {
			this->_vptType.current = 0;
			this->_surroundPosition.current = SOUND_POSITION_PRESET::OFF;
		}
		if (this->_vptType.current == this->_vptType.desired
			&& this->_surroundPosition.current == this->_surroundPosition.desired) {
			this->_vptType.fulfill();
			this->_surroundPosition.fulfill();
			this->_virtualSoundDirty = false;
		}
	};

	// Always read the device first — cached Property can say VPT=0 while XM3 is still in surround.
	this->snapshotVirtualSoundFromDevice();
	{
		std::lock_guard guard(this->_propertyMtx);
		currentVpt = this->_vptType.current;
		currentPosition = this->_surroundPosition.current;
	}

	bool virtualSoundApplied = false;
	if (wantPosition && currentVpt == 0) {
		const bool positionToPosition = currentPosition != SOUND_POSITION_PRESET::OFF
			&& desiredPosition != SOUND_POSITION_PRESET::OFF
			&& currentPosition != desiredPosition;
		if (positionToPosition) {
			sendSoundPosition(desiredPosition, kFastDrainMs);
		} else if (currentPosition != SOUND_POSITION_PRESET::OFF && currentPosition != desiredPosition) {
			sendSoundPosition(SOUND_POSITION_PRESET::OFF, kFastDrainMs);
			pauseMs(70);
			sendSoundPosition(desiredPosition, kFastDrainMs);
		} else {
			sendSoundPosition(desiredPosition, kFastDrainMs);
		}
		pauseMs(80);
		this->snapshotVirtualSoundFromDevice();
		{
			std::lock_guard guard(this->_propertyMtx);
			currentVpt = this->_vptType.current;
			currentPosition = this->_surroundPosition.current;
		}
		if (currentVpt == 0 && currentPosition == desiredPosition) {
			commitVirtualSound(false, true);
			virtualSoundApplied = true;
		}
	}

	if (!virtualSoundApplied && wantVpt) {
		if (currentPosition != SOUND_POSITION_PRESET::OFF) {
			sendSoundPosition(SOUND_POSITION_PRESET::OFF, kSlowDrainMs);
			pauseMs(80);
		}
		if (currentVpt != 0) {
			sendVpt(0, kSlowDrainMs);
			pauseMs(100);
		}
		sendVpt(static_cast<unsigned char>(desiredVpt), kSlowDrainMs);
		appliedSurround = true;
		virtualSoundApplied = true;
	} else if (!virtualSoundApplied && wantPosition) {
		sendVpt(0, kSlowDrainMs);
		pauseMs(100);
		sendSoundPosition(SOUND_POSITION_PRESET::OFF, kSlowDrainMs);
		pauseMs(80);
		sendSoundPosition(desiredPosition, kSlowDrainMs);
		appliedPosition = true;
		virtualSoundApplied = true;
		pauseMs(120);
	} else if (!virtualSoundApplied) {
		if (currentPosition != SOUND_POSITION_PRESET::OFF) {
			sendSoundPosition(SOUND_POSITION_PRESET::OFF, kSlowDrainMs);
			pauseMs(80);
		}
		if (currentVpt != 0) {
			sendVpt(0, kSlowDrainMs);
			pauseMs(80);
		}
	}

	if (virtualSoundApplied) {
		if (!wantVpt && !wantPosition) {
			pauseMs(120);
		} else if (appliedSurround || appliedPosition) {
			pauseMs(80);
		}
		this->updateVirtualSoundCurrentFromDevice();
		commitVirtualSound(appliedSurround, appliedPosition);
	}
}

void Headphones::setTouchAndVoiceChangesIfNeeded()
{
	if (this->_capabilities.supportsTouchSensor && !this->_touchSensorEnabled.isFulfilled()) {
		this->_conn.sendCommand(CommandSerializer::serializeTouchSensor(this->_touchSensorEnabled.desired));
		std::lock_guard guard(this->_propertyMtx);
		this->_touchSensorEnabled.fulfill();
	}

	if (this->_capabilities.supportsVoiceGuidance && !this->_voiceGuidanceEnabled.isFulfilled()) {
		this->_conn.sendCommand(
			CommandSerializer::serializeVoiceGuidance(this->_voiceGuidanceEnabled.desired),
			DATA_TYPE::DATA_MDR_NO2);
		std::lock_guard guard(this->_propertyMtx);
		this->_voiceGuidanceEnabled.fulfill();
	}
}

void Headphones::setChanges()
{
	this->setAmbientChangesIfNeeded();
	this->setVirtualSoundChangesIfNeeded();
	this->resyncAmbientAfterVirtualSoundIfNeeded();
	this->setEqChangesIfNeeded();
	this->setTouchAndVoiceChangesIfNeeded();
}
