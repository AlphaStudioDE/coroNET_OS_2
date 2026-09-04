package de.alphastudio.coronet2.transport

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class LedFrameProtocolTest {
    @Test
    fun parsesCompleteRgbFrame() {
        val bytes = ByteArray(192)
        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put(1).put(1).putShort(192).putInt(0x12345678)
        buffer.put(42).put(18).putShort(0)
        repeat(60) { index ->
            buffer.put(index.toByte()).put((index + 1).toByte()).put((index + 2).toByte())
        }

        val frame = parseLedFrame(bytes)

        requireNotNull(frame)
        assertTrue(frame.available)
        assertEquals(0x12345678L, frame.sequence)
        assertEquals(42, frame.outer.size)
        assertEquals(18, frame.inside.size)
        assertEquals(0x000102, frame.outer.first())
        assertEquals(0x2A2B2C, frame.inside.first())
        assertEquals(0x3B3C3D, frame.inside.last())
    }

    @Test
    fun rejectsWrongSizeAndPixelFormat() {
        assertNull(parseLedFrame(ByteArray(191)))
        val bytes = ByteArray(192)
        bytes[0] = 1
        bytes[1] = 2
        bytes[2] = 192.toByte()
        bytes[8] = 42
        bytes[9] = 18
        assertNull(parseLedFrame(bytes))
    }
}
