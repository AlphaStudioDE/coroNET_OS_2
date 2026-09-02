package de.alphastudio.coronet2.transport

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import androidx.core.content.ContextCompat
import de.alphastudio.coronet2.model.*
import org.json.JSONObject
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

class CoronetBleManager(
    private val context: Context,
    private val onFound: (CoronetDevice) -> Unit,
    private val onSnapshot: (DeviceSnapshot) -> Unit,
    private val onSettings: (JSONObject) -> Unit,
    private val onPairing: (String, String) -> Boolean,
    private val onEvent: (String, String) -> Unit,
) {
    companion object {
        val ServiceUuid: UUID = UUID.fromString("7b7e0001-9f2a-4f3c-8d2a-c0a0e7c0ffee")
        private val StateUuid = UUID.fromString("7b7e0002-9f2a-4f3c-8d2a-c0a0e7c0ffee")
        private val CommandUuid = UUID.fromString("7b7e0003-9f2a-4f3c-8d2a-c0a0e7c0ffee")
        private val EventUuid = UUID.fromString("7b7e0004-9f2a-4f3c-8d2a-c0a0e7c0ffee")
        private val Cccd = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val MaxCommandBytes = 384
        private const val MaxPendingCommands = 16
    }

    private data class AssemblyKey(val source: UUID, val type: Int, val messageId: Int)
    private data class Assembly(val total: Int, val chunks: Array<ByteArray?>, val startedAtMs: Long)
    private val adapter get() = context.getSystemService(BluetoothManager::class.java)?.adapter
    private val assemblies = mutableMapOf<AssemblyKey, Assembly>()
    private val mainHandler = Handler(Looper.getMainLooper())
    @Volatile private var gatt: BluetoothGatt? = null
    @Volatile private var command: BluetoothGattCharacteristic? = null
    private var activeDevice: CoronetDevice? = null
    private val commandWrites = ArrayDeque<ByteArray>()
    private val descriptorWrites = ArrayDeque<BluetoothGattDescriptor>()
    private var commandWriteInFlight = false
    @Volatile private var subscriptionsReady = false
    @Volatile private var serviceDiscoveryStarted = false
    private val serviceDiscoveryFallback = Runnable { gatt?.let(::discoverServicesOnce) }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!hasScanPermission()) return
            val name = result.scanRecord?.deviceName ?: runCatching { result.device.name }.getOrNull() ?: return
            val hasService = result.scanRecord?.serviceUuids?.any { it.uuid == ServiceUuid } == true
            if (!hasService && !name.startsWith("coroNET", ignoreCase = true)) return
            onFound(CoronetDevice(id = result.device.address.replace(":", ""), name = name, address = result.device.address))
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(): Boolean {
        if (!hasScanPermission()) return false
        adapter?.bluetoothLeScanner?.startScan(scanCallback) ?: return false
        return true
    }

    @SuppressLint("MissingPermission")
    fun stopScan() { if (hasScanPermission()) adapter?.bluetoothLeScanner?.stopScan(scanCallback) }

    @SuppressLint("MissingPermission")
    fun connect(device: CoronetDevice) {
        if (!hasConnectPermission() || device.address.isBlank()) return
        stopScan(); gatt?.close(); activeDevice = device
        synchronized(commandWrites) { commandWrites.clear(); commandWriteInFlight = false }
        descriptorWrites.clear(); synchronized(assemblies) { assemblies.clear() }; subscriptionsReady = false
        serviceDiscoveryStarted = false
        mainHandler.removeCallbacks(serviceDiscoveryFallback)
        gatt = adapter?.getRemoteDevice(device.address)?.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        mainHandler.removeCallbacks(serviceDiscoveryFallback)
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        command = null
        subscriptionsReady = false
        serviceDiscoveryStarted = false
        synchronized(assemblies) { assemblies.clear() }
    }

    @SuppressLint("MissingPermission")
    fun send(json: String): Boolean {
        if (gatt == null || command == null) return false
        val bytes = json.toByteArray()
        if (bytes.size > MaxCommandBytes) return false
        synchronized(commandWrites) {
            if (commandWrites.size >= MaxPendingCommands) return false
            commandWrites.addLast(bytes)
        }
        writeNextCommand()
        return true
    }

    @SuppressLint("MissingPermission")
    private fun writeNextCommand() {
        val currentGatt = gatt ?: return
        val characteristic = command ?: return
        if (!subscriptionsReady) return
        val bytes = synchronized(commandWrites) {
            if (commandWriteInFlight || commandWrites.isEmpty()) return
            commandWriteInFlight = true
            commandWrites.first()
        }
        val started = if (Build.VERSION.SDK_INT >= 33) {
            currentGatt.writeCharacteristic(characteristic, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            characteristic.value = bytes
            @Suppress("DEPRECATION")
            currentGatt.writeCharacteristic(characteristic)
        }
        if (!started) {
            synchronized(commandWrites) { commandWriteInFlight = false; if (commandWrites.isNotEmpty()) commandWrites.removeFirst() }
            writeNextCommand()
        }
    }

    private val callback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (this@CoronetBleManager.gatt !== gatt) {
                if (newState == BluetoothProfile.STATE_DISCONNECTED) gatt.close()
                return
            }
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                val mtuRequestStarted = gatt.requestMtu(247)
                if (mtuRequestStarted) mainHandler.postDelayed(serviceDiscoveryFallback, 1200)
                else discoverServicesOnce(gatt)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                mainHandler.removeCallbacks(serviceDiscoveryFallback)
                command = null
                subscriptionsReady = false
                serviceDiscoveryStarted = false
                synchronized(assemblies) { assemblies.clear() }
                activeDevice?.let { onSnapshot(DeviceSnapshot(device = it)) }
                if (this@CoronetBleManager.gatt === gatt) this@CoronetBleManager.gatt = null
                gatt.close()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            mainHandler.removeCallbacks(serviceDiscoveryFallback)
            discoverServicesOnce(gatt)
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (gatt !== this@CoronetBleManager.gatt || status != BluetoothGatt.GATT_SUCCESS) return
            val service = gatt.getService(ServiceUuid) ?: return
            command = service.getCharacteristic(CommandUuid)
            queueNotify(gatt, service.getCharacteristic(StateUuid))
            queueNotify(gatt, service.getCharacteristic(EventUuid))
            writeNextDescriptor(gatt)
        }

        @SuppressLint("MissingPermission")
        private fun queueNotify(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic?) {
            characteristic ?: return
            gatt.setCharacteristicNotification(characteristic, true)
            characteristic.getDescriptor(Cccd)?.let(descriptorWrites::addLast)
        }

        @SuppressLint("MissingPermission")
        private fun writeNextDescriptor(gatt: BluetoothGatt) {
            val descriptor = descriptorWrites.firstOrNull()
            if (descriptor == null) {
                subscriptionsReady = true
                send("{\"cmd\":\"snapshot\"}")
                send("{\"cmd\":\"getSettings\"}")
                send("{\"cmd\":\"getPairingToken\"}")
                return
            }
            val started = if (Build.VERSION.SDK_INT >= 33) {
                gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                gatt.writeDescriptor(descriptor)
            }
            if (!started) { descriptorWrites.removeFirst(); writeNextDescriptor(gatt) }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (gatt !== this@CoronetBleManager.gatt) return
            if (descriptorWrites.firstOrNull() == descriptor) descriptorWrites.removeFirst()
            writeNextDescriptor(gatt)
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (gatt !== this@CoronetBleManager.gatt) return
            synchronized(commandWrites) {
                if (commandWrites.isNotEmpty()) commandWrites.removeFirst()
                commandWriteInFlight = false
            }
            writeNextCommand()
        }

        @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (gatt === this@CoronetBleManager.gatt && Build.VERSION.SDK_INT < 33) {
                acceptFrame(characteristic.uuid, characteristic.value ?: return)
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (gatt === this@CoronetBleManager.gatt) acceptFrame(characteristic.uuid, value)
        }
    }

    @SuppressLint("MissingPermission")
    @Synchronized
    private fun discoverServicesOnce(targetGatt: BluetoothGatt) {
        if (targetGatt !== gatt || serviceDiscoveryStarted) return
        serviceDiscoveryStarted = targetGatt.discoverServices()
        if (!serviceDiscoveryStarted) mainHandler.postDelayed(serviceDiscoveryFallback, 350)
    }

    private fun acceptFrame(source: UUID, frame: ByteArray) {
        if (frame.size < 8 || frame[0].toInt() != 1) return
        val header = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN)
        header.get(); val type = header.get().toInt() and 0xff
        val messageId = header.short.toInt() and 0xffff
        val total = header.short.toInt() and 0xffff
        val index = header.get().toInt() and 0xff
        val count = header.get().toInt() and 0xff
        if (total !in 1..4096 || count !in 1..64 || index >= count) return
        val payload = synchronized(assemblies) {
            val now = SystemClock.elapsedRealtime()
            assemblies.entries.removeAll { now - it.value.startedAtMs > 5000L }
            val key = AssemblyKey(source, type, messageId)
            if (key !in assemblies && assemblies.size >= 8) {
                assemblies.minByOrNull { it.value.startedAtMs }?.key?.let(assemblies::remove)
            }
            val assembly = assemblies.getOrPut(key) { Assembly(total, arrayOfNulls(count), now) }
            if (assembly.total != total || assembly.chunks.size != count) {
                assemblies.remove(key)
                return@synchronized null
            }
            assembly.chunks[index] = frame.copyOfRange(8, frame.size)
            if (assembly.chunks.any { it == null }) return@synchronized null
            val completeChunks = assembly.chunks.filterNotNull()
            if (completeChunks.sumOf { it.size } != total) {
                assemblies.remove(key)
                return@synchronized null
            }
            ByteArray(total).also { complete ->
                var offset = 0
                completeChunks.forEach { chunk ->
                    chunk.copyInto(complete, offset)
                    offset += chunk.size
                }
                assemblies.remove(key)
            }
        } ?: return
        if (type == 1) parseSnapshot(payload) else parseJson(payload)
    }

    private fun parseSnapshot(bytes: ByteArray) {
        if (bytes.size < 175) return
        val b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        b.get(); b.get(); b.short; b.int; b.int
        val flags = b.short.toInt() and 0xffff
        val stateValue = b.get().toInt() and 0xff
        val progress = b.get().toInt() and 0xff
        val tool = b.get().toInt() and 0xff
        b.get()
        val toolTemp = b.short.toInt().toTemperature()
        val bedTemp = b.short.toInt().toTemperature()
        val chamberTemp = b.short.toInt().toTemperature()
        val id = b.readCString(13)
        val name = b.readCString(25)
        val status = b.readCString(48)
        val filename = b.readCString(65)
        val telemetryRevision = if (b.remaining() >= 4) b.int.toLong() and 0xffffffffL else 0L
        val eventSequence = if (b.remaining() >= 4) b.int.toLong() and 0xffffffffL else 0L
        val eventFromValue = if (b.remaining() >= 1) b.get().toInt() and 0xff else 0
        val eventToValue = if (b.remaining() >= 1) b.get().toInt() and 0xff else 0
        val device = (activeDevice ?: CoronetDevice(id, name)).copy(id = id, name = name)
        val states = arrayOf("unknown", "idle", "printing", "paused", "error", "complete")
        onSnapshot(DeviceSnapshot(device, ConnectionKind.Ble, printer = PrinterSnapshot(
            connected = flags and (1 shl 7) != 0, state = states.getOrElse(stateValue) { "unknown" },
            status = status, filename = filename, progress = progress, tool = tool,
            toolTemp = toolTemp, bedTemp = bedTemp, chamberTemp = chamberTemp,
            telemetryValid = flags and (1 shl 10) != 0, telemetryRevision = telemetryRevision,
            eventSequence = eventSequence,
            eventFrom = states.getOrElse(eventFromValue) { "unknown" },
            eventTo = states.getOrElse(eventToValue) { "unknown" })))
    }

    private fun parseJson(bytes: ByteArray) = runCatching {
        val json = JSONObject(bytes.toString(Charsets.UTF_8))
        when (json.optString("t")) {
            "e" -> onEvent(json.optString("type"), json.optString("msg"))
            "settings" -> onSettings(json)
            "pairing" -> {
                val id = json.optString("id")
                val token = json.optString("token")
                if (id.isNotBlank() && token.isNotBlank() && onPairing(id, token)) send("{\"cmd\":\"confirmPairing\"}")
            }
        }
    }

    private fun hasScanPermission(): Boolean = if (Build.VERSION.SDK_INT >= 31) {
        ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
            hasConnectPermission()
    } else {
        ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
    }

    private fun hasConnectPermission(): Boolean = Build.VERSION.SDK_INT < 31 ||
        ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
}

private fun Int.toTemperature(): Double? = if (this == Short.MIN_VALUE.toInt()) null else this / 10.0
private fun ByteBuffer.readCString(length: Int): String {
    val bytes = ByteArray(length); get(bytes)
    val end = bytes.indexOf(0.toByte()).let { if (it < 0) bytes.size else it }
    return bytes.copyOf(end).toString(Charsets.UTF_8)
}
