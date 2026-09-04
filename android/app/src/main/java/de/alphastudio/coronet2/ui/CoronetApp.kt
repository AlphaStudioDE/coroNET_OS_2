package de.alphastudio.coronet2.ui

import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
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
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
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
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
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
fun CoronetApp(
    model: CoronetViewModel,
    showBluetoothPermissionWarning: Boolean = false,
    onRetryPermissions: () -> Unit = {},
    onDismissPermissionWarning: () -> Unit = {},
) {
    val devices by model.devices.collectAsState()
    val selectedId by model.selectedId.collectAsState()
    val snapshot by model.snapshot.collectAsState()
    val temperatureHistory by model.temperatureHistory.collectAsState()
    val settings by model.settings.collectAsState()
    val soundLibrary by model.soundLibrary.collectAsState()
    val ledCatalog by model.ledCatalog.collectAsState()
    val ledFrame by model.ledFrame.collectAsState()
    val discovered by model.discovered.collectAsState()
    val scanning by model.scanning.collectAsState()
    val pairingChallenge by model.pairingChallenge.collectAsState()
    val pairingCandidate by model.pairingCandidate.collectAsState()
    var pageName by rememberSaveable { mutableStateOf(AppPage.Home.name) }
    var manageDevices by rememberSaveable { mutableStateOf(devices.isEmpty()) }
    var showBluetoothEnableDialog by rememberSaveable { mutableStateOf(false) }
    val page = AppPage.entries.firstOrNull { it.name == pageName } ?: AppPage.Home
    val bluetoothEnableLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) model.startScan()
    }

    LaunchedEffect(page) {
        model.setLedPreviewVisible(page == AppPage.Led)
    }
    DisposableEffect(Unit) {
        onDispose { model.setLedPreviewVisible(false) }
    }

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
                    AppPage.Home -> HomePage(snapshot, temperatureHistory)
                    AppPage.Led -> LedPage(
                        settings, snapshot, ledCatalog, ledFrame,
                        model::sendSettings, model::previewLed, model::calibrateLed,
                    )
                    AppPage.Vent -> VentPage(settings, snapshot, model::sendSettings)
                    AppPage.Sound -> SoundPage(
                        settings, soundLibrary, model::sendSettings, model::loadSoundLibrary,
                        model::selectSound, model::playSound, model::stopSound,
                    )
                    AppPage.Settings -> SettingsPage(
                        settings, snapshot, model::sendSettings, model::sendOtaAction,
                        model::setDeviceName, model::setCompanionTransport,
                    )
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
                onScan = {
                    if (model.isBluetoothEnabled()) {
                        model.startScan()
                    } else {
                        showBluetoothEnableDialog = true
                    }
                },
                onAdd = model::addAndConnect,
                onSave = model::saveDevice,
                onRemove = model::removeSelected,
                onDismiss = { manageDevices = false },
            )
        }

        if (showBluetoothPermissionWarning) {
            AlertDialog(
                onDismissRequest = onDismissPermissionWarning,
                title = { Text("Bluetooth permission required") },
                text = { Text("Allow nearby-device access so coroNET can be discovered, paired and used as a recovery connection.") },
                confirmButton = { TextButton(onClick = onRetryPermissions) { Text("TRY AGAIN") } },
                dismissButton = { TextButton(onClick = onDismissPermissionWarning) { Text("NOT NOW") } },
            )
        }
        if (showBluetoothEnableDialog) {
            AlertDialog(
                onDismissRequest = { showBluetoothEnableDialog = false },
                title = { Text("Turn on Bluetooth") },
                text = { Text("Bluetooth must be turned on before coroNET can search for nearby devices.") },
                confirmButton = {
                    TextButton(onClick = {
                        showBluetoothEnableDialog = false
                        bluetoothEnableLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
                    }) { Text("TURN ON") }
                },
                dismissButton = {
                    TextButton(onClick = { showBluetoothEnableDialog = false }) { Text("CANCEL") }
                },
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
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            if (maxWidth < 600.dp) {
                Row(
                    Modifier.fillMaxWidth().height(52.dp).padding(horizontal = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Row(Modifier.width(142.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("coro", color = palette.text, fontSize = 20.sp, fontWeight = FontWeight.Light)
                        Text("NET", color = palette.accent, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                        Text(
                            "  /  ${page.label}",
                            color = palette.muted,
                            fontSize = 12.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Clip,
                        )
                    }
                    Row(
                        Modifier.weight(1f),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Spacer(Modifier.weight(1f))
                        ConnectionStatusPill(snapshot, compact = true)
                        Spacer(Modifier.width(3.dp))
                        PrinterStatusPill(snapshot, compact = true)
                        Spacer(Modifier.width(5.dp))
                        Row(
                            modifier = Modifier.width(132.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                snapshot.device?.name ?: "No device",
                                modifier = Modifier.weight(1f),
                                color = palette.text,
                                fontSize = 11.sp,
                                textAlign = TextAlign.End,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                            )
                            IconButton(onClick = onDevices, modifier = Modifier.size(38.dp)) {
                                Icon(Icons.Rounded.Devices, contentDescription = "Manage coroNET devices", tint = palette.accent)
                            }
                        }
                    }
                }
            } else {
                Row(
                    Modifier.fillMaxWidth().height(52.dp).padding(horizontal = 16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("coro", color = palette.text, fontSize = 20.sp, fontWeight = FontWeight.Light)
                    Text("NET", color = palette.accent, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                    Text("  /  ${page.label}", color = palette.muted, fontSize = 12.sp)
                    Spacer(Modifier.weight(1f))
                    ConnectionStatusPill(snapshot)
                    Spacer(Modifier.width(8.dp))
                    PrinterStatusPill(snapshot)
                    Spacer(Modifier.width(10.dp))
                    Text(snapshot.device?.name ?: "No device", color = palette.text, fontSize = 13.sp, maxLines = 1)
                    IconButton(onClick = onDevices, modifier = Modifier.size(42.dp)) {
                        Icon(Icons.Rounded.Devices, contentDescription = "Manage coroNET devices", tint = palette.accent)
                    }
                }
            }
        }
    }
}

@Composable
private fun ConnectionStatusPill(snapshot: DeviceSnapshot, compact: Boolean = false) {
    StatusPill(
        text = when (snapshot.connection) {
            ConnectionKind.Wifi -> "Wi-Fi"
            ConnectionKind.Ble -> "BLE"
            ConnectionKind.Offline -> if (snapshot.cached && !compact) "OFFLINE CACHE" else "OFFLINE"
        },
        color = connectionColor(snapshot.connection),
        compact = compact,
    )
}

@Composable
private fun PrinterStatusPill(snapshot: DeviceSnapshot, compact: Boolean = false) {
    val palette = LocalCoronetPalette.current
    StatusPill(
        text = if (snapshot.printer.connected) snapshot.printer.state.uppercase() else if (compact) "PRN OFF" else "PRINTER OFFLINE",
        color = if (snapshot.printer.connected) stateColor(snapshot.printer.state) else palette.muted,
        compact = compact,
    )
}

@Composable
private fun StatusPill(text: String, color: Color, compact: Boolean = false) {
    Surface(
        color = color.copy(alpha = 0.10f),
        border = BorderStroke(1.dp, color.copy(alpha = 0.75f)),
        shape = RoundedCornerShape(5.dp),
    ) {
        Row(
            Modifier.padding(horizontal = if (compact) 5.dp else 8.dp, vertical = if (compact) 3.dp else 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(if (compact) 3.dp else 6.dp),
        ) {
            Box(Modifier.size(if (compact) 5.dp else 6.dp).background(color, CircleShape))
            Text(text, color = color, fontSize = if (compact) 9.sp else 10.sp, fontWeight = FontWeight.Bold, maxLines = 1)
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
            val discoveryContent: @Composable ColumnScope.() -> Unit = {
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
                    if (scanning || discovered.isNotEmpty()) {
                        Text(
                            "NEARBY DEVICES",
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                    if (scanning && discovered.isEmpty()) {
                        Text("Searching for coroNET devices...", color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    discovered.forEach { device ->
                        val saved = devices.firstOrNull { it.id == device.id || it.address == device.address }
                        Surface(
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(4.dp),
                            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
                            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f),
                        ) {
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(10.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                            ) {
                                Column(Modifier.weight(1f)) {
                                    Text(device.name, fontWeight = FontWeight.SemiBold, maxLines = 1)
                                    Text(
                                        device.address.ifBlank { "ID ${device.id}" },
                                        style = MaterialTheme.typography.labelSmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        maxLines = 1,
                                    )
                                }
                                Button(onClick = {
                                    if (saved != null) {
                                        onSave(saved.copy(name = device.name, address = device.address))
                                    } else {
                                        onAdd(device)
                                    }
                                    onDismiss()
                                }) {
                                    Text(if (saved != null) "CONNECT" else "PAIR")
                                }
                            }
                        }
                    }
            }
            val connectionContent: @Composable ColumnScope.() -> Unit = {
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
            BoxWithConstraints(Modifier.fillMaxWidth()) {
                if (maxWidth < 560.dp) {
                    Column(
                        Modifier.fillMaxWidth().heightIn(max = 560.dp).verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        discoveryContent()
                        HorizontalDivider(Modifier.padding(vertical = 4.dp))
                        connectionContent()
                    }
                } else {
                    Row(
                        Modifier.fillMaxWidth().heightIn(min = 260.dp, max = 480.dp),
                        horizontalArrangement = Arrangement.spacedBy(16.dp),
                    ) {
                        Column(
                            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                            content = discoveryContent,
                        )
                        Column(
                            Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                            content = connectionContent,
                        )
                    }
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
