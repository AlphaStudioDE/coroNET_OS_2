package de.alphastudio.coronet2.model

enum class ConnectionKind { Offline, Ble, Wifi }

data class CoronetDevice(
    val id: String,
    val name: String,
    val address: String = "",
    val host: String = "",
    val token: String = "",
)

data class PairingChallenge(
    val deviceId: String,
    val deviceName: String,
    val sessionId: Long,
    val code: Int,
    val expiresMs: Long,
    val confirmedOnPhone: Boolean = false,
)

data class PrinterSnapshot(
    val connected: Boolean = false,
    val state: String = "unknown",
    val status: String = "Waiting for coroNET",
    val filename: String = "",
    val progress: Int = 0,
    val tool: Int = 0,
    val toolTemp: Double? = null,
    val toolTemps: List<Double?> = List(4) { null },
    val bedTemp: Double? = null,
    val chamberTemp: Double? = null,
    val telemetryValid: Boolean = false,
    val telemetryRevision: Long = 0,
    val eventSequence: Long = 0,
    val eventFrom: String = "unknown",
    val eventTo: String = "unknown",
)

data class TemperatureSample(
    val timestampEpochMs: Long,
    val telemetryRevision: Long,
    val toolTemps: List<Double?> = List(4) { null },
    val bedTemp: Double? = null,
    val chamberTemp: Double? = null,
)

data class OtaSnapshot(
    val state: Int = 0,
    val progress: Int = 0,
    val updateAvailable: Boolean = false,
    val availableVersion: String = "",
    val status: String = "Ready",
) {
    val busy: Boolean get() = state == 1 || state in 4..6
}

data class SoundFileEntry(
    val name: String,
    val path: String,
)

data class SoundLibrarySnapshot(
    val loaded: Boolean = false,
    val loading: Boolean = false,
    val sdReady: Boolean = false,
    val folder: Int = 0,
    val folderCount: Int = 0,
    val folderName: String = "",
    val page: Int = 0,
    val pageCount: Int = 1,
    val fileCount: Int = 0,
    val files: List<SoundFileEntry> = emptyList(),
    val error: String? = null,
)

data class LedFrame(
    val sequence: Long = 0,
    val outer: List<Int> = List(42) { 0 },
    val inside: List<Int> = List(18) { 0 },
    val available: Boolean = false,
)

data class DeviceSnapshot(
    val device: CoronetDevice? = null,
    val connection: ConnectionKind = ConnectionKind.Offline,
    val firmware: String = "--",
    val printer: PrinterSnapshot = PrinterSnapshot(),
    val fanPercent: Int = 0,
    val flapPercent: Int = 0,
    val audioPlaying: Boolean = false,
    val quietActive: Boolean = false,
    val ota: OtaSnapshot = OtaSnapshot(),
    val error: String? = null,
    val updatedAtEpochMs: Long = 0,
    val cached: Boolean = false,
)

data class DeviceSettings(
    val loaded: Boolean = false,
    val revision: Long = 0,
    val setupDone: Boolean = false,
    val bleEnabled: Boolean = true,
    val apiPaired: Boolean = false,
    val companionTransport: Int = 0,
    val wifiSsid: String = "",
    val printerHost: String = "",
    val printerPort: Int = 7125,
    val displayBrightness: Int = 80,
    val uiSkin: Int = 0,
    val uiColorMode: Int = 0,
    val accentHueDegrees: Int = 190,
    val screenSaverMode: Int = 2,
    val screenSaverDelayMinutes: Int = 5,
    val clockBrightness: Int = 35,
    val clockStyle: Int = 0,
    val clock24Hour: Boolean = true,
    val timeZone: String = "CET-1CEST,M3.5.0,M10.5.0/3",
    val quietTarget: Int = 0,
    val quietDurationMinutes: Int = 60,
    val quietErrorsBypass: Boolean = true,
    val ledEnabled: Boolean = true,
    val ledOtherMode: Boolean = false,
    val ledBrightness: List<Int> = listOf(70, 70, 70, 70),
    val ledDimmEnabled: List<Boolean> = listOf(false, false, false, false),
    val ledDimmPercent: List<Int> = listOf(20, 20, 20, 20),
    val insideColorStyle: Int = 0,
    val mirrorLedLayout: Boolean = false,
    val ledAnimation: List<Int> = List(6) { 0 },
    val ledColorRemixDegrees: List<Int> = List(6) { 0 },
    val ledCalibrationHue: List<Int> = List(8) { 0 },
    val ledCalibrationSaturation: List<Int> = List(8) { 100 },
    val ledCalibrationBrightness: List<Int> = List(8) { 100 },
    val soundVolume: List<Int> = listOf(75, 75, 85, 70, 60),
    val soundRepeat: List<Boolean> = listOf(false, false, true, false, false),
    val soundPath: List<String> = List(5) { "" },
    val ventMode: Int = 0,
    val ventTargetTempC: Int = 40,
    val manualFanPercent: Int = 0,
    val manualFlapPercent: Int = 0,
    val fanMinPercent: Int = 30,
    val fanMaxPercent: Int = 100,
    val failsafeFanPercent: Int = 100,
    val failsafeFlapPercent: Int = 100,
    val servoClosedUs: Int = 1000,
    val servoOpenUs: Int = 2000,
    val servoReverse: Boolean = false,
    val diyHeaterOutputHigh: Boolean = false,
    val pandaEnabled: Boolean = false,
    val pandaHost: String = "",
    val pandaMode: Int = 0,
    val pandaTargetTempC: Int = 40,
    val pandaPrintTargetTempC: Int = 40,
    val pandaDryPreset: Int = 0,
    val pandaDryHours: Int = 12,
    val pandaPreheatHoldMinutes: Int = 15,
    val pandaTemperingDurationMinutes: Int = 30,
    val pandaTemperingEndTempC: Int = 0,
    val pandaTemperingAfterPrint: Boolean = false,
)
