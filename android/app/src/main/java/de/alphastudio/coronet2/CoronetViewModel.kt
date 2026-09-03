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
import java.util.concurrent.atomic.AtomicLong

class CoronetViewModel(application: Application) : AndroidViewModel(application) {
    private data class PendingSetting(val mutation: Long, val value: Any, val createdAtMs: Long)

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
    private val wifiReachable = AtomicBoolean(false)
    private val pendingSettingsLock = Any()
    private val pendingSettings = mutableMapOf<String, PendingSetting>()
    private val lastCacheWriteMs = ConcurrentHashMap<String, Long>()
    @Volatile private var settingsRevisionSeen = 0L
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
        clearAllPendingSettings()
        settingsRevisionSeen = 0L
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
        clearAllPendingSettings()
        wifiReachable.set(false)
        val device = _devices.value.firstOrNull { it.id == id }
        Log.i("coroNET", "select id=${device?.id.orEmpty()} ble=${device?.address.orEmpty()} wifi=${device?.host.orEmpty()}")
        val cached = device?.let(store::loadCache)
        _snapshot.value = cached?.snapshot ?: DeviceSnapshot(device = device)
        _settings.value = cached?.settings ?: DeviceSettings()
        settingsRevisionSeen = 0L
        if (device == null) return
        if (device.address.isNotBlank()) ble.connect(device)
        if (device.token.isNotBlank()) {
            pollingJob = viewModelScope.launch(Dispatchers.IO) {
                var settingsCountdown = 0
                while (isActive) {
                    val currentDevice = _devices.value.firstOrNull { it.id == device.id } ?: device
                    val wifiSnapshot = wifi.fetch(currentDevice)
                    val reachable = wifiSnapshot.connection == ConnectionKind.Wifi
                    wifiReachable.set(reachable)
                    if (reachable || _snapshot.value.connection != ConnectionKind.Ble) {
                        onSnapshot(wifiSnapshot)
                    }
                    if (reachable && settingsCountdown-- <= 0) {
                        val fetched = wifi.fetchSettings(currentDevice)
                        if (fetched != null && _selectedId.value == device.id) acceptRemoteSettings(fetched)
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
        store.clearCache(removedId)
        removedId?.let(lastCacheWriteMs::remove)
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
                rememberPendingSettings(patch, revision)
                _settings.update { parseSettings(patch, it) }
                persistCurrentState(true)
                viewModelScope.launch(Dispatchers.IO) {
                    settingsMutationMutex.withLock {
                        val posted = wifi.post(device, "/api/settings", json)
                        val confirmed = wifi.fetchSettings(device)
                        clearPendingSettings(revision)
                        if (_selectedId.value == device.id && confirmed != null) {
                            acceptRemoteSettings(confirmed)
                        } else if (!posted && _selectedId.value == device.id) {
                            _snapshot.update { it.copy(error = "Setting change could not be delivered") }
                        }
                    }
                }
            }
            ConnectionKind.Ble -> {
                val revision = settingsMutationRevision.incrementAndGet()
                val command = JSONObject(patch.toString()).put("cmd", "setSettings")
                if (ble.send(command.toString())) {
                    rememberPendingSettings(patch, revision)
                    _settings.update { parseSettings(patch, it) }
                    persistCurrentState(true)
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

    fun sendOtaAction(action: String) {
        val device = _snapshot.value.device ?: return
        val path = when (action) {
            "check" -> "/api/ota/check"
            "install" -> "/api/ota/install"
            "reinstall" -> "/api/ota/reinstall"
            else -> return
        }
        if (!wifiReachable.get() || device.host.isBlank() || device.token.isBlank()) {
            _snapshot.update {
                it.copy(ota = it.ota.copy(state = 8, status = "Connect to coroNET over Wi-Fi to update"))
            }
            return
        }
        if (_snapshot.value.ota.busy) return

        val initialState = if (action == "check") 1 else 4
        val initialStatus = if (action == "check") "Checking GitHub releases" else "Preparing update"
        _snapshot.update {
            it.copy(ota = it.ota.copy(state = initialState, progress = 0, status = initialStatus))
        }
        viewModelScope.launch(Dispatchers.IO) {
            if (!wifi.post(device, path)) {
                _snapshot.update {
                    it.copy(ota = it.ota.copy(state = 8, progress = 0, status = "Update request failed"))
                }
            }
        }
    }

    fun previewLed(category: Int, animation: Int) {
        val device = _snapshot.value.device ?: return
        val payload = JSONObject()
            .put("category", category.coerceIn(0, 5))
            .put("animation", animation.coerceIn(0, 255))
            .put("durationMs", 10000)
        if (wifiReachable.get() && device.host.isNotBlank() && device.token.isNotBlank()) {
            viewModelScope.launch(Dispatchers.IO) {
                if (!wifi.post(device, "/api/led/preview", payload.toString())) {
                    _snapshot.update { it.copy(error = "LED preview could not be started") }
                }
            }
        } else if (_snapshot.value.connection == ConnectionKind.Ble) {
            payload.put("cmd", "previewLed")
            if (!ble.send(payload.toString())) {
                _snapshot.update { it.copy(error = "LED preview could not be delivered") }
            }
        }
    }

    fun calibrateLed(active: Boolean, color: Int) {
        val device = _snapshot.value.device ?: return
        val payload = JSONObject().put("active", active).put("color", color.coerceIn(0, 7))
        if (wifiReachable.get() && device.host.isNotBlank() && device.token.isNotBlank()) {
            viewModelScope.launch(Dispatchers.IO) {
                wifi.post(device, "/api/led/calibration", payload.toString())
            }
        } else if (_snapshot.value.connection == ConnectionKind.Ble) {
            payload.put("cmd", "calibrateLed")
            ble.send(payload.toString())
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
        val current = _snapshot.value
        if (value.connection == ConnectionKind.Offline) {
            if (wifiReachable.get() && current.connection == ConnectionKind.Wifi) return
            settingsRevisionSeen = 0L
            _snapshot.value = current.copy(
                connection = ConnectionKind.Offline,
                error = value.error,
                cached = current.updatedAtEpochMs > 0,
            )
            return
        }
        val effective = when {
            value.connection == ConnectionKind.Ble -> value.copy(
                firmware = current.firmware,
                fanPercent = current.fanPercent,
                flapPercent = current.flapPercent,
                audioPlaying = current.audioPlaying,
                quietActive = current.quietActive,
                ota = current.ota,
            )
            current.ota.busy && value.connection != ConnectionKind.Wifi -> value.copy(ota = current.ota)
            else -> value
        }
        val live = effective.copy(
            updatedAtEpochMs = System.currentTimeMillis(),
            cached = false,
            error = null,
        )
        val stateKey = live.device?.id?.takeIf { it.isNotBlank() }
            ?: live.device?.address?.takeIf { it.isNotBlank() }
            ?: return
        val sequence = live.printer.eventSequence
        val previousSequence = previousPrinterEventSequences.put(stateKey, sequence)
        if (live.printer.telemetryValid && previousSequence != null && sequence != previousSequence &&
            (live.printer.eventTo == "error" || live.printer.eventTo == "complete")) {
            notifyPrinter(live.printer.eventTo, live.printer.filename)
        }
        _snapshot.value = live
        live.device?.let(::rememberResolvedDevice)
        persistCurrentState()
    }

    private fun onBleSettings(json: JSONObject) {
        val incomingRevision = json.optLong("sr", 0L)
        if (incomingRevision > 0L && incomingRevision < settingsRevisionSeen) return
        acknowledgeMatchingPendingSettings(json)
        acceptRemoteSettings(parseSettings(json, _settings.value))
    }

    private fun acceptRemoteSettings(remote: DeviceSettings) {
        if (remote.revision > 0L && remote.revision < settingsRevisionSeen) return
        if (remote.revision > settingsRevisionSeen) settingsRevisionSeen = remote.revision
        _settings.value = applyPendingSettings(remote)
        persistCurrentState()
    }

    private fun rememberPendingSettings(patch: JSONObject, mutation: Long) {
        val now = android.os.SystemClock.elapsedRealtime()
        synchronized(pendingSettingsLock) {
            patch.keys().forEach { key ->
                if (key != "cmd") pendingSettings[key] = PendingSetting(mutation, cloneJsonValue(patch.opt(key)), now)
            }
        }
    }

    private fun acknowledgeMatchingPendingSettings(remote: JSONObject) {
        val now = android.os.SystemClock.elapsedRealtime()
        synchronized(pendingSettingsLock) {
            pendingSettings.entries.removeAll { (key, pending) ->
                now - pending.createdAtMs > 5000L ||
                    (remote.has(key) && jsonValuesEqual(remote.opt(key), pending.value))
            }
        }
    }

    private fun clearPendingSettings(mutation: Long) {
        synchronized(pendingSettingsLock) {
            pendingSettings.entries.removeAll { it.value.mutation == mutation }
        }
    }

    private fun clearAllPendingSettings() = synchronized(pendingSettingsLock) { pendingSettings.clear() }

    private fun applyPendingSettings(remote: DeviceSettings): DeviceSettings {
        val patch = JSONObject()
        val now = android.os.SystemClock.elapsedRealtime()
        synchronized(pendingSettingsLock) {
            pendingSettings.entries.removeAll { now - it.value.createdAtMs > 5000L }
            pendingSettings.forEach { (key, pending) -> patch.put(key, cloneJsonValue(pending.value)) }
        }
        return if (patch.length() == 0) remote else parseSettings(patch, remote)
    }

    private fun cloneJsonValue(value: Any?): Any = when (value) {
        is org.json.JSONArray -> org.json.JSONArray(value.toString())
        is JSONObject -> JSONObject(value.toString())
        null -> JSONObject.NULL
        else -> value
    }

    private fun jsonValuesEqual(left: Any?, right: Any?): Boolean = when {
        left === JSONObject.NULL && right === JSONObject.NULL -> true
        left is org.json.JSONArray && right is org.json.JSONArray -> left.toString() == right.toString()
        left is JSONObject && right is JSONObject -> left.toString() == right.toString()
        else -> left?.toString() == right?.toString()
    }

    private fun persistCurrentState(force: Boolean = false) {
        val device = _snapshot.value.device ?: return
        if (_snapshot.value.updatedAtEpochMs <= 0L) return
        val now = android.os.SystemClock.elapsedRealtime()
        val previous = lastCacheWriteMs[device.id] ?: 0L
        if (!force && now - previous < 5000L) return
        lastCacheWriteMs[device.id] = now
        store.saveCache(device.id, _snapshot.value, _settings.value)
    }

    private fun rememberResolvedDevice(device: CoronetDevice) {
        val index = _devices.value.indexOfFirst { it.id == device.id }
        if (index < 0) return
        val saved = _devices.value[index]
        if (saved.host == device.host && saved.name == device.name) return
        _devices.value = _devices.value.toMutableList().also {
            it[index] = saved.copy(name = device.name, host = device.host)
        }
        store.save(_devices.value)
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
