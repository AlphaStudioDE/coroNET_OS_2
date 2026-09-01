package de.alphastudio.coronet2.transport

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
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
    }

    private data class Assembly(val type: Int, val total: Int, val chunks: Array<ByteArray?>)
    private val adapter get() = context.getSystemService(BluetoothManager::class.java)?.adapter
    private val assemblies = mutableMapOf<Int, Assembly>()
    private var gatt: BluetoothGatt? = null
    private var command: BluetoothGattCharacteristic? = null
    private var activeDevice: CoronetDevice? = null
    private val commandWrites = ArrayDeque<ByteArray>()
    private val descriptorWrites = ArrayDeque<BluetoothGattDescriptor>()
    private var commandWriteInFlight = false
    private var subscriptionsReady = false

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.scanRecord?.deviceName ?: runCatching { result.device.name }.getOrNull() ?: return
            val hasService = result.scanRecord?.serviceUuids?.any { it.uuid == ServiceUuid } == true
            if (!hasService && !name.startsWith("coroNET", ignoreCase = true)) return
            onFound(CoronetDevice(id = result.device.address.replace(":", ""), name = name, address = result.device.address))
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(): Boolean {
        if (!hasBlePermission()) return false
        adapter?.bluetoothLeScanner?.startScan(scanCallback) ?: return false
        return true
    }

    @SuppressLint("MissingPermission")
    fun stopScan() { if (hasBlePermission()) adapter?.bluetoothLeScanner?.stopScan(scanCallback) }

    @SuppressLint("MissingPermission")
    fun connect(device: CoronetDevice) {
        if (!hasBlePermission() || device.address.isBlank()) return
        stopScan(); gatt?.close(); activeDevice = device
        synchronized(commandWrites) { commandWrites.clear(); commandWriteInFlight = false }
        descriptorWrites.clear(); subscriptionsReady = false
        gatt = adapter?.getRemoteDevice(device.address)?.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() { gatt?.disconnect(); gatt?.close(); gatt = null; command = null }

    @SuppressLint("MissingPermission")
    fun send(json: String): Boolean {
        if (gatt == null || command == null) return false
        synchronized(commandWrites) { commandWrites.addLast(json.toByteArray()) }
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
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                gatt.requestMtu(247); gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                command = null
                subscriptionsReady = false
                activeDevice?.let { onSnapshot(DeviceSnapshot(device = it)) }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) return
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
            if (descriptorWrites.firstOrNull() == descriptor) descriptorWrites.removeFirst()
            writeNextDescriptor(gatt)
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            synchronized(commandWrites) {
                if (commandWrites.isNotEmpty()) commandWrites.removeFirst()
                commandWriteInFlight = false
            }
            writeNextCommand()
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            acceptFrame(value)
        }
    }

    private fun acceptFrame(frame: ByteArray) {
        if (frame.size < 8 || frame[0].toInt() != 1) return
        val header = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN)
        header.get(); val type = header.get().toInt() and 0xff
        val messageId = header.short.toInt() and 0xffff
        val total = header.short.toInt() and 0xffff
        val index = header.get().toInt() and 0xff
        val count = header.get().toInt() and 0xff
        if (total !in 1..4096 || count !in 1..64 || index >= count) return
        val assembly = assemblies.getOrPut(messageId) { Assembly(type, total, arrayOfNulls(count)) }
        if (assembly.type != type || assembly.total != total || assembly.chunks.size != count) return
        assembly.chunks[index] = frame.copyOfRange(8, frame.size)
        if (assembly.chunks.any { it == null }) return
        val payload = assembly.chunks.filterNotNull().fold(ByteArray(0)) { acc, bytes -> acc + bytes }.copyOf(total)
        assemblies.remove(messageId)
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
        val device = (activeDevice ?: CoronetDevice(id, name)).copy(id = id, name = name)
        val states = arrayOf("unknown", "idle", "printing", "paused", "error", "complete")
        onSnapshot(DeviceSnapshot(device, ConnectionKind.Ble, printer = PrinterSnapshot(
            connected = flags and (1 shl 7) != 0, state = states.getOrElse(stateValue) { "unknown" },
            status = status, filename = filename, progress = progress, tool = tool,
            toolTemp = toolTemp, bedTemp = bedTemp, chamberTemp = chamberTemp)))
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

    private fun hasBlePermission(): Boolean = android.os.Build.VERSION.SDK_INT < 31 ||
        (ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
         ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED)
}

private fun Int.toTemperature(): Double? = if (this == Short.MIN_VALUE.toInt()) null else this / 10.0
private fun ByteBuffer.readCString(length: Int): String {
    val bytes = ByteArray(length); get(bytes)
    val end = bytes.indexOf(0.toByte()).let { if (it < 0) bytes.size else it }
    return bytes.copyOf(end).toString(Charsets.UTF_8)
}
