package de.alphastudio.coronet2.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.roundToInt

@Composable
internal fun AdaptivePage(
    title: String,
    subtitle: String,
    primary: @Composable ColumnScope.() -> Unit,
    secondary: @Composable ColumnScope.() -> Unit,
) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val compact = maxWidth < 680.dp
        if (compact) {
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                PageHeading(title, subtitle)
                primary()
                secondary()
            }
        } else {
            Column(Modifier.fillMaxSize().padding(horizontal = 14.dp, vertical = 10.dp)) {
                PageHeading(title, subtitle)
                Spacer(Modifier.height(8.dp))
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    Column(
                        Modifier.weight(1.12f).fillMaxHeight().verticalScroll(rememberScrollState()).padding(bottom = 12.dp),
                        verticalArrangement = Arrangement.spacedBy(10.dp),
                        content = primary,
                    )
                    Column(
                        Modifier.weight(0.88f).fillMaxHeight().verticalScroll(rememberScrollState()).padding(bottom = 12.dp),
                        verticalArrangement = Arrangement.spacedBy(10.dp),
                        content = secondary,
                    )
                }
            }
        }
    }
}

@Composable
internal fun PageHeading(title: String, subtitle: String) {
    val palette = LocalCoronetPalette.current
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.Bottom) {
        Text(title, fontSize = 22.sp, fontWeight = FontWeight.Light, color = palette.text)
        Spacer(Modifier.width(10.dp))
        Text(subtitle, fontSize = 12.sp, color = palette.muted, modifier = Modifier.padding(bottom = 3.dp))
    }
}

@Composable
internal fun SectionPanel(
    title: String,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    val palette = LocalCoronetPalette.current
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(7.dp),
        color = palette.surface,
        border = BorderStroke(1.dp, palette.border),
        tonalElevation = 0.dp,
    ) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title.uppercase(), fontSize = 11.sp, fontWeight = FontWeight.Bold, color = palette.accent)
            content()
        }
    }
}

@Composable
internal fun SettingSwitch(label: String, checked: Boolean, note: String? = null, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(label, fontSize = 14.sp, color = MaterialTheme.colorScheme.onSurface)
            note?.let { Text(it, fontSize = 10.sp, color = MaterialTheme.colorScheme.onSurfaceVariant) }
        }
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

@Composable
internal fun ChoiceButton(label: String, value: String, enabled: Boolean = true, onClick: () -> Unit) {
    OutlinedButton(
        onClick = onClick,
        enabled = enabled,
        modifier = Modifier.fillMaxWidth().height(46.dp),
        shape = RoundedCornerShape(6.dp),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 12.dp),
    ) {
        Text(label, modifier = Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurface, maxLines = 1)
        Text(value, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold, maxLines = 1, overflow = TextOverflow.Ellipsis)
        Icon(Icons.Rounded.ChevronRight, contentDescription = null, modifier = Modifier.size(18.dp))
    }
}

@Composable
internal fun CompactChoices(values: List<String>, selected: Int, onSelect: (Int) -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        values.forEachIndexed { index, value ->
            val active = index == selected
            Button(
                onClick = { onSelect(index) },
                modifier = Modifier.weight(1f).height(44.dp),
                shape = RoundedCornerShape(5.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                    contentColor = if (active) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant,
                ),
                contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 4.dp),
            ) { Text(value, fontSize = 10.sp, fontWeight = FontWeight.Bold, maxLines = 1) }
        }
    }
}

@Composable
internal fun ValueSlider(
    label: String,
    value: Int,
    range: IntRange,
    suffix: String = "",
    onCommit: (Int) -> Unit,
) {
    var local by remember(label) { mutableFloatStateOf(value.toFloat()) }
    LaunchedEffect(value) { local = value.toFloat() }
    Column(Modifier.fillMaxWidth()) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(label, modifier = Modifier.weight(1f), fontSize = 13.sp)
            Text("${local.roundToInt()}$suffix", color = MaterialTheme.colorScheme.primary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
        }
        Slider(
            value = local.coerceIn(range.first.toFloat(), range.last.toFloat()),
            onValueChange = { local = it },
            onValueChangeFinished = { onCommit(local.roundToInt().coerceIn(range)) },
            valueRange = range.first.toFloat()..range.last.toFloat(),
            modifier = Modifier.fillMaxWidth().height(30.dp),
        )
    }
}

