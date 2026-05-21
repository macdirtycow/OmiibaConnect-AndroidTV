package dev.omiiba.connect.tv

import android.app.Application

class OmiibaTvApplication : Application() {
    val repository: HeadphonesRepository by lazy { HeadphonesRepository(this) }
}
