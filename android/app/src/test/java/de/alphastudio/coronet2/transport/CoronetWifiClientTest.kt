package de.alphastudio.coronet2.transport

import de.alphastudio.coronet2.model.ConnectionKind
import de.alphastudio.coronet2.model.CoronetDevice
import de.alphastudio.coronet2.model.DeviceSettings
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class CoronetWifiClientTest {
    @Test
    fun malformedHostBecomesOfflineSnapshot() {
        val device = CoronetDevice(id = "", name = "Broken host", host = "[", token = "token")

        val result = CoronetWifiClient().fetch(device)

        assertEquals(ConnectionKind.Offline, result.connection)
        assertNotNull(result.error)
    }

    @Test
    fun partialSettingsResponsePreservesPreviousValues() {
        val previous = DeviceSettings(
            loaded = true,
            revision = 9,
            displayBrightness = 72,
            fanMinPercent = 35,
            fanMaxPercent = 90,
        )

        val result = parseSettings(JSONObject().put("displayBrightness", 81), previous)

        assertEquals(81, result.displayBrightness)
        assertEquals(35, result.fanMinPercent)
        assertEquals(90, result.fanMaxPercent)
        assertEquals(9, result.revision)
    }

    @Test
    fun soundLibraryFiltersIncompleteEntries() {
        val json = JSONObject()
            .put("sdReady", true)
            .put("folder", 1)
            .put("folderCount", 3)
            .put("files", JSONArray()
                .put(JSONObject().put("name", "Start").put("path", "/sounds/start.wav"))
                .put(JSONObject().put("name", "Missing path")))

        val result = parseSoundLibrary(json)

        assertEquals(true, result.sdReady)
        assertEquals(1, result.files.size)
        assertEquals("/sounds/start.wav", result.files.single().path)
    }
}
