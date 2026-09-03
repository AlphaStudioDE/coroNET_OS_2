package de.alphastudio.coronet2.ui

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.VolumeUp
import androidx.compose.material.icons.rounded.Air
import androidx.compose.material.icons.rounded.Devices
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Lightbulb
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.CoronetViewModel
import de.alphastudio.coronet2.model.ConnectionKind
import de.alphastudio.coronet2.model.CoronetDevice
import de.alphastudio.coronet2.model.DeviceSnapshot
import de.alphastudio.coronet2.model.PairingChallenge

private enum class AppPage(val label: String, val icon: ImageVector) {
    Home("HOME", Icons.Rounded.Home),
    Led("LED", Icons.Rounded.Lightbulb),
    Vent("VENT", Icons.Rounded.Air),
    Sound("SOUND", Icons.AutoMirrored.Rounded.VolumeUp),
    Settings("SET", Icons.Rounded.Settings),
}

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
    var pageName by rememberSaveable { mutableStateOf(AppPage.Home.name) }
    var manageDevices by rememberSaveable { mutableStateOf(devices.isEmpty()) }
    val page = AppPage.entries.firstOrNull { it.name == pageName } ?: AppPage.Home

    CoronetTheme(settings) {
        Scaffold(
            modifier = Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing),
            contentWindowInsets = WindowInsets(0, 0, 0, 0),
            containerColor = MaterialTheme.colorScheme.background,
            topBar = {
                ConsoleHeader(page, snapshot) { manageDevices = true }
            },
            bottomBar = {
                ConsoleNavigation(page) { pageName = it.name }
            },
        ) { padding ->
            AnimatedContent(
                targetState = page,
                transitionSpec = { fadeIn() togetherWith fadeOut() },
                label = "page",
                modifier = Modifier.padding(padding).fillMaxSize(),
            ) { selected ->
                when (selected) {
                    AppPage.Home -> HomePage(snapshot)
                    AppPage.Led -> LedPage(settings, snapshot, model::sendSettings, model::previewLed, model::calibrateLed)
                    AppPage.Vent -> VentPage(settings, snapshot, model::sendSettings)
                    AppPage.Sound -> SoundPage(settings, snapshot, model::sendSettings)
                    AppPage.Settings -> SettingsPage(settings, snapshot, model::sendSettings, model::sendOtaAction)
                }
            }
        }

        if (manageDevices) {
            DeviceManager(
                devices = devices,
                selectedId = selectedId,
                discovered = discovered,
                scanning = scanning,
                onSelect = model::select,
                onScan = model::startScan,
                onAdd = model::addAndConnect,
                onSave = model::saveDevice,
                onRemove = model::removeSelected,
                onDismiss = { manageDevices = false },
            )
        }
        pairingChallenge?.let {
            PairingDialog(it, model::confirmPairingCodesMatch, model::cancelPairing)
        }
        if (pairingCandidate != null && pairingChallenge == null) {
            PairingWaitingDialog(pairingCandidate!!, model::cancelPairing)
        }
    }
}

@Composable
private fun ConsoleHeader(page: AppPage, snapshot: DeviceSnapshot, onDevices: () -> Unit) {
    val palette = LocalCoronetPalette.current
    Surface(
        color = palette.surface,
        tonalElevation = 0.dp,
        shadowElevation = 0.dp,
        border = BorderStroke(1.dp, palette.border),
    ) {
        Row(
            Modifier.fillMaxWidth().height(52.dp).padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("coro", color = palette.text, fontSize = 20.sp, fontWeight = FontWeight.Light)
            Text("NET", color = palette.accent, fontSize = 20.sp, fontWeight = FontWeight.Bold)
            Text("  /  ${page.label}", color = palette.muted, fontSize = 12.sp)
            Spacer(Modifier.weight(1f))
            StatusPill(
                text = when (snapshot.connection) {
                    ConnectionKind.Wifi -> "Wi-Fi"
                    ConnectionKind.Ble -> "BLE"
                    ConnectionKind.Offline -> if (snapshot.cached) "OFFLINE CACHE" else "OFFLINE"
                },
                color = connectionColor(snapshot.connection),
            )
            Spacer(Modifier.width(8.dp))
            StatusPill(
                text = if (snapshot.printer.connected) snapshot.printer.state.uppercase() else "PRINTER OFFLINE",
                color = if (snapshot.printer.connected) stateColor(snapshot.printer.state) else palette.muted,
            )
            Spacer(Modifier.width(10.dp))
            Text(snapshot.device?.name ?: "No device", color = palette.text, fontSize = 13.sp, maxLines = 1)
            IconButton(onClick = onDevices, modifier = Modifier.size(42.dp)) {
                Icon(Icons.Rounded.Devices, contentDescription = "Manage coroNET devices", tint = palette.accent)
            }
        }
    }
}

