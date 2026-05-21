package dev.omiiba.connect.tv

import android.app.Application
import android.content.Context
import android.util.Log
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object CrashReporter {
    private const val LOG_TAG = "OmiibaCrash"
    private const val FILE_NAME = "last_crash.txt"

    fun install(app: Application) {
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            save(app, throwable)
            previous?.uncaughtException(thread, throwable)
        }
    }

    fun save(context: Context, throwable: Throwable) {
        val text = format(throwable)
        Log.e(LOG_TAG, text, throwable)
        runCatching {
            context.openFileOutput(FILE_NAME, Context.MODE_PRIVATE).use { out ->
                out.write(text.toByteArray())
            }
        }
    }

    fun readLast(context: Context): String {
        return runCatching {
            context.openFileInput(FILE_NAME).bufferedReader().use { it.readText() }
        }.getOrElse { "" }
    }

    fun format(throwable: Throwable): String {
        val time = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
        val sw = StringWriter()
        throwable.printStackTrace(PrintWriter(sw))
        return "[$time]\n${sw}"
    }
}
