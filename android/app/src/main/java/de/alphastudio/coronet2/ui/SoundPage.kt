package de.alphastudio.coronet2.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronLeft
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.GraphicEq
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Stop
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
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
import de.alphastudio.coronet2.model.SoundLibrarySnapshot

@Composable
internal fun SoundPage(
    settings: DeviceSettings,
    snapshot: DeviceSnapshot,
    library: SoundLibrarySnapshot,
    send: (String) -> Unit,
    loadLibrary: (Int, Int) -> Unit,
    play: (Int) -> Unit,
    stop: () -> Unit,
    rescan: () -> Unit,
) {
    var scenario by rememberSaveable { mutableIntStateOf(0) }
    var showLibrary by rememberSaveable { mutableStateOf(false) }
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
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
                ChoiceButton("Sound file", path.substringAfterLast('/').ifBlank { "DEFAULT" }) {
                    showLibrary = true
                    loadLibrary(0, 0)
                }
                ValueSlider("Volume", settings.soundVolume.getOrElse(scenario) { 75 }, 0..100, "%") {
                    send(intArraySetting("soundVolume", settings.soundVolume, 5, scenario, it, 75))
                }
                SettingSwitch("Repeat until status changes", settings.soundRepeat.getOrElse(scenario) { false }) {
                    send(booleanArraySetting("soundRepeat", settings.soundRepeat, 5, scenario, it))
                }
            }
        },
        secondary = {
            SectionPanel("Playback") {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { play(scenario) }, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Rounded.PlayArrow, contentDescription = null)
                        Text("PLAY")
                    }
                    OutlinedButton(onClick = stop, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Rounded.Stop, contentDescription = null)
                        Text("STOP")
                    }
                    IconButton(onClick = rescan) {
                        Icon(Icons.Rounded.Refresh, contentDescription = "Rescan sound library")
                    }
                }
                Text(
                    if (snapshot.audioPlaying) "coroNET is currently playing audio." else "Audio engine ready.",
                    color = if (snapshot.audioPlaying) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text("WAV files are read from /sounds on the microSD card.", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp)
            }
            SectionPanel("Assignments") {
                soundScenarioNames.forEachIndexed { index, name ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Text(name, modifier = Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp)
                        Text(
                            settings.soundPath.getOrElse(index) { "" }.substringAfterLast('/').ifBlank { "DEFAULT" },
                            modifier = Modifier.weight(1f),
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = if (index == scenario) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
            }
        },
    )

    if (showLibrary) {
        SoundLibraryDialog(
            currentPath = path,
            scenario = scenario,
            settings = settings,
            library = library,
            send = send,
            load = loadLibrary,
            onDismiss = { showLibrary = false },
        )
    }
}

@Composable
private fun SoundLibraryDialog(
    currentPath: String,
    scenario: Int,
    settings: DeviceSettings,
    library: SoundLibrarySnapshot,
    send: (String) -> Unit,
    load: (Int, Int) -> Unit,
    onDismiss: () -> Unit,
) {
    LaunchedEffect(Unit) { if (!library.loaded && !library.loading) load(0, 0) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                "${soundScenarioNames[scenario]} SOUND",
                fontSize = 18.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = { load((library.folder - 1).coerceAtLeast(0), 0) }, enabled = library.folder > 0) {
                        Icon(Icons.Rounded.ChevronLeft, contentDescription = "Previous folder")
                    }
                    Text(
                        if (library.folderCount > 0) "${library.folderName}  ${library.folder + 1}/${library.folderCount}" else "NO SOUND FOLDERS",
                        modifier = Modifier.weight(1f),
                        textAlign = TextAlign.Center,
                        fontWeight = FontWeight.Bold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                    IconButton(onClick = { load(library.folder + 1, 0) }, enabled = library.folder + 1 < library.folderCount) {
                        Icon(Icons.Rounded.ChevronRight, contentDescription = "Next folder")
                    }
                }
                OutlinedButton(
                    onClick = {
                        send(stringArraySetting("soundPath", settings.soundPath, 5, scenario, ""))
                        onDismiss()
                    },
                    modifier = Modifier.fillMaxWidth(),
                ) { Text(if (currentPath.isBlank()) "SELECTED  BUILT-IN DEFAULT" else "BUILT-IN DEFAULT") }
                when {
                    library.loading -> Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.Center) { CircularProgressIndicator() }
                    library.error != null -> Text(library.error, color = MaterialTheme.colorScheme.error)
                    !library.sdReady -> Text("microSD card unavailable", color = MaterialTheme.colorScheme.error)
                    library.files.isEmpty() -> Text("No WAV files in this folder", color = MaterialTheme.colorScheme.onSurfaceVariant)
                    else -> LazyColumn(Modifier.fillMaxWidth().heightIn(max = 100.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        items(library.files, key = { it.path }) { file ->
                            OutlinedButton(
                                onClick = {
                                    send(stringArraySetting("soundPath", settings.soundPath, 5, scenario, file.path))
                                    onDismiss()
                                },
                                modifier = Modifier.fillMaxWidth(),
                            ) {
                                Text(
                                    (if (file.path == currentPath) "SELECTED  " else "") + file.name,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                )
                            }
                        }
                    }
                }
                if (library.pageCount > 1) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center) {
                        IconButton(onClick = { load(library.folder, library.page - 1) }, enabled = library.page > 0) {
                            Icon(Icons.Rounded.ChevronLeft, contentDescription = "Previous page")
                        }
                        Text("PAGE ${library.page + 1}/${library.pageCount}", fontSize = 11.sp)
                        IconButton(onClick = { load(library.folder, library.page + 1) }, enabled = library.page + 1 < library.pageCount) {
                            Icon(Icons.Rounded.ChevronRight, contentDescription = "Next page")
                        }
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = { TextButton(onClick = onDismiss) { Text("CLOSE") } },
    )
}
