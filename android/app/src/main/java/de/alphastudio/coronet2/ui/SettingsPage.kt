package de.alphastudio.coronet2.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.ConnectionKind
import de.alphastudio.coronet2.model.DeviceSettings
import de.alphastudio.coronet2.model.DeviceSnapshot

private data class TimeZoneChoice(val label: String, val offset: String, val spec: String)

private val timeZoneChoices = listOf(
    TimeZoneChoice("Apia / Nuku'alofa", "UTC+13", "<+13>-13"),
    TimeZoneChoice("Auckland", "UTC+12/+13", "NZST-12NZDT,M9.5.0,M4.1.0/3"),
    TimeZoneChoice("Fiji", "UTC+12", "<+12>-12"),
    TimeZoneChoice("Honiara", "UTC+11", "<+11>-11"),
    TimeZoneChoice("Sydney / Melbourne", "UTC+10/+11", "AEST-10AEDT,M10.1.0,M4.1.0/3"),
    TimeZoneChoice("Adelaide", "UTC+9:30/+10:30", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"),
    TimeZoneChoice("Darwin", "UTC+9:30", "ACST-9:30"),
    TimeZoneChoice("Tokyo / Seoul", "UTC+9", "JST-9"),
    TimeZoneChoice("Shanghai / Singapore", "UTC+8", "CST-8"),
    TimeZoneChoice("Bangkok / Jakarta", "UTC+7", "<+07>-7"),
    TimeZoneChoice("Yangon", "UTC+6:30", "<+0630>-6:30"),
    TimeZoneChoice("Dhaka", "UTC+6", "<+06>-6"),
    TimeZoneChoice("Kolkata / Mumbai", "UTC+5:30", "IST-5:30"),
    TimeZoneChoice("Almaty / Karachi / Tashkent", "UTC+5", "<+05>-5"),
    TimeZoneChoice("Dubai / Muscat", "UTC+4", "<+04>-4"),
    TimeZoneChoice("Moscow / Minsk", "UTC+3", "MSK-3"),
    TimeZoneChoice("Cairo", "UTC+2/+3", "EET-2EEST,M4.5.5/0,M10.5.4/24"),
    TimeZoneChoice("Helsinki / Athens / Kyiv", "UTC+2/+3", "EET-2EEST,M3.5.0/3,M10.5.0/4"),
    TimeZoneChoice("Johannesburg", "UTC+2", "SAST-2"),
    TimeZoneChoice("Berlin / Warsaw", "UTC+1/+2", "CET-1CEST,M3.5.0,M10.5.0/3"),
    TimeZoneChoice("London / Dublin", "UTC+0/+1", "GMT0BST,M3.5.0/1,M10.5.0"),
    TimeZoneChoice("UTC / Reykjavik", "UTC+0", "UTC0"),
    TimeZoneChoice("Azores", "UTC-1/+0", "<-01>1<+00>,M3.5.0/0,M10.5.0/1"),
    TimeZoneChoice("Sao Paulo", "UTC-3", "BRT3"),
    TimeZoneChoice("Buenos Aires", "UTC-3", "<-03>3"),
    TimeZoneChoice("New York / Toronto", "UTC-5/-4", "EST5EDT,M3.2.0,M11.1.0"),
    TimeZoneChoice("Chicago", "UTC-6/-5", "CST6CDT,M3.2.0,M11.1.0"),
    TimeZoneChoice("Mexico City", "UTC-6", "CST6"),
    TimeZoneChoice("Denver", "UTC-7/-6", "MST7MDT,M3.2.0,M11.1.0"),
    TimeZoneChoice("Phoenix", "UTC-7", "MST7"),
    TimeZoneChoice("Los Angeles / Vancouver", "UTC-8/-7", "PST8PDT,M3.2.0,M11.1.0"),
    TimeZoneChoice("Anchorage / Juneau", "UTC-9/-8", "AKST9AKDT,M3.2.0,M11.1.0"),
    TimeZoneChoice("Honolulu / Hawaii", "UTC-10", "HST10"),
    TimeZoneChoice("Pago Pago / Midway", "UTC-11", "<-11>11"),
)

@Composable
internal fun SettingsPage(
    settings: DeviceSettings,
    snapshot: DeviceSnapshot,
    send: (String) -> Unit,
    sendOta: (String) -> Unit,
    setDeviceName: (String) -> Unit,
    setTransport: (Int) -> Unit,
) {
    var showZones by remember { mutableStateOf(false) }
    var pendingOta by remember { mutableStateOf<String?>(null) }
    var deviceName by remember(snapshot.device?.id, snapshot.device?.name) {
        mutableStateOf(snapshot.device?.name.orEmpty())
    }
    AdaptivePage(
        primary = {
            SectionPanel("Appearance") {
                CompactChoices(listOf("CORONET", "GRAPHITE", "AURORA", "MINIMAL"), settings.uiSkin.coerceIn(0, 3)) {
                    send(jsonSetting("uiSkin", it))
                }
                CompactChoices(listOf("DARK", "LIGHT", "AUTO"), settings.uiColorMode.coerceIn(0, 2)) {
                    send(jsonSetting("uiColorMode", it))
                }
                ValueSlider("Accent hue", settings.accentHueDegrees, 0..359, " deg") { send(jsonSetting("accentHueDegrees", it)) }
                ValueSlider("Display brightness", settings.displayBrightness, 10..100, "%") { send(jsonSetting("displayBrightness", it)) }
            }
            SectionPanel("Screen saver") {
                CompactChoices(listOf("OFF", "DISPLAY OFF", "CLOCK"), settings.screenSaverMode.coerceIn(0, 2)) {
                    send(jsonSetting("screenSaverMode", it))
                }
                ChoiceButton("Clock style", listOf("DIGITAL", "RETRO", "ANALOG", "LINHO", "BAUHAUS", "MATRIX", "ARC").getOrElse(settings.clockStyle) { "DIGITAL" }) {
                    send(jsonSetting("clockStyle", (settings.clockStyle + 1) % 7))
                }
                ChoiceButton("Time format", if (settings.clock24Hour) "24 HOUR" else "12 HOUR") {
                    send(jsonSetting("clock24Hour", !settings.clock24Hour))
                }
                ChoiceButton("Time zone", timeZoneLabel(settings.timeZone)) { showZones = true }
                ValueSlider("Activate after", settings.screenSaverDelayMinutes, 1..60, " min") { send(jsonSetting("screenSaverDelayMinutes", it)) }
                ValueSlider("Clock brightness", settings.clockBrightness, 5..100, "%") { send(jsonSetting("clockBrightness", it)) }
            }
        },
        secondary = {
            SectionPanel("Quiet mode") {
                CompactChoices(listOf("OFF", "SOUND", "LEDS", "BOTH"), settings.quietTarget.coerceIn(0, 3)) {
                    send(jsonSetting("quietTarget", it))
                }
                ValueSlider("Duration", settings.quietDurationMinutes.coerceIn(5, 240), 5..240, " min") { send(jsonSetting("quietDurationMinutes", it)) }
                SettingSwitch(
                    "Errors bypass quiet mode",
                    settings.quietErrorsBypass,
                    "Critical printer states remain visible and audible.",
                ) { send(jsonSetting("quietErrorsBypass", it)) }
            }
            SectionPanel("Connections") {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = deviceName,
                        onValueChange = { deviceName = it.take(24) },
                        label = { Text("Device name") },
                        singleLine = true,
                        modifier = Modifier.weight(1f),
                    )
                    Button(
                        onClick = { setDeviceName(deviceName) },
                        enabled = snapshot.connection != ConnectionKind.Offline,
                        modifier = Modifier.height(56.dp),
                    ) { Text("SAVE") }
                }
                Text("COMPANION CONNECTION", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp)
                CompactChoices(
                    listOf("AUTO", "BLE", "WI-FI"),
                    settings.companionTransport.coerceIn(0, 2),
                    setTransport,
                )
                SettingsInfo("Phone link", when (snapshot.connection) {
                    ConnectionKind.Wifi -> "Local Wi-Fi"
                    ConnectionKind.Ble -> "Bluetooth LE"
                    ConnectionKind.Offline -> "Offline"
                })
                SettingsInfo("Network", settings.wifiSsid.ifBlank { "Not configured" })
                SettingsInfo("Printer", settings.printerHost.ifBlank { "Not configured" })
                SettingsInfo("BLE", if (settings.bleEnabled) "Enabled" else "Disabled")
                SettingsInfo("Pairing", if (settings.apiPaired) "Securely paired" else "Not paired")
            }
            SectionPanel("Firmware update") {
                SettingsInfo("Installed", snapshot.firmware)
                if (snapshot.ota.availableVersion.isNotBlank()) SettingsInfo("Available", snapshot.ota.availableVersion)
                Text(snapshot.ota.status, color = otaStatusColor(snapshot.ota.state), fontSize = 12.sp)
                if (snapshot.ota.busy || snapshot.ota.progress > 0) {
                    LinearProgressIndicator(
                        progress = { snapshot.ota.progress.coerceIn(0, 100) / 100f },
                        modifier = Modifier.fillMaxWidth().height(7.dp),
                    )
                }
                if (snapshot.connection != ConnectionKind.Wifi) {
                    Text("Firmware updates require local Wi-Fi.", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp)
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { sendOta("check") },
                        enabled = snapshot.connection == ConnectionKind.Wifi && !snapshot.ota.busy,
                        modifier = Modifier.weight(1f),
                    ) { Text(if (snapshot.ota.state == 1) "CHECKING" else "CHECK") }
                    Button(
                        onClick = { pendingOta = "install" },
                        enabled = snapshot.connection == ConnectionKind.Wifi && snapshot.ota.updateAvailable && !snapshot.ota.busy,
                        modifier = Modifier.weight(1f),
                    ) { Text("INSTALL") }
                }
                TextButton(
                    onClick = { pendingOta = "reinstall" },
                    enabled = snapshot.connection == ConnectionKind.Wifi && !snapshot.ota.busy,
                    modifier = Modifier.fillMaxWidth(),
                ) { Text("REINSTALL CURRENT RELEASE") }
            }
        },
    )

    if (showZones) TimeZoneDialog(settings.timeZone, send) { showZones = false }
    pendingOta?.let { action ->
        AlertDialog(
            onDismissRequest = { pendingOta = null },
            title = { Text(if (action == "reinstall") "Reinstall firmware?" else "Install update?") },
            text = { Text("coroNET will stop background services, install verified firmware and restart. Keep power connected.") },
            confirmButton = {
                Button(onClick = { pendingOta = null; sendOta(action) }) { Text("CONTINUE") }
            },
            dismissButton = { TextButton(onClick = { pendingOta = null }) { Text("CANCEL") } },
        )
    }
}

