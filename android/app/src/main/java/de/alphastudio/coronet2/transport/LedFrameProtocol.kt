package de.alphastudio.coronet2.transport

import de.alphastudio.coronet2.model.LedFrame
import java.nio.ByteBuffer
import java.nio.ByteOrder

internal const val LedFrameMessageType = 5
private const val LedFrameVersion = 1
private const val PixelFormatRgb888 = 1
private const val HeaderBytes = 12
private const val OuterCount = 42
private const val InsideCount = 18
private const val FrameBytes = HeaderBytes + (OuterCount + InsideCount) * 3

internal fun parseLedFrame(bytes: ByteArray): LedFrame? {
    if (bytes.size != FrameBytes) return null
    val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
    if ((buffer.get().toInt() and 0xff) != LedFrameVersion ||
        (buffer.get().toInt() and 0xff) != PixelFormatRgb888 ||
        (buffer.short.toInt() and 0xffff) != FrameBytes) return null
    val sequence = buffer.int.toLong() and 0xffffffffL
    if ((buffer.get().toInt() and 0xff) != OuterCount ||
        (buffer.get().toInt() and 0xff) != InsideCount) return null
    buffer.short

    fun readPixels(count: Int) = List(count) {
        val red = buffer.get().toInt() and 0xff
        val green = buffer.get().toInt() and 0xff
        val blue = buffer.get().toInt() and 0xff
        (red shl 16) or (green shl 8) or blue
    }
    return LedFrame(sequence, readPixels(OuterCount), readPixels(InsideCount), true)
}
