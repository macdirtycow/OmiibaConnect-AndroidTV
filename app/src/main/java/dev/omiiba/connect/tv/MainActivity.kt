package dev.omiiba.connect.tv

import android.content.Intent
import android.os.Build
import android.os.Bundle
import androidx.fragment.app.FragmentActivity
import dev.omiiba.connect.tv.bluetooth.BluetoothAccess

/**
 * TV launcher activity — matches Android TV docs: setContentView + Leanback fragment in XML.
 * @see <a href="https://developer.android.com/training/tv/playback/leanback/browse">Leanback browse</a>
 */
class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                !BluetoothAccess.hasRuntimePermissions(this)
            ) {
                startActivity(
                    Intent(this, TvDiagnosticsActivity::class.java)
                        .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK),
                )
                finish()
                return
            }
            super.onCreate(savedInstanceState)
            setContentView(R.layout.activity_tv)
        } catch (t: Throwable) {
            CrashReporter.save(this, t)
            startActivity(
                Intent(this, TvDiagnosticsActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK),
            )
            finish()
        }
    }
}
