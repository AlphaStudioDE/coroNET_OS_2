package de.alphastudio.coronet2.data

import android.content.Context
import android.content.SharedPreferences
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import de.alphastudio.coronet2.model.*
import org.json.JSONArray
import org.json.JSONObject

data class CachedDeviceState(val snapshot: DeviceSnapshot, val settings: DeviceSettings)

class DeviceStore(context: Context) {
    private val prefs: SharedPreferences = runCatching {
        val masterKey = MasterKey.Builder(context).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build()
        EncryptedSharedPreferences.create(
            context, "coronet_devices_secure", masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }.getOrElse { context.getSharedPreferences("coronet_devices", Context.MODE_PRIVATE) }

    fun load(): List<CoronetDevice> = runCatching {
        val array = JSONArray(prefs.getString("devices", "[]"))
        buildList {
            for (i in 0 until array.length()) {
                val item = array.getJSONObject(i)
                add(CoronetDevice(item.getString("id"), item.optString("name", item.getString("id")),
                    item.optString("address"), item.optString("host"), item.optString("token")))
            }
        }
    }.getOrDefault(emptyList())

    fun save(devices: List<CoronetDevice>) {
        val array = JSONArray()
        devices.forEach { device ->
            array.put(JSONObject().put("id", device.id).put("name", device.name)
                .put("address", device.address).put("host", device.host).put("token", device.token))
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
                .putNullable("toolTemp", printer.toolTemp).putNullable("bedTemp", printer.bedTemp)
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

    private fun cacheKey(deviceId: String) = "cache_$deviceId"
}

private fun JSONObject.putNullable(key: String, value: Double?): JSONObject =
    put(key, value ?: JSONObject.NULL)

private fun JSONObject.optNullableDouble(key: String): Double? =
    if (!has(key) || isNull(key)) null else optDouble(key).takeUnless(Double::isNaN)

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