@Composable
internal fun MetricTile(label: String, value: String, accent: Color, modifier: Modifier = Modifier) {
    val palette = LocalCoronetPalette.current
    Surface(
        modifier = modifier.height(76.dp),
        shape = RoundedCornerShape(6.dp),
        color = palette.raised,
        border = BorderStroke(1.dp, palette.border),
    ) {
        Column(Modifier.padding(10.dp), verticalArrangement = Arrangement.SpaceBetween) {
            Text(label.uppercase(), fontSize = 10.sp, color = palette.muted, fontWeight = FontWeight.Bold)
            Text(value, fontSize = 21.sp, color = accent, fontWeight = FontWeight.Light, maxLines = 1)
        }
    }
}

@Composable
internal fun LedLayoutPreview(
    brightness: List<Int>,
    selectedSection: Int,
    insideAmbient: Boolean,
    mirror: Boolean,
) {
    val palette = LocalCoronetPalette.current
    Canvas(
        Modifier.fillMaxWidth().height(92.dp).background(palette.background, RoundedCornerShape(5.dp)).padding(8.dp)
    ) {
        val margin = 12.dp.toPx()
        val topY = size.height * 0.32f
        val insideY = size.height * 0.72f
        val outerStart = margin
        val outerWidth = size.width - margin * 2f
        val ledGap = outerWidth / 42f
        val radius = (ledGap * 0.28f).coerceAtLeast(2.2f)
        val sectionColors = listOf(Color(0xFFED4E4E), Color(0xFF45CF84), Color(0xFF3E8DFF))
        for (logical in 0 until 42) {
            val section = when {
                logical < 11 -> 0
                logical < 31 -> 1
                else -> 2
            }
            val visual = if (mirror) 41 - logical else logical
            val alpha = (brightness.getOrElse(section) { 70 } / 100f).coerceIn(0.12f, 1f)
            drawCircle(
                color = sectionColors[section].copy(alpha = if (selectedSection == section) alpha else alpha * 0.58f),
                radius = if (selectedSection == section) radius * 1.25f else radius,
                center = Offset(outerStart + (visual + 0.5f) * ledGap, topY),
            )
        }
        val innerGap = outerWidth / 18f
        val innerBase = if (insideAmbient) palette.accent else Color.White
        val innerAlpha = (brightness.getOrElse(3) { 70 } / 100f).coerceIn(0.12f, 1f)
        for (index in 0 until 18) {
            val color = if (insideAmbient) {
                when {
                    index < 6 -> sectionColors[0]
                    index < 12 -> sectionColors[1]
                    else -> sectionColors[2]
                }
            } else innerBase
            drawCircle(
                color = color.copy(alpha = if (selectedSection == 3) innerAlpha else innerAlpha * 0.58f),
                radius = if (selectedSection == 3) radius * 1.25f else radius,
                center = Offset(outerStart + (index + 0.5f) * innerGap, insideY),
            )
        }
        drawLine(palette.border, Offset(margin, size.height * 0.52f), Offset(size.width - margin, size.height * 0.52f), 1.dp.toPx(), StrokeCap.Round)
    }
}

internal fun jsonSetting(key: String, value: Any): String = JSONObject().put(key, value).toString()

internal fun intArraySetting(key: String, source: List<Int>, size: Int, index: Int, value: Int, fallback: Int): String {
    val values = MutableList(size) { source.getOrElse(it) { fallback } }
    values[index.coerceIn(0, size - 1)] = value
    return JSONObject().put(key, JSONArray(values)).toString()
}

internal fun booleanArraySetting(key: String, source: List<Boolean>, size: Int, index: Int, value: Boolean): String {
    val values = MutableList(size) { source.getOrElse(it) { false } }
    values[index.coerceIn(0, size - 1)] = value
    return JSONObject().put(key, JSONArray(values)).toString()
}

internal fun stringArraySetting(key: String, source: List<String>, size: Int, index: Int, value: String): String {
    val values = MutableList(size) { source.getOrElse(it) { "" } }
    values[index.coerceIn(0, size - 1)] = value
    return JSONObject().put(key, JSONArray(values)).toString()
}

internal fun stateColorValue(state: String, palette: CoronetPalette): Color = when (state.lowercase()) {
    "printing" -> palette.accent
    "complete", "finished" -> palette.green
    "paused" -> palette.amber
    "error", "timeout" -> palette.red
    else -> palette.muted
}

@Composable
internal fun stateColor(state: String): Color = stateColorValue(state, LocalCoronetPalette.current)
