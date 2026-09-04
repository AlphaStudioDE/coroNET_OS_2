package de.alphastudio.coronet2.transport

import de.alphastudio.coronet2.model.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

class CoronetWifiClient {
    fun fetch(device: CoronetDevice): DeviceSnapshot {
        if (device.token.isBlank()) return DeviceSnapshot(device = device)
        var lastError: String? = null
        for (host in hostCandidates(device)) {
            val result = fetchFromHost(device, host)
            if (result.connection == ConnectionKind.Wifi) return result
            lastError = result.error
        }
        return DeviceSnapshot(device = device, error = lastError ?: "coroNET is offline")
    }

    private fun fetchFromHost(device: CoronetDevice, host: String): DeviceSnapshot {
        var connection: HttpURLConnection? = null
        return try {
            val active = open(device, host, "/api/state", 1800)
            connection = active
            if (active.responseCode != 200) throw IllegalStateException("HTTP ${active.responseCode}")
            val json = JSONObject(active.inputStream.bufferedReader().use { it.readText() })
            val printer = json.optJSONObject("printer") ?: JSONObject()
            val ota = json.optJSONObject("ota") ?: JSONObject()
            DeviceSnapshot(
                device = device.copy(name = json.optString("name", device.name), host = host),
                connection = ConnectionKind.Wifi,
                firmware = json.optString("firmware", "--"),
                printer = PrinterSnapshot(
                    connected = printer.optBoolean("connected"), state = printer.optString("state", "unknown"),
                    status = printer.optString("status", ""), filename = printer.optString("filename", ""),
                    progress = printer.optInt("progress"), tool = printer.optInt("activeTool"),
                    toolTemp = printer.optDoubleOrNull("activeToolTempC"),
                    bedTemp = printer.optDoubleOrNull("bedTempC"),
                    chamberTemp = printer.optDoubleOrNull("chamberTempC"),
                    telemetryValid = printer.optBoolean("telemetryValid"),
                    telemetryRevision = printer.optLong("telemetryRevision"),
                    eventSequence = printer.optLong("eventSequence"),
                    eventFrom = printer.optString("eventFrom", "unknown"),
                    eventTo = printer.optString("eventTo", "unknown"),
                ),
                fanPercent = json.optInt("fanPercent"), flapPercent = json.optInt("flapPercent"),
                audioPlaying = json.optBoolean("audioPlaying"), quietActive = json.optBoolean("quietActive"),
                ota = OtaSnapshot(
                    state = ota.optInt("state"),
                    progress = ota.optInt("progress").coerceIn(0, 100),
                    updateAvailable = ota.optBoolean("available"),
                    availableVersion = ota.optString("version", ""),
                    status = ota.optString("status", "Ready"),
                ),
                updatedAtEpochMs = System.currentTimeMillis(),
            )
        } catch (error: Exception) {
            DeviceSnapshot(device = device, error = error.message)
        } finally { connection?.disconnect() }
    }

    fun fetchSettings(device: CoronetDevice): DeviceSettings? {
        if (device.token.isBlank()) return null
        hostCandidates(device).forEach { host ->
            var connection: HttpURLConnection? = null
            try {
                val active = open(device, host, "/api/settings", 2200)
                connection = active
                if (active.responseCode == 200) {
                    return parseSettings(JSONObject(active.inputStream.bufferedReader().use { it.readText() }))
                }
            } catch (_: Exception) {
                // The saved IP may have changed; try the stable mDNS hostname next.
            } finally {
                connection?.disconnect()
            }
        }
        return null
    }

    fun fetchSoundLibrary(device: CoronetDevice, folder: Int, page: Int): SoundLibrarySnapshot? {
        if (device.token.isBlank()) return null
        hostCandidates(device).forEach { host ->
            var connection: HttpURLConnection? = null
            try {
                val active = open(device, host, "/api/audio/library?folder=$folder&page=$page", 2500)
                connection = active
                if (active.responseCode == 200) {
                    return parseSoundLibrary(JSONObject(active.inputStream.bufferedReader().use { it.readText() }))
                }
            } catch (_: Exception) {
                // Try the next saved IP or mDNS hostname.
            } finally {
                connection?.disconnect()
            }
        }
        return null
    }

