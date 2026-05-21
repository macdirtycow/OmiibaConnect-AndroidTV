#pragma once

#include <cstddef>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

enum class SonyProtocolVersion {
	V1,
	V2,
	Unknown
};

enum class SonyHeadphoneModel {
	Unknown,
	WH_1000XM3,
	WH_1000XM4,
	WH_1000XM5,
	WH_1000XM6
};

struct DeviceCapabilities {
	SonyHeadphoneModel model = SonyHeadphoneModel::Unknown;
	SonyProtocolVersion protocol = SonyProtocolVersion::Unknown;
	int asmMaxLevel = 19;
	bool supportsVirtualSound = true;
	bool supportsEqualizer = true;
	bool supportsTouchSensor = true;
	bool supportsVoiceGuidance = true;
	bool usesV2AmbientSound = false;
	bool supportsWindNoiseMode = false;
};

inline std::string toUpperAscii(std::string_view value) {
	std::string out(value);
	for (char& ch : out) {
		ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
	}
	return out;
}

inline SonyHeadphoneModel detectModelFromName(std::string_view deviceName) {
	const std::string upper = toUpperAscii(deviceName);
	if (upper.find("WH-1000XM6") != std::string::npos || upper.find("WH1000XM6") != std::string::npos) {
		return SonyHeadphoneModel::WH_1000XM6;
	}
	if (upper.find("WH-1000XM5") != std::string::npos || upper.find("WH1000XM5") != std::string::npos) {
		return SonyHeadphoneModel::WH_1000XM5;
	}
	if (upper.find("WH-1000XM4") != std::string::npos || upper.find("WH1000XM4") != std::string::npos) {
		return SonyHeadphoneModel::WH_1000XM4;
	}
	if (upper.find("WH-1000XM3") != std::string::npos || upper.find("WH1000XM3") != std::string::npos) {
		return SonyHeadphoneModel::WH_1000XM3;
	}
	return SonyHeadphoneModel::Unknown;
}

inline const char* modelDisplayName(SonyHeadphoneModel model) {
	switch (model) {
	case SonyHeadphoneModel::WH_1000XM3: return "WH-1000XM3";
	case SonyHeadphoneModel::WH_1000XM4: return "WH-1000XM4";
	case SonyHeadphoneModel::WH_1000XM5: return "WH-1000XM5";
	case SonyHeadphoneModel::WH_1000XM6: return "WH-1000XM6";
	default: return "Sony headphones";
	}
}

inline SonyProtocolVersion detectProtocolFromInitPayload(std::optional<size_t> initPayloadLength) {
	// Gadgetbridge: 4-byte init reply => v1 (XM3, XM4, …), 8-byte => v2 (XM5, XM6, …).
	if (!initPayloadLength) {
		return SonyProtocolVersion::Unknown;
	}
	if (*initPayloadLength >= 8) {
		return SonyProtocolVersion::V2;
	}
	if (*initPayloadLength >= 4) {
		return SonyProtocolVersion::V1;
	}
	return SonyProtocolVersion::Unknown;
}

inline SonyProtocolVersion protocolForModel(SonyHeadphoneModel model) {
	switch (model) {
	case SonyHeadphoneModel::WH_1000XM3:
	case SonyHeadphoneModel::WH_1000XM4:
		return SonyProtocolVersion::V1;
	case SonyHeadphoneModel::WH_1000XM5:
	case SonyHeadphoneModel::WH_1000XM6:
		return SonyProtocolVersion::V2;
	default:
		return SonyProtocolVersion::Unknown;
	}
}

inline DeviceCapabilities capabilitiesFor(SonyHeadphoneModel model, SonyProtocolVersion protocol) {
	DeviceCapabilities caps;
	caps.model = model;
	caps.protocol = protocol;

	switch (model) {
	case SonyHeadphoneModel::WH_1000XM3:
	case SonyHeadphoneModel::WH_1000XM4:
		caps.asmMaxLevel = 19;
		caps.supportsVirtualSound = true;
		caps.supportsEqualizer = true;
		caps.supportsTouchSensor = true;
		caps.supportsVoiceGuidance = true;
		caps.usesV2AmbientSound = false;
		break;
	case SonyHeadphoneModel::WH_1000XM5:
		caps.asmMaxLevel = 20;
		caps.supportsVirtualSound = true;
		caps.supportsEqualizer = true;
		caps.supportsTouchSensor = false;
		caps.supportsVoiceGuidance = true;
		caps.usesV2AmbientSound = true;
		break;
	case SonyHeadphoneModel::WH_1000XM6:
		caps.asmMaxLevel = 20;
		caps.supportsVirtualSound = false;
		caps.supportsEqualizer = false;
		caps.supportsTouchSensor = false;
		caps.supportsVoiceGuidance = false;
		caps.usesV2AmbientSound = true;
		caps.supportsWindNoiseMode = true;
		break;
	default:
		if (protocol == SonyProtocolVersion::V2) {
			caps.asmMaxLevel = 20;
			caps.usesV2AmbientSound = true;
			caps.supportsTouchSensor = false;
		} else {
			caps.asmMaxLevel = 19;
		}
		break;
	}

	if (protocol == SonyProtocolVersion::V2) {
		caps.usesV2AmbientSound = true;
		if (caps.asmMaxLevel < 20) {
			caps.asmMaxLevel = 20;
		}
	}

	return caps;
}

inline DeviceCapabilities buildDeviceProfile(std::string_view deviceName, std::optional<size_t> initPayloadLength) {
	const auto model = detectModelFromName(deviceName);
	auto protocol = detectProtocolFromInitPayload(initPayloadLength);
	if (protocol == SonyProtocolVersion::Unknown) {
		protocol = protocolForModel(model);
	}
	if (model == SonyHeadphoneModel::Unknown && protocol == SonyProtocolVersion::Unknown) {
		protocol = SonyProtocolVersion::V1;
	}
	return capabilitiesFor(model, protocol);
}
