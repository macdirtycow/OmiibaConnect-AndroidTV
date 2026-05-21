package dev.omiiba.connect.tv

import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.core.content.ContextCompat
import androidx.leanback.app.BrowseSupportFragment
import androidx.leanback.widget.ArrayObjectAdapter
import androidx.leanback.widget.HeaderItem
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.ListRowPresenter
import androidx.leanback.widget.OnItemViewClickedListener
import androidx.lifecycle.lifecycleScope
import dev.omiiba.connect.tv.native.DeviceState
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

class MainBrowseFragment : BrowseSupportFragment() {
    private var repository: HeadphonesRepository? = null
    private var stateJob: Job? = null
    private val rowsAdapter = ArrayObjectAdapter(ListRowPresenter())
    private var latest: DeviceState? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupUiElements()
        buildDisconnectedRows()
    }

    override fun onDestroyView() {
        stateJob?.cancel()
        stateJob = null
        super.onDestroyView()
    }

    private fun setupUiElements() {
        title = getString(R.string.app_name)
        headersState = HEADERS_ENABLED
        isHeadersTransitionOnBackEnabled = true
        adapter = rowsAdapter
        brandColor = ContextCompat.getColor(requireContext(), R.color.omiiba_accent)
        onItemViewClickedListener = OnItemViewClickedListener { _, item, _, _ ->
            handleClick(item)
        }
    }

    /** Native/Bluetooth stack loads only when the user first interacts (not at cold start). */
    private fun ensureRepository(): HeadphonesRepository? {
        repository?.let { return it }
        return try {
            val repo = (requireActivity().application as OmiibaTvApplication).repository
            repo.startupError?.let { message ->
                buildMessageRow(message)
                return null
            }
            repository = repo
            observeRepositoryState(repo)
            repo
        } catch (t: Throwable) {
            buildMessageRow(t.message ?: t.javaClass.simpleName)
            null
        }
    }

    private fun observeRepositoryState(repo: HeadphonesRepository) {
        if (stateJob != null) {
            return
        }
        stateJob = viewLifecycleOwner.lifecycleScope.launch {
            repo.state.collectLatest { state ->
                when (state) {
                    is HeadphonesRepository.UiState.Disconnected -> buildDisconnectedRows()
                    is HeadphonesRepository.UiState.Connecting -> buildMessageRow(getString(R.string.status_connecting))
                    is HeadphonesRepository.UiState.Connected -> {
                        latest = state.device
                        buildConnectedRows(state.device)
                    }
                    is HeadphonesRepository.UiState.Error -> {
                        Toast.makeText(requireContext(), state.message, Toast.LENGTH_LONG).show()
                        buildMessageRow(state.message)
                    }
                }
            }
        }
    }

    private fun handleClick(item: Any) {
        val repo = ensureRepository() ?: return
        when (item) {
            is ActionItem.Connect -> {
                if (item.mac.isNotEmpty()) {
                    val device = repo.bondedSonyDevices().firstOrNull { it.address == item.mac }
                    if (device != null) {
                        repo.connect(device)
                    }
                } else {
                    showDevicePicker(repo)
                }
            }
            ActionItem.Disconnect -> repo.disconnect()
            ActionItem.Refresh -> repo.refresh()
            is ActionItem.AmbientToggle -> repo.setAmbient(!item.enabled, latest?.focusOnVoice == true, latest?.asmLevel ?: 0)
            is ActionItem.AsmLevel -> repo.setAmbient(latest?.ambientEnabled == true, latest?.focusOnVoice == true, item.level)
            is ActionItem.FocusOnVoice -> repo.setAmbient(latest?.ambientEnabled == true, !item.current, latest?.asmLevel ?: 0)
            is ActionItem.Surround -> repo.setVpt(item.index)
            is ActionItem.SoundPosition -> repo.setSoundPosition(item.preset)
            is ActionItem.EqPreset -> repo.setEqPreset(item.preset)
            is ActionItem.TouchSensor -> repo.setTouchSensor(!item.enabled)
            is ActionItem.VoiceGuidance -> repo.setVoiceGuidance(!item.enabled)
        }
    }

    private fun showDevicePicker(repo: HeadphonesRepository) {
        val devices = repo.bondedSonyDevices()
        if (devices.isEmpty()) {
            Toast.makeText(requireContext(), R.string.no_paired_devices, Toast.LENGTH_LONG).show()
            return
        }
        val rowAdapter = ArrayObjectAdapter()
        devices.forEach { device ->
            rowAdapter.add(ActionItem.Connect(device.name ?: device.address, device.address))
        }
        rowsAdapter.clear()
        rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_connect)), rowAdapter))
    }

    private fun buildDisconnectedRows() {
        rowsAdapter.clear()
        val row = ArrayObjectAdapter()
        row.add(ActionItem.Connect())
        rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_connect)), row))
    }

    private fun buildMessageRow(message: String) {
        rowsAdapter.clear()
        val row = ArrayObjectAdapter()
        row.add(message)
        rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_status)), row))
    }

    private fun buildConnectedRows(device: DeviceState) {
        rowsAdapter.clear()

        val status = ArrayObjectAdapter()
        status.add(getString(R.string.status_model, device.modelName))
        status.add(getString(R.string.status_battery, if (device.batteryPercent >= 0) "${device.batteryPercent}%" else "—"))
        status.add(getString(R.string.status_codec, device.codec))
        status.add(getString(R.string.status_firmware, device.firmware))
        status.add(ActionItem.Refresh)
        status.add(ActionItem.Disconnect)
        rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_status)), status))

        val ambient = ArrayObjectAdapter()
        ambient.add(ActionItem.AmbientToggle(device.ambientEnabled))
        ambient.add(ActionItem.FocusOnVoice(device.focusOnVoice))
        for (level in 0..device.asmMaxLevel) {
            ambient.add(ActionItem.AsmLevel(level, level == device.asmLevel))
        }
        rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_ambient)), ambient))

        if (device.supportsVirtualSound) {
            val virtual = ArrayObjectAdapter()
            Presets.SURROUND.forEachIndexed { index, label ->
                virtual.add(ActionItem.Surround(index, label, index == device.vptType))
            }
            Presets.SOUND_POSITION.forEach { (preset, label) ->
                virtual.add(ActionItem.SoundPosition(preset, label, preset == device.soundPosition))
            }
            rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_virtual_sound)), virtual))
        }

        if (device.supportsEqualizer) {
            val eq = ArrayObjectAdapter()
            Presets.EQ.forEach { (preset, label) ->
                eq.add(ActionItem.EqPreset(preset, label, preset == device.eqPreset))
            }
            rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_equalizer)), eq))
        }

        val extra = ArrayObjectAdapter()
        if (device.supportsTouchSensor) {
            extra.add(ActionItem.TouchSensor(device.touchSensorEnabled))
        }
        if (device.supportsVoiceGuidance) {
            extra.add(ActionItem.VoiceGuidance(device.voiceGuidanceEnabled))
        }
        if (extra.size() > 0) {
            rowsAdapter.add(ListRow(HeaderItem(getString(R.string.row_extra)), extra))
        }
    }

    private sealed interface ActionItem {
        data class Connect(val label: String = "", val mac: String = "") : ActionItem {
            override fun toString(): String = label.ifEmpty { "Connect headphones" }
        }
        data object Disconnect : ActionItem { override fun toString() = "Disconnect" }
        data object Refresh : ActionItem { override fun toString() = "Refresh from headphones" }
        data class AmbientToggle(val enabled: Boolean) : ActionItem {
            override fun toString() = if (enabled) "Turn ambient off" else "Turn ambient on"
        }
        data class FocusOnVoice(val current: Boolean) : ActionItem {
            override fun toString() = if (current) "Disable focus on voice" else "Enable focus on voice"
        }
        data class AsmLevel(val level: Int, val selected: Boolean) : ActionItem {
            override fun toString() = "Ambient level $level${if (selected) " ✓" else ""}"
        }
        data class Surround(val index: Int, val label: String, val selected: Boolean) : ActionItem {
            override fun toString() = "$label${if (selected) " ✓" else ""}"
        }
        data class SoundPosition(val preset: Int, val label: String, val selected: Boolean) : ActionItem {
            override fun toString() = "Position: $label${if (selected) " ✓" else ""}"
        }
        data class EqPreset(val preset: Int, val label: String, val selected: Boolean) : ActionItem {
            override fun toString() = "$label${if (selected) " ✓" else ""}"
        }
        data class TouchSensor(val enabled: Boolean) : ActionItem {
            override fun toString() = if (enabled) "Disable touch sensor" else "Enable touch sensor"
        }
        data class VoiceGuidance(val enabled: Boolean) : ActionItem {
            override fun toString() = if (enabled) "Disable voice guidance" else "Enable voice guidance"
        }
    }

    private object Presets {
        val SURROUND = listOf("Off", "Club", "Outdoor", "Lounge", "Arena", "Concert")
        val SOUND_POSITION = listOf(
            0 to "Off",
            1 to "Front Left",
            2 to "Front Right",
            3 to "Front",
            17 to "Rear Left",
            18 to "Rear Right",
        )
        val EQ = listOf(
            0x00 to "Off",
            0x10 to "Bright",
            0x11 to "Excited",
            0x12 to "Mellow",
            0x13 to "Relaxed",
            0x14 to "Vocal",
            0x15 to "Treble Boost",
            0x16 to "Bass Boost",
            0x17 to "Speech",
            0xa0 to "Manual",
        )
    }
}
