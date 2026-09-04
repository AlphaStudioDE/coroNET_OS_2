package de.alphastudio.coronet2.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.ConnectionKind
import de.alphastudio.coronet2.model.DeviceSnapshot
import de.alphastudio.coronet2.model.TemperatureSample

@Composable
internal fun HomePage(snapshot: DeviceSnapshot, temperatureHistory: List<TemperatureSample>) {
    val printer = snapshot.printer
    val palette = LocalCoronetPalette.current
    AdaptivePage(
        primary = {
            Surface(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(7.dp),
                color = palette.surface,
                border = BorderStroke(1.dp, stateColor(printer.state).copy(alpha = 0.85f)),
            ) {
                Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(
                                if (printer.connected) printer.state.uppercase() else "WAITING FOR PRINTER",
                                color = stateColor(printer.state),
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Bold,
                            )
                            Text(
                                printer.filename.ifBlank { printer.status.ifBlank { "No active job" } },
                                fontSize = 18.sp,
                                color = palette.text,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                            )
                        }
                        Text("${printer.progress.coerceIn(0, 100)}%", fontSize = 34.sp, fontWeight = FontWeight.Light, color = palette.text)
                    }
                    LinearProgressIndicator(
                        progress = { printer.progress.coerceIn(0, 100) / 100f },
                        modifier = Modifier.fillMaxWidth().height(7.dp),
                        color = stateColor(printer.state),
                        trackColor = palette.raised,
                    )
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        MetricTile(
                            "Tool ${printer.tool + 1}",
                            formatTemp(printer.toolTemp),
                            TemperatureChartColors.tools[printer.tool.coerceIn(0, 3)],
                            Modifier.weight(1f),
                        )
                        MetricTile("Bed", formatTemp(printer.bedTemp), TemperatureChartColors.bed, Modifier.weight(1f))
                        MetricTile("Chamber", formatTemp(printer.chamberTemp), TemperatureChartColors.chamber, Modifier.weight(1f))
                    }
                }
            }
            snapshot.error?.let {
                SectionPanel("Attention") { Text(it, color = MaterialTheme.colorScheme.error, fontSize = 13.sp) }
            }
        },
        secondary = {
            SectionPanel("Environment") {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    MetricTile("Fan", "${snapshot.fanPercent}%", palette.accent, Modifier.weight(1f))
                    MetricTile("Flap", "${snapshot.flapPercent}%", palette.amber, Modifier.weight(1f))
                }
                Text(
                    if (snapshot.quietActive) "Quiet mode active" else "Standard operating profile",
                    color = if (snapshot.quietActive) palette.amber else palette.muted,
                    fontSize = 12.sp,
                )
            }
            TemperatureHistoryPanel(temperatureHistory, printer.tool.coerceIn(0, 3))
            SectionPanel("System") {
                InfoLine("Firmware", snapshot.firmware)
                InfoLine("coroNET", snapshot.device?.name ?: "Not selected")
                InfoLine("Phone link", when (snapshot.connection) {
                    ConnectionKind.Wifi -> "Local Wi-Fi"
                    ConnectionKind.Ble -> "Bluetooth LE"
                    ConnectionKind.Offline -> if (snapshot.cached) "Offline, cached data" else "Offline"
                })
                InfoLine("Printer telemetry", if (printer.telemetryValid) "Live" else "Unavailable")
                InfoLine("Audio", if (snapshot.audioPlaying) "Playing" else "Ready")
            }
        },
    )
}

@Composable
private fun InfoLine(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, modifier = Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
        Text(value, color = MaterialTheme.colorScheme.onSurface, fontWeight = FontWeight.SemiBold, fontSize = 12.sp, maxLines = 1)
    }
}

private fun formatTemp(value: Double?): String = value?.let { "%.1f C".format(it) } ?: "--"
