package de.alphastudio.coronet2.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronLeft
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.ColorLens
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.DeviceSettings
import de.alphastudio.coronet2.model.DeviceSnapshot

@Composable
internal fun LedPage(
    settings: DeviceSettings,
    snapshot: DeviceSnapshot,
    send: (String) -> Unit,
    preview: (Int, Int) -> Unit,
    calibrate: (Boolean, Int) -> Unit,
) {
    var category by rememberSaveable { mutableIntStateOf(categoryForState(snapshot.printer.state, settings.ledOtherMode)) }
    var section by rememberSaveable { mutableIntStateOf(1) }
    var showCalibration by remember { mutableStateOf(false) }
    val animations = ledAnimationCatalog.getOrElse(category) { emptyList() }
    val animationIndex = settings.ledAnimation.getOrElse(category) { 0 }.coerceIn(0, (animations.size - 1).coerceAtLeast(0))
    val animationName = animationDisplayName(animations.getOrElse(animationIndex) { "None" })

    AdaptivePage(
        title = "LED",
        subtitle = "Status light engine",
        primary = {
            SectionPanel("Animation") {
                CompactChoices(ledCategoryNames, category) { category = it }
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    IconButton(
                        onClick = {
                            if (animations.isNotEmpty()) {
                                val next = (animationIndex - 1 + animations.size) % animations.size
                                send(intArraySetting("ledAnimation", settings.ledAnimation, 6, category, next, 0))
                            }
                        },
                        modifier = Modifier.size(52.dp),
                    ) { Icon(Icons.Rounded.ChevronLeft, contentDescription = "Previous animation") }
                    Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(animationName, fontSize = 17.sp, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        Text(
                            "${animationIndex + 1} / ${animations.size}",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 11.sp,
                        )
                    }
                    IconButton(
                        onClick = {
                            if (animations.isNotEmpty()) {
                                val next = (animationIndex + 1) % animations.size
                                send(intArraySetting("ledAnimation", settings.ledAnimation, 6, category, next, 0))
                            }
                        },
                        modifier = Modifier.size(52.dp),
                    ) { Icon(Icons.Rounded.ChevronRight, contentDescription = "Next animation") }
                }
                LedLayoutPreview(
                    brightness = settings.ledBrightness,
                    selectedSection = section,
                    insideAmbient = settings.insideColorStyle != 0,
                    mirror = settings.mirrorLedLayout,
                )
                Text(
                    "The diagram follows the physical right, center, left and inside sections.",
                    modifier = Modifier.fillMaxWidth(),
                    textAlign = TextAlign.Center,
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Button(
                    onClick = { preview(category, animationIndex) },
                    enabled = snapshot.device != null && snapshot.connection != de.alphastudio.coronet2.model.ConnectionKind.Offline,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                ) {
                    Icon(Icons.Rounded.PlayArrow, contentDescription = null, modifier = Modifier.size(18.dp))
                    Text("  PREVIEW 10 S")
                }
            }
            SectionPanel("Color remix") {
                val remix = settings.ledColorRemixDegrees.getOrElse(category) { 0 }
                SettingSwitch(
                    label = "Use original colors",
                    checked = remix == 0,
                    note = "Meaningful telemetry colors remain protected.",
                ) { useDefault ->
                    if (useDefault) send(intArraySetting("ledColorRemixDegrees", settings.ledColorRemixDegrees, 6, category, 0, 0))
                    else if (remix == 0) send(intArraySetting("ledColorRemixDegrees", settings.ledColorRemixDegrees, 6, category, 30, 0))
                }
                ValueSlider("Hue shift", remix, -180..180, " deg") {
                    send(intArraySetting("ledColorRemixDegrees", settings.ledColorRemixDegrees, 6, category, it, 0))
                }
            }
        },
        secondary = {
            SectionPanel("Light output") {
                SettingSwitch("LED output", settings.ledEnabled) { send(jsonSetting("ledEnabled", it)) }
                CompactChoices(ledSectionNames, section) { section = it }
                ValueSlider(
                    "${ledSectionNames[section]} brightness",
                    settings.ledBrightness.getOrElse(section) { 70 },
                    0..100,
                    "%",
                ) { send(intArraySetting("ledBrightness", settings.ledBrightness, 4, section, it, 70)) }
                SettingSwitch(
                    "Dim ${ledSectionNames[section].lowercase()} after inactivity",
                    settings.ledDimmEnabled.getOrElse(section) { false },
                ) { send(booleanArraySetting("ledDimmEnabled", settings.ledDimmEnabled, 4, section, it)) }
                ValueSlider(
                    "Inactive brightness",
                    settings.ledDimmPercent.getOrElse(section) { 20 },
                    0..100,
                    "%",
                ) { send(intArraySetting("ledDimmPercent", settings.ledDimmPercent, 4, section, it, 20)) }
            }
            SectionPanel("Layout") {
                ChoiceButton("Inside color style", if (settings.insideColorStyle == 0) "WHITE" else "AMBIENT") {
                    send(jsonSetting("insideColorStyle", if (settings.insideColorStyle == 0) 1 else 0))
                }
                SettingSwitch("Mirror LED layout", settings.mirrorLedLayout) { send(jsonSetting("mirrorLedLayout", it)) }
                SettingSwitch(
                    "Other mode",
                    settings.ledOtherMode,
                    "Use the decorative animation category independently of printer state.",
                ) { send(jsonSetting("ledOtherMode", it)) }
                Button(onClick = { showCalibration = true; calibrate(true, 0) }, modifier = Modifier.fillMaxWidth().height(44.dp)) {
                    Icon(Icons.Rounded.ColorLens, contentDescription = null, modifier = Modifier.size(18.dp))
                    Text("  COLOR CALIBRATION")
                }
            }
        },
    )

    if (showCalibration) {
        LedCalibrationDialog(settings, send, calibrate) { showCalibration = false }
    }
}

