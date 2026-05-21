package dev.omiiba.connect.tv.bluetooth

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

object BluetoothAccess {
    val runtimePermissions: Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.BLUETOOTH_SCAN,
            )
        } else {
            emptyArray()
        }

    fun hasRuntimePermissions(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return true
        }
        return runtimePermissions.all {
            ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    fun permissionSummary(context: Context): String {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return "niet nodig (API < 31)"
        }
        val connect = ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT)
        val scan = ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN)
        return buildString {
            append("CONNECT=")
            append(if (connect == PackageManager.PERMISSION_GRANTED) "ja" else "nee")
            append(", SCAN=")
            append(if (scan == PackageManager.PERMISSION_GRANTED) "ja" else "nee")
        }
    }

    /**
     * Bonded + currently connected audio devices (A2DP). Requires BLUETOOTH_CONNECT on API 31+.
     */
    fun findHeadphones(context: Context): HeadphoneScan {
        if (!hasRuntimePermissions(context)) {
            return HeadphoneScan(
                permissionGranted = false,
                sonyDevices = emptyList(),
                otherBonded = emptyList(),
                error = "Bluetooth-rechten ontbreken. Geef CONNECT + SCAN toe.",
            )
        }

        val manager = context.getSystemService(BluetoothManager::class.java)
            ?: return HeadphoneScan.error("Bluetooth niet beschikbaar op dit apparaat")
        val adapter = manager.adapter
            ?: return HeadphoneScan.error("Bluetooth adapter uitgeschakeld")

        if (!adapter.isEnabled) {
            return HeadphoneScan.error("Bluetooth staat uit. Zet Bluetooth aan in TV-instellingen.")
        }

        return try {
            val merged = linkedMapOf<String, BluetoothDevice>()
            adapter.bondedDevices?.forEach { device ->
                merged[device.address] = device
            }
            try {
                manager.getConnectedDevices(BluetoothProfile.A2DP).forEach { device ->
                    merged[device.address] = device
                }
            } catch (_: SecurityException) {
                // bonded list may still work
            }

            val all = merged.values.toList()
            val sony = all.filter { isSonyHeadphone(it) }.sortedBy { it.name ?: it.address }
            val other = all.filter { !isSonyHeadphone(it) }.sortedBy { it.name ?: it.address }
            HeadphoneScan(
                permissionGranted = true,
                sonyDevices = sony,
                otherBonded = other,
                error = null,
            )
        } catch (sec: SecurityException) {
            HeadphoneScan(
                permissionGranted = false,
                sonyDevices = emptyList(),
                otherBonded = emptyList(),
                error = "Geen toegang tot gekoppelde apparaten: ${sec.message}",
            )
        }
    }

    fun deviceLabel(device: BluetoothDevice): String {
        val name = device.name?.trim()
        return if (!name.isNullOrEmpty()) {
            "$name (${device.address})"
        } else {
            device.address
        }
    }

    private fun isSonyHeadphone(device: BluetoothDevice): Boolean {
        val name = device.name ?: return false
        val n = name.uppercase()
        if (n.contains("SONY")) {
            return true
        }
        return n.contains("WH-1000") ||
            n.contains("WH1000") ||
            n.contains("1000XM") ||
            n.contains("MDR-") ||
            n.contains("MDR ") ||
            n.contains("XM3") ||
            n.contains("XM4") ||
            n.contains("XM5") ||
            n.contains("XM6")
    }

    data class HeadphoneScan(
        val permissionGranted: Boolean,
        val sonyDevices: List<BluetoothDevice>,
        val otherBonded: List<BluetoothDevice>,
        val error: String?,
    ) {
        val preferredDevices: List<BluetoothDevice>
            get() = sonyDevices.ifEmpty { otherBonded }

        companion object {
            fun error(message: String) = HeadphoneScan(
                permissionGranted = false,
                sonyDevices = emptyList(),
                otherBonded = emptyList(),
                error = message,
            )
        }
    }
}
