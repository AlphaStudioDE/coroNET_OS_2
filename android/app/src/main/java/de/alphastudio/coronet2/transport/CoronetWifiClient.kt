package de.alphastudio.coronet2.transport

import de.alphastudio.coronet2.model.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

class CoronetWifiClient {
    fun fetch(device: CoronetDevice): DeviceSnapshot {
        if (device.host.isBlank() || device.token.isBlank()) return DeviceSnapshot(device = device)
        val connection = URL("http://${device.host}/api/state").openConnection() as HttpURLConnection
        connection.connectTimeout = 1500
        connection.readTimeout = 1800
        connection.setRequestProperty("X-coroNET-Token", device.token)
        return try {
            if (connection.responseCode != 200) throw IllegalStateException("HTTP ${connection.responseCode}")
            val json = JSONObject(connection.inputStream.bufferedReader().use { it.readText() })
            val printer = json.optJSONObject("printer") ?: JSONObject()
            DeviceSnapshot(
                device = device.copy(name = json.optString("name", device.name)),
                connection = ConnectionKind.Wifi,
                firmware = json.optString("firmware", "--"),
                printer = PrinterSnapshot(
                    connected = printer.optBoolean("connected"), state = printer.optString("state", "unknown"),
                    status = printer.optString("status", ""), filename = printer.optString("filename", ""),
                    progress = printer.optInt("progress"), tool = printer.optInt("activeTool"),
                    toolTemp = printer.optDoubleOrNull("activeToolTempC"),
                    bedTemp = printer.optDoubleOrNull("bedTempC"),
                    chamberTemp = printer.optDoubleOrNull("chamberTempC"),
                ),
                fanPercent = json.optInt("fanPercent"), flapPercent = json.optInt("flapPercent"),
                audioPlaying = json.optBoolean("audioPlaying"), quietActive = json.optBoolean("quietActive"),
            )
        } catch (error: Exception) {
            DeviceSnapshot(device = device, error = error.message)
        } finally { connection.disconnect() }
    }

    fun fetchSettings(device: CoronetDevice): DeviceSettings? {
        if (device.host.isBlank() || device.token.isBlank()) return null
        val connection = URL("http://${device.host}/api/settings").openConnection() as HttpURLConnection
        connection.connectTimeout = 1500
        connection.readTimeout = 2200
        connection.setRequestProperty("X-coroNET-Token", device.token)
        return try {
            if (connection.responseCode != 200) return null
            parseSettings(JSONObject(connection.inputStream.bufferedReader().use { it.readText() }))
        } catch (_: Exception) { null } finally { connection.disconnect() }
    }

    fun post(device: CoronetDevice, path: String, body: String = "{}"): Boolean = runCatching {
        val connection = URL("http://${device.host}$path").openConnection() as HttpURLConnection
        connection.requestMethod = "POST"
        connection.connectTimeout = 1500
        connection.readTimeout = 2500
        connection.doOutput = true
        connection.setRequestProperty("Content-Type", "application/json")
        connection.setRequestProperty("X-coroNET-Token", device.token)
        connection.outputStream.use { it.write(body.toByteArray()) }
        val ok = connection.responseCode in 200..299
        connection.disconnect()
        ok
    }.getOrDefault(false)
}

fun parseSettings(json: JSONObject, previous: DeviceSettings = DeviceSettings()): DeviceSettings = previous.copy(
    loaded = true,
    displayBrightness = json.optInt("displayBrightness", json.optInt("brightness", previous.displayBrightness)),
    uiSkin = json.optInt("uiSkin", previous.uiSkin),
    uiColorMode = json.optInt("uiColorMode", json.optInt("uiColor", previous.uiColorMode)),
    accentHueDegrees = json.optInt("accentHueDegrees", previous.accentHueDegrees),
    screenSaverMode = json.optInt("screenSaverMode", previous.screenSaverMode),
    screenSaverDelayMinutes = json.optInt("screenSaverDelayMinutes", previous.screenSaverDelayMinutes),
    clockBrightness = json.optInt("clockBrightness", previous.clockBrightness),
    clockStyle = json.optInt("clockStyle", previous.clockStyle),
    quietTarget = json.optInt("quietTarget", previous.quietTarget),
    quietDurationMinutes = json.optInt("quietDurationMinutes", previous.quietDurationMinutes),
    ledEnabled = json.optBoolean("ledEnabled", previous.ledEnabled),
    ledBrightness = json.optIntList("ledBrightness", previous.ledBrightness),
    insideColorStyle = json.optInt("insideColorStyle", previous.insideColorStyle),
    mirrorLedLayout = json.optBoolean("mirrorLedLayout", previous.mirrorLedLayout),
    soundVolume = json.optIntList("soundVolume", previous.soundVolume),
    ventMode = json.optInt("ventMode", previous.ventMode),
    ventTargetTempC = json.optInt("ventTargetTempC", previous.ventTargetTempC),
    manualFanPercent = json.optInt("manualFanPercent", previous.manualFanPercent),
    manualFlapPercent = json.optInt("manualFlapPercent", previous.manualFlapPercent),
    servoClosedUs = json.optInt("servoClosedUs", previous.servoClosedUs),
    servoOpenUs = json.optInt("servoOpenUs", previous.servoOpenUs),
    servoReverse = json.optBoolean("servoReverse", previous.servoReverse),
    pandaEnabled = json.optBoolean("pandaEnabled", previous.pandaEnabled),
    pandaMode = json.optInt("pandaMode", previous.pandaMode),
    pandaTargetTempC = json.optInt("pandaTargetTempC", previous.pandaTargetTempC),
)

private fun JSONObject.optDoubleOrNull(key: String): Double? =
    if (isNull(key) || !has(key)) null else optDouble(key).takeUnless { it.isNaN() }

private fun JSONObject.optIntList(key: String, fallback: List<Int>): List<Int> {
    val array = optJSONArray(key) ?: return fallback
    return List(array.length()) { array.optInt(it, fallback.getOrElse(it) { 0 }) }
}
