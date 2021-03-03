package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Button
import androidx.compose.material.Slider
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer


@Composable
fun AvailablePlugins(onItemClick: (PluginInformation) -> Unit = {}, pluginServices: List<AudioPluginServiceInformation>) {
    val small = TextStyle(fontSize = 12.sp)

    LazyColumn {
        items(pluginServices.flatMap { s -> s.plugins }) { plugin ->
            Row(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp)
                    .then(Modifier.clickable { onItemClick(plugin) })
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(plugin.displayName)
                    Text(plugin.packageName, style = small)
                }
            }
        }
    }
}

val headerModifier = Modifier.width(120.dp)

@Composable
fun Header(text: String) {
    Text(modifier = headerModifier, fontWeight = FontWeight.Bold, text = text)
}

@ExperimentalUnsignedTypes
@Composable
fun PluginDetails(plugin: PluginInformation, state: PluginListViewModel.State) {
    val scrollState = rememberScrollState(0)

    var parameters by remember { mutableStateOf(plugin.ports.map { p -> p.default }.toFloatArray()) }
    var pluginAppliedState by remember { mutableStateOf(false) }
    var waveViewSource = state.preview.inBuf
    var waveState by remember { mutableStateOf(waveViewSource) }

    Column(modifier = Modifier.padding(8.dp).verticalScroll(scrollState)) {
        Row {
            Text(text = plugin.displayName, fontSize = 20.sp)
        }
        Row {
            Header("package: ")
        }
        Row {
            Text(plugin.packageName, fontSize = 14.sp)
        }
        Row {
            Header("classname: ")
        }
        Row {
            Text(plugin.localName, fontSize = 14.sp)
        }
        if (plugin.author != null) {
            Row {
                Header("author: ")
            }
            Row {
                Text(plugin.author ?: "")
            }
        }
        if (plugin.backend != null) {
            Row {
                Header("backend: ")
            }
            Row {
                Text(plugin.backend ?: "")
            }
        }
        if (plugin.manufacturer != null) {
            Row {
                Header("manfufacturer: ")
            }
            Row {
                Text(plugin.manufacturer ?: "")
            }
        }
        Row {
            Column {
            }
        }
        WaveformDrawable(waveData = waveState)
        Row {
            Button(onClick = {
                if (!pluginAppliedState) {
                    state.preview.processAudioCompleted = {
                        waveState = state.preview.outBuf
                        pluginAppliedState = true
                        state.preview.unbindHost()
                    }
                    state.preview.applyPlugin(state.availablePluginServices.first(), plugin, parameters)
                } else {
                    waveState = state.preview.inBuf
                    pluginAppliedState = false
                }
            }) { Text(if (pluginAppliedState) "On" else "Off") }
            Button(onClick = {}) { Text("UI") }
            Button(onClick = {
                state.preview.playSound(pluginAppliedState)
                }) {
                Text("Play")
            }
        }
        Text(text = "Ports", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            for (port in plugin.ports) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Column {
                        Text(
                            fontSize = 14.sp,
                            text = when (port.content) {
                                PortInformation.PORT_CONTENT_TYPE_AUDIO -> "Audio"
                                PortInformation.PORT_CONTENT_TYPE_MIDI -> "MIDI"
                                else -> "-"
                            },
                            modifier = Modifier.width(50.dp)
                        )
                        Text(
                            fontSize = 14.sp,
                            text = when (port.direction) {
                                PortInformation.PORT_DIRECTION_INPUT -> "In"
                                else -> "Out"
                            },
                            modifier = Modifier.width(30.dp)
                        )
                    }
                    Header(port.name)
                    var sliderPosition by remember { mutableStateOf(port.default) }
                    Text(
                        fontSize = 10.sp,
                        text = sliderPosition.toString(),
                        modifier = Modifier.width(40.dp).align(Alignment.CenterVertically)
                    )
                    if (port.content != PortInformation.PORT_CONTENT_TYPE_AUDIO && port.content != PortInformation.PORT_CONTENT_TYPE_MIDI)
                        Slider(
                            value = sliderPosition,
                            valueRange = if (port.minimum < port.maximum) port.minimum .. port.maximum else Float.MIN_VALUE..Float.MAX_VALUE,
                            steps = 10,
                            onValueChange = {
                                parameters[plugin.ports.indexOf(port)] = it
                                sliderPosition = it
                            })
                }
            }
        }
    }
}

@Composable
fun WaveformDrawable(waveData: ByteArray,
                     canvasModifier : Modifier = Modifier.fillMaxWidth().height(64.dp).border(width = 1.dp, color = Color.Gray)) {
    val floatBuffer = ByteBuffer.wrap(waveData).asFloatBuffer()

    // Is there any way to get max() from FloatBuffer?
    val fa = FloatArray(waveData.size / 4)
    floatBuffer.get(fa)
    val max = fa.maxOrNull() ?: 0f

    Canvas(modifier = canvasModifier, onDraw = {
        val width = this.size.width.toInt()
        val height = this.size.height
        val delta = (waveData.size / 4 / width).and(Int.MAX_VALUE - 1) // - mod 2
        for (wp in 0..width / 2) {
            var i = wp * 2
            val frL = floatBuffer[delta * i] / max
            val hL = frL * height / 2
            drawLine(
                Color.Black,
                Offset(i.toFloat(), height / 2),
                Offset((i + 1).toFloat(), height / 2 - hL)
            )
            i = wp * 2 + 1
            val frR = floatBuffer[delta * i] / max
            val hR = frR * height / 2
            drawLine(
                Color.Black,
                Offset(i.toFloat(), height / 2),
                Offset((i + 1).toFloat(), height / 2 + hR)
            )
        }
    })
}
