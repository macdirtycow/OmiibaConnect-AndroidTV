package dev.omiiba.connect.tv

import android.content.Intent
import android.graphics.Typeface
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.fragment.app.FragmentActivity
import dev.omiiba.connect.tv.bluetooth.BluetoothAccess

/**
 * TV launcher activity — matches Android TV docs: setContentView + Leanback fragment in XML.
 */
class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
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
        try {
            super.onCreate(savedInstanceState)
            setContentView(R.layout.activity_tv)
        } catch (t: Throwable) {
            CrashReporter.save(this, t)
            showFatalError(t)
        }
    }

    private fun showFatalError(error: Throwable) {
        val message = TextView(this).apply {
            textSize = 20f
            typeface = Typeface.MONOSPACE
            setTextColor(0xFFE0E0E0.toInt())
            text = buildString {
                append("Omiiba Connect kon niet starten.\n\n")
                append(error.message ?: error.javaClass.simpleName)
                append("\n\n")
                append(ConnectStatusStore.read(this@MainActivity).takeIf { it.isNotBlank() }?.let {
                    "Laatste connect:\n$it\n\n"
                }.orEmpty())
                append(CrashReporter.readLast(this@MainActivity).take(800))
            }
            setPadding(48, 48, 48, 24)
        }
        val back = Button(this).apply {
            text = getString(R.string.diagnostics_open_main_back)
            setOnClickListener {
                startActivity(
                    Intent(this@MainActivity, TvDiagnosticsActivity::class.java)
                        .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK),
                )
                finish()
            }
        }
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF101010.toInt())
            gravity = Gravity.CENTER_HORIZONTAL
            addView(message, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
            addView(back)
        }
        setContentView(ScrollView(this).apply { addView(root) })
    }
}
