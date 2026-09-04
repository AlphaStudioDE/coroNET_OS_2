package de.alphastudio.coronet2.ui

import android.graphics.Paint
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.TemperatureSample
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.ceil
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.pow
import kotlin.math.roundToInt

private data class TemperatureSeries(
    val label: String,
    val color: Color,
    val value: (TemperatureSample) -> Double?,
)

internal object TemperatureChartColors {
    val tools = listOf(
        Color(0xFFF2B84B),
        Color(0xFF3E8DFF),
        Color(0xFF58D68D),
        Color(0xFFA970FF),
    )
    val bed = Color(0xFFF4F8FA)
    val chamber = Color(0xFFFF6B6B)
}

private data class TemperatureScale(
    val minimum: Double,
    val maximum: Double,
    val step: Double,
) {
    val ticks: List<Double>
        get() {
            val count = ((maximum - minimum) / step).roundToInt().coerceIn(1, 10)
            return (0..count).map { minimum + it * step }
        }
}

private fun temperatureScale(values: List<Double>): TemperatureScale {
    if (values.isEmpty()) return TemperatureScale(0.0, 100.0, 20.0)
    val rawMinimum = values.min()
    val rawMaximum = values.max()
    val padding = max(1.0, (rawMaximum - rawMinimum) * 0.08)
    val paddedMinimum = rawMinimum - padding
    val paddedMaximum = rawMaximum + padding
    val roughStep = max(0.1, (paddedMaximum - paddedMinimum) / 6.0)
    val magnitude = 10.0.pow(floor(log10(roughStep)))
    val normalized = roughStep / magnitude
    val factor = when {
        normalized < 1.5 -> 1.0
        normalized < 3.0 -> 2.0
        normalized < 7.0 -> 5.0
        else -> 10.0
    }
    val step = factor * magnitude
    var minimum = floor(paddedMinimum / step) * step
    if (rawMinimum >= 0.0 && minimum < 0.0) minimum = 0.0
    var maximum = ceil(paddedMaximum / step) * step
    if (maximum <= minimum) maximum = minimum + step
    return TemperatureScale(minimum, maximum, step)
}

private fun formatScaleValue(value: Double, step: Double): String =
    if (step < 1.0) "%.1f".format(Locale.getDefault(), value) else value.roundToInt().toString()

private fun smoothedTemperatureSeries(
    samples: List<TemperatureSample>,
    value: (TemperatureSample) -> Double?,
): List<Double?> {
    val recent = ArrayList<Double>(7)
    var filtered: Double? = null
    var previousTime = 0L
    return samples.map { sample ->
        val raw = value(sample)
        if (raw == null || !raw.isFinite()) {
            recent.clear()
            filtered = null
            previousTime = 0L
            null
        } else {
            val elapsed = sample.timestampEpochMs - previousTime
            if (previousTime == 0L || elapsed < 0L) {
                recent.clear()
                recent.add(raw)
                filtered = raw
            } else {
                if (recent.size == 7) recent.removeAt(0)
                recent.add(raw)
                val ordered = recent.sorted()
                val median = if (ordered.size % 2 == 0) {
                    (ordered[ordered.size / 2 - 1] + ordered[ordered.size / 2]) / 2.0
                } else {
                    ordered[ordered.size / 2]
                }
                val difference = filtered?.let { kotlin.math.abs(median - it) } ?: 0.0
                val timeConstantMs = when {
                    difference <= 1.5 -> 30_000.0
                    difference <= 5.0 -> 6_000.0
                    else -> 1_800.0
                }
                val filterElapsed = elapsed.coerceIn(100L, 5_000L)
                val alpha = 1.0 - exp(-filterElapsed.toDouble() / timeConstantMs)
                filtered = filtered?.let { it + alpha * (median - it) } ?: median
            }
            previousTime = sample.timestampEpochMs
            filtered
        }
    }
}

