package de.alphastudio.coronet2.data

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import de.alphastudio.coronet2.model.*
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap

data class CachedDeviceState(val snapshot: DeviceSnapshot, val settings: DeviceSettings)

class DeviceStore(context: Context) {
    private val legacyPrefs = context.getSharedPreferences("coronet_devices", Context.MODE_PRIVATE)
    private val historyPrefs = context.getSharedPreferences("coronet_temperature_history", Context.MODE_PRIVATE)
    private val historyLocks = ConcurrentHashMap<String, Any>()
    private val securePrefs = runCatching {
        val masterKey = MasterKey.Builder(context).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build()
        EncryptedSharedPreferences.create(
            context, "coronet_devices_secure", masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }
    val secureStorageAvailable: Boolean = securePrefs.isSuccess
    private val prefs: SharedPreferences = securePrefs.getOrElse {
        Log.e("coroNET", "Encrypted device storage is unavailable; pairing tokens will not be persisted", it)
        legacyPrefs
    }

    init {
        if (secureStorageAvailable && !prefs.contains("devices") && legacyPrefs.contains("devices")) {
            val editor = prefs.edit()
            legacyPrefs.all.forEach { (key, value) ->
                when (value) {
                    is String -> editor.putString(key, value)
                    is Boolean -> editor.putBoolean(key, value)
                    is Int -> editor.putInt(key, value)
                    is Long -> editor.putLong(key, value)
                    is Float -> editor.putFloat(key, value)
                }
            }
            editor.apply()
            legacyPrefs.edit().clear().apply()
        } else if (!secureStorageAvailable) {
            runCatching {
                val devices = JSONArray(legacyPrefs.getString("devices", "[]"))
                for (index in 0 until devices.length()) devices.getJSONObject(index).put("token", "")
                legacyPrefs.edit().putString("devices", devices.toString()).apply()
            }
        }
    }

    fun load(): List<CoronetDevice> = runCatching {
        val array = JSONArray(prefs.getString("devices", "[]"))
        buildList {
            for (i in 0 until array.length()) {
                val item = array.getJSONObject(i)
                add(CoronetDevice(item.getString("id"), item.optString("name", item.getString("id")),
                    item.optString("address"), item.optString("host"),
                    item.optString("token").takeIf { secureStorageAvailable }.orEmpty()))
            }
        }
    }.getOrDefault(emptyList())

    fun save(devices: List<CoronetDevice>) {
        val array = JSONArray()
        devices.forEach { device ->
            array.put(JSONObject().put("id", device.id).put("name", device.name)
                .put("address", device.address).put("host", device.host)
                .put("token", device.token.takeIf { secureStorageAvailable }.orEmpty()))
        }
        prefs.edit().putString("devices", array.toString()).apply()
    }

    fun loadCache(device: CoronetDevice): CachedDeviceState? = runCatching {
        val root = JSONObject(prefs.getString(cacheKey(device.id), null) ?: return null)
        val printer = root.getJSONObject("printer")
        val ota = root.optJSONObject("ota") ?: JSONObject()
        val settings = root.getJSONObject("settings")
        CachedDeviceState(
            snapshot = DeviceSnapshot(
                device = device,
                connection = ConnectionKind.Offline,
                firmware = root.optString("firmware", "--"),
                printer = PrinterSnapshot(
                    connected = printer.optBoolean("connected"),
                    state = printer.optString("state", "unknown"),
                    status = printer.optString("status", ""),
                    filename = printer.optString("filename", ""),
                    progress = printer.optInt("progress"),
                    tool = printer.optInt("tool"),
                    toolTemp = printer.optNullableDouble("toolTemp"),
                    toolTemps = printer.optNullableDoubleList("toolTemps", 4).let { values ->
                        if (values.any { it != null }) values else List(4) { index ->
                            printer.optNullableDouble("toolTemp").takeIf { index == printer.optInt("tool").coerceIn(0, 3) }
                        }
                    },
                    bedTemp = printer.optNullableDouble("bedTemp"),
                    chamberTemp = printer.optNullableDouble("chamberTemp"),
                    telemetryValid = printer.optBoolean("telemetryValid"),
                    telemetryRevision = printer.optLong("telemetryRevision"),
                    eventSequence = printer.optLong("eventSequence"),
                    eventFrom = printer.optString("eventFrom", "unknown"),
                    eventTo = printer.optString("eventTo", "unknown"),
                ),
                fanPercent = root.optInt("fanPercent"),
                flapPercent = root.optInt("flapPercent"),
                audioPlaying = root.optBoolean("audioPlaying"),
                quietActive = root.optBoolean("quietActive"),
                ota = OtaSnapshot(
                    state = ota.optInt("state"),
                    progress = ota.optInt("progress"),
                    updateAvailable = ota.optBoolean("available"),
                    availableVersion = ota.optString("version", ""),
                    status = ota.optString("status", "Ready"),
                ),
                updatedAtEpochMs = root.optLong("savedAt"),
                cached = true,
            ),
            settings = DeviceSettings(
                loaded = settings.optBoolean("loaded"),
                revision = settings.optLong("revision"),
                setupDone = settings.optBoolean("setupDone"),
                bleEnabled = settings.optBoolean("bleEnabled", true),
                apiPaired = settings.optBoolean("apiPaired"),
                companionTransport = settings.optInt("companionTransport"),
                wifiSsid = settings.optString("wifiSsid"),
                printerHost = settings.optString("printerHost"),
                printerPort = settings.optInt("printerPort", 7125),
                displayBrightness = settings.optInt("displayBrightness", 80),
                uiSkin = settings.optInt("uiSkin"),
                uiColorMode = settings.optInt("uiColorMode"),
                accentHueDegrees = settings.optInt("accentHueDegrees", 190),
                screenSaverMode = settings.optInt("screenSaverMode", 2),
                screenSaverDelayMinutes = settings.optInt("screenSaverDelayMinutes", 5),
                clockBrightness = settings.optInt("clockBrightness", 35),
                clockStyle = settings.optInt("clockStyle"),
                clock24Hour = settings.optBoolean("clock24Hour", true),
                timeZone = settings.optString("timeZone", "CET-1CEST,M3.5.0,M10.5.0/3"),
                quietTarget = settings.optInt("quietTarget"),
                quietDurationMinutes = settings.optInt("quietDurationMinutes", 60),
                quietErrorsBypass = settings.optBoolean("quietErrorsBypass", true),
                ledEnabled = settings.optBoolean("ledEnabled", true),
                ledOtherMode = settings.optBoolean("ledOtherMode"),
                ledBrightness = settings.optIntList("ledBrightness", listOf(70, 70, 70, 70)),
                ledDimmEnabled = settings.optBooleanList("ledDimmEnabled", List(4) { false }),
                ledDimmPercent = settings.optIntList("ledDimmPercent", List(4) { 20 }),
                insideColorStyle = settings.optInt("insideColorStyle"),
                mirrorLedLayout = settings.optBoolean("mirrorLedLayout"),
                ledAnimation = settings.optIntList("ledAnimation", List(6) { 0 }),
                ledColorRemixDegrees = settings.optIntList("ledColorRemixDegrees", List(6) { 0 }),
                ledCalibrationHue = settings.optIntList("ledCalibrationHue", List(8) { 0 }),
                ledCalibrationSaturation = settings.optIntList("ledCalibrationSaturation", List(8) { 100 }),
                ledCalibrationBrightness = settings.optIntList("ledCalibrationBrightness", List(8) { 100 }),
                soundVolume = settings.optIntList("soundVolume", listOf(75, 75, 85, 70, 60)),
                soundRepeat = settings.optBooleanList("soundRepeat", listOf(false, false, true, false, false)),
                soundPath = settings.optStringList("soundPath", List(5) { "" }),
                ventMode = settings.optInt("ventMode"),
                ventTargetTempC = settings.optInt("ventTargetTempC", 40),
                manualFanPercent = settings.optInt("manualFanPercent"),
                manualFlapPercent = settings.optInt("manualFlapPercent"),
                fanMinPercent = settings.optInt("fanMinPercent", 30),
                fanMaxPercent = settings.optInt("fanMaxPercent", 100),
                failsafeFanPercent = settings.optInt("failsafeFanPercent", 100),
                failsafeFlapPercent = settings.optInt("failsafeFlapPercent", 100),
                servoClosedUs = settings.optInt("servoClosedUs", 1000),
                servoOpenUs = settings.optInt("servoOpenUs", 2000),
                servoReverse = settings.optBoolean("servoReverse"),
                diyHeaterOutputHigh = settings.optBoolean("diyHeaterOutputHigh"),
                pandaEnabled = settings.optBoolean("pandaEnabled"),
                pandaHost = settings.optString("pandaHost", ""),
                pandaMode = settings.optInt("pandaMode"),
                pandaTargetTempC = settings.optInt("pandaTargetTempC", 40),
                pandaPrintTargetTempC = settings.optInt("pandaPrintTargetTempC", 40),
                pandaDryPreset = settings.optInt("pandaDryPreset"),
                pandaDryHours = settings.optInt("pandaDryHours", 12),
                pandaPreheatHoldMinutes = settings.optInt("pandaPreheatHoldMinutes", 15),
                pandaTemperingDurationMinutes = settings.optInt("pandaTemperingDurationMinutes", 30),
                pandaTemperingEndTempC = settings.optInt("pandaTemperingEndTempC"),
                pandaTemperingAfterPrint = settings.optBoolean("pandaTemperingAfterPrint"),
            ),
        )
    }.getOrNull()

    fun saveCache(deviceId: String, snapshot: DeviceSnapshot, settings: DeviceSettings) {
        if (deviceId.isBlank()) return
        val printer = snapshot.printer
        val root = JSONObject()
            .put("savedAt", System.currentTimeMillis())
            .put("firmware", snapshot.firmware)
            .put("fanPercent", snapshot.fanPercent)
            .put("flapPercent", snapshot.flapPercent)
            .put("audioPlaying", snapshot.audioPlaying)
            .put("quietActive", snapshot.quietActive)
            .put("printer", JSONObject()
                .put("connected", printer.connected).put("state", printer.state)
                .put("status", printer.status).put("filename", printer.filename)
                .put("progress", printer.progress).put("tool", printer.tool)
                .putNullable("toolTemp", printer.toolTemp).put("toolTemps", printer.toolTemps.toJsonArray())
                .putNullable("bedTemp", printer.bedTemp)
                .putNullable("chamberTemp", printer.chamberTemp)
                .put("telemetryValid", printer.telemetryValid)
                .put("telemetryRevision", printer.telemetryRevision)
                .put("eventSequence", printer.eventSequence)
                .put("eventFrom", printer.eventFrom).put("eventTo", printer.eventTo))
            .put("ota", JSONObject().put("state", snapshot.ota.state)
                .put("progress", snapshot.ota.progress).put("available", snapshot.ota.updateAvailable)
                .put("version", snapshot.ota.availableVersion).put("status", snapshot.ota.status))
            .put("settings", JSONObject()
                .put("loaded", settings.loaded).put("revision", settings.revision)
                .put("setupDone", settings.setupDone).put("bleEnabled", settings.bleEnabled)
                .put("apiPaired", settings.apiPaired).put("companionTransport", settings.companionTransport)
                .put("wifiSsid", settings.wifiSsid).put("printerHost", settings.printerHost)
                .put("printerPort", settings.printerPort)
                .put("displayBrightness", settings.displayBrightness)
                .put("uiSkin", settings.uiSkin).put("uiColorMode", settings.uiColorMode)
                .put("accentHueDegrees", settings.accentHueDegrees)
                .put("screenSaverMode", settings.screenSaverMode)
                .put("screenSaverDelayMinutes", settings.screenSaverDelayMinutes)
                .put("clockBrightness", settings.clockBrightness).put("clockStyle", settings.clockStyle)
                .put("clock24Hour", settings.clock24Hour).put("timeZone", settings.timeZone)
                .put("quietTarget", settings.quietTarget).put("quietDurationMinutes", settings.quietDurationMinutes)
                .put("quietErrorsBypass", settings.quietErrorsBypass)
                .put("ledEnabled", settings.ledEnabled).put("ledOtherMode", settings.ledOtherMode)
                .put("ledBrightness", JSONArray(settings.ledBrightness))
                .put("ledDimmEnabled", JSONArray(settings.ledDimmEnabled))
                .put("ledDimmPercent", JSONArray(settings.ledDimmPercent))
                .put("insideColorStyle", settings.insideColorStyle).put("mirrorLedLayout", settings.mirrorLedLayout)
                .put("ledAnimation", JSONArray(settings.ledAnimation))
                .put("ledColorRemixDegrees", JSONArray(settings.ledColorRemixDegrees))
                .put("ledCalibrationHue", JSONArray(settings.ledCalibrationHue))
                .put("ledCalibrationSaturation", JSONArray(settings.ledCalibrationSaturation))
                .put("ledCalibrationBrightness", JSONArray(settings.ledCalibrationBrightness))
                .put("soundVolume", JSONArray(settings.soundVolume))
                .put("soundRepeat", JSONArray(settings.soundRepeat)).put("soundPath", JSONArray(settings.soundPath))
                .put("ventMode", settings.ventMode)
                .put("ventTargetTempC", settings.ventTargetTempC)
                .put("manualFanPercent", settings.manualFanPercent)
                .put("manualFlapPercent", settings.manualFlapPercent)
                .put("fanMinPercent", settings.fanMinPercent).put("fanMaxPercent", settings.fanMaxPercent)
                .put("failsafeFanPercent", settings.failsafeFanPercent).put("failsafeFlapPercent", settings.failsafeFlapPercent)
                .put("servoClosedUs", settings.servoClosedUs).put("servoOpenUs", settings.servoOpenUs)
                .put("servoReverse", settings.servoReverse)
                .put("diyHeaterOutputHigh", settings.diyHeaterOutputHigh)
                .put("pandaEnabled", settings.pandaEnabled).put("pandaHost", settings.pandaHost)
                .put("pandaMode", settings.pandaMode).put("pandaTargetTempC", settings.pandaTargetTempC)
                .put("pandaPrintTargetTempC", settings.pandaPrintTargetTempC)
                .put("pandaDryPreset", settings.pandaDryPreset).put("pandaDryHours", settings.pandaDryHours)
                .put("pandaPreheatHoldMinutes", settings.pandaPreheatHoldMinutes)
                .put("pandaTemperingDurationMinutes", settings.pandaTemperingDurationMinutes)
                .put("pandaTemperingEndTempC", settings.pandaTemperingEndTempC)
                .put("pandaTemperingAfterPrint", settings.pandaTemperingAfterPrint))
        prefs.edit().putString(cacheKey(deviceId), root.toString()).apply()
    }

    fun clearCache(deviceId: String?) {
        if (!deviceId.isNullOrBlank()) prefs.edit().remove(cacheKey(deviceId)).apply()
    }

    fun loadTemperatureHistory(deviceId: String): List<TemperatureSample> = synchronized(historyLock(deviceId)) { runCatching {
        val cutoff = System.currentTimeMillis() - TemperatureHistoryDurationMs
        val array = JSONArray(historyPrefs.getString(historyKey(deviceId), "[]"))
        buildList {
            for (index in 0 until array.length()) {
                val sample = when (val item = array.opt(index)) {
                    is JSONArray -> TemperatureSample(
                        timestampEpochMs = item.optLong(0),
                        telemetryRevision = item.optLong(1),
                        toolTemps = item.optNullableDoubleList(2, 4),
                        bedTemp = item.optNullableDouble(3),
                        chamberTemp = item.optNullableDouble(4),
                    )
                    is JSONObject -> TemperatureSample(
                        timestampEpochMs = item.optLong("timestamp"),
                        telemetryRevision = item.optLong("revision"),
                        toolTemps = item.optNullableDoubleList("tools", 4),
                        bedTemp = item.optNullableDouble("bed"),
                        chamberTemp = item.optNullableDouble("chamber"),
                    )
                    else -> continue
                }
                if (sample.timestampEpochMs >= cutoff) add(sample)
            }
        }.takeLast(MaxTemperatureSamples)
    }.getOrDefault(emptyList()) }

    fun saveTemperatureHistory(deviceId: String, samples: List<TemperatureSample>) = synchronized(historyLock(deviceId)) {
        if (deviceId.isBlank()) return
        val cutoff = System.currentTimeMillis() - TemperatureHistoryDurationMs
        val array = JSONArray()
        samples.asSequence().filter { it.timestampEpochMs >= cutoff }
            .toList().takeLast(MaxTemperatureSamples).forEach { sample ->
                array.put(JSONArray()
                    .put(sample.timestampEpochMs)
                    .put(sample.telemetryRevision)
                    .put(sample.toolTemps.toJsonArray())
                    .put(sample.bedTemp ?: JSONObject.NULL)
                    .put(sample.chamberTemp ?: JSONObject.NULL))
            }
        historyPrefs.edit().putString(historyKey(deviceId), array.toString()).apply()
    }

    fun clearTemperatureHistory(deviceId: String?) {
        if (deviceId.isNullOrBlank()) return
        synchronized(historyLock(deviceId)) {
            historyPrefs.edit().remove(historyKey(deviceId)).apply()
        }
        historyLocks.remove(deviceId)
    }

    private fun cacheKey(deviceId: String) = "cache_$deviceId"
    private fun historyKey(deviceId: String) = "temperature_$deviceId"
    private fun historyLock(deviceId: String): Any = historyLocks.getOrPut(deviceId) { Any() }

    private companion object {
        const val TemperatureHistoryDurationMs = 2L * 60L * 60L * 1000L
        const val MaxTemperatureSamples = 14_400
    }
}

private fun JSONObject.putNullable(key: String, value: Double?): JSONObject =
    put(key, value ?: JSONObject.NULL)

private fun JSONObject.optNullableDouble(key: String): Double? =
    if (!has(key) || isNull(key)) null else optDouble(key).takeUnless(Double::isNaN)

private fun JSONObject.optNullableDoubleList(key: String, size: Int): List<Double?> {
    val array = optJSONArray(key) ?: return List(size) { null }
    return List(size) { index ->
        if (index >= array.length() || array.isNull(index)) null
        else array.optDouble(index).takeUnless(Double::isNaN)
    }
}

private fun JSONArray.optNullableDouble(index: Int): Double? =
    if (index >= length() || isNull(index)) null else optDouble(index).takeUnless(Double::isNaN)

private fun JSONArray.optNullableDoubleList(index: Int, size: Int): List<Double?> {
    val array = optJSONArray(index) ?: return List(size) { null }
    return List(size) { item -> array.optNullableDouble(item) }
}

private fun List<Double?>.toJsonArray(): JSONArray = JSONArray().also { array ->
    forEach { value -> array.put(value ?: JSONObject.NULL) }
}

private fun JSONObject.optIntList(key: String, fallback: List<Int>): List<Int> {
    val array = optJSONArray(key) ?: return fallback
    return List(array.length()) { array.optInt(it, fallback.getOrElse(it) { 0 }) }
}

private fun JSONObject.optBooleanList(key: String, fallback: List<Boolean>): List<Boolean> {
    val array = optJSONArray(key) ?: return fallback
    return List(array.length()) { array.optBoolean(it, fallback.getOrElse(it) { false }) }
}

private fun JSONObject.optStringList(key: String, fallback: List<String>): List<String> {
    val array = optJSONArray(key) ?: return fallback
    return List(array.length()) { array.optString(it, fallback.getOrElse(it) { "" }) }
}
