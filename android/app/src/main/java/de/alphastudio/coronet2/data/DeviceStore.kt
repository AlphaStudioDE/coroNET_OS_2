package de.alphastudio.coronet2.data

import android.content.Context
import android.content.SharedPreferences
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import de.alphastudio.coronet2.model.CoronetDevice
import org.json.JSONArray
import org.json.JSONObject

class DeviceStore(context: Context) {
    private val prefs: SharedPreferences = runCatching {
        val masterKey = MasterKey.Builder(context).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build()
        EncryptedSharedPreferences.create(
            context, "coronet_devices_secure", masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }.getOrElse { context.getSharedPreferences("coronet_devices", Context.MODE_PRIVATE) }

    fun load(): List<CoronetDevice> = runCatching {
        val array = JSONArray(prefs.getString("devices", "[]"))
        buildList {
            for (i in 0 until array.length()) {
                val item = array.getJSONObject(i)
                add(CoronetDevice(item.getString("id"), item.optString("name", item.getString("id")),
                    item.optString("address"), item.optString("host"), item.optString("token")))
            }
        }
    }.getOrDefault(emptyList())

    fun save(devices: List<CoronetDevice>) {
        val array = JSONArray()
        devices.forEach { device ->
            array.put(JSONObject().put("id", device.id).put("name", device.name)
                .put("address", device.address).put("host", device.host).put("token", device.token))
        }
        prefs.edit().putString("devices", array.toString()).apply()
    }
}
