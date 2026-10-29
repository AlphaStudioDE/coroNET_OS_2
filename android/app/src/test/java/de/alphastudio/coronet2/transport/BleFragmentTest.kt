package de.alphastudio.coronet2.transport

import de.alphastudio.coronet2.model.DeviceSettings
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

/**
 * Unit tests for F10 (BLE fragment bounds) and F13 (integer booleans) and F17 (transport alias).
 * These tests cover pure parser/validation logic without requiring Android runtime.
 */
class BleFragmentTest {
    private val source = UUID(0, 1)
    private fun frame(total: Int, index: Int, count: Int, payload: ByteArray = byteArrayOf(index.toByte())) =
        ByteBuffer.allocate(8 + payload.size).order(ByteOrder.LITTLE_ENDIAN)
            .put(2).put(3).putShort(1).putShort(total.toShort())
            .put(index.toByte()).put(count.toByte()).put(payload).array()

    @Test fun realAssemblerAcceptsAllFirmwareChunkCounts() {
        for (count in listOf(1, 64, 65, 255)) {
            val assembler = BleFrameAssembler { 0 }
            var result: BleFrameAssembler.Message? = null
            for (index in 0 until count) result = assembler.accept(source, frame(count, index, count))
            assertEquals(3, result!!.type)
            assertArrayEquals(ByteArray(count) { it.toByte() }, result.bytes)
        }
    }

    @Test fun acceptsFull4096BytePayloadOutOfOrder() {
        val assembler = BleFrameAssembler { 0 }
        val expected = ByteArray(4096) { (it % 239).toByte() }
        val chunks = expected.toList().chunked(17).map { it.toByteArray() }
        var result: BleFrameAssembler.Message? = null
        for (index in chunks.indices.reversed()) {
            result = assembler.accept(source, frame(4096, index, chunks.size, chunks[index]))
        }
        assertArrayEquals(expected, result!!.bytes)
    }

    @Test fun rejectsMalformedBoundsAndLength() {
        val assembler = BleFrameAssembler { 0 }
        assertNull(assembler.accept(source, frame(1, 0, 0)))
        assertNull(assembler.accept(source, frame(4097, 0, 1)))
        assertNull(assembler.accept(source, frame(1, 1, 1)))
        assertNull(assembler.accept(source, frame(1, 0, 1, byteArrayOf(1, 2))))
        assertNull(assembler.accept(source, frame(2, 0, 1)))
    }

    @Test fun duplicateFragmentsDoNotInflatePayload() {
        val assembler = BleFrameAssembler { 0 }
        assertNull(assembler.accept(source, frame(2, 0, 2)))
        assertNull(assembler.accept(source, frame(2, 0, 2)))
        assertArrayEquals(byteArrayOf(0, 1), assembler.accept(source, frame(2, 1, 2))!!.bytes)
    }

    @Test fun expiresIncompleteMessagesAndClearsSessions() {
        var now = 0L
        val assembler = BleFrameAssembler { now }
        assertNull(assembler.accept(source, frame(2, 0, 2)))
        now = 5001L
        assertNull(assembler.accept(source, frame(2, 1, 2)))
        assembler.clear()
        assertNull(assembler.accept(source, frame(2, 0, 2)))
    }

    @Test fun sourceCharacteristicsCannotMixFragments() {
        val assembler = BleFrameAssembler { 0 }
        assertNull(assembler.accept(source, frame(2, 0, 2)))
        assertNull(assembler.accept(UUID(0, 2), frame(2, 1, 2)))
        assertArrayEquals(byteArrayOf(0, 1), assembler.accept(source, frame(2, 1, 2))!!.bytes)
    }

    // ---- F13: integer booleans ----

    /**
     * Verifies that parseSettings correctly interprets integer 1 as true for setupDone,
     * bleEnabled, and apiPaired — reproducing the F13 firmware wire contract.
     */
    @Test
    fun integerOneIsParsedAsTrueForBooleanFields() {
        val json = JSONObject()
            .put("setupDone", 1)
            .put("bleEnabled", 1)
            .put("apiPaired", 1)

        val result = parseSettings(json, DeviceSettings(setupDone = false, bleEnabled = false, apiPaired = false))

        assertTrue("setupDone=1 must parse as true", result.setupDone)
        assertTrue("bleEnabled=1 must parse as true", result.bleEnabled)
        assertTrue("apiPaired=1 must parse as true", result.apiPaired)
    }

    @Test
    fun integerZeroIsParsedAsFalseForBooleanFields() {
        val json = JSONObject()
            .put("setupDone", 0)
            .put("bleEnabled", 0)
            .put("apiPaired", 0)

        val result = parseSettings(json, DeviceSettings(setupDone = true, bleEnabled = true, apiPaired = true))

        assertFalse("setupDone=0 must parse as false", result.setupDone)
        assertFalse("bleEnabled=0 must parse as false", result.bleEnabled)
        assertFalse("apiPaired=0 must parse as false", result.apiPaired)
    }

    @Test
    fun missingBooleanFieldFallsBackToPreviousValue() {
        val json = JSONObject()
        val previous = DeviceSettings(setupDone = true, bleEnabled = false, apiPaired = true)

        val result = parseSettings(json, previous)

        assertTrue("Missing setupDone must retain previous true", result.setupDone)
        assertFalse("Missing bleEnabled must retain previous false", result.bleEnabled)
        assertTrue("Missing apiPaired must retain previous true", result.apiPaired)
    }

    @Test fun preservesBooleanStringsAndRejectsInvalidNumbers() {
        assertTrue(parseSettings(JSONObject().put("setupDone", "true")).setupDone)
        assertFalse(parseSettings(JSONObject().put("bleEnabled", "false")).bleEnabled)
        assertTrue(parseSettings(JSONObject().put("setupDone", 7), DeviceSettings(setupDone = true)).setupDone)
        assertFalse(parseSettings(JSONObject().put("setupDone", 0.5), DeviceSettings(setupDone = false)).setupDone)
    }

    // ---- F17: transport alias normalization ----

    @Test
    fun companionTransportKeyIsRecognizedByParser() {
        val json = JSONObject().put("companionTransport", 2)

        val result = parseSettings(json, DeviceSettings(companionTransport = 0))

        assertEquals("companionTransport=2 must be parsed", 2, result.companionTransport)
    }

    @Test
    fun transportValueKeyIsRecognizedByParser() {
        val json = JSONObject().put("transportValue", 1)

        val result = parseSettings(json, DeviceSettings(companionTransport = 0))

        assertEquals("transportValue=1 must be parsed", 1, result.companionTransport)
    }

    @Test
    fun companionTransportTakesPrecedenceOverTransportValue() {
        val json = JSONObject().put("companionTransport", 2).put("transportValue", 1)

        val result = parseSettings(json, DeviceSettings(companionTransport = 0))

        assertEquals("companionTransport must take precedence over transportValue", 2, result.companionTransport)
    }
}
