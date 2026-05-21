#include "ProtocolParser.h"
#include "Headphones.h"

#include <algorithm>
#include <sstream>

namespace ProtocolParser {
	std::optional<int> parseBatteryPercent(const Buffer& payload) {
		if (payload.size() < 3) {
			return std::nullopt;
		}

		const auto batteryType = static_cast<unsigned char>(payload[1]);
		// 0x00 = single (WH-1000XM3), 0x01/0x03 = dual L/R, 0x02 = case
		if (batteryType == 0x00 || batteryType == 0x02) {
			return static_cast<int>(static_cast<unsigned char>(payload[2]));
		}

		if ((batteryType == 0x01 || batteryType == 0x03) && payload.size() >= 5) {
			const int left = static_cast<unsigned char>(payload[2]);
			const int right = static_cast<unsigned char>(payload[4]);
			return std::max(left, right);
		}

		return std::nullopt;
	}

	std::optional<std::string> parseAudioCodec(const Buffer& payload) {
		if (payload.size() < 3 || payload[1] != 0x00) {
			return std::nullopt;
		}

		// com.sony.songpal.tandemfamily.message.mdr.v1.table1.param.AudioCodec (APK 9.5)
		switch (static_cast<unsigned char>(payload[2])) {
		case 0x00: return "Unsettled";
		case 0x01: return "SBC";
		case 0x02: return "AAC";
		case 0x10: return "LDAC";
		case 0x20: return "aptX";
		case 0x21: return "aptX HD";
		default: {
			std::ostringstream ss;
			ss << "Unknown (0x" << std::hex << static_cast<int>(static_cast<unsigned char>(payload[2])) << ")";
			return ss.str();
		}
		}
	}

	std::optional<std::string> parseFirmwareVersion(const Buffer& payload) {
		if (payload.size() < 4 || payload[1] != 0x02) {
			return std::nullopt;
		}

		const auto expectedLength = static_cast<unsigned char>(payload[2]);
		if (payload.size() < 3 + expectedLength) {
			return std::nullopt;
		}

		return std::string(payload.begin() + 3, payload.begin() + 3 + expectedLength);
	}

