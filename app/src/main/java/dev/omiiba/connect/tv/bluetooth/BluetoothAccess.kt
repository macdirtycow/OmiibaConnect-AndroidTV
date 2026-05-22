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
     * Sony MDR protocol needs Bluetooth Classic RFCOMM — not the LE (Low Energy) sideband entry.
     * TVs often list only "LE-WH-1000XM3"; connecting to that hangs until timeout.
     */
    fun safeName(device: BluetoothDevice): String? {
        return try {
            device.name?.trim()?.takeIf { it.isNotEmpty() }
        } catch (_: SecurityException) {
            null
        }
    }

    fun safeType(device: BluetoothDevice): Int {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
            return BluetoothDevice.DEVICE_TYPE_UNKNOWN
        }
        return try {
            device.type
        } catch (_: SecurityException) {
            BluetoothDevice.DEVICE_TYPE_UNKNOWN
        }
    }

    fun isLeOnlyAccessory(device: BluetoothDevice): Boolean {
        val name = safeName(device)?.uppercase().orEmpty()
        if (name.startsWith("LE-") || name.startsWith("LE_") || name.contains(" LOW ENERGY")) {
            return true
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            if (safeType(device) == BluetoothDevice.DEVICE_TYPE_LE) {
                return true
            }
        }
        return false
    }

    fun isRfcommCandidate(device: BluetoothDevice): Boolean {
        if (isLeOnlyAccessory(device)) {
            return false
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            return when (safeType(device)) {
                BluetoothDevice.DEVICE_TYPE_CLASSIC,
                BluetoothDevice.DEVICE_TYPE_DUAL,
                BluetoothDevice.DEVICE_TYPE_UNKNOWN,
                -> true
                BluetoothDevice.DEVICE_TYPE_LE -> false
                else -> true
            }
        }
        return true
    }

    fun rfcommRejectReason(device: BluetoothDevice): String? {
        if (!isLeOnlyAccessory(device)) {
            return null
        }
        return "“${safeName(device) ?: device.address}” is een LE (Low Energy) koppeling. " +
            "Omiiba heeft Classic Bluetooth (WH-1000XM3 zonder LE-). " +
            "Koppel opnieuw in TV-instellingen → Bluetooth voor audio/classic."
    }

    /**
     * Bonded + A2DP-connected devices. Requires BLUETOOTH_CONNECT on API 31+.
     */
    fun findHeadphones(context: Context): HeadphoneScan {
        if (!hasRuntimePermissions(context)) {
            return HeadphoneScan(
                permissionGranted = false,
                sonyClassic = emptyList(),
                sonyLeOnly = emptyList(),
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
            try {
                adapter.bondedDevices?.forEach { device ->
                    merged[device.address] = device
                }
            } catch (sec: SecurityException) {
                return HeadphoneScan(
                    permissionGranted = false,
                    sonyClassic = emptyList(),
                    sonyLeOnly = emptyList(),
                    otherBonded = emptyList(),
                    error = "Geen toegang tot gekoppelde apparaten: ${sec.message}",
                )
            }
            try {
                manager.getConnectedDevices(BluetoothProfile.A2DP).forEach { device ->
                    merged[device.address] = device
                }
            } catch (_: SecurityException) {
            } catch (_: IllegalArgumentException) {
                // Some Android TV builds have no A2DP proxy — bonded list is enough.
            }

            val all = merged.values.toList()
            // Prefer A2DP-active Sony (audio already on TV) over stale bonded LE entry.
            val sony = all.filter { isSonyHeadphone(it) }
                .sortedWith(
                    compareByDescending<BluetoothDevice> { device ->
                        !isLeOnlyAccessory(device)
                    }.thenBy { safeName(it) ?: it.address },
                )
            val sonyClassic = sony.filter { isRfcommCandidate(it) }.sortedBy { safeName(it) ?: it.address }
            val sonyLeOnly = sony.filter { isLeOnlyAccessory(it) }.sortedBy { safeName(it) ?: it.address }
            val other = all.filter { !isSonyHeadphone(it) && isRfcommCandidate(it) }
                .sortedBy { safeName(it) ?: it.address }

            val error = when {
                sonyClassic.isNotEmpty() -> null
                sonyLeOnly.isNotEmpty() ->
                    "Alleen LE-WH-1000XM3 gevonden. Die werkt niet met Omiiba (RFCOMM). " +
                        "Ontkoppel LE in TV Bluetooth en koppel opnieuw voor audio — zoek WH-1000XM3 zonder LE-."
                else -> null
            }

            HeadphoneScan(
                permissionGranted = true,
                sonyClassic = sonyClassic,
                sonyLeOnly = sonyLeOnly,
                otherBonded = other,
                error = error,
            )
        } catch (sec: SecurityException) {
            HeadphoneScan(
                permissionGranted = false,
                sonyClassic = emptyList(),
                sonyLeOnly = emptyList(),
                otherBonded = emptyList(),
                error = "Geen toegang tot gekoppelde apparaten: ${sec.message}",
            )
        } catch (t: Throwable) {
            HeadphoneScan.error("Bluetooth scan mislukt: ${t.javaClass.simpleName}: ${t.message}")
        }
    }

    fun deviceLabel(device: BluetoothDevice): String {
        val name = safeName(device)
        val suffix = if (isLeOnlyAccessory(device)) " [LE — niet gebruiken]" else ""
        return if (!name.isNullOrEmpty()) {
            "$name (${device.address})$suffix"
        } else {
            device.address + suffix
        }
    }

    private fun isSonyHeadphone(device: BluetoothDevice): Boolean {
        val name = safeName(device) ?: return false
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
        /** Classic / dual — safe for RFCOMM (WH-1000XM3 without LE- prefix). */
        val sonyClassic: List<BluetoothDevice>,
        /** LE sideband only — must not be used for MDR protocol. */
        val sonyLeOnly: List<BluetoothDevice>,
        val otherBonded: List<BluetoothDevice>,
        val error: String?,
    ) {
        val preferredDevices: List<BluetoothDevice>
            get() = sonyClassic.ifEmpty { otherBonded }

        companion object {
            fun error(message: String) = HeadphoneScan(
                permissionGranted = false,
                sonyClassic = emptyList(),
                sonyLeOnly = emptyList(),
                otherBonded = emptyList(),
                error = message,
            )
        }
    }
}
