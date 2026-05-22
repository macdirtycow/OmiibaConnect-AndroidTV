package dev.omiiba.connect.tv

import android.app.Application

class OmiibaTvApplication : Application() {
    val repository: HeadphonesRepository by lazy { HeadphonesRepository(this) }

    override fun onCreate() {
        super.onCreate()
        try {
            CrashReporter.install(this)
        } catch (_: Throwable) {
            // Never block app launch if crash logging fails.
        }
    }
}
