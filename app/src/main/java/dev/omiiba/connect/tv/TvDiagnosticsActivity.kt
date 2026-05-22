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
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import dev.omiiba.connect.tv.bluetooth.BluetoothAccess
import dev.omiiba.connect.tv.bluetooth.BluetoothRfcommTransport
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

/**
 * First screen on TV — plain UI, no Leanback. Shows self-test results on screen.
 */
class TvDiagnosticsActivity : ComponentActivity() {

    private lateinit var reportView: TextView
    private val report = StringBuilder()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var rfcommProbe: BluetoothRfcommTransport? = null

    private val requestBluetooth = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) {
        runSelfTest()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            super.onCreate(savedInstanceState)
            rfcommProbe = BluetoothRfcommTransport(applicationContext)
            setContentView(buildLayout())
            reportView.text = getString(R.string.diagnostics_loading)
            reportView.post { runSelfTest() }
        } catch (t: Throwable) {
            CrashReporter.save(applicationContext, t)
            showBootstrapError(t)
        }
    }

    private fun showBootstrapError(error: Throwable) {
        val tv = TextView(this).apply {
            textSize = 18f
            typeface = Typeface.MONOSPACE
            setTextColor(0xFFE0E0E0.toInt())
            setPadding(48, 48, 48, 48)
            text = buildString {
                append("Omiiba Connect TV kon niet starten.\n\n")
                append(error.javaClass.simpleName)
                append(": ")
                append(error.message ?: "(geen bericht)")
                append("\n\nVerwijder de app volledig en installeer opnieuw.")
            }
        }
        setContentView(
            ScrollView(this).apply {
                addView(tv)
            },
        )
    }

    private fun buildLayout(): ScrollView {
        val report = TextView(this).apply {
            textSize = 20f
            typeface = Typeface.MONOSPACE
            setTextColor(0xFFE0E0E0.toInt())
            setPadding(48, 48, 48, 24)
        }
        reportView = report

        val grantBt = Button(this).apply {
            text = getString(R.string.diagnostics_grant_bluetooth)
            textSize = 18f
            isFocusable = true
            isFocusableInTouchMode = true
            setOnClickListener { requestBluetoothPermissions() }
        }

        val openMain = Button(this).apply {
            text = getString(R.string.diagnostics_open_main)
            textSize = 18f
            isFocusable = true
            isFocusableInTouchMode = true
            setOnClickListener { openMainApp() }
        }

        val clearCrash = Button(this).apply {
            text = getString(R.string.diagnostics_clear_crash)
            textSize = 18f
            isFocusable = true
            isFocusableInTouchMode = true
            setOnClickListener {
                CrashReporter.clear(this@TvDiagnosticsActivity)
                ConnectStatusStore.clear(this@TvDiagnosticsActivity)
                runSelfTest()
            }
        }

        val rfcommTest = Button(this).apply {
            text = getString(R.string.diagnostics_rfcomm_test)
            textSize = 18f
            isFocusable = true
            isFocusableInTouchMode = true
            setOnClickListener { runRfcommProbe() }
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
            addView(grantBt, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
            addView(rfcommTest, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
            addView(clearCrash, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
            addView(openMain, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        }

        return ScrollView(this).apply {
            addView(root, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT))
        }
    }

    private fun requestBluetoothPermissions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            runSelfTest()
            return
        }
        if (BluetoothAccess.hasRuntimePermissions(this)) {
            runSelfTest()
            return
        }
        requestBluetooth.launch(BluetoothAccess.runtimePermissions)
    }

    private fun runSelfTest() {
        if (!::reportView.isInitialized) {
            return
        }
        report.clear()
        try {
            line("Omiiba Connect TV — diagnose")
            line("Versie: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
            line("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
            line("ABI: ${Build.SUPPORTED_ABIS.joinToString()}")
            line("")

            line("Activity: OK (dit scherm werkt)")
            testNative()
            testBluetooth()
            testLeanbackOnClasspath()

            val lastConnect = ConnectStatusStore.read(this)
            if (lastConnect.isNotBlank()) {
                line("")
                line("Laatste connect-poging:")
                line(lastConnect.trim().take(600))
            }

            val last = CrashReporter.readLast(this)
            if (last.isNotBlank()) {
                line("")
                line("Laatste crash:")
                line(last.trim().take(1200))
            }

            line("")
            line(getString(R.string.diagnostics_hint))
        } catch (t: Throwable) {
            CrashReporter.save(this, t)
            line("")
            line("Diagnose mislukt (app blijft open):")
            line("${t.javaClass.simpleName}: ${t.message}")
        }
        reportView.text = report.toString()
    }

    private fun testNative() {
        try {
            System.loadLibrary("omiiba_core")
            line("Native bibliotheek: OK")
        } catch (t: Throwable) {
            line("Native bibliotheek: MISLUKT")
            line("  ${t.javaClass.simpleName}: ${t.message}")
        }
    }

    private fun testBluetooth() {
        line("Bluetooth rechten: ${BluetoothAccess.permissionSummary(this)}")
        val scan = try {
            BluetoothAccess.findHeadphones(this)
        } catch (t: Throwable) {
            line("Bluetooth scan: MISLUKT — ${t.javaClass.simpleName}: ${t.message}")
            return
        }
        if (scan.error != null) {
            line("Bluetooth scan: ${scan.error}")
            if (!scan.permissionGranted) {
                line("  → Selecteer knop: ${getString(R.string.diagnostics_grant_bluetooth)}")
            }
            return
        }
        if (scan.sonyClassic.isNotEmpty()) {
            line("Sony Classic (gebruik deze): ${scan.sonyClassic.size}")
            scan.sonyClassic.forEach { device ->
                line("  • ${BluetoothAccess.deviceLabel(device)}")
            }
        }
        if (scan.sonyLeOnly.isNotEmpty()) {
            line("Sony LE (niet gebruiken voor Omiiba): ${scan.sonyLeOnly.size}")
            scan.sonyLeOnly.forEach { device ->
                line("  • ${BluetoothAccess.deviceLabel(device)}")
            }
        }
        if (scan.error != null) {
            line("Let op: ${scan.error}")
        }
        if (scan.sonyClassic.isEmpty() && scan.otherBonded.isNotEmpty()) {
            line("Geen Sony Classic, wel andere apparaten:")
            scan.otherBonded.forEach { device ->
                line("  • ${BluetoothAccess.deviceLabel(device)}")
            }
        }
        if (scan.sonyClassic.isEmpty() && scan.sonyLeOnly.isEmpty() && scan.otherBonded.isEmpty()) {
            line("Geen gekoppelde BT-apparaten zichtbaar voor de app.")
            line("  Koppel WH-1000XM3 in TV Instellingen → Bluetooth.")
            line("  Daarna opnieuw Bluetooth toestaan hier.")
        }
    }

    private fun testLeanbackOnClasspath() {
        try {
            Class.forName("androidx.leanback.app.BrowseSupportFragment")
            line("Leanback UI: OK")
        } catch (t: Throwable) {
            line("Leanback UI: MISLUKT — ${t.message}")
        }
    }

    private fun runRfcommProbe() {
        val transport = rfcommProbe
        if (transport == null) {
            ConnectStatusStore.save(this, "RFCOMM-test: transport niet klaar")
            runSelfTest()
            return
        }
        val scan = BluetoothAccess.findHeadphones(this)
        val device = scan.sonyClassic.firstOrNull() ?: scan.preferredDevices.firstOrNull()
        if (device == null) {
            ConnectStatusStore.save(this, "RFCOMM-test: geen apparaat in lijst")
            runSelfTest()
            return
        }
        val label = BluetoothAccess.safeName(device) ?: device.address
        scope.launch(Dispatchers.IO) {
            try {
                ConnectStatusStore.save(this@TvDiagnosticsActivity, "RFCOMM-test start: $label")
                transport.connect(device.address) { step ->
                    ConnectStatusStore.save(this@TvDiagnosticsActivity, step)
                }
                transport.disconnect()
                ConnectStatusStore.save(
                    this@TvDiagnosticsActivity,
                    "RFCOMM-test OK — TV kan Sony MDR-kanaal openen",
                )
            } catch (t: Throwable) {
                ConnectStatusStore.save(
                    this@TvDiagnosticsActivity,
                    "RFCOMM-test MISLUKT: ${t.message ?: t.javaClass.simpleName}",
                )
            }
            runOnUiThread { runSelfTest() }
        }
    }

    private fun openMainApp() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && !BluetoothAccess.hasRuntimePermissions(this)) {
            line("")
            line("Eerst Bluetooth-rechten toestaan.")
            reportView.text = report.toString()
            requestBluetoothPermissions()
            return
        }
        try {
            startActivity(Intent(this, MainActivity::class.java))
        } catch (t: Throwable) {
            CrashReporter.save(this, t)
            line("Hoofdapp starten mislukt: ${t.message}")
            reportView.text = report.toString()
        }
    }

    private fun line(text: String) {
        report.append(text).append('\n')
    }
}
