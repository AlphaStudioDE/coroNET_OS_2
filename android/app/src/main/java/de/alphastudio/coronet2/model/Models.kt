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
    val bedTemp: Double? = null,
    val chamberTemp: Double? = null,
    val telemetryValid: Boolean = false,
    val telemetryRevision: Long = 0,
    val eventSequence: Long = 0,
    val eventFrom: String = "unknown",
    val eventTo: String = "unknown",
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
)

data class DeviceSettings(
    val loaded: Boolean = false,
    val displayBrightness: Int = 80,
    val uiSkin: Int = 0,
    val uiColorMode: Int = 0,
    val accentHueDegrees: Int = 190,
    val screenSaverMode: Int = 2,
    val screenSaverDelayMinutes: Int = 5,
    val clockBrightness: Int = 35,
    val clockStyle: Int = 0,
    val quietTarget: Int = 0,
    val quietDurationMinutes: Int = 60,
    val ledEnabled: Boolean = true,
    val ledBrightness: List<Int> = listOf(70, 70, 70, 70),
    val insideColorStyle: Int = 0,
    val mirrorLedLayout: Boolean = false,
    val soundVolume: List<Int> = listOf(75, 75, 85, 70, 60),
    val ventMode: Int = 0,
    val ventTargetTempC: Int = 40,
    val manualFanPercent: Int = 0,
    val manualFlapPercent: Int = 0,
    val servoClosedUs: Int = 1000,
    val servoOpenUs: Int = 2000,
    val servoReverse: Boolean = false,
    val pandaEnabled: Boolean = false,
    val pandaMode: Int = 0,
    val pandaTargetTempC: Int = 40,
)