@Composable
private fun LedCalibrationDialog(
    settings: DeviceSettings,
    send: (String) -> Unit,
    calibrate: (Boolean, Int) -> Unit,
    onDismiss: () -> Unit,
) {
    var color by rememberSaveable { mutableIntStateOf(0) }
    DisposableEffect(Unit) {
        onDispose { calibrate(false, color) }
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("LED color calibration") },
        text = {
            Column(
                Modifier.fillMaxWidth().verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text("Match the physical LEDs to the reference shown on screen.", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
                CompactChoices(calibrationColorNames.take(4), if (color < 4) color else -1) { color = it; calibrate(true, color) }
                CompactChoices(calibrationColorNames.drop(4), if (color >= 4) color - 4 else -1) { color = it + 4; calibrate(true, color) }
                Text(calibrationColorNames[color], color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
                ValueSlider("Hue correction", settings.ledCalibrationHue.getOrElse(color) { 0 }, -45..45, " deg") {
                    send(intArraySetting("ledCalibrationHue", settings.ledCalibrationHue, 8, color, it, 0))
                }
                ValueSlider("Saturation", settings.ledCalibrationSaturation.getOrElse(color) { 100 }, 50..150, "%") {
                    send(intArraySetting("ledCalibrationSaturation", settings.ledCalibrationSaturation, 8, color, it, 100))
                }
                ValueSlider("Brightness", settings.ledCalibrationBrightness.getOrElse(color) { 100 }, 50..150, "%") {
                    send(intArraySetting("ledCalibrationBrightness", settings.ledCalibrationBrightness, 8, color, it, 100))
                }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text("DONE") } },
    )
}

private fun categoryForState(state: String, other: Boolean): Int = when {
    other -> 5
    state.equals("printing", true) -> 1
    state.equals("paused", true) -> 2
    state.equals("error", true) || state.equals("timeout", true) -> 3
    state.equals("complete", true) || state.equals("finished", true) -> 4
    else -> 0
}
