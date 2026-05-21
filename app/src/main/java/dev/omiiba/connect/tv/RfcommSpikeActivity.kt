package dev.omiiba.connect.tv

import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import dev.omiiba.connect.tv.bluetooth.BluetoothRfcommTransport
import dev.omiiba.connect.tv.native.HeadphonesNative
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Phase-0 RFCOMM feasibility screen. Launch via adb:
 * adb shell am start -a dev.omiiba.connect.tv.RFCOMM_SPIKE -n dev.omiiba.connect.tv/.RfcommSpikeActivity
 */
class RfcommSpikeActivity : Activity() {
    private val scope = CoroutineScope(Dispatchers.Main)
    private val logView by lazy { TextView(this) }
    private val transport = BluetoothRfcommTransport()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        HeadphonesNative.nativeBindTransport(transport)

        val runButton = Button(this).apply {
            text = "Run RFCOMM + handshake test"
            setOnClickListener { runSpike() }
        }
        logView.textSize = 16f
        val scroll = ScrollView(this).apply {
            addView(
                LinearLayout(context).apply {
                    orientation = LinearLayout.VERTICAL
                    addView(runButton)
                    addView(logView)
                },
            )
        }
        setContentView(scroll)
        append("Pair WH-1000XM3 in system settings first.\n")
    }

    private fun runSpike() {
        scope.launch {
            val devices = withContext(Dispatchers.IO) {
                BluetoothAdapter.getDefaultAdapter()?.bondedDevices?.toList() ?: emptyList()
            }
            val target = devices.firstOrNull {
                it.name?.contains("WH-1000X", true) == true
            } ?: devices.firstOrNull()
            if (target == null) {
                append("No bonded devices.\n")
                return@launch
            }
            append("Device: ${target.name} ${target.address}\n")
            try {
                withContext(Dispatchers.IO) {
                    HeadphonesNative.nativeConnect(target.name ?: "Sony", target.address)
                    val ok = HeadphonesNative.nativeProbe()
                    append("Probe: $ok\n")
                    HeadphonesNative.nativeRefresh(true)
                    append("Handshake + refresh OK\n")
                    HeadphonesNative.nativeDisconnect()
                }
                append("RFCOMM spike succeeded on this TV.\n")
            } catch (exc: Exception) {
                append("FAILED: ${exc.message}\n")
                append("This TV may not support MDR RFCOMM — see docs/android-tv-bluetooth.md\n")
            }
        }
    }

    private fun append(line: String) {
        logView.append(line)
    }
}
