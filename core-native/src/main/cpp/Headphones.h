#pragma once

#include "BluetoothWrapper.h"
#include "Constants.h"
#include "DeviceProfile.h"
#include "DeviceStatus.h"

#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

template <class T>
struct Property {
	T current;
	T desired;

	void fulfill();
	bool isFulfilled() const;
};

class Headphones {
public:
	Headphones(BluetoothWrapper& conn);

	void setAmbientSoundControl(bool val);
	bool getAmbientSoundControl();

	bool isFocusOnVoiceAvailable();
	void setFocusOnVoice(bool val);
	bool getFocusOnVoice();

	bool isSetAsmLevelAvailable();
	void setAsmLevel(int val);
	int getAsmLevel();
	int getDisplayAsmLevel() const;
	bool hasPendingAmbientChanges() const;

	void setSurroundPosition(SOUND_POSITION_PRESET val);
	SOUND_POSITION_PRESET getSurroundPosition();

	void setVptType(int val);
	int getVptType();
	int getDisplayVptType() const;
	SOUND_POSITION_PRESET getDisplaySurroundPosition() const;
	bool hasPendingVirtualSoundChanges() const;

	void setEqualizerPreset(EQ_PRESET preset);
	void setEqualizerWithBands(EQ_PRESET preset, int clearBass, const std::array<int, EQ_BAND_COUNT>& bands);
	/** Read current EQ from headphones into manual sliders (does not send EQ_SET). */
	bool primeManualEqFromDevice();
	EQ_PRESET getEqualizerPreset() const;
	EQ_PRESET getDisplayEqPreset() const;
	int getDisplayEqBass() const;
	std::array<int, EQ_BAND_COUNT> getDisplayEqBands() const;
	bool hasPendingEqChanges() const;

	void setTouchSensorEnabled(bool enabled);
	bool getTouchSensorEnabled() const;

	void setVoiceGuidanceEnabled(bool enabled);
	bool getVoiceGuidanceEnabled() const;

	const DeviceStatus& getDeviceStatus() const;
	const DeviceCapabilities& getCapabilities() const;

	void configureForDevice(std::string_view deviceName);

	bool performConnectHandshake();
	bool ensureConnectHandshake();
	void ensureTransportReady();
	/** Call before applying user settings after idle or background. */
	void prepareForControl();
	void invalidateConnectHandshake();
	/** Lightweight round-trip; false means the RFCOMM link is stale. */
	bool probeConnection();
	bool refreshFromDevice(bool includeExtendedSettings = true);
	bool isChanged();
	bool hasAnyPendingChanges() const;
	void setChanges();
	void setAmbientChangesIfNeeded();
	void setVirtualSoundChangesIfNeeded();
	void resyncAmbientAfterVirtualSoundIfNeeded();
	void setEqChangesIfNeeded();
	void setTouchAndVoiceChangesIfNeeded();

	void applyDeviceState(bool ambientEnabled, bool focusOnVoice, int asmLevel, bool syncDesired = false);
	void applyDeviceVpt(int vptType, bool syncDesired = false);
	void applyDeviceSurroundPosition(SOUND_POSITION_PRESET preset, bool syncDesired = false);
	void sendAmbientV1Changes();
	void updateEqCurrentFromDevice();
	void applyEqReadback(const DeviceStatus& eqStatus);
	void sendEqChanges();
	void updateVirtualSoundCurrentFromDevice(bool syncDesired = false);
	/** Updates current VPT/position from device only (leaves desired and dirty unchanged). */
	void snapshotVirtualSoundFromDevice();
	void updateAmbientCurrentFromDevice();
private:
	Property<bool> _ambientSoundControl = { 0 };
	Property<bool> _focusOnVoice = { 0 };
	Property<int> _asmLevel = { 0 };
	Property<SOUND_POSITION_PRESET> _surroundPosition = { SOUND_POSITION_PRESET::OFF, SOUND_POSITION_PRESET::OFF };
	Property<int> _vptType = { 0 };
	Property<EQ_PRESET> _eqPreset = { EQ_PRESET::OFF };
	Property<int> _eqBass = { 0 };
	Property<std::array<int, EQ_BAND_COUNT>> _eqBands = { std::array<int, EQ_BAND_COUNT>{} };
	bool _eqUseBandPayload = false;
	bool _eqApplyPending = false;
	Property<bool> _touchSensorEnabled = { true };
	Property<bool> _voiceGuidanceEnabled = { true };
	mutable std::mutex _propertyMtx;

	DeviceStatus _deviceStatus;
	DeviceCapabilities _capabilities;
	std::string _deviceName;
	bool _handshakeComplete = false;
	bool _virtualSoundDirty = false;
	std::chrono::steady_clock::time_point _lastHandshakeAt{};
	BluetoothWrapper& _conn;
};

template<class T>
inline void Property<T>::fulfill()
{
	this->current = this->desired;
}

template<class T>
inline bool Property<T>::isFulfilled() const
{
	return this->desired == this->current;
}
