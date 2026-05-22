package dev.omiiba.connect.tv.bluetooth

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
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

    fun connect(mac: String) {
        disconnect()
        val adapter = bluetoothAdapter()
            ?: throw IOException("Bluetooth not available on this device")
        val device: BluetoothDevice = adapter.getRemoteDevice(mac)
        adapter.cancelDiscovery()

        val uuid = UUID.fromString(SONY_MDR_UUID)
        val failures = mutableListOf<String>()
        val factories = buildSocketFactories(device, uuid)

        for ((label, factory) in factories) {
            val sock = try {
                factory()
            } catch (exc: Exception) {
                failures.add("$label: ${exc.message ?: exc.javaClass.simpleName}")
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
                failures.add("$label: socket not connected after connect()")
            } catch (exc: IOException) {
                failures.add("$label: ${exc.message ?: "connect failed"}")
            } finally {
                if (socket !== sock) {
                    runCatching { sock.close() }
                }
            }
        }

        throw IOException(
            buildString {
                append("RFCOMM mislukt naar $mac. ")
                append("TV-audio (A2DP) kan werken terwijl Sony-bediening (RFCOMM) niet opent op deze TV.\n")
                append("Geprobeerd: ${factories.size} methodes.\n")
                failures.takeLast(4).forEach { append("• $it\n") }
            },
        )
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
        // Read thread continuously feeds inbound; nothing extra required on Android.
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
        for (channel in 1..CHANNEL_SCAN_MAX) {
            val ch = channel
            list.add("channel-$ch" to {
                val method = device.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType)
                method.invoke(device, ch) as BluetoothSocket
            })
        }
        return list
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
        private const val PER_ATTEMPT_TIMEOUT_MS = 8_000L
        private const val CHANNEL_SCAN_MAX = 12
        private const val POLL_TIMEOUT_MS = 5_000L
    }
}
