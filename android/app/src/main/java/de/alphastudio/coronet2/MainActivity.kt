package de.alphastudio.coronet2

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.viewmodel.compose.viewModel
import de.alphastudio.coronet2.ui.CoronetApp

class MainActivity : ComponentActivity() {
    private var showBluetoothPermissionWarning by mutableStateOf(false)
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grants ->
        showBluetoothPermissionWarning = grants.any { (permission, granted) ->
            !granted && permission != Manifest.permission.POST_NOTIFICATIONS
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestRuntimePermissions()
        setContent {
            val model: CoronetViewModel = viewModel(factory = CoronetViewModel.Factory(application))
            CoronetApp(
                model = model,
                showBluetoothPermissionWarning = showBluetoothPermissionWarning,
                onRetryPermissions = ::requestRuntimePermissions,
                onDismissPermissionWarning = { showBluetoothPermissionWarning = false },
            )
        }
    }

    private fun requestRuntimePermissions() {
        showBluetoothPermissionWarning = false
        val permissions = buildList {
            if (Build.VERSION.SDK_INT in 23..30) {
                add(Manifest.permission.ACCESS_COARSE_LOCATION)
                add(Manifest.permission.ACCESS_FINE_LOCATION)
            }
            if (Build.VERSION.SDK_INT >= 31) {
                add(Manifest.permission.BLUETOOTH_SCAN)
                add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            if (Build.VERSION.SDK_INT >= 33) add(Manifest.permission.POST_NOTIFICATIONS)
        }
        if (permissions.isNotEmpty()) permissionLauncher.launch(permissions.toTypedArray())
    }
}
