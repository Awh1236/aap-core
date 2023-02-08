package org.androidaudioplugin.ui.web

import android.content.Context
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import java.io.ByteArrayOutputStream
import java.io.InputStreamReader
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class WebUIHelper {
    companion object {

        fun getUIZipArchive(ctx: Context, pluginId: String) : ByteArray {
            val ms = ByteArrayOutputStream()
            val os = ZipOutputStream(ms)
            os.writer().use {w ->
                for (s in arrayOf("index.html", "webcomponents-lite.js", "webaudio-controls.js")) {
                    os.putNextEntry(ZipEntry(s))
                    ctx.assets.open("web/$s").use {
                        InputStreamReader(it).use {
                            w.write(it.readText())
                        }
                    }
                    w.flush()
                }
                for (s in arrayOf("bright_life.png")) {
                    os.putNextEntry(ZipEntry(s))
                    ctx.assets.open("web/$s").use {
                        it.copyTo(os)
                    }
                    w.flush()
                }
            }
            return ms.toByteArray()
        }
    }
}

