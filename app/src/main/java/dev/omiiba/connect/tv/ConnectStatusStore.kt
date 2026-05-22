package dev.omiiba.connect.tv

import android.content.Context

/** Last connect attempt message for on-TV diagnostics (no logcat needed). */
object ConnectStatusStore {
    private const val PREFS = "omiiba_connect_status"
    private const val KEY_LAST = "last_connect"

    fun save(context: Context, message: String) {
        context.applicationContext
            .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY_LAST, message)
            .apply()
    }

    fun read(context: Context): String {
        return context.applicationContext
            .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getString(KEY_LAST, "")
            .orEmpty()
    }

    fun clear(context: Context) {
        context.applicationContext
            .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .remove(KEY_LAST)
            .apply()
    }
}
