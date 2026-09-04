package de.alphastudio.coronet2.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.DeviceSettings
import de.alphastudio.coronet2.model.DeviceSnapshot

@Composable
internal fun VentPage(settings: DeviceSettings, snapshot: DeviceSnapshot, send: (String) -> Unit) {
    var pandaHost by remember(settings.pandaHost) { mutableStateOf(settings.pandaHost) }
    val palette = LocalCoronetPalette.current
    AdaptivePage(
        primary = {
            SectionPanel("Operating mode") {
                CompactChoices(listOf("AUTO", "TARGET", "MANUAL"), settings.ventMode.coerceIn(0, 2)) {
                    send(jsonSetting("ventMode", it))
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    MetricTile("Fan", "${snapshot.fanPercent}%", palette.accent, Modifier.weight(1f))
                    MetricTile("Flap", "${snapshot.flapPercent}%", palette.amber, Modifier.weight(1f))
                    MetricTile("Chamber", snapshot.printer.chamberTemp?.let { "%.1f C".format(it) } ?: "--", palette.green, Modifier.weight(1f))
                }
                if (settings.ventMode == 1) {
                    ValueSlider("Target temperature", settings.ventTargetTempC, 20..80, " C") { send(jsonSetting("ventTargetTempC", it)) }
                }
                if (settings.ventMode == 2) {
                    ValueSlider("Manual fan", settings.manualFanPercent, 0..100, "%") { send(jsonSetting("manualFanPercent", it)) }
                    ValueSlider("Manual flap", settings.manualFlapPercent, 0..100, "%") { send(jsonSetting("manualFlapPercent", it)) }
                }
            }
            SectionPanel("Fan limits") {
                ValueSlider("Minimum fan", settings.fanMinPercent, 0..100, "%") { send(jsonSetting("fanMinPercent", it)) }
                ValueSlider("Maximum fan", settings.fanMaxPercent, 0..100, "%") { send(jsonSetting("fanMaxPercent", it)) }
                ValueSlider("Failsafe fan", settings.failsafeFanPercent, 0..100, "%") { send(jsonSetting("failsafeFanPercent", it)) }
                ValueSlider("Failsafe flap", settings.failsafeFlapPercent, 0..100, "%") { send(jsonSetting("failsafeFlapPercent", it)) }
            }
            SectionPanel("Flap servo") {
                ValueSlider("Closed pulse", settings.servoClosedUs, 500..2500, " us") { send(jsonSetting("servoClosedUs", it)) }
                ValueSlider("Open pulse", settings.servoOpenUs, 500..2500, " us") { send(jsonSetting("servoOpenUs", it)) }
                SettingSwitch("Reverse direction", settings.servoReverse) { send(jsonSetting("servoReverse", it)) }
            }
        },
        secondary = {
            SectionPanel("Panda Breath") {
                SettingSwitch("Panda integration", settings.pandaEnabled) { send(jsonSetting("pandaEnabled", it)) }
                OutlinedTextField(
                    value = pandaHost,
                    onValueChange = { pandaHost = it.take(64) },
                    modifier = Modifier.fillMaxWidth(),
                    label = { Text("Address or hostname") },
                    placeholder = { Text("PandaBreath.local") },
                    singleLine = true,
                )
                Button(
                    onClick = { send(jsonSetting("pandaHost", pandaHost.trim())) },
                    modifier = Modifier.fillMaxWidth().height(42.dp),
                ) { Text("SAVE ADDRESS") }
                CompactChoices(pandaModeNames, settings.pandaMode.coerceIn(0, pandaModeNames.lastIndex)) {
                    send(jsonSetting("pandaMode", it))
                }
                ValueSlider("Auto target", settings.pandaTargetTempC, 30..60, " C") { send(jsonSetting("pandaTargetTempC", it)) }
                ValueSlider("Print target", settings.pandaPrintTargetTempC, 30..60, " C") { send(jsonSetting("pandaPrintTargetTempC", it)) }
                ValueSlider("Preheat hold", settings.pandaPreheatHoldMinutes, 1..180, " min") { send(jsonSetting("pandaPreheatHoldMinutes", it)) }
                ValueSlider("Tempering time", settings.pandaTemperingDurationMinutes, 1..180, " min") { send(jsonSetting("pandaTemperingDurationMinutes", it)) }
                ValueSlider("Tempering end", settings.pandaTemperingEndTempC, 0..60, " C") { send(jsonSetting("pandaTemperingEndTempC", it)) }
                SettingSwitch("Temper after print", settings.pandaTemperingAfterPrint) { send(jsonSetting("pandaTemperingAfterPrint", it)) }
            }
            SectionPanel("Drying") {
                ChoiceButton(
                    "Material preset",
                    pandaDryPresetNames.getOrElse(settings.pandaDryPreset) { "CUSTOM" },
                ) { send(jsonSetting("pandaDryPreset", (settings.pandaDryPreset + 1) % pandaDryPresetNames.size)) }
                ValueSlider("Drying duration", settings.pandaDryHours, 1..24, " h") { send(jsonSetting("pandaDryHours", it)) }
            }
            SectionPanel("DIY heater") {
                SettingSwitch(
                    "GPIO 46 output HIGH",
                    settings.diyHeaterOutputHigh,
                    "3.3 V logic only. Use an external relay, MOSFET or optocoupler.",
                ) { send(jsonSetting("diyHeaterOutputHigh", it)) }
                Text("Temperature input always comes from printer telemetry.", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        },
    )
}
