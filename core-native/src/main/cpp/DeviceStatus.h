#pragma once

#include <array>
#include <optional>
#include <string>

struct DeviceStatus {
	int batteryPercent = -1;
	bool batteryCharging = false;
	std::string audioCodec;
	std::string firmwareVersion;

	int eqPresetCode = -1;
	std::array<int, 5> eqBands{};
	int eqBass = 0;

	bool touchSensorEnabled = false;
	bool voiceGuidanceEnabled = false;

	bool hasBattery = false;
	bool hasCodec = false;
	bool hasFirmware = false;
	bool hasEqualizer = false;
	bool hasTouchSensor = false;
	bool hasVoiceGuidance = false;

	std::string modelName;
	std::string protocolLabel;
};