    fun fetchLedCatalog(device: CoronetDevice, category: Int): List<String>? {
        if (device.token.isBlank() || category !in 0..5) return null
        hostCandidates(device).forEach { host ->
            var connection: HttpURLConnection? = null
            try {
                val active = open(device, host, "/api/led/catalog?category=$category", 2200)
                connection = active
                if (active.responseCode == 200) {
                    val array = JSONObject(active.inputStream.bufferedReader().use { it.readText() })
                        .optJSONArray("animations") ?: return@forEach
                    return List(array.length()) { index -> array.optString(index) }.filter(String::isNotBlank)
                }
            } catch (_: Exception) {
                // Try the next saved IP or mDNS hostname.
            } finally {
                connection?.disconnect()
            }
        }
        return null
    }

    fun post(device: CoronetDevice, path: String, body: String = "{}"): Boolean {
        for (host in hostCandidates(device)) {
            val ok = runCatching {
                val connection = URL("http://$host$path").openConnection() as HttpURLConnection
                connection.requestMethod = "POST"
                connection.connectTimeout = 1500
                connection.readTimeout = 2500
                connection.doOutput = true
                connection.setRequestProperty("Content-Type", "application/json")
                connection.setRequestProperty("X-coroNET-Token", device.token)
                try {
                    connection.outputStream.use { it.write(body.toByteArray()) }
                    connection.responseCode in 200..299
                } finally {
                    connection.disconnect()
                }
            }.getOrDefault(false)
            if (ok) return true
        }
        return false
    }

    private fun hostCandidates(device: CoronetDevice): List<String> {
        val mdns = device.id.takeIf { it.isNotBlank() }
            ?.let { "coronet-${it.takeLast(4).lowercase()}.local" }
            .orEmpty()
        return listOf(device.host, mdns).filter { it.isNotBlank() }.distinct()
    }

    private fun open(device: CoronetDevice, host: String, path: String, readTimeoutMs: Int): HttpURLConnection =
        (URL("http://$host$path").openConnection() as HttpURLConnection).apply {
            connectTimeout = 1500
            readTimeout = readTimeoutMs
            setRequestProperty("X-coroNET-Token", device.token)
        }
}

fun parseSoundLibrary(json: JSONObject): SoundLibrarySnapshot {
    val filesJson = json.optJSONArray("files")
    val files = if (filesJson == null) emptyList() else List(filesJson.length()) { index ->
        val item = filesJson.optJSONObject(index) ?: JSONObject()
        SoundFileEntry(item.optString("name"), item.optString("path"))
    }.filter { it.name.isNotBlank() && it.path.isNotBlank() }
    return SoundLibrarySnapshot(
        loaded = true,
        sdReady = json.optBoolean("sdReady"),
        folder = json.optInt("folder").coerceAtLeast(0),
        folderCount = json.optInt("folderCount").coerceAtLeast(0),
        folderName = json.optString("folderName"),
        page = json.optInt("page").coerceAtLeast(0),
        pageCount = json.optInt("pageCount", 1).coerceAtLeast(1),
        fileCount = json.optInt("fileCount").coerceAtLeast(0),
        files = files,
    )
}

