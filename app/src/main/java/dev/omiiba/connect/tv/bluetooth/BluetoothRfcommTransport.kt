package dev.omiiba.connect.tv.bluetooth

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.os.Build
import android.util.Log
import java.io.IOException
import java.util.UUID
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference

/**
 * RFCOMM transport for Sony MDR protocol (same UUID as Sound Connect / Omiiba Mac).
 */
class BluetoothRfcommTransport(context: Context) {
    private val appContext = context.applicationContext
    private val connected = AtomicBoolean(false)
    private var socket: BluetoothSocket? = null
    private var readThread: Thread? = null
    private val inbound = ArrayBlockingQueue<ByteArray>(32)

    fun connect(device: BluetoothDevice, onProgress: ((String) -> Unit)? = null) {
        connectInternal(device, device.address, onProgress)
    }

    fun connect(mac: String, onProgress: ((String) -> Unit)? = null) {
        val adapter = bluetoothAdapter()
            ?: throw IOException("Bluetooth niet beschikbaar op dit apparaat")
        connectInternal(adapter.getRemoteDevice(mac), mac, onProgress)
    }

    private fun connectInternal(
        device: BluetoothDevice,
        mac: String,
        onProgress: ((String) -> Unit)?,
    ) {
        disconnect()
        val adapter = bluetoothAdapter()
            ?: throw IOException("Bluetooth niet beschikbaar op dit apparaat")
        adapter.cancelDiscovery()
        prepareDevice(device, onProgress)

        val failures = linkedSetOf<String>()
        val factories = buildSocketFactories(device, UUID.fromString(SONY_MDR_UUID))

        for (round in 1..CONNECT_ROUNDS) {
            if (round > 1) {
                onProgress?.invoke("RFCOMM opnieuw ($round/$CONNECT_ROUNDS)…")
                Thread.sleep(BETWEEN_ROUNDS_MS)
            }
            val deadlineMs = System.currentTimeMillis() + RFCOMM_ROUND_BUDGET_MS
            for ((index, pair) in factories.withIndex()) {
                val (label, factory) = pair
                if (System.currentTimeMillis() >= deadlineMs) {
                    failures.add("tijd verstreken in ronde $round")
                    break
                }
                onProgress?.invoke("RFCOMM ${index + 1}/${factories.size} ($label)…")
                val sock = try {
                    factory()
                } catch (exc: Exception) {
                    failures.add(failureLabel(label, exc))
                    continue
                }
                try {
                    connectWithTimeout(sock, PER_ATTEMPT_TIMEOUT_MS)
                    if (sock.isConnected) {
                        socket = sock
                        connected.set(true)
                        inbound.clear()
                        readThread = Thread({ readLoop(sock) }, "omiiba-bt-read").also {
                            it.isDaemon = true
                            it.start()
                        }
                        Log.i(TAG, "RFCOMM connected via $label to $mac")
                        return
                    }
                    failures.add("$label: niet verbonden na connect()")
                } catch (exc: IOException) {
                    failures.add(failureLabel(label, exc))
                } finally {
                    if (socket !== sock) {
                        runCatching { sock.close() }
                    }
                }
            }
        }

        throw IOException(formatConnectFailure(mac, failures))
    }

