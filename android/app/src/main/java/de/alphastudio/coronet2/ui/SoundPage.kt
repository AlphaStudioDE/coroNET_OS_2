package de.alphastudio.coronet2.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronLeft
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.GraphicEq
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Stop
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.DeviceSettings
import de.alphastudio.coronet2.model.SoundFileEntry
import de.alphastudio.coronet2.model.SoundLibrarySnapshot

@Composable
internal fun SoundPage(
    settings: DeviceSettings,
    library: SoundLibrarySnapshot,
    send: (String) -> Unit,
    loadLibrary: (Int, Int) -> Unit,
    selectSound: (Int, String) -> Unit,
    play: (Int) -> Unit,
    stop: () -> Unit,
) {
    var scenario by rememberSaveable { mutableIntStateOf(0) }
    var showLibrary by rememberSaveable { mutableStateOf(false) }
    val path = settings.soundPath.getOrElse(scenario) { "" }
    AdaptivePage(
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
                    showLibrary = !showLibrary
                    if (showLibrary) loadLibrary(0, 0)
                }
                if (showLibrary) {
                    SoundLibraryCarousel(
                        currentPath = path,
                        scenario = scenario,
                        library = library,
                        selectSound = selectSound,
                        load = loadLibrary,
                        onSelected = { showLibrary = false },
                    )
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { play(scenario) }, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Rounded.PlayArrow, contentDescription = null)
                        Text("PLAY")
                    }
                    OutlinedButton(onClick = stop, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Rounded.Stop, contentDescription = null)
                        Text("STOP")
                    }
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
}