@Composable
internal fun TemperatureHistoryPanel(
    samples: List<TemperatureSample>,
    activeTool: Int,
) {
    val palette = LocalCoronetPalette.current
    val series = remember {
        listOf(
            TemperatureSeries("T1", TemperatureChartColors.tools[0]) { it.toolTemps.getOrNull(0) },
            TemperatureSeries("T2", TemperatureChartColors.tools[1]) { it.toolTemps.getOrNull(1) },
            TemperatureSeries("T3", TemperatureChartColors.tools[2]) { it.toolTemps.getOrNull(2) },
            TemperatureSeries("T4", TemperatureChartColors.tools[3]) { it.toolTemps.getOrNull(3) },
            TemperatureSeries("BED", TemperatureChartColors.bed) { it.bedTemp },
            TemperatureSeries("CHAMBER", TemperatureChartColors.chamber) { it.chamberTemp },
        )
    }
    var hiddenMask by rememberSaveable { mutableIntStateOf(0) }
    val visible = series.indices.filter { hiddenMask and (1 shl it) == 0 }
    val smoothed = remember(samples, series) {
        series.map { item -> smoothedTemperatureSeries(samples, item.value) }
    }
    val values = visible.asSequence().flatMap { index ->
        smoothed[index].asSequence().filterNotNull()
    }.filter(Double::isFinite).toList()
    val scale = temperatureScale(values)
    val timeFormatter = remember { SimpleDateFormat("HH:mm", Locale.getDefault()) }

    SectionPanel(
        title = "Temperature history",
        headerEnd = {
            Text(
                if (values.isEmpty()) "--" else
                    "${formatScaleValue(scale.minimum, scale.step)}-${formatScaleValue(scale.maximum, scale.step)} C",
                color = palette.text,
                fontSize = 10.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            series.chunked(3).forEach { row ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    row.forEach { item ->
                        val index = series.indexOf(item)
                        val enabled = hiddenMask and (1 shl index) == 0
                        val latest = samples.asReversed().firstNotNullOfOrNull(item.value)
                        Surface(
                            modifier = Modifier.weight(1f).clickable {
                                hiddenMask = hiddenMask xor (1 shl index)
                            },
                            shape = RoundedCornerShape(4.dp),
                            color = if (enabled) item.color.copy(alpha = 0.10f) else palette.raised.copy(alpha = 0.45f),
                            border = BorderStroke(1.dp, if (enabled) item.color.copy(alpha = 0.65f) else palette.border),
                        ) {
                            Row(
                                Modifier.padding(horizontal = 7.dp, vertical = 6.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(5.dp),
                            ) {
                                Surface(
                                    modifier = Modifier.size(if (index == activeTool) 8.dp else 6.dp),
                                    shape = CircleShape,
                                    color = item.color.copy(alpha = if (enabled) 1f else 0.25f),
                                ) {}
                                Text(
                                    buildString {
                                        append(item.label)
                                        latest?.let { append("  ").append("%.1f".format(it)) }
                                    },
                                    modifier = Modifier.weight(1f),
                                    color = if (enabled) palette.text else palette.muted.copy(alpha = 0.55f),
                                    fontSize = 9.sp,
                                    fontWeight = if (index == activeTool) FontWeight.Bold else FontWeight.Medium,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                )
                            }
                        }
                    }
                }
            }
        }
        Box(
            Modifier.fillMaxWidth().height(150.dp),
            contentAlignment = Alignment.Center,
        ) {
            Canvas(Modifier.fillMaxSize()) {
                val gridColor = palette.border.copy(alpha = 0.52f)
                val plotLeft = 34.dp.toPx()
                val plotRight = size.width - 4.dp.toPx()
                val plotTop = 6.dp.toPx()
                val plotBottom = size.height - 6.dp.toPx()
                val plotWidth = max(1f, plotRight - plotLeft)
                val plotHeight = max(1f, plotBottom - plotTop)
                val range = scale.maximum - scale.minimum
                val axisPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    color = palette.muted.toArgb()
                    textSize = 9.sp.toPx()
                }
                if (values.isNotEmpty()) {
                    scale.ticks.forEach { tick ->
                        val y = plotBottom - (((tick - scale.minimum) / range).toFloat() * plotHeight)
                        drawLine(gridColor, Offset(plotLeft, y), Offset(plotRight, y), strokeWidth = 1f)
                        drawContext.canvas.nativeCanvas.drawText(
                            formatScaleValue(tick, scale.step),
                            1.dp.toPx(),
                            y + axisPaint.textSize * 0.34f,
                            axisPaint,
                        )
                    }
                }
                if (samples.size < 2 || values.isEmpty()) return@Canvas
                val firstTime = samples.first().timestampEpochMs
                val lastTime = max(firstTime + 1L, samples.last().timestampEpochMs)
                visible.sortedBy { it == activeTool }.forEach { index ->
                    val item = series[index]
                    val path = Path()
                    var started = false
                    var previousX = -1f
                    samples.forEachIndexed sampleLoop@ { sampleIndex, sample ->
                        val value = smoothed[index].getOrNull(sampleIndex)
                        if (value == null || !value.isFinite()) {
                            started = false
                            return@sampleLoop
                        }
                        val x = plotLeft + ((sample.timestampEpochMs - firstTime).toDouble() /
                            (lastTime - firstTime).toDouble()).toFloat() * plotWidth
                        if (started && x - previousX < 0.7f) return@sampleLoop
                        val y = plotBottom - (((value - scale.minimum) / range).toFloat().coerceIn(0f, 1f) * plotHeight)
                        if (started) path.lineTo(x, y) else path.moveTo(x, y)
                        started = true
                        previousX = x
                    }
                    drawPath(
                        path,
                        color = item.color,
                        style = Stroke(width = if (index == activeTool) 5f else 2.5f),
                    )
                }
            }
            if (samples.size < 2 || values.isEmpty()) {
                Text("Collecting temperature history...", color = palette.muted, fontSize = 12.sp)
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(samples.firstOrNull()?.let { timeFormatter.format(Date(it.timestampEpochMs)) } ?: "--:--", color = palette.muted, fontSize = 10.sp)
            Text(samples.lastOrNull()?.let { timeFormatter.format(Date(it.timestampEpochMs)) } ?: "--:--", color = palette.muted, fontSize = 10.sp)
        }
    }
}
