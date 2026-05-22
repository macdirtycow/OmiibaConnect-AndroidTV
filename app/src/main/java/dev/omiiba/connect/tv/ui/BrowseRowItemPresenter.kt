package dev.omiiba.connect.tv.ui

import android.graphics.Typeface
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.leanback.widget.Presenter
import dev.omiiba.connect.tv.R

/** Leanback row cell — every list row item must use a Presenter (raw String crashes). */
data class BrowseRowItem(
    val label: String,
    val action: Any? = null,
    /** Larger text for short error lines on TV. */
    val prominent: Boolean = false,
)

class BrowseRowItemPresenter : Presenter() {
    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        val textView = TextView(parent.context).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            )
            val pad = parent.resources.getDimensionPixelSize(R.dimen.tv_row_padding)
            setPadding(pad, pad / 2, pad, pad / 2)
            textSize = 20f
            typeface = Typeface.SANS_SERIF
            isFocusable = true
            isFocusableInTouchMode = true
            setTextColor(ContextCompat.getColor(context, R.color.tv_row_text))
        }
        return ViewHolder(textView)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any?) {
        val row = item as BrowseRowItem
        val tv = viewHolder.view as TextView
        tv.text = row.label
        tv.textSize = if (row.prominent) 28f else 20f
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        (viewHolder.view as TextView).text = null
    }
}
