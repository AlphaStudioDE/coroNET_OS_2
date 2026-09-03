package de.alphastudio.coronet2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.CoronetViewModel
import de.alphastudio.coronet2.model.*
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.roundToInt

private val CoronetDark = darkColorScheme(
    primary = Color(0xFF16C7E8), secondary = Color(0xFFFFB323), tertiary = Color(0xFF42E19B),
    onPrimary = Color(0xFF001F27),
    background = Color(0xFF071018), surface = Color(0xFF0D1821), surfaceVariant = Color(0xFF14232E),
    onBackground = Color(0xFFEAF7FA), onSurface = Color(0xFFEAF7FA), outline = Color(0xFF365363),
)

private enum class Page(val label: String) { Home("Home"), Led("LED"), Vent("Vent"), Sound("Sound"), Settings("Settings") }

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

private fun timeZoneLabel(spec: String): String =
    timeZoneChoices.firstOrNull { it.spec == spec }?.let { "${it.offset}  ${it.label}" } ?: "Custom"

@Composable
fun CoronetApp(model: CoronetViewModel) {
    val devices by model.devices.collectAsState()
    val selectedId by model.selectedId.collectAsState()
    val snapshot by model.snapshot.collectAsState()
    val settings by model.settings.collectAsState()
    val discovered by model.discovered.collectAsState()
    val scanning by model.scanning.collectAsState()
    val pairingChallenge by model.pairingChallenge.collectAsState()
    val pairingCandidate by model.pairingCandidate.collectAsState()
    var page by remember { mutableStateOf(Page.Home) }
    var manageDevices by remember { mutableStateOf(devices.isEmpty()) }

    MaterialTheme(colorScheme = CoronetDark, typography = Typography()) {
        Scaffold(
            containerColor = MaterialTheme.colorScheme.background,
            topBar = { DeviceHeader(snapshot, onManage = { manageDevices = true }) },
            bottomBar = {
                NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                    Page.entries.forEach { item ->
                        NavigationBarItem(
                            selected = page == item, onClick = { page = item },
                            icon = { Text(item.label.take(1), fontWeight = FontWeight.Bold) },
                            label = { Text(item.label, fontSize = 11.sp) },
                        )
                    }
                }
            },
        ) { padding ->
            Box(Modifier.padding(padding).fillMaxSize()) {
                when (page) {
                    Page.Home -> HomePage(snapshot)
                    Page.Led -> LedPage(settings, model::sendSettings)
                    Page.Vent -> VentPage(settings, snapshot, model::sendSettings)
                    Page.Sound -> SoundPage(settings, snapshot, model::sendSettings)
                    Page.Settings -> SettingsPage(settings, snapshot, model::sendSettings, model::sendOtaAction)
                }
            }
        }
        if (manageDevices) DeviceManager(
            devices = devices, selectedId = selectedId, discovered = discovered, scanning = scanning,
            onSelect = { model.select(it); manageDevices = false },
            onScan = model::startScan, onAdd = { model.addAndConnect(it); manageDevices = false },
            onSave = { model.saveDevice(it); manageDevices = false },
            onRemove = model::removeSelected, onDismiss = { manageDevices = false },
        )
        pairingChallenge?.let { challenge ->
            PairingDialog(
                challenge = challenge,
                onConfirm = model::confirmPairingCodesMatch,
                onCancel = model::cancelPairing,
            )
        }
        if (pairingCandidate != null && pairingChallenge == null) {
            PairingWaitingDialog(pairingCandidate!!, model::cancelPairing)
        }
    }
}

@Composable
private fun PairingWaitingDialog(device: CoronetDevice, onCancel: () -> Unit) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text("Pair ${device.name}") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                CircularProgressIndicator(Modifier.align(Alignment.CenterHorizontally))
                Text("Waiting for a secure pairing session.")
                Text(
                    "On coroNET, open Settings > Companion connection and tap PAIR PHONE. " +
                        "This device will only be saved after both codes are confirmed.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        confirmButton = {},
        dismissButton = { TextButton(onClick = onCancel) { Text("CANCEL") } },
    )
}

