package dev.omiiba.connect.tv.native

import dev.omiiba.connect.tv.bluetooth.BluetoothRfcommTransport

object HeadphonesNative {
    init {
        System.loadLibrary("omiiba_core")
    }

    external fun nativeBindTransport(transport: BluetoothRfcommTransport)
    external fun nativeConnect(deviceName: String, mac: String)
    external fun nativeDisconnect()
    external fun nativeIsConnected(): Boolean
    external fun nativePrepareForControl()
    external fun nativeProbe(): Boolean
    external fun nativeRefresh(extended: Boolean)
    external fun nativeApplyPending()
    external fun nativeSetAmbient(enabled: Boolean, focusOnVoice: Boolean, asmLevel: Int)
    external fun nativeSetVpt(vpt: Int)
    external fun nativeSetSoundPosition(preset: Int)
    external fun nativeSetEqPreset(preset: Int)
    external fun nativeSetEqManual(bass: Int, bands: IntArray)
    external fun nativeSetTouchSensor(enabled: Boolean)
    external fun nativeSetVoiceGuidance(enabled: Boolean)
    external fun nativeGetState(): DeviceState?
}
