package de.alphastudio.coronet2.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.GraphicEq
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.DeviceSettings
import de.alphastudio.coronet2.model.DeviceSnapshot

@Composable
internal fun SoundPage(settings: DeviceSettings, snapshot: DeviceSnapshot, send: (String) -> Unit) {
    var scenario by rememberSaveable { mutableIntStateOf(0) }
    var editPath by remember { mutableStateOf(false) }
    val path = settings.soundPath.getOrElse(scenario) { "" }
    AdaptivePage(
        title = "Sound",
        subtitle = if (snapshot.audioPlaying) "Now playing" else "Status sound library",
        primary = {
            SectionPanel("Status scenario") {
                CompactChoices(listOf("START", "FINISH", "ERROR", "PAUSE", "IDLE"), scenario) { scenario = it }
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Icon(Icons.Rounded.GraphicEq, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                    Column(Modifier.weight(1f)) {
                        Text(soundScenarioNames[scenario], fontWeight = FontWeight.Bold, fontSize = 13.sp)
                        Text(
                            path.substringAfterLast('/').ifBlank { "Built-in default" },
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 12.sp,
                            maxLines = 1,
                        )
                    }
                }
                ChoiceButton("Sound file", path.substringAfterLast('/').ifBlank { "DEFAULT" }) { editPath = true }
                ValueSlider("Volume", settings.soundVolume.getOrElse(scenario) { 75 }, 0..100, "%") {
                    send(intArraySetting("soundVolume", settings.soundVolume, 5, scenario, it, 75))
                }
                SettingSwitch("Repeat until status changes", settings.soundRepeat.getOrElse(scenario) { false }) {
                    send(booleanArraySetting("soundRepeat", settings.soundRepeat, 5, scenario, it))
                }
            }
        },
        secondary = {
            SectionPanel("Assignments") {
                soundScenarioNames.forEachIndexed { index, name ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Text(name, modifier = Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp)
                        Text(
                            settings.soundPath.getOrElse(index) { "" }.substringAfterLast('/').ifBlank { "DEFAULT" },
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = if (index == scenario) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                            maxLines = 1,
                        )
                    }
                }
            }
            SectionPanel("Playback") {
                Text(
                    if (snapshot.audioPlaying) "coroNET is currently playing audio." else "Audio engine ready.",
                    color = if (snapshot.audioPlaying) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "Sound files are read from the /sounds directory on the microSD card.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 11.sp,
                )
            }
        },
    )

    if (editPath) {
        SoundPathDialog(path, scenario, settings, send) { editPath = false }
    }
}

@Composable
private fun SoundPathDialog(
    currentPath: String,
    scenario: Int,
    settings: DeviceSettings,
    send: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var value by remember(currentPath) { mutableStateOf(currentPath) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Select ${soundScenarioNames[scenario]} sound") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Enter a WAV path from the microSD sound library.", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
                OutlinedTextField(
                    value = value,
                    onValueChange = { value = it.take(64) },
                    modifier = Modifier.fillMaxWidth(),
                    label = { Text("/sounds/folder/file.wav") },
                    singleLine = true,
                )
                TextButton(onClick = { value = "" }) { Text("USE BUILT-IN DEFAULT") }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                send(stringArraySetting("soundPath", settings.soundPath, 5, scenario, value.trim()))
                onDismiss()
            }) { Text("SAVE") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("CANCEL") } },
    )
}