@Composable
private fun PairingDialog(
    challenge: PairingChallenge,
    onConfirm: () -> Unit,
    onCancel: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text("Pair ${challenge.deviceName}") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Text("Check that this code matches the code shown on coroNET.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    "%03d %03d".format(challenge.code / 1000, challenge.code % 1000),
                    fontSize = 38.sp,
                    fontWeight = FontWeight.Light,
                    color = MaterialTheme.colorScheme.primary,
                )
                Text(
                    if (challenge.confirmedOnPhone) "Confirmed here. Complete confirmation on coroNET."
                    else "Only continue when both displays show exactly the same number.",
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        confirmButton = {
            Button(onClick = onConfirm, enabled = !challenge.confirmedOnPhone) {
                Text(if (challenge.confirmedOnPhone) "CONFIRMED" else "CODES MATCH")
            }
        },
        dismissButton = { TextButton(onClick = onCancel) { Text("CANCEL") } },
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun DeviceHeader(snapshot: DeviceSnapshot, onManage: () -> Unit) {
    TopAppBar(
        colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surface),
        title = {
            Column {
                Text(snapshot.device?.name ?: "coroNET", fontWeight = FontWeight.SemiBold, letterSpacing = 0.sp)
                Text(
                    when (snapshot.connection) {
                        ConnectionKind.Wifi -> "Connected via Wi-Fi"
                        ConnectionKind.Ble -> "Connected via BLE"
                        ConnectionKind.Offline -> if (snapshot.cached) "Offline - showing saved data" else "Offline"
                    },
                    fontSize = 12.sp, color = connectionColor(snapshot.connection), letterSpacing = 0.sp,
                )
            }
        },
        actions = { TextButton(onClick = onManage) { Text("DEVICES") } },
    )
}

@Composable
private fun HomePage(snapshot: DeviceSnapshot) = PageColumn {
    Text("Printer", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Light)
    StatusPanel(snapshot.printer)
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Metric("Tool", snapshot.printer.toolTemp?.let { "%.1f C".format(it) } ?: "--", Modifier.weight(1f))
        Metric("Bed", snapshot.printer.bedTemp?.let { "%.1f C".format(it) } ?: "--", Modifier.weight(1f))
        Metric("Chamber", snapshot.printer.chamberTemp?.let { "%.1f C".format(it) } ?: "--", Modifier.weight(1f))
    }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Metric("Fan", "${snapshot.fanPercent}%", Modifier.weight(1f))
        Metric("Flap", "${snapshot.flapPercent}%", Modifier.weight(1f))
        Metric("Firmware", snapshot.firmware, Modifier.weight(1f))
    }
    snapshot.error?.let { Text(it, color = MaterialTheme.colorScheme.error, fontSize = 13.sp) }
}

@Composable
private fun StatusPanel(printer: PrinterSnapshot) = SectionCard {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(printer.state.uppercase(), color = stateColor(printer.state), fontWeight = FontWeight.Bold)
            Text(printer.filename.ifBlank { printer.status.ifBlank { "Waiting for printer" } }, maxLines = 2)
        }
        Text("${printer.progress}%", fontSize = 30.sp, fontWeight = FontWeight.Light)
    }
    LinearProgressIndicator(
        progress = { printer.progress.coerceIn(0, 100) / 100f },
        modifier = Modifier.fillMaxWidth().height(7.dp), color = stateColor(printer.state),
    )
}