	bool applyAmbientSoundControlV1(Headphones& headphones, const Buffer& payload, bool syncDesired) {
		// NCASM_RET: 67 02 + 6 data bytes (Plutoberth layout or SongPal zk/b0).
		if (payload.size() < 8 || static_cast<unsigned char>(payload[1]) != 0x02) {
			return false;
		}

		const unsigned char ncAsmSetting = static_cast<unsigned char>(payload[3]);
		const unsigned char dualSingle = static_cast<unsigned char>(payload[4]);
		const unsigned char asmSetting = static_cast<unsigned char>(payload[5]);
		const unsigned char asmId = static_cast<unsigned char>(payload[6]);
		const unsigned char levelByte = static_cast<unsigned char>(payload[7]);

		const bool focusOnVoice = asmId == static_cast<unsigned char>(ASM_ID::VOICE);

		if (dualSingle == static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::DUAL)
			|| dualSingle == static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::SINGLE)) {
			headphones.applyDeviceState(false, focusOnVoice, 0, syncDesired);
			return true;
		}

		if (ncAsmSetting == static_cast<unsigned char>(NC_ASM_SETTING_TYPE::LEVEL_ADJUSTMENT)
			|| (ncAsmSetting == static_cast<unsigned char>(NC_ASM_SETTING_TYPE::DUAL_SINGLE_OFF)
				&& asmSetting == static_cast<unsigned char>(ASM_SETTING_TYPE::LEVEL_ADJUSTMENT))) {
			const int level = static_cast<int>(levelByte);
			if (level < 0) {
				headphones.applyDeviceState(false, focusOnVoice, 0, syncDesired);
			} else {
				headphones.applyDeviceState(true, focusOnVoice, level, syncDesired);
			}
			return true;
		}

		return false;
	}

	bool applyAmbientSoundControl(Headphones& headphones, const Buffer& payload, bool syncDesired) {
		if (payload.size() != 8) {
			return false;
		}

		bool ambientEnabled = false;
		int asmLevel = 0;
		bool focusOnVoice = payload[6] == 0x01;

		if (payload[2] == 0x00) {
			ambientEnabled = false;
			asmLevel = 0;
		}
		else if (payload[3] == 0x00 || payload[3] == 0x01) {
			if (payload[4] == static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::DUAL)
				|| payload[4] == static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::SINGLE)) {
				ambientEnabled = false;
				asmLevel = 0;
			}
			else if (payload[4] == static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::OFF)) {
				ambientEnabled = true;
				asmLevel = static_cast<unsigned char>(payload[7]);
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}

		headphones.applyDeviceState(ambientEnabled, focusOnVoice, asmLevel, syncDesired);
		return true;
	}

	bool applyAmbientSoundControlV2(Headphones& headphones, const Buffer& payload, bool syncDesired) {
		if (payload.size() < 7) {
			return false;
		}
		if (payload[1] != 0x15 && payload[1] != 0x17) {
			return false;
		}

		const bool includesWindNoise = payload[1] == 0x17 && payload.size() > 7;
		bool ambientEnabled = false;
		int asmLevel = 0;

		if (payload[3] == 0x01 && payload[4] == 0x01) {
			ambientEnabled = true;
		}

		const int focusIndex = includesWindNoise ? 6 : 5;
		const int levelIndex = focusIndex + 1;
		if (payload.size() <= static_cast<size_t>(levelIndex)) {
			return false;
		}

		const bool focusOnVoice = payload[focusIndex] == 0x01;
		if (ambientEnabled) {
			asmLevel = static_cast<unsigned char>(payload[levelIndex]);
		}

		headphones.applyDeviceState(ambientEnabled, focusOnVoice, asmLevel, syncDesired);
		return true;
	}

	bool applyVirtualSound(Headphones& headphones, const Buffer& payload, bool syncDesired) {
		if (payload.size() != 3) {
			return false;
		}

		if (payload[1] == 0x01) {
			headphones.applyDeviceVpt(static_cast<int>(static_cast<unsigned char>(payload[2])), syncDesired);
			headphones.applyDeviceSurroundPosition(SOUND_POSITION_PRESET::OFF, syncDesired);
			return true;
		}

		if (payload[1] == 0x02) {
			headphones.applyDeviceVpt(0, syncDesired);
			headphones.applyDeviceSurroundPosition(static_cast<SOUND_POSITION_PRESET>(payload[2]), syncDesired);
			return true;
		}

		return false;
	}

	bool applyEqualizer(DeviceStatus& status, const Buffer& payload) {
		if (payload.size() < 10) {
			return false;
		}

		status.eqPresetCode = static_cast<int>(static_cast<unsigned char>(payload[2]));
		status.eqBass = static_cast<int>(static_cast<unsigned char>(payload[4])) - 10;
		for (int i = 0; i < 5; i++) {
			status.eqBands[i] = static_cast<int>(static_cast<unsigned char>(payload[5 + i])) - 10;
		}
		status.hasEqualizer = true;
		return true;
	}

	bool applyTouchSensor(DeviceStatus& status, const Buffer& payload) {
		if (payload.size() != 4) {
			return false;
		}

		if (payload[3] == 0x00) {
			status.touchSensorEnabled = false;
		}
		else if (payload[3] == 0x01) {
			status.touchSensorEnabled = true;
		}
		else {
			return false;
		}

		status.hasTouchSensor = true;
		return true;
	}

	bool applyVoiceGuidance(DeviceStatus& status, const Buffer& payload) {
		if (payload.size() != 4) {
			return false;
		}

		if (payload[3] == 0x00) {
			status.voiceGuidanceEnabled = false;
		}
		else if (payload[3] == 0x01) {
			status.voiceGuidanceEnabled = true;
		}
		else {
			return false;
		}

		status.hasVoiceGuidance = true;
		return true;
	}
}
