package de.alphastudio.coronet2

import android.app.*
import androidx.core.app.NotificationCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import de.alphastudio.coronet2.data.DeviceStore
import de.alphastudio.coronet2.model.*
import de.alphastudio.coronet2.transport.CoronetBleManager
import de.alphastudio.coronet2.transport.CoronetWifiClient
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap

class CoronetViewModel(application: Application) : AndroidViewModel(application) {
    private val store = DeviceStore(application)
    private val wifi = CoronetWifiClient()
    private val _devices = MutableStateFlow(store.load())
    val devices: StateFlow<List<CoronetDevice>> = _devices
    private val _discovered = MutableStateFlow<List<CoronetDevice>>(emptyList())
    val discovered: StateFlow<List<CoronetDevice>> = _discovered
    private val _snapshot = MutableStateFlow(DeviceSnapshot())
    val snapshot: StateFlow<DeviceSnapshot> = _snapshot
    private val _settings = MutableStateFlow(DeviceSettings())
    val settings: StateFlow<DeviceSettings> = _settings
    private val _selectedId = MutableStateFlow(_devices.value.firstOrNull()?.id)
    val selectedId: StateFlow<String?> = _selectedId
    private val _scanning = MutableStateFlow(false)
    val scanning: StateFlow<Boolean> = _scanning
    private val previousPrinterEventSequences = ConcurrentHashMap<String, Long>()
    private var pendingPairingId: String? = null
    private var scanJob: Job? = null
    private var pollingJob: Job? = null
    private val ble = CoronetBleManager(application, ::onFound, ::onSnapshot, ::onBleSettings, ::onPairing, ::onEvent)

    init { createNotificationChannel(); select(_selectedId.value) }

    fun startScan() {
        scanJob?.cancel()
        ble.stopScan()
        _discovered.value = emptyList()
        _scanning.value = ble.startScan()
        if (_scanning.value) {
            scanJob = viewModelScope.launch {
                delay(12000)
                ble.stopScan()
                _scanning.value = false
            }
        }
    }

    fun addAndConnect(device: CoronetDevice) {
        scanJob?.cancel()
        ble.stopScan()
        _scanning.value = false
        val existing = _devices.value.indexOfFirst { it.id == device.id || it.address == device.address }
        _devices.value = if (existing >= 0) _devices.value.toMutableList().also { it[existing] = device }
                         else _devices.value + device
        store.save(_devices.value); select(device.id); ble.connect(device)
    }

    fun select(id: String?) {
        _selectedId.value = id
        scanJob?.cancel(); ble.stopScan(); _scanning.value = false
        pollingJob?.cancel(); ble.disconnect()
        val device = _devices.value.firstOrNull { it.id == id }
        _snapshot.value = DeviceSnapshot(device = device)
        _settings.value = DeviceSettings()
        if (device == null) return
        if (device.host.isNotBlank() && device.token.isNotBlank()) {
            pollingJob = viewModelScope.launch(Dispatchers.IO) {
                var settingsCountdown = 0
                while (isActive) {
                    onSnapshot(wifi.fetch(device))
                    if (settingsCountdown-- <= 0) {
                        wifi.fetchSettings(device)?.let { _settings.value = it }
                        settingsCountdown = 1
                    }
                    delay(1500)
                }
            }
        } else if (device.address.isNotBlank()) ble.connect(device)
    }

    fun saveDevice(device: CoronetDevice) {
        val index = _devices.value.indexOfFirst { it.id == device.id }
        _devices.value = if (index >= 0) _devices.value.toMutableList().also { it[index] = device }
                         else _devices.value + device
        store.save(_devices.value)
        select(device.id)
    }

    fun removeSelected() {
        _devices.value = _devices.value.filterNot { it.id == _selectedId.value }
        store.save(_devices.value); select(_devices.value.firstOrNull()?.id)
    }

    fun sendSettings(json: String) {
        val device = _snapshot.value.device ?: return
        if (_snapshot.value.connection == ConnectionKind.Wifi) {
            viewModelScope.launch(Dispatchers.IO) {
                if (wifi.post(device, "/api/settings", json)) {
                    wifi.fetchSettings(device)?.let { _settings.value = it }
                }
            }
        } else {
            val patch = JSONObject(json)
            patch.put("cmd", "setSettings")
            ble.send(patch.toString())
        }
    }

    fun requestBleSettings() { ble.send("{\"cmd\":\"getSettings\"}") }

    private fun onFound(device: CoronetDevice) {
        _discovered.value = (_discovered.value.filterNot { it.address == device.address } + device).sortedBy { it.name }
    }

    private fun onSnapshot(value: DeviceSnapshot) {
        if (value.device?.id != _selectedId.value && value.device?.address != _snapshot.value.device?.address) return
        val stateKey = value.device?.id?.takeIf { it.isNotBlank() }
            ?: value.device?.address?.takeIf { it.isNotBlank() }
            ?: return
        val sequence = value.printer.eventSequence
        val previousSequence = previousPrinterEventSequences.put(stateKey, sequence)
        if (value.printer.telemetryValid && previousSequence != null && sequence != previousSequence &&
            (value.printer.eventTo == "error" || value.printer.eventTo == "complete")) {
            notifyPrinter(value.printer.eventTo, value.printer.filename)
        }
        _snapshot.value = value
    }

    private fun onBleSettings(json: JSONObject) {
        _settings.value = de.alphastudio.coronet2.transport.parseSettings(json, _settings.value)
    }

    private fun onPairing(id: String, token: String): Boolean = runCatching {
        val current = _snapshot.value.device ?: return false
        val hostname = "coronet-${id.takeLast(4).lowercase()}.local"
        val paired = current.copy(id = id, token = token, host = current.host.ifBlank { hostname })
        _devices.value = _devices.value.filterNot { it.id == current.id || it.address == current.address } + paired
        store.save(_devices.value)
        _selectedId.value = id
        _snapshot.value = _snapshot.value.copy(device = paired)
        pendingPairingId = id
        true
    }.getOrDefault(false)

    private fun onEvent(type: String, message: String) {
        if (type == "ack" && message == "pairing_confirmed") {
            pendingPairingId?.let { id -> pendingPairingId = null; select(id) }
        }
        if (type == "printer_error" || type == "print_complete") notifyPrinter(type, message)
    }

    private fun createNotificationChannel() {
        val manager = getApplication<Application>().getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel("printer-events", "Printer events", NotificationManager.IMPORTANCE_HIGH))
    }

    private fun notifyPrinter(event: String, detail: String) {
        val notification = NotificationCompat.Builder(getApplication<Application>(), "printer-events")
            .setSmallIcon(android.R.drawable.stat_notify_error)
            .setContentTitle(if (event.contains("error")) "coroNET: printer attention" else "coroNET: print complete")
            .setContentText(detail.ifBlank { _snapshot.value.device?.name ?: "coroNET" })
            .setPriority(NotificationCompat.PRIORITY_HIGH).setAutoCancel(true).build()
        runCatching {
            getApplication<Application>().getSystemService(NotificationManager::class.java)
                .notify(event.hashCode(), notification)
        }
    }

    override fun onCleared() {
        scanJob?.cancel()
        pollingJob?.cancel()
        ble.disconnect()
        super.onCleared()
    }

    class Factory(private val application: Application) : ViewModelProvider.Factory {
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            require(modelClass.isAssignableFrom(CoronetViewModel::class.java)) { "Unsupported ViewModel" }
            @Suppress("UNCHECKED_CAST")
            return CoronetViewModel(application) as T
        }
    }
}
