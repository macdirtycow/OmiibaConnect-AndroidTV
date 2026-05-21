package dev.omiiba.connect.tv.bluetooth

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
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
class BluetoothRfcommTransport {
    private val connected = AtomicBoolean(false)
    private var socket: BluetoothSocket? = null
    private var readThread: Thread? = null
    private val inbound = ArrayBlockingQueue<ByteArray>(32)

    fun connect(mac: String) {
        disconnect()
        val adapter = BluetoothAdapter.getDefaultAdapter()
            ?: throw IOException("Bluetooth not available on this device")
        val device: BluetoothDevice = adapter.getRemoteDevice(mac)
        val uuid = UUID.fromString(SONY_MDR_UUID)
        val sock = try {
            device.createRfcommSocketToServiceRecord(uuid)
        } catch (_: IOException) {
            val method = device.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType)
            method.invoke(device, 1) as BluetoothSocket
        }
        adapter.cancelDiscovery()
        connectWithTimeout(sock, CONNECT_TIMEOUT_MS)
        socket = sock
        connected.set(true)
        inbound.clear()
        readThread = Thread({ readLoop(sock) }, "omiiba-bt-read").also { it.isDaemon = true; it.start() }
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
            throw IOException("RFCOMM connect timed out (${timeoutMs / 1000}s). This TV may not support Sony MDR Bluetooth.")
        }
        error.get()?.let { throw it }
        if (!sock.isConnected) {
            throw IOException("RFCOMM connect failed (socket not connected)")
        }
    }

    companion object {
        const val SONY_MDR_UUID = "96CC203E-5068-46ad-B32D-E316F5E069BA"
        private const val CONNECT_TIMEOUT_MS = 20_000L
        private const val POLL_TIMEOUT_MS = 5_000L
    }
}
