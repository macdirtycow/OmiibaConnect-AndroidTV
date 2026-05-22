package dev.omiiba.connect.tv.bluetooth

/**
 * Short lines for TV UI; full detail goes to [dev.omiiba.connect.tv.ConnectStatusStore].
 */
object ConnectErrorMessages {
    fun isRfcommRefused(message: String): Boolean {
        val m = message.lowercase()
        return m.contains("socket gesloten") ||
            m.contains("read ret: -1") ||
            m.contains("socket might closed") ||
            m.contains("rfcomm geweigerd")
    }

    /** Max ~40 chars per line for Leanback status rows. */
    fun userLines(throwable: Throwable): List<String> {
        val raw = throwable.message.orEmpty()
        if (isRfcommRefused(raw)) {
            return listOf(
                "RFCOMM geweigerd",
                "TV-audio kan wél werken",
                "Sony-bediening niet",
                "XM3: uit → 5 sec → aan",
                "Telefoon: XM3 loskoppelen",
                "Deze TV: waarsch. niet",
                "mogelijk voor XM3",
            )
        }
        val short = raw.lineSequence().firstOrNull()?.take(60)?.trim().orEmpty()
        return listOf(
            "Verbinden mislukt",
            if (short.isNotEmpty()) short else "Onbekende fout",
            "Zie diagnose-scherm",
        )
    }

    fun userSummary(throwable: Throwable): String =
        userLines(throwable).joinToString("\n")

    fun detail(throwable: Throwable): String {
        val raw = throwable.message?.trim().orEmpty()
        return if (raw.isNotEmpty()) raw else throwable.javaClass.simpleName
    }
}