@Composable
private fun LedPage(settings: DeviceSettings, send: (String) -> Unit) = PageColumn {
    PageTitle("LED", "Physical light engine")
    SectionCard {
        SettingSwitch("LED output", settings.ledEnabled) { send(json("ledEnabled", it)) }
        val labels = listOf("Right", "Center", "Left", "Inside")
        labels.forEachIndexed { index, label ->
            ValueSlider(label, settings.ledBrightness.getOrElse(index) { 70 }, 0..100, "%") { value ->
                val values = settings.ledBrightness.toMutableList().also { while (it.size < 4) it.add(70); it[index] = value }
                send(JSONObject().put("ledBrightness", JSONArray(values)).toString())
            }
        }
    }
    SectionCard {
        ChoiceRow("Inside light", if (settings.insideColorStyle == 0) "WHITE" else "AMBIENT") {
            send(json("insideColorStyle", if (settings.insideColorStyle == 0) 1 else 0))
        }
        SettingSwitch("Mirror LED layout", settings.mirrorLedLayout) { send(json("mirrorLedLayout", it)) }
    }
}

@Composable
private fun VentPage(settings: DeviceSettings, snapshot: DeviceSnapshot, send: (String) -> Unit) {
    var pandaHost by remember(settings.pandaHost) { mutableStateOf(settings.pandaHost) }
    PageColumn {
    PageTitle("Vent", "Local chamber airflow")
    SectionCard {
        ChoiceRow("Mode", listOf("AUTO", "TARGET", "MANUAL").getOrElse(settings.ventMode) { "AUTO" }) {
            send(json("ventMode", (settings.ventMode + 1) % 3))
        }
        ValueSlider("Target temperature", settings.ventTargetTempC, 20..80, " C") { send(json("ventTargetTempC", it)) }
        if (settings.ventMode == 2) {
            ValueSlider("Manual fan", settings.manualFanPercent, 0..100, "%") { send(json("manualFanPercent", it)) }
            ValueSlider("Manual flap", settings.manualFlapPercent, 0..100, "%") { send(json("manualFlapPercent", it)) }
        }
        Text("Live: fan ${snapshot.fanPercent}%  |  flap ${snapshot.flapPercent}%", color = MaterialTheme.colorScheme.primary)
    }
    SectionCard {
        Text("Servo calibration", fontWeight = FontWeight.SemiBold)
        ValueSlider("Closed", settings.servoClosedUs, 500..2500, " us") { send(json("servoClosedUs", it)) }
        ValueSlider("Open", settings.servoOpenUs, 500..2500, " us") { send(json("servoOpenUs", it)) }
        SettingSwitch("Reverse servo", settings.servoReverse) { send(json("servoReverse", it)) }
    }
    SectionCard {
        Text("DIY chamber heater", fontWeight = FontWeight.SemiBold)
        SettingSwitch("GPIO46 output HIGH", settings.diyHeaterOutputHigh) {
            send(json("diyHeaterOutputHigh", it))
        }
        Text("3.3 V logic only. Use an external relay, MOSFET or optocoupler.",
             style = MaterialTheme.typography.bodySmall,
             color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
    SectionCard {
        Text("Panda Breath", fontWeight = FontWeight.SemiBold)
        OutlinedTextField(
            value = pandaHost,
            onValueChange = { pandaHost = it.take(64) },
            modifier = Modifier.fillMaxWidth(),
            label = { Text("Address or hostname") },
            placeholder = { Text("PandaBreath.local") },
            singleLine = true,
        )
        Button(onClick = { send(json("pandaHost", pandaHost.trim())) }) {
            Text("Save address")
        }
        SettingSwitch("Integration", settings.pandaEnabled) { send(json("pandaEnabled", it)) }
        ChoiceRow("Workflow", listOf("OFF", "AUTO", "PREHEAT", "TEMPER", "FORCED", "DRY").getOrElse(settings.pandaMode) { "OFF" }) {
            send(json("pandaMode", (settings.pandaMode + 1) % 6))
        }
        ValueSlider("Panda target", settings.pandaTargetTempC, 30..60, " C") { send(json("pandaTargetTempC", it)) }
    }
}
}

@Composable
private fun SoundPage(settings: DeviceSettings, snapshot: DeviceSnapshot, send: (String) -> Unit) = PageColumn {
    PageTitle("Sound", if (snapshot.audioPlaying) "Audio is playing" else "Scenario mixer")
    SectionCard {
        listOf("Start", "Finish", "Error", "Pause", "Idle").forEachIndexed { index, label ->
            ValueSlider(label, settings.soundVolume.getOrElse(index) { 75 }, 0..100, "%") { value ->
                val values = settings.soundVolume.toMutableList().also { while (it.size < 5) it.add(75); it[index] = value }
                send(JSONObject().put("soundVolume", JSONArray(values)).toString())
            }
        }
    }
}

@Composable
private fun SettingsPage(
    settings: DeviceSettings,
    snapshot: DeviceSnapshot,
    send: (String) -> Unit,
    sendOta: (String) -> Unit,
) {
    var pendingOtaAction by remember { mutableStateOf<String?>(null) }
    var showTimeZonePicker by remember { mutableStateOf(false) }
    PageColumn {
        PageTitle("Settings", "Appearance and behavior")
        SectionCard {
            ChoiceRow("Interface", listOf("CORONET", "GRAPHITE", "AURORA", "MINIMAL").getOrElse(settings.uiSkin) { "CORONET" }) {
                send(json("uiSkin", (settings.uiSkin + 1) % 4))
            }
            ChoiceRow("Color mode", listOf("DARK", "LIGHT", "AUTO").getOrElse(settings.uiColorMode) { "DARK" }) {
                send(json("uiColorMode", (settings.uiColorMode + 1) % 3))
            }
            ValueSlider("Accent hue", settings.accentHueDegrees, 0..359, " deg") { send(json("accentHueDegrees", it)) }
            ValueSlider("Display", settings.displayBrightness, 10..100, "%") { send(json("displayBrightness", it)) }
        }
        SectionCard {
            Text("Screen saver", fontWeight = FontWeight.SemiBold)
            ChoiceRow("Mode", listOf("DISABLED", "DISPLAY OFF", "CLOCK").getOrElse(settings.screenSaverMode) { "CLOCK" }) {
                send(json("screenSaverMode", (settings.screenSaverMode + 1) % 3))
            }
            ChoiceRow("Clock", listOf("DIGITAL", "RETRO", "ANALOG", "LINHO", "BAUHAUS", "MATRIX", "ARC").getOrElse(settings.clockStyle) { "DIGITAL" }) {
                send(json("clockStyle", (settings.clockStyle + 1) % 7))
            }
            ChoiceRow("Time format", if (settings.clock24Hour) "24 HOUR" else "12 HOUR") {
                send(json("clock24Hour", !settings.clock24Hour))
            }
            ChoiceRow("Time zone", timeZoneLabel(settings.timeZone)) { showTimeZonePicker = true }
            ValueSlider("After", settings.screenSaverDelayMinutes, 1..60, " min") { send(json("screenSaverDelayMinutes", it)) }
            ValueSlider("Clock brightness", settings.clockBrightness, 5..100, "%") { send(json("clockBrightness", it)) }
        }
        SectionCard {
            ChoiceRow("Quiet mode", listOf("OFF", "SOUND", "LEDS", "BOTH").getOrElse(settings.quietTarget) { "OFF" }) {
                send(json("quietTarget", (settings.quietTarget + 1) % 4))
            }
            ValueSlider("Duration", settings.quietDurationMinutes.coerceAtMost(240), 1..240, " min") { send(json("quietDurationMinutes", it)) }
        }
        SectionCard {
            Text("Firmware update", fontWeight = FontWeight.SemiBold)
            Text("Installed ${snapshot.firmware}", color = MaterialTheme.colorScheme.onSurfaceVariant)
            if (snapshot.ota.availableVersion.isNotBlank()) {
                Text("Latest ${snapshot.ota.availableVersion}", color = MaterialTheme.colorScheme.primary)
            }
            Text(snapshot.ota.status, fontSize = 13.sp, color = otaStatusColor(snapshot.ota.state))
            if (snapshot.ota.busy || snapshot.ota.progress > 0) {
                LinearProgressIndicator(
                    progress = { snapshot.ota.progress / 100f },
                    modifier = Modifier.fillMaxWidth().height(7.dp),
                )
                Text("${snapshot.ota.progress}%", modifier = Modifier.align(Alignment.End), fontSize = 12.sp)
            }
            if (snapshot.connection != ConnectionKind.Wifi) {
                Text("Firmware updates require a local Wi-Fi connection.", fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(
                    onClick = { sendOta("check") },
                    enabled = snapshot.connection == ConnectionKind.Wifi && !snapshot.ota.busy,
                    modifier = Modifier.weight(1f),
                ) { Text(if (snapshot.ota.state == 1) "CHECKING" else "CHECK") }
                Button(
                    onClick = { pendingOtaAction = "install" },
                    enabled = snapshot.connection == ConnectionKind.Wifi && snapshot.ota.updateAvailable && !snapshot.ota.busy,
                    modifier = Modifier.weight(1f),
                ) { Text("INSTALL") }
            }
            TextButton(
                onClick = { pendingOtaAction = "reinstall" },
                enabled = snapshot.connection == ConnectionKind.Wifi && !snapshot.ota.busy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("REINSTALL CURRENT RELEASE") }
        }
    }

    pendingOtaAction?.let { action ->
        AlertDialog(
            onDismissRequest = { pendingOtaAction = null },
            title = { Text(if (action == "reinstall") "Reinstall firmware?" else "Install update?") },
            text = { Text("coroNET will stop background services, install the verified firmware, and restart. Do not disconnect power.") },
            confirmButton = {
                Button(onClick = { pendingOtaAction = null; sendOta(action) }) { Text("CONTINUE") }
            },
            dismissButton = { TextButton(onClick = { pendingOtaAction = null }) { Text("CANCEL") } },
        )
    }

    if (showTimeZonePicker) {
        AlertDialog(
            onDismissRequest = { showTimeZonePicker = false },
            title = { Text("Select time zone") },
            text = {
                Column(
                    Modifier.fillMaxWidth().heightIn(max = 520.dp).verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(2.dp),
                ) {
                    timeZoneChoices.forEach { choice ->
                        TextButton(
                            onClick = {
                                send(json("timeZone", choice.spec))
                                showTimeZonePicker = false
                            },
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Text(
                                "${choice.offset}  ${choice.label}",
                                modifier = Modifier.fillMaxWidth(),
                                color = if (choice.spec == settings.timeZone)
                                    MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                            )
                        }
                    }
                }
            },
            confirmButton = { TextButton(onClick = { showTimeZonePicker = false }) { Text("CLOSE") } },
        )
    }
}

@Composable
private fun otaStatusColor(state: Int): Color = when (state) {
    2, 7 -> MaterialTheme.colorScheme.tertiary
    8 -> MaterialTheme.colorScheme.error
    else -> MaterialTheme.colorScheme.onSurfaceVariant
}

@Composable
private fun DeviceManager(
    devices: List<CoronetDevice>, selectedId: String?, discovered: List<CoronetDevice>, scanning: Boolean,
    onSelect: (String) -> Unit, onScan: () -> Unit, onAdd: (CoronetDevice) -> Unit,
    onSave: (CoronetDevice) -> Unit, onRemove: () -> Unit, onDismiss: () -> Unit,
) {
    var edited by remember(devices, selectedId) { mutableStateOf(devices.firstOrNull { it.id == selectedId } ?: devices.firstOrNull()) }
    var host by remember(edited) { mutableStateOf(edited?.host.orEmpty()) }
    var token by remember(edited) { mutableStateOf(edited?.token.orEmpty()) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("coroNET devices") },
        text = {
            Column(Modifier.fillMaxWidth().heightIn(max = 560.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                devices.forEach { device ->
                    OutlinedButton(onClick = { edited = device; host = device.host; token = device.token; onSelect(device.id) }, Modifier.fillMaxWidth()) {
                        Text(if (device.id == selectedId) "${device.name}  SELECTED" else device.name)
                    }
                }
                HorizontalDivider()
                Button(onClick = onScan, enabled = !scanning, modifier = Modifier.fillMaxWidth()) { Text(if (scanning) "SCANNING..." else "SCAN BLE") }
                discovered.forEach { device ->
                    val saved = devices.any { it.id == device.id || it.address == device.address }
                    TextButton(onClick = { onAdd(device) }, Modifier.fillMaxWidth()) {
                        Text("${if (saved) "CONNECT" else "PAIR"} ${device.name}")
                    }
                }
                edited?.let { device ->
                    Text("Wi-Fi connection", fontWeight = FontWeight.SemiBold)
                    OutlinedTextField(host, { host = it }, label = { Text("IP address or host") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                    OutlinedTextField(token, { token = it }, label = { Text("API token") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                    Button(onClick = { onSave(device.copy(host = host.trim(), token = token.trim())) }, Modifier.fillMaxWidth()) { Text("SAVE CONNECTION") }
                    TextButton(onClick = onRemove, Modifier.fillMaxWidth()) { Text("REMOVE SELECTED", color = MaterialTheme.colorScheme.error) }
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("DONE") } },
    )
}

@Composable
private fun PageColumn(content: @Composable ColumnScope.() -> Unit) = Column(
    Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
    verticalArrangement = Arrangement.spacedBy(12.dp), content = content,
)

@Composable
private fun PageTitle(title: String, subtitle: String) = Column {
    Text(title, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Light)
    Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
}

@Composable
private fun SectionCard(content: @Composable ColumnScope.() -> Unit) = Surface(
    shape = RoundedCornerShape(8.dp), color = MaterialTheme.colorScheme.surface,
    tonalElevation = 1.dp, modifier = Modifier.fillMaxWidth(),
) { Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp), content = content) }

@Composable
private fun Metric(label: String, value: String, modifier: Modifier = Modifier) = Surface(
    modifier, shape = RoundedCornerShape(8.dp), color = MaterialTheme.colorScheme.surface,
) { Column(Modifier.padding(12.dp)) { Text(label, fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant); Text(value, fontWeight = FontWeight.SemiBold) } }

@Composable
private fun SettingSwitch(label: String, checked: Boolean, changed: (Boolean) -> Unit) = Row(
    Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
) { Text(label, Modifier.weight(1f)); Switch(checked, changed) }

@Composable
private fun ChoiceRow(label: String, value: String, clicked: () -> Unit) = Row(
    Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
) { Text(label, Modifier.weight(1f)); OutlinedButton(onClick = clicked) { Text(value) } }

@Composable
private fun ValueSlider(label: String, value: Int, range: IntRange, suffix: String, changed: (Int) -> Unit) {
    var local by remember(value) { mutableFloatStateOf(value.toFloat()) }
    Column {
        Row(Modifier.fillMaxWidth()) { Text(label, Modifier.weight(1f)); Text("${local.roundToInt()}$suffix", color = MaterialTheme.colorScheme.primary) }
        Slider(
            value = local, onValueChange = { local = it },
            onValueChangeFinished = { changed(local.roundToInt()) },
            valueRange = range.first.toFloat()..range.last.toFloat(),
        )
    }
}

private fun json(key: String, value: Any): String = JSONObject().put(key, value).toString()
private fun connectionColor(kind: ConnectionKind) = if (kind == ConnectionKind.Offline) Color(0xFFFF8A80) else Color(0xFF42E19B)
private fun stateColor(state: String) = when (state) {
    "error" -> Color(0xFFFF5C62); "complete" -> Color(0xFF42E19B); "paused" -> Color(0xFFFFB323); else -> Color(0xFF16C7E8)
}
