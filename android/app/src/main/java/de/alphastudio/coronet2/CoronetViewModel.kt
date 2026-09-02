package de.alphastudio.coronet2

import android.app.*
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import de.alphastudio.coronet2.data.DeviceStore
import de.alphastudio.coronet2.model.*
import de.alphastudio.coronet2.transport.CoronetBleManager
import de.alphastudio.coronet2.transport.CoronetWifiClient
import de.alphastudio.coronet2.transport.parseSettings
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong

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
    private val _pairingChallenge = MutableStateFlow<PairingChallenge?>(null)
    val pairingChallenge: StateFlow<PairingChallenge?> = _pairingChallenge
    private val _pairingCandidate = MutableStateFlow<CoronetDevice?>(null)
    val pairingCandidate: StateFlow<CoronetDevice?> = _pairingCandidate
    private val previousPrinterEventSequences = ConcurrentHashMap<String, Long>()
    private var pendingPairingId: String? = null
    private var scanJob: Job? = null
    private var pollingJob: Job? = null
    private var bleSettingsRefreshJob: Job? = null
    private var pairingChallengeJob: Job? = null
    private val settingsMutationMutex = Mutex()
    private val settingsMutationRevision = AtomicLong(0)
    private val pendingSettingsMutations = AtomicInteger(0)
    private val wifiReachable = AtomicBoolean(false)
    @Volatile private var ignoreBleSettingsUntilMs = 0L
    private val ble = CoronetBleManager(application, ::onFound, ::onSnapshot, ::onBleSettings,
        ::onPairingChallenge, ::onPairingResult, ::onEvent)

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
        if (existing >= 0) {
            val saved = _devices.value[existing]
            val selected = saved.copy(
                name = device.name.ifBlank { saved.name },
                address = device.address.ifBlank { saved.address },
            )
            _devices.value = _devices.value.toMutableList().also { it[existing] = selected }
            store.save(_devices.value)
            _pairingCandidate.value = null
            select(selected.id)
            return
        }

        pollingJob?.cancel(); bleSettingsRefreshJob?.cancel(); pairingChallengeJob?.cancel(); ble.disconnect()
        settingsMutationRevision.incrementAndGet()
        wifiReachable.set(false)
        pendingPairingId = null
        _pairingChallenge.value = null
        _pairingCandidate.value = device
        _selectedId.value = device.id
        _snapshot.value = DeviceSnapshot(device = device)
        _settings.value = DeviceSettings()
        ble.connect(device)
        startPairingChallengeRequests()
    }

    fun select(id: String?) {
        _selectedId.value = id
        scanJob?.cancel(); ble.stopScan(); _scanning.value = false
        pollingJob?.cancel(); bleSettingsRefreshJob?.cancel(); pairingChallengeJob?.cancel(); ble.disconnect()
        settingsMutationRevision.incrementAndGet()
        wifiReachable.set(false)
        ignoreBleSettingsUntilMs = 0L
        val device = _devices.value.firstOrNull { it.id == id }
        Log.i("coroNET", "select id=${device?.id.orEmpty()} ble=${device?.address.orEmpty()} wifi=${device?.host.orEmpty()}")
        _snapshot.value = DeviceSnapshot(device = device)
        _settings.value = DeviceSettings()
        if (device == null) return
        if (device.address.isNotBlank()) ble.connect(device)
        if (device.host.isNotBlank() && device.token.isNotBlank()) {
            pollingJob = viewModelScope.launch(Dispatchers.IO) {
                var settingsCountdown = 0
                while (isActive) {
                    val wifiSnapshot = wifi.fetch(device)
                    val reachable = wifiSnapshot.connection == ConnectionKind.Wifi
                    wifiReachable.set(reachable)
                    if (reachable || _snapshot.value.connection != ConnectionKind.Ble) {
                        onSnapshot(wifiSnapshot)
                    }
                    if (reachable && settingsCountdown-- <= 0) {
                        val expectedRevision = settingsMutationRevision.get()
                        val fetched = wifi.fetchSettings(device)
                        if (fetched != null && pendingSettingsMutations.get() == 0 &&
                            settingsMutationRevision.get() == expectedRevision) {
                            _settings.value = fetched
                        }
                        settingsCountdown = 1
                    }
                    delay(1500)
                }
            }
        }
    }

    fun saveDevice(device: CoronetDevice) {
        val index = _devices.value.indexOfFirst { it.id == device.id }
        _devices.value = if (index >= 0) _devices.value.toMutableList().also { it[index] = device }
                         else _devices.value + device
        store.save(_devices.value)
        select(device.id)
    }

    fun removeSelected() {
        pairingChallengeJob?.cancel()
        pendingPairingId = null
        _pairingChallenge.value = null
        _pairingCandidate.value = null
        val removedId = _selectedId.value
        _devices.value = _devices.value.filterNot { it.id == removedId }
        store.save(_devices.value)
        select(_devices.value.firstOrNull()?.id)
    }

    fun sendSettings(json: String) {
        val device = _snapshot.value.device ?: return
        val patch = runCatching { JSONObject(json) }.getOrNull() ?: return
        val connection = when {
            wifiReachable.get() && device.host.isNotBlank() && device.token.isNotBlank() -> ConnectionKind.Wifi
            _snapshot.value.connection == ConnectionKind.Ble -> ConnectionKind.Ble
            else -> ConnectionKind.Offline
        }
        when (connection) {
            ConnectionKind.Wifi -> {
                val revision = settingsMutationRevision.incrementAndGet()
                pendingSettingsMutations.incrementAndGet()
                _settings.update { parseSettings(patch, it) }
                viewModelScope.launch(Dispatchers.IO) {
                    try {
                        settingsMutationMutex.withLock {
                            wifi.post(device, "/api/settings", json)
                            val confirmed = wifi.fetchSettings(device)
                            if (confirmed != null && settingsMutationRevision.get() == revision) {
                                _settings.value = confirmed
                            }
                        }
                    } finally {
                        pendingSettingsMutations.decrementAndGet()
                    }
                }
            }
            ConnectionKind.Ble -> {
                patch.put("cmd", "setSettings")
                if (ble.send(patch.toString())) {
                    settingsMutationRevision.incrementAndGet()
                    _settings.update { parseSettings(patch, it) }
                    ignoreBleSettingsUntilMs = android.os.SystemClock.elapsedRealtime() + 350L
                    bleSettingsRefreshJob?.cancel()
                    bleSettingsRefreshJob = viewModelScope.launch {
                        delay(500)
                        requestBleSettings()
                    }
                }
            }
            ConnectionKind.Offline -> Unit
        }
    }

    fun requestBleSettings() { ble.send("{\"cmd\":\"getSettings\"}") }

    fun confirmPairingCodesMatch() {
        val challenge = _pairingChallenge.value ?: return
        if (ble.confirmPairing(challenge, true)) {
            _pairingChallenge.value = challenge.copy(confirmedOnPhone = true)
        }
    }

    fun cancelPairing() {
        pairingChallengeJob?.cancel()
        _pairingChallenge.value?.let { ble.confirmPairing(it, false) }
        _pairingChallenge.value = null
        _pairingCandidate.value = null
        ble.disconnect()
        select(_devices.value.firstOrNull()?.id)
    }

    private fun onFound(device: CoronetDevice) {
        _discovered.value = (_discovered.value.filterNot { it.address == device.address } + device).sortedBy { it.name }
    }

    private fun onSnapshot(value: DeviceSnapshot) {
        if (value.device?.id != _selectedId.value && value.device?.address != _snapshot.value.device?.address) return
        if (value.connection == ConnectionKind.Ble && wifiReachable.get()) return
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
        if (android.os.SystemClock.elapsedRealtime() < ignoreBleSettingsUntilMs) return
        _settings.update { parseSettings(json, it) }
    }

    private fun onPairingChallenge(challenge: PairingChallenge) {
        val current = _snapshot.value.device ?: return
        pairingChallengeJob?.cancel()
        pollingJob?.cancel()
        wifiReachable.set(false)
        val unpaired = current.copy(id = challenge.deviceId, name = challenge.deviceName, host = "", token = "")
        _devices.value = _devices.value.filterNot { it.id == current.id || it.address == current.address }
        store.save(_devices.value)
        _selectedId.value = unpaired.id
        _pairingCandidate.value = unpaired
        _snapshot.value = _snapshot.value.copy(device = unpaired, connection = ConnectionKind.Ble)
        val existing = _pairingChallenge.value
        _pairingChallenge.value = if (existing?.sessionId == challenge.sessionId && existing.confirmedOnPhone) {
            challenge.copy(confirmedOnPhone = true)
        } else challenge
        Log.i("coroNET", "pairing challenge accepted id=${challenge.deviceId} session=${challenge.sessionId}")
    }

    private fun onPairingResult(id: String, token: String, wifiHost: String, session: Long): Boolean = runCatching {
        val challenge = _pairingChallenge.value ?: return false
        if (challenge.sessionId != session || challenge.deviceId != id || !challenge.confirmedOnPhone) return false
        val current = _snapshot.value.device ?: return false
        val hostname = "coronet-${id.takeLast(4).lowercase()}.local"
        val paired = current.copy(
            id = id,
            token = token,
            host = wifiHost.takeIf(::isValidLocalHost) ?: current.host.ifBlank { hostname },
        )
        _devices.value = _devices.value.filterNot { it.id == current.id || it.address == current.address } + paired
        store.save(_devices.value)
        _selectedId.value = id
        _snapshot.value = _snapshot.value.copy(device = paired)
        pendingPairingId = id
        pairingChallengeJob?.cancel()
        true
    }.getOrDefault(false)

    private fun isValidLocalHost(value: String): Boolean = value.isNotBlank() &&
        value.length <= 64 && value.all { it.isLetterOrDigit() || it == '.' || it == ':' || it == '-' }

    private fun onEvent(type: String, message: String) {
        if (type == "ack" && message == "pairing_confirmed") {
            pairingChallengeJob?.cancel()
            _pairingChallenge.value = null
            _pairingCandidate.value = null
            pendingPairingId?.let { id -> pendingPairingId = null; select(id) }
        }
        if ((type == "ack" && message == "pairing_cancelled") ||
            (type == "error" && (message == "pairing_code_rejected" || message == "pairing_expired"))) {
            pairingChallengeJob?.cancel()
            _pairingChallenge.value = null
            _pairingCandidate.value = null
            ble.disconnect()
            viewModelScope.launch {
                delay(100)
                select(_devices.value.firstOrNull()?.id)
            }
        }
        if (type == "printer_error" || type == "print_complete") notifyPrinter(type, message)
    }

    private fun startPairingChallengeRequests() {
        pairingChallengeJob?.cancel()
        pairingChallengeJob = viewModelScope.launch {
            repeat(120) {
                if (_pairingCandidate.value == null || _pairingChallenge.value != null) return@launch
                ble.requestPairingChallenge()
                delay(1000)
            }
        }
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
        bleSettingsRefreshJob?.cancel()
        pairingChallengeJob?.cancel()
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
