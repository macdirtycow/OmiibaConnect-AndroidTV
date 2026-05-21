package dev.omiiba.connect.tv

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Typeface
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity

/**
 * First screen on TV — plain UI, no Leanback. Shows self-test results on screen so you
 * do not need logcat. Open the main app with OK / Enter on the remote.
 */
class TvDiagnosticsActivity : FragmentActivity() {

    private lateinit var reportView: TextView
    private val report = StringBuilder()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildLayout())
        runSelfTest()
    }

    private fun buildLayout(): ScrollView {
        val report = TextView(this).apply {
            textSize = 20f
            typeface = Typeface.MONOSPACE
            setTextColor(0xFFE0E0E0.toInt())
            setPadding(48, 48, 48, 24)
        }
        reportView = report

        val openMain = Button(this).apply {
            text = getString(R.string.diagnostics_open_main)
            textSize = 18f
            isFocusable = true
            isFocusableInTouchMode = true
            setOnClickListener { openMainApp() }
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF101010.toInt())
            gravity = Gravity.CENTER_HORIZONTAL
            addView(
                report,
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    0,
                    1f,
                ),
            )
            addView(openMain, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        }

        return ScrollView(this).apply {
            addView(root, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT))
        }
    }

    private fun runSelfTest() {
        report.clear()
        line("Omiiba Connect TV — diagnose")
        line("Versie: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
        line("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        line("ABI: ${Build.SUPPORTED_ABIS.joinToString()}")
        line("")

        line("Activity: OK (dit scherm werkt)")
        testNative()
        testBluetoothPermissions()
        testLeanbackOnClasspath()

        val last = CrashReporter.readLast(this)
        if (last.isNotBlank()) {
            line("")
            line("Laatste crash opgeslagen:")
            line(last.trim())
        }

        line("")
        line(getString(R.string.diagnostics_hint))
        reportView.text = report.toString()
    }

    private fun testNative() {
        try {
            System.loadLibrary("omiiba_core")
            line("Native bibliotheek (omiiba_core): OK")
        } catch (t: Throwable) {
            line("Native bibliotheek: MISLUKT")
            line("  ${t.javaClass.simpleName}: ${t.message}")
        }
    }

    private fun testBluetoothPermissions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            line("Bluetooth rechten: niet nodig (API < 31)")
            return
        }
        val connect = ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
        val scan = ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
        line(
            "Bluetooth CONNECT: ${if (connect == PackageManager.PERMISSION_GRANTED) "toegestaan" else "nog niet"}",
        )
        line(
            "Bluetooth SCAN: ${if (scan == PackageManager.PERMISSION_GRANTED) "toegestaan" else "nog niet"}",
        )
    }

    private fun testLeanbackOnClasspath() {
        try {
            Class.forName("androidx.leanback.app.BrowseSupportFragment")
            line("Leanback UI: klasse gevonden")
        } catch (t: Throwable) {
            line("Leanback UI: MISLUKT — ${t.message}")
        }
    }

    private fun openMainApp() {
        line("")
        line("Hoofdapp starten…")
        reportView.text = report.toString()
        try {
            startActivity(Intent(this, MainActivity::class.java))
        } catch (t: Throwable) {
            CrashReporter.save(this, t)
            line("Hoofdapp: MISLUKT")
            line("  ${t.javaClass.simpleName}: ${t.message}")
            reportView.text = report.toString()
        }
    }

    private fun line(text: String) {
        report.append(text).append('\n')
    }
}
