package de.alphastudio.coronet2.transport

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

/** Bounded protocol-v2 reassembly, independent of Android's GATT lifecycle. */
internal class BleFrameAssembler(private val clock: () -> Long) {
    data class Message(val type: Int, val bytes: ByteArray)
    private data class Key(val source: UUID, val type: Int, val id: Int)
    private data class Assembly(val total: Int, val chunks: Array<ByteArray?>, val started: Long)
    private val pending = mutableMapOf<Key, Assembly>()

    fun clear() = pending.clear()

    fun accept(source: UUID, frame: ByteArray): Message? {
        if (frame.size <= 8 || (frame[0].toInt() and 255) != 2) return null
        val header = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN)
        header.get()
        val type = header.get().toInt() and 255
        val id = header.short.toInt() and 65535
        val total = header.short.toInt() and 65535
        val index = header.get().toInt() and 255
        val count = header.get().toInt() and 255
        if (total !in 1..4096 || count !in 1..255 || index >= count || frame.size - 8 > total) return null
        val now = clock()
        pending.entries.removeAll { now - it.value.started > 5000L }
        val key = Key(source, type, id)
        if (key !in pending && pending.size >= 8) {
            pending.minByOrNull { it.value.started }?.key?.let(pending::remove)
        }
        val assembly = pending.getOrPut(key) { Assembly(total, arrayOfNulls(count), now) }
        if (assembly.total != total || assembly.chunks.size != count) {
            pending.remove(key)
            return null
        }
        assembly.chunks[index] = frame.copyOfRange(8, frame.size)
        val received = assembly.chunks.sumOf { it?.size ?: 0 }
        if (received > total) {
            pending.remove(key)
            return null
        }
        if (assembly.chunks.any { it == null }) return null
        pending.remove(key)
        if (received != total) return null
        val bytes = ByteArray(total)
        var offset = 0
        for (chunk in assembly.chunks) {
            chunk!!.copyInto(bytes, offset)
            offset += chunk.size
        }
        return Message(type, bytes)
    }
}
