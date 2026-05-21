package dev.omiiba.connect.tv

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.commit

class MainActivity : AppCompatActivity(R.layout.activity_tv) {

    private val requestBluetooth = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { /* granted in onResume / user can retry from Connect */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestBluetoothPermissionsIfNeeded()
        if (savedInstanceState == null) {
            supportFragmentManager.commit {
                replace(R.id.main_container, MainBrowseFragment())
            }
        }
    }

    private fun requestBluetoothPermissionsIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return
        }
        val needed = buildList {
            if (ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) {
                add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            if (ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) {
                add(Manifest.permission.BLUETOOTH_SCAN)
            }
        }
        if (needed.isNotEmpty()) {
            requestBluetooth.launch(needed.toTypedArray())
        }
    }
}