@Composable
private fun SettingsInfo(label: String, value: String) {
    Row(Modifier.fillMaxWidth()) {
        Text(label, modifier = Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
        Text(value, fontWeight = FontWeight.SemiBold, fontSize = 12.sp, maxLines = 1)
    }
}

@Composable
private fun TimeZoneDialog(current: String, send: (String) -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Select time zone") },
        text = {
            Column(
                Modifier.fillMaxWidth().height(360.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(2.dp),
            ) {
                timeZoneChoices.forEach { zone ->
                    TextButton(
                        onClick = { send(jsonSetting("timeZone", zone.spec)); onDismiss() },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text(
                            "${zone.offset}  ${zone.label}",
                            modifier = Modifier.fillMaxWidth(),
                            color = if (zone.spec == current) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                        )
                    }
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("CLOSE") } },
    )
}

private fun timeZoneLabel(spec: String): String = timeZoneChoices.firstOrNull { it.spec == spec }?.let { "${it.offset} ${it.label}" } ?: "CUSTOM"

@Composable
private fun otaStatusColor(state: Int): Color = when (state) {
    2, 7 -> MaterialTheme.colorScheme.tertiary
    8 -> MaterialTheme.colorScheme.error
    else -> MaterialTheme.colorScheme.onSurfaceVariant
}