@Composable
private fun StatusPill(text: String, color: Color) {
    Surface(
        color = color.copy(alpha = 0.10f),
        border = BorderStroke(1.dp, color.copy(alpha = 0.75f)),
        shape = RoundedCornerShape(5.dp),
    ) {
        Row(
            Modifier.padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Box(Modifier.size(6.dp).background(color, CircleShape))
            Text(text, color = color, fontSize = 10.sp, fontWeight = FontWeight.Bold, maxLines = 1)
        }
    }
}

@Composable
private fun ConsoleNavigation(selected: AppPage, onSelect: (AppPage) -> Unit) {
    val palette = LocalCoronetPalette.current
    NavigationBar(containerColor = palette.surface, tonalElevation = 0.dp, modifier = Modifier.height(58.dp)) {
        AppPage.entries.forEach { page ->
            NavigationBarItem(
                selected = page == selected,
                onClick = { onSelect(page) },
                icon = { Icon(page.icon, contentDescription = page.label, modifier = Modifier.size(20.dp)) },
                label = { Text(page.label, fontSize = 10.sp, fontWeight = FontWeight.SemiBold) },
                colors = NavigationBarItemDefaults.colors(
                    selectedIconColor = palette.background,
                    selectedTextColor = palette.accent,
                    indicatorColor = palette.accent,
                    unselectedIconColor = palette.muted,
                    unselectedTextColor = palette.muted,
                ),
            )
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
                    "On coroNET, open Settings > Companion connection and tap PAIR PHONE.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        confirmButton = {},
        dismissButton = { TextButton(onClick = onCancel) { Text("CANCEL") } },
    )
}

@Composable
private fun PairingDialog(challenge: PairingChallenge, onConfirm: () -> Unit, onCancel: () -> Unit) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text("Pair ${challenge.deviceName}") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Text("Confirm that both displays show the same code.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    "%03d %03d".format(challenge.code / 1000, challenge.code % 1000),
                    fontSize = 38.sp,
                    fontWeight = FontWeight.Light,
                    color = MaterialTheme.colorScheme.primary,
                )
                Text(
                    if (challenge.confirmedOnPhone) "Confirmed here. Finish confirmation on coroNET."
                    else "Continue only when the numbers match exactly.",
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

@Composable
private fun DeviceManager(
    devices: List<CoronetDevice>,
    selectedId: String?,
    discovered: List<CoronetDevice>,
    scanning: Boolean,
    onSelect: (String?) -> Unit,
    onScan: () -> Unit,
    onAdd: (CoronetDevice) -> Unit,
    onSave: (CoronetDevice) -> Unit,
    onRemove: () -> Unit,
    onDismiss: () -> Unit,
) {
    var edited by remember(devices, selectedId) {
        mutableStateOf(devices.firstOrNull { it.id == selectedId } ?: devices.firstOrNull())
    }
    var host by remember(edited) { mutableStateOf(edited?.host.orEmpty()) }
    var token by remember(edited) { mutableStateOf(edited?.token.orEmpty()) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("coroNET devices") },
        text = {
            Row(Modifier.fillMaxWidth().heightIn(min = 260.dp, max = 480.dp), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                Column(
                    Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text("SAVED", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                    devices.forEach { device ->
                        OutlinedButton(
                            onClick = {
                                edited = device
                                host = device.host
                                token = device.token
                                onSelect(device.id)
                            },
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text(if (device.id == selectedId) "${device.name}  ACTIVE" else device.name, maxLines = 1) }
                    }
                    HorizontalDivider()
                    Button(onClick = onScan, enabled = !scanning, modifier = Modifier.fillMaxWidth()) {
                        Text(if (scanning) "SCANNING..." else "SCAN BLE")
                    }
                    discovered.forEach { device ->
                        val saved = devices.any { it.id == device.id || it.address == device.address }
                        TextButton(onClick = { onAdd(device) }, modifier = Modifier.fillMaxWidth()) {
                            Text("${if (saved) "CONNECT" else "PAIR"}  ${device.name}", maxLines = 1)
                        }
                    }
                }
                Column(
                    Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text("LOCAL WI-FI", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                    edited?.let { device ->
                        Text(device.name, fontWeight = FontWeight.SemiBold)
                        OutlinedTextField(host, { host = it.take(64) }, label = { Text("IP address or host") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                        OutlinedTextField(token, { token = it.take(96) }, label = { Text("API token") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                        Button(
                            onClick = { onSave(device.copy(host = host.trim(), token = token.trim())) },
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("SAVE CONNECTION") }
                        TextButton(onClick = onRemove, modifier = Modifier.fillMaxWidth()) {
                            Text("REMOVE SELECTED", color = MaterialTheme.colorScheme.error)
                        }
                    } ?: Text("Scan and securely pair a coroNET to begin.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("DONE") } },
    )
}

@Composable
private fun connectionColor(connection: ConnectionKind): Color {
    val palette = LocalCoronetPalette.current
    return when (connection) {
        ConnectionKind.Wifi -> palette.green
        ConnectionKind.Ble -> palette.accent
        ConnectionKind.Offline -> palette.red
    }
}