@Composable
private fun SoundLibraryCarousel(
    currentPath: String,
    scenario: Int,
    library: SoundLibrarySnapshot,
    selectSound: (Int, String) -> Unit,
    load: (Int, Int) -> Unit,
    onSelected: () -> Unit,
) {
    val files = remember { mutableStateListOf<SoundFileEntry>() }
    val listState = rememberLazyListState()
    var collectedFolder by remember { mutableIntStateOf(-1) }
    var collectedThroughPage by remember { mutableIntStateOf(-1) }

    LaunchedEffect(Unit) { if (!library.loaded && !library.loading) load(0, 0) }
    LaunchedEffect(library.loaded, library.folder, library.page, library.files) {
        if (!library.loaded) return@LaunchedEffect
        if (library.folder != collectedFolder || library.page == 0) {
            files.clear()
            collectedFolder = library.folder
            collectedThroughPage = -1
        }
        if (library.page == collectedThroughPage + 1) {
            library.files.forEach { file ->
                if (files.none { it.path == file.path }) files.add(file)
            }
            collectedThroughPage = library.page
        }
    }
    val shouldLoadNextPage by remember(
        listState,
        library.loading,
        library.pageCount,
        collectedFolder,
        collectedThroughPage,
    ) {
        derivedStateOf {
            val lastVisible = listState.layoutInfo.visibleItemsInfo.lastOrNull()?.index ?: -1
            files.isNotEmpty() &&
                lastVisible >= files.lastIndex - 1 &&
                !library.loading &&
                collectedFolder >= 0 &&
                collectedThroughPage + 1 < library.pageCount
        }
    }
    LaunchedEffect(shouldLoadNextPage, collectedFolder, collectedThroughPage, library.pageCount) {
        if (shouldLoadNextPage) load(collectedFolder, collectedThroughPage + 1)
    }
    val showScrollHint by remember(listState, library.loading, library.pageCount, collectedThroughPage) {
        derivedStateOf {
            listState.canScrollForward ||
                library.loading ||
                collectedThroughPage + 1 < library.pageCount
        }
    }
    val showBackHint by remember(listState) {
        derivedStateOf { listState.canScrollBackward }
    }
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            IconButton(
                onClick = {
                    val folder = (library.folder - 1).coerceAtLeast(0)
                    files.clear()
                    collectedFolder = folder
                    collectedThroughPage = -1
                    load(folder, 0)
                },
                enabled = library.folder > 0,
            ) {
                Icon(Icons.Rounded.ChevronLeft, contentDescription = "Previous folder")
            }
            Text(
                if (library.folderCount > 0) library.folderName else "NO SOUND FOLDERS",
                modifier = Modifier.weight(1f),
                textAlign = TextAlign.Center,
                fontWeight = FontWeight.Bold,
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            IconButton(
                onClick = {
                    val folder = library.folder + 1
                    files.clear()
                    collectedFolder = folder
                    collectedThroughPage = -1
                    load(folder, 0)
                },
                enabled = library.folder + 1 < library.folderCount,
            ) {
                Icon(Icons.Rounded.ChevronRight, contentDescription = "Next folder")
            }
        }
        when {
            library.loading && files.isEmpty() -> Row(
                Modifier.fillMaxWidth().height(76.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center,
            ) {
                CircularProgressIndicator(Modifier.size(28.dp))
            }
            library.error != null -> Text(library.error, color = MaterialTheme.colorScheme.error)
            !library.sdReady -> Text("microSD card unavailable", color = MaterialTheme.colorScheme.error)
            else -> Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                LazyRow(
                    modifier = Modifier.fillMaxWidth(),
                    state = listState,
                    horizontalArrangement = Arrangement.spacedBy(7.dp),
                ) {
                    item(key = "built-in-default") {
                        SoundCarouselItem(
                            title = "BUILT-IN DEFAULT",
                            selected = currentPath.isBlank(),
                            onClick = {
                                selectSound(scenario, "")
                                onSelected()
                            },
                        )
                    }
                    items(files, key = { it.path }) { file ->
                        SoundCarouselItem(
                            title = file.name,
                            selected = file.path == currentPath,
                            onClick = {
                                selectSound(scenario, file.path)
                                onSelected()
                            },
                        )
                    }
                    if (library.loading) {
                        item(key = "loading") {
                            Row(
                                Modifier.width(72.dp).height(76.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.Center,
                            ) {
                                CircularProgressIndicator(Modifier.size(26.dp))
                            }
                        }
                    }
                }
                if (showBackHint || showScrollHint) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        if (showBackHint) {
                            listOf(0.56f, 0.34f, 0.18f).forEach { opacity ->
                                Icon(
                                    Icons.Rounded.ChevronLeft,
                                    contentDescription = null,
                                    modifier = Modifier.size(16.dp).alpha(opacity),
                                    tint = MaterialTheme.colorScheme.primary,
                                )
                            }
                        }
                        Spacer(Modifier.weight(1f))
                        if (showScrollHint) {
                            listOf(0.18f, 0.34f, 0.56f).forEach { opacity ->
                                Icon(
                                    Icons.Rounded.ChevronRight,
                                    contentDescription = null,
                                    modifier = Modifier.size(16.dp).alpha(opacity),
                                    tint = MaterialTheme.colorScheme.primary,
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SoundCarouselItem(title: String, selected: Boolean, onClick: () -> Unit) {
    val palette = LocalCoronetPalette.current
    Surface(
        modifier = Modifier.width(184.dp).height(76.dp).clickable(onClick = onClick),
        shape = RoundedCornerShape(6.dp),
        color = if (selected) palette.accent.copy(alpha = 0.12f) else palette.raised,
        border = BorderStroke(1.dp, if (selected) palette.accent else palette.border),
    ) {
        Column(
            Modifier.padding(horizontal = 11.dp, vertical = 9.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text(
                title,
                color = if (selected) palette.accent else palette.text,
                fontSize = 12.sp,
                fontWeight = FontWeight.SemiBold,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
            if (selected) Text("SELECTED", color = palette.accent, fontSize = 9.sp, fontWeight = FontWeight.Bold)
        }
    }
}
