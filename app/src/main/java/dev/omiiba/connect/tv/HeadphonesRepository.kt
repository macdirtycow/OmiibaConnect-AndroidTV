package dev.omiiba.connect.tv

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import dev.omiiba.connect.tv.bluetooth.BluetoothRfcommTransport
import dev.omiiba.connect.tv.native.DeviceState
import dev.omiiba.connect.tv.native.HeadphonesNative
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout

class HeadphonesRepository {
    private val transport = BluetoothRfcommTransport()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var keepaliveJob: Job? = null

    private val _state = MutableStateFlow<UiState>(UiState.Disconnected)
    val state: StateFlow<UiState> = _state

    /** Non-null when native library or JNI bind failed at startup. */
    val startupError: String?

    init {
        startupError = HeadphonesNative.safeBind(transport)
        if (startupError != null) {
            _state.value = UiState.Error(startupError)
        }
    }

    private fun requireNative() {
        check(startupError == null) { startupError ?: "Native layer unavailable" }
    }

    fun bondedSonyDevices(): List<BluetoothDevice> {
        val adapter = BluetoothAdapter.getDefaultAdapter() ?: return emptyList()
        return adapter.bondedDevices
            .filter { it.name?.contains("WH-1000X", ignoreCase = true) == true || it.name?.contains("Sony", ignoreCase = true) == true }
            .sortedBy { it.name ?: it.address }
    }

    fun connect(device: BluetoothDevice) {
        scope.launch {
            try {
                requireNative()
                _state.value = UiState.Connecting
                withTimeout(CONNECT_TIMEOUT_MS) {
                    HeadphonesNative.nativeConnect(device.name ?: device.address, device.address)
                    refreshState()
                }
                startKeepalive()
                _state.value = UiState.Connected(requireState())
            } catch (exc: Exception) {
                HeadphonesNative.nativeDisconnect()
                _state.value = UiState.Error(exc.message ?: "Connect failed")
            }
        }
    }

    fun disconnect() {
        keepaliveJob?.cancel()
        scope.launch {
            HeadphonesNative.nativeDisconnect()
            _state.value = UiState.Disconnected
        }
    }

    fun refresh() {
        scope.launch {
            runCatching {
                HeadphonesNative.nativePrepareForControl()
                HeadphonesNative.nativeRefresh(true)
                refreshState()
            }.onFailure { exc ->
                _state.value = UiState.Error(exc.message ?: "Refresh failed")
            }
        }
    }

    fun applyPending() {
        scope.launch {
            runCatching { applyPendingInternal() }
                .onFailure { exc -> _state.value = UiState.Error(exc.message ?: "Apply failed") }
        }
    }

    private suspend fun applyPendingInternal() {
        requireNative()
        HeadphonesNative.nativePrepareForControl()
        HeadphonesNative.nativeApplyPending()
        refreshState()
    }

    fun setAmbient(enabled: Boolean, focusOnVoice: Boolean, asmLevel: Int) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetAmbient(enabled, focusOnVoice, asmLevel)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    fun setVpt(index: Int) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetVpt(index)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    fun setSoundPosition(preset: Int) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetSoundPosition(preset)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    fun setEqPreset(preset: Int) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetEqPreset(preset)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    fun setEqManual(bass: Int, bands: IntArray) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetEqManual(bass, bands)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    fun setTouchSensor(enabled: Boolean) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetTouchSensor(enabled)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    fun setVoiceGuidance(enabled: Boolean) {
        scope.launch {
            runCatching {
                HeadphonesNative.nativeSetVoiceGuidance(enabled)
                applyPendingInternal()
            }.onFailure { setError(it) }
        }
    }

    private suspend fun refreshState() {
        val device = HeadphonesNative.nativeGetState() ?: return
        _state.value = UiState.Connected(device)
    }

    private fun requireState(): DeviceState {
        return HeadphonesNative.nativeGetState()
            ?: throw IllegalStateException("No device state")
    }

    private fun setError(throwable: Throwable) {
        _state.value = UiState.Error(throwable.message ?: "Error")
    }

    private fun startKeepalive() {
        keepaliveJob?.cancel()
        keepaliveJob = scope.launch {
            while (isActive) {
                delay(25_000)
                if (!HeadphonesNative.nativeIsConnected()) {
                    continue
                }
                runCatching {
                    HeadphonesNative.nativePrepareForControl()
                    if (!HeadphonesNative.nativeProbe()) {
                        HeadphonesNative.nativeRefresh(false)
                    }
                    refreshState()
                }
            }
        }
    }

    companion object {
        /** RFCOMM + handshake budget (must exceed transport connect timeout). */
        private const val CONNECT_TIMEOUT_MS = 45_000L
    }

    sealed interface UiState {
        data object Disconnected : UiState
        data object Connecting : UiState
        data class Connected(val device: DeviceState) : UiState
        data class Error(val message: String) : UiState
    }
}
