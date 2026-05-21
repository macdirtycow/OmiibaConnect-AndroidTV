package dev.omiiba.connect.tv.native

data class DeviceState(
    val asmLevel: Int,
    val asmMaxLevel: Int,
    val ambientEnabled: Boolean,
    val focusOnVoice: Boolean,
    val vptType: Int,
    val soundPosition: Int,
    val eqPreset: Int,
    val eqBass: Int,
    val batteryPercent: Int,
    val supportsVirtualSound: Boolean,
    val supportsEqualizer: Boolean,
    val supportsTouchSensor: Boolean,
    val supportsVoiceGuidance: Boolean,
    val touchSensorEnabled: Boolean,
    val voiceGuidanceEnabled: Boolean,
    val usesV2Ambient: Boolean,
    val modelName: String,
    val codec: String,
    val firmware: String,
)