fun parseSettings(json: JSONObject, previous: DeviceSettings = DeviceSettings()): DeviceSettings = previous.copy(
    loaded = true,
    revision = when {
        json.has("settingsRevision") -> json.optLong("settingsRevision", previous.revision)
        json.has("sr") -> json.optLong("sr", previous.revision)
        else -> previous.revision
    },
    setupDone = json.optBoolean("setupDone", previous.setupDone),
    bleEnabled = json.optBoolean("bleEnabled", previous.bleEnabled),
    apiPaired = json.optBoolean("apiPaired", previous.apiPaired),
    companionTransport = json.optInt("transportValue", json.optInt("transport", previous.companionTransport)),
    wifiSsid = json.optString("wifiSsid", previous.wifiSsid),
    printerHost = json.optString("printerHost", previous.printerHost),
    printerPort = json.optInt("printerPort", previous.printerPort),
    displayBrightness = json.optInt("displayBrightness", json.optInt("brightness", previous.displayBrightness)),
    uiSkin = json.optInt("uiSkin", previous.uiSkin),
    uiColorMode = json.optInt("uiColorMode", json.optInt("uiColor", previous.uiColorMode)),
    accentHueDegrees = json.optInt("accentHueDegrees", previous.accentHueDegrees),
    screenSaverMode = json.optInt("screenSaverMode", previous.screenSaverMode),
    screenSaverDelayMinutes = json.optInt("screenSaverDelayMinutes", previous.screenSaverDelayMinutes),
    clockBrightness = json.optInt("clockBrightness", previous.clockBrightness),
    clockStyle = json.optInt("clockStyle", previous.clockStyle),
    clock24Hour = json.optBoolean("clock24Hour", previous.clock24Hour),
    timeZone = json.optString("timeZone", previous.timeZone),
    quietTarget = json.optInt("quietTarget", previous.quietTarget),
    quietDurationMinutes = json.optInt("quietDurationMinutes", previous.quietDurationMinutes),
    quietErrorsBypass = json.optBoolean("quietErrorsBypass", previous.quietErrorsBypass),
    ledEnabled = json.optBoolean("ledEnabled", previous.ledEnabled),
    ledOtherMode = json.optBoolean("ledOtherMode", previous.ledOtherMode),
    ledBrightness = json.optIntList("ledBrightness", previous.ledBrightness),
    ledDimmEnabled = json.optBooleanList("ledDimmEnabled", previous.ledDimmEnabled),
    ledDimmPercent = json.optIntList("ledDimmPercent", previous.ledDimmPercent),
    insideColorStyle = json.optInt("insideColorStyle", previous.insideColorStyle),
    mirrorLedLayout = json.optBoolean("mirrorLedLayout", previous.mirrorLedLayout),
    ledAnimation = json.optIntList("ledAnimation", previous.ledAnimation),
    ledColorRemixDegrees = json.optIntList("ledColorRemixDegrees", previous.ledColorRemixDegrees),
    ledCalibrationHue = json.optIntList("ledCalibrationHue", previous.ledCalibrationHue),
    ledCalibrationSaturation = json.optIntList("ledCalibrationSaturation", previous.ledCalibrationSaturation),
    ledCalibrationBrightness = json.optIntList("ledCalibrationBrightness", previous.ledCalibrationBrightness),
    soundVolume = json.optIntList("soundVolume", previous.soundVolume),
    soundRepeat = json.optBooleanList("soundRepeat", previous.soundRepeat),
    soundPath = if (json.has("soundPathIndex")) {
        previous.soundPath.toMutableList().also { values ->
            while (values.size < 5) values.add("")
            json.optInt("soundPathIndex", -1).takeIf { it in values.indices }?.let { index ->
                values[index] = json.optString("soundPathValue", values[index])
            }
        }
    } else json.optStringList("soundPath", previous.soundPath),
    ventMode = json.optInt("ventMode", previous.ventMode),
    ventTargetTempC = json.optInt("ventTargetTempC", previous.ventTargetTempC),
    manualFanPercent = json.optInt("manualFanPercent", previous.manualFanPercent),
    manualFlapPercent = json.optInt("manualFlapPercent", previous.manualFlapPercent),
    fanMinPercent = json.optInt("fanMinPercent", previous.fanMinPercent),
    fanMaxPercent = json.optInt("fanMaxPercent", previous.fanMaxPercent),
    failsafeFanPercent = json.optInt("failsafeFanPercent", previous.failsafeFanPercent),
    failsafeFlapPercent = json.optInt("failsafeFlapPercent", previous.failsafeFlapPercent),
    servoClosedUs = json.optInt("servoClosedUs", previous.servoClosedUs),
    servoOpenUs = json.optInt("servoOpenUs", previous.servoOpenUs),
    servoReverse = json.optBoolean("servoReverse", previous.servoReverse),
    diyHeaterOutputHigh = json.optBoolean("diyHeaterOutputHigh", previous.diyHeaterOutputHigh),
    pandaEnabled = json.optBoolean("pandaEnabled", previous.pandaEnabled),
    pandaHost = json.optString("pandaHost", previous.pandaHost),
    pandaMode = json.optInt("pandaMode", previous.pandaMode),
    pandaTargetTempC = json.optInt("pandaTargetTempC", previous.pandaTargetTempC),
    pandaPrintTargetTempC = json.optInt("pandaPrintTargetTempC", previous.pandaPrintTargetTempC),
    pandaDryPreset = json.optInt("pandaDryPreset", previous.pandaDryPreset),
    pandaDryHours = json.optInt("pandaDryHours", previous.pandaDryHours),
    pandaPreheatHoldMinutes = json.optInt("pandaPreheatHoldMinutes", previous.pandaPreheatHoldMinutes),
    pandaTemperingDurationMinutes = json.optInt("pandaTemperingDurationMinutes", previous.pandaTemperingDurationMinutes),
    pandaTemperingEndTempC = json.optInt("pandaTemperingEndTempC", previous.pandaTemperingEndTempC),
    pandaTemperingAfterPrint = json.optBoolean("pandaTemperingAfterPrint", previous.pandaTemperingAfterPrint),
)

private fun JSONObject.optDoubleOrNull(key: String): Double? =
    if (isNull(key) || !has(key)) null else optDouble(key).takeUnless { it.isNaN() }

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
