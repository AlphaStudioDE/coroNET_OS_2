package de.alphastudio.coronet2.ui

import android.graphics.Color as AndroidColor
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.sp
import de.alphastudio.coronet2.model.DeviceSettings

@Immutable
data class CoronetPalette(
    val background: Color,
    val surface: Color,
    val raised: Color,
    val border: Color,
    val text: Color,
    val muted: Color,
    val accent: Color,
    val accentDark: Color,
    val amber: Color,
    val red: Color,
    val green: Color,
    val light: Boolean,
)

val LocalCoronetPalette = staticCompositionLocalOf { palette(0, false, 190) }

@Composable
fun CoronetTheme(settings: DeviceSettings, content: @Composable () -> Unit) {
    val light = settings.uiColorMode == 1 || (settings.uiColorMode == 2 && !isSystemInDarkTheme())
    val colors = palette(settings.uiSkin.coerceIn(0, 3), light, settings.accentHueDegrees)
    val scheme = if (light) {
        lightColorScheme(
            primary = colors.accent,
            onPrimary = colors.background,
            secondary = colors.amber,
            tertiary = colors.green,
            background = colors.background,
            onBackground = colors.text,
            surface = colors.surface,
            onSurface = colors.text,
            surfaceVariant = colors.raised,
            onSurfaceVariant = colors.muted,
            outline = colors.border,
            error = colors.red,
        )
    } else {
        darkColorScheme(
            primary = colors.accent,
            onPrimary = colors.background,
            secondary = colors.amber,
            tertiary = colors.green,
            background = colors.background,
            onBackground = colors.text,
            surface = colors.surface,
            onSurface = colors.text,
            surfaceVariant = colors.raised,
            onSurfaceVariant = colors.muted,
            outline = colors.border,
            error = colors.red,
        )
    }
    val base = MaterialTheme.typography
    MaterialTheme(
        colorScheme = scheme,
        typography = base.copy(
            bodyLarge = base.bodyLarge.withoutTracking(),
            bodyMedium = base.bodyMedium.withoutTracking(),
            bodySmall = base.bodySmall.withoutTracking(),
            titleLarge = base.titleLarge.withoutTracking(),
            titleMedium = base.titleMedium.withoutTracking(),
            labelLarge = base.labelLarge.withoutTracking(),
        ),
    ) {
        androidx.compose.runtime.CompositionLocalProvider(LocalCoronetPalette provides colors, content = content)
    }
}

private fun TextStyle.withoutTracking() = copy(letterSpacing = 0.sp)

private fun palette(skin: Int, light: Boolean, hue: Int): CoronetPalette {
    val base = if (light) {
        when (skin) {
            1 -> intArrayOf(0xF0F1F2.toInt(), 0xFFFFFF, 0xE4E7E9.toInt(), 0xB8C0C5.toInt())
            2 -> intArrayOf(0xF1F8F5.toInt(), 0xFFFFFF, 0xDDEFE8.toInt(), 0xA8C7BC.toInt())
            3 -> intArrayOf(0xFAFAFA.toInt(), 0xFFFFFF, 0xEEEEEE.toInt(), 0xCCCCCC.toInt())
            else -> intArrayOf(0xEFF7F8.toInt(), 0xFFFFFF, 0xDDEDEF.toInt(), 0xAEC8CD.toInt())
        }
    } else {
        when (skin) {
            1 -> intArrayOf(0x0B0C0E, 0x15171A, 0x202329, 0x3B4148)
            2 -> intArrayOf(0x07110D, 0x0E1C17, 0x173029, 0x2C5145)
            3 -> intArrayOf(0x050607, 0x101214, 0x1A1D20, 0x33383D)
            else -> intArrayOf(0x071018, 0x0D1A24, 0x132531, 0x29414F)
        }
    }
    val text = if (light) 0x142027 else 0xF4F8FA
    val muted = if (light) 0x607780 else if (skin == 2) 0x92B5A7 else 0x8FAAB7
    val accent = hsv(hue, if (light) 210 else 205, if (light) 190 else 230)
    val accentDark = hsv(hue, 210, if (light) 150 else 105)
    return CoronetPalette(
        background = rgb(base[0]),
        surface = rgb(base[1]),
        raised = rgb(base[2]),
        border = rgb(base[3]),
        text = rgb(text),
        muted = rgb(muted),
        accent = accent,
        accentDark = accentDark,
        amber = rgb(0xF1B84B),
        red = rgb(if (light) 0xD94343 else 0xFF6B6B),
        green = rgb(if (light) 0x289C5B else 0x55D88A),
        light = light,
    )
}

private fun hsv(hue: Int, saturation: Int, value: Int): Color = Color(
    AndroidColor.HSVToColor(
        floatArrayOf(
            hue.mod(360).toFloat(),
            saturation.coerceIn(0, 255) / 255f,
            value.coerceIn(0, 255) / 255f,
        )
    )
)

private fun rgb(value: Int) = Color(0xFF000000L or value.toLong())