    private fun prepareDevice(device: BluetoothDevice, onProgress: ((String) -> Unit)?) {
        onProgress?.invoke("Bluetooth SDP voorbereiden…")
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
                device.fetchUuidsWithSdp()
            }
        } catch (_: SecurityException) {
        }
        Thread.sleep(SDP_SETTLE_MS)
    }

    fun disconnect() {
        connected.set(false)
        readThread?.interrupt()
        readThread = null
        try {
            socket?.close()
        } catch (_: IOException) {
        }
        socket = null
        inbound.clear()
    }

    fun isConnected(): Boolean = connected.get() && socket?.isConnected == true

    fun send(data: ByteArray): Int {
        val out = socket?.outputStream ?: return 0
        out.write(data)
        out.flush()
        return data.size
    }

    fun recv(maxLen: Int, blocking: Boolean): ByteArray? {
        return if (blocking) {
            val chunk = inbound.poll(POLL_TIMEOUT_MS, TimeUnit.MILLISECONDS) ?: return null
            chunk.copyOf(minOf(chunk.size, maxLen))
        } else {
            val chunk = inbound.poll() ?: return null
            chunk.copyOf(minOf(chunk.size, maxLen))
        }
    }

    fun discardPendingReceive() {
        inbound.clear()
    }

    fun serviceTransport() {
        // Read thread continuously feeds inbound.
    }

    private fun bluetoothAdapter(): BluetoothAdapter? {
        val manager = appContext.getSystemService(BluetoothManager::class.java) ?: return null
        return manager.adapter
    }

    private fun buildSocketFactories(
        device: BluetoothDevice,
        uuid: UUID,
    ): List<Pair<String, () -> BluetoothSocket>> {
        val list = mutableListOf<Pair<String, () -> BluetoothSocket>>()
        list.add("insecure+uuid" to { device.createInsecureRfcommSocketToServiceRecord(uuid) })
        list.add("secure+uuid" to { device.createRfcommSocketToServiceRecord(uuid) })
        for (channel in PREFERRED_CHANNELS) {
            val ch = channel
            list.add("insecure-ch$ch" to { createInsecureRfcommChannel(device, ch) })
            list.add("channel-$ch" to { createRfcommChannel(device, ch) })
        }
        return list
    }

    private fun createRfcommChannel(device: BluetoothDevice, channel: Int): BluetoothSocket {
        val method = device.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType)
        return method.invoke(device, channel) as BluetoothSocket
    }

    private fun createInsecureRfcommChannel(device: BluetoothDevice, channel: Int): BluetoothSocket {
        return try {
            val method = device.javaClass.getMethod("createInsecureRfcommSocket", Int::class.javaPrimitiveType)
            method.invoke(device, channel) as BluetoothSocket
        } catch (_: NoSuchMethodException) {
            createRfcommChannel(device, channel)
        }
    }

    private fun connectWithTimeout(sock: BluetoothSocket, timeoutMs: Long) {
        val error = AtomicReference<IOException?>(null)
        val done = CountDownLatch(1)
        val thread = Thread({
            try {
                sock.connect()
            } catch (exc: IOException) {
                error.set(exc)
            } finally {
                done.countDown()
            }
        }, "omiiba-bt-connect")
        thread.isDaemon = true
        thread.start()
        if (!done.await(timeoutMs, TimeUnit.MILLISECONDS)) {
            thread.interrupt()
            runCatching { sock.close() }
            throw IOException("timeout na ${timeoutMs / 1000}s")
        }
        error.get()?.let { throw it }
        if (!sock.isConnected) {
            throw IOException("socket not connected")
        }
    }

    private fun readLoop(sock: BluetoothSocket) {
        val input = sock.inputStream
        val buffer = ByteArray(512)
        try {
            while (connected.get() && !Thread.currentThread().isInterrupted) {
                val read = input.read(buffer)
                if (read <= 0) {
                    break
                }
                inbound.offer(buffer.copyOf(read))
            }
        } catch (_: IOException) {
        } finally {
            connected.set(false)
        }
    }

    companion object {
        private const val TAG = "OmiibaRfcomm"
        const val SONY_MDR_UUID = "96CC203E-5068-46ad-B32D-E316F5E069BA"
        private const val PER_ATTEMPT_TIMEOUT_MS = 6_000L
        private const val RFCOMM_ROUND_BUDGET_MS = 50_000L
        private const val BETWEEN_ROUNDS_MS = 2_500L
        private const val SDP_SETTLE_MS = 1_500L
        private const val CONNECT_ROUNDS = 2
        private val PREFERRED_CHANNELS = intArrayOf(1, 2, 3, 4, 5, 6, 7, 8)
        private const val POLL_TIMEOUT_MS = 5_000L

        fun failureLabel(label: String, exc: Throwable): String {
            val msg = exc.message?.trim().orEmpty()
            return when {
                msg.contains("read ret: -1", ignoreCase = true) ||
                    msg.contains("socket might closed", ignoreCase = true) ->
                    "$label: socket gesloten (koptelefoon/TV weigerde RFCOMM)"
                msg.isNotEmpty() -> "$label: $msg"
                else -> "$label: ${exc.javaClass.simpleName}"
            }
        }

        fun formatConnectFailure(mac: String, failures: Collection<String>): String {
            val unique = failures.toList().takeLast(8).distinct()
            val socketRefused = unique.any {
                it.contains("socket gesloten", ignoreCase = true) ||
                    it.contains("read ret: -1", ignoreCase = true)
            }
            return buildString {
                append("RFCOMM mislukt ($mac). ")
                if (socketRefused) {
                    append("TV-audio OK, Sony RFCOMM geweigerd. ")
                }
                append(unique.joinToString(" | "))
            }.trim()
        }
    }
}
