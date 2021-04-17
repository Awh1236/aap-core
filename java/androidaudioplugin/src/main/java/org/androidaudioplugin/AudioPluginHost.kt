package org.androidaudioplugin

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.util.Log
import java.lang.UnsupportedOperationException

// These classes are not to endorse any official API for hosting AAP.
// Its implementation is hacky and not really with decent API design.
// It is to provide usable utilities for plugin developers as a proof of concept.

class AudioPluginServiceConnector(private val applicationContext: Context) : AutoCloseable {
    val serviceConnectedListeners = mutableListOf<(conn: PluginServiceConnection) -> Unit>()
    val connectedServices = mutableListOf<PluginServiceConnection>()

    fun bindAudioPluginService(service: AudioPluginServiceInformation, sampleRate: Int) {
        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(
            service.packageName,
            service.className
        )
        intent.putExtra("sampleRate", sampleRate)

        val conn = PluginServiceConnection(service) { c -> onBindAudioPluginService(c) }

        Log.d("AudioPluginHost", "bindAudioPluginService: ${service.packageName} | ${service.className}")
        applicationContext.bindService(intent, conn, Context.BIND_AUTO_CREATE)
    }

    private fun onBindAudioPluginService(conn: PluginServiceConnection) {
        AudioPluginNatives.addBinderForHost(conn.serviceInfo.packageName, conn.serviceInfo.className, conn.binder!!)
        connectedServices.add(conn)

        // avoid conflicting concurrent updates
        val currentListeners = serviceConnectedListeners.toTypedArray()
        currentListeners.forEach { f -> f(conn) }
    }

    fun findExistingServiceConnection(packageName: String, className: String) =
        connectedServices.firstOrNull { conn -> conn.serviceInfo.packageName == packageName && conn.serviceInfo.className == className }

    fun unbindAudioPluginService(packageName: String, localName: String) {
        val conn = findExistingServiceConnection(packageName, localName) ?: return
        connectedServices.remove(conn)
        AudioPluginNatives.removeBinderForHost(conn.serviceInfo.packageName, conn.serviceInfo.className)
    }

    override fun close() {
        while (connectedServices.any()) {
            val list = connectedServices.toTypedArray()
            for (conn in list)
                AudioPluginNatives.removeBinderForHost(conn.serviceInfo.packageName, conn.serviceInfo.className)
        }
    }
}

class AudioPluginHost(private var applicationContext: Context) {

    // Service connection
    val serviceConnector = AudioPluginServiceConnector(applicationContext)

    val pluginInstantiatedListeners = mutableListOf<(conn: AudioPluginInstance) -> Unit>()

    val instantiatedPlugins = mutableListOf<AudioPluginInstance>()

    val extensions = mutableListOf<AudioPluginExtensionData>()

    fun dispose() {
    }

    // Plugin instancing

    fun instantiatePlugin(pluginInfo: PluginInformation)
    {
        val conn = serviceConnector.findExistingServiceConnection(pluginInfo.packageName, pluginInfo.localName)
        if (conn == null) {
            var serviceConnectedListener: (PluginServiceConnection) -> Unit ={}
            serviceConnectedListener = { c ->
                serviceConnector.serviceConnectedListeners.remove(serviceConnectedListener)
                instantiatePlugin(pluginInfo, c)
            }
            serviceConnector.serviceConnectedListeners.add(serviceConnectedListener)
            val service = AudioPluginHostHelper.queryAudioPluginServices(applicationContext).first { c -> c.plugins.any { p -> p.pluginId == pluginInfo.pluginId }}
            serviceConnector.bindAudioPluginService(service, sampleRate)
        }
        else
            instantiatePlugin(pluginInfo, conn)
    }

    private fun instantiatePlugin(pluginInfo: PluginInformation, conn: PluginServiceConnection)
    {
        val instance = conn.instantiatePlugin(pluginInfo, sampleRate, extensions)
        instantiatedPlugins.add(instance)
        pluginInstantiatedListeners.forEach { l -> l (instance) }
    }

    // Audio buses and buffers management

    var audioInputs = mutableListOf<ByteArray>()
    var controlInputs = mutableListOf<ByteArray>()
    var audioOutputs = mutableListOf<ByteArray>()

    private var isConfigured = false
    private var isActive = false

    var sampleRate = 44100

    var audioBufferSizeInBytes = 4096 * 4
        set(value) {
            field = value
            resetInputBuffer()
            resetOutputBuffer()
        }
    var controlBufferSizeInBytes = 4096 * 4
        set(value) {
            field = value
            resetControlBuffer()
        }

    private fun throwIfRunning() { if (isActive) throw UnsupportedOperationException("AudioPluginHost is already running") }

    var inputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetInputBuffer()
        }
    private fun resetInputBuffer() = expandBufferArrays(audioInputs, inputAudioBus.map.size, audioBufferSizeInBytes)

    var inputControlBus = AudioBusPresets.mono
        set(value) {
            throwIfRunning()
            field = value
            resetControlBuffer()
        }
    private fun resetControlBuffer() = expandBufferArrays(controlInputs, inputControlBus.map.size, controlBufferSizeInBytes)

    var outputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetOutputBuffer()
        }
    private fun resetOutputBuffer() = expandBufferArrays(audioOutputs, outputAudioBus.map.size, audioBufferSizeInBytes)

    private fun expandBufferArrays(list : MutableList<ByteArray>, newSize : Int, bufferSize : Int) {
        if (newSize > list.size)
            (0 until newSize - list.size).forEach {list.add(ByteArray(bufferSize)) }
        while (newSize < list.size)
            list.remove(list.last())
        for (i in 0 until newSize)
            if (list[i].size < bufferSize)
                list[i] = ByteArray(bufferSize)
    }

    init {
        AudioPluginNatives.setApplicationContext(applicationContext)
        AudioPluginNatives.initialize(AudioPluginHostHelper.queryAudioPluginServices(applicationContext).flatMap { s -> s.plugins }.toTypedArray())
        inputAudioBus = AudioBusPresets.stereo
        outputAudioBus = AudioBusPresets.stereo
    }
}

// FIXME: We are still unsure if they will be kept alive or not.
class AudioBus(var name : String, var map : Map<String,Int>)

class AudioBusPresets
{
    companion object {
        val mono = AudioBus("Mono", mapOf(Pair("center", 0)))
        val stereo = AudioBus("Stereo", mapOf(Pair("Left", 0), Pair("Right", 1)))
        val surround50 = AudioBus(
            "5.0 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("RearLeft", 3), Pair("RearRight", 4)
            )
        )
        val surround51 = AudioBus(
            "5.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("RearLeft", 3), Pair("RearRight", 4), Pair("LowFrequencyEffect", 5)
            )
        )
        val surround61 = AudioBus(
            "6.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("SideLeft", 3), Pair("SideRight", 4),
                Pair("RearCenter", 5), Pair("LowFrequencyEffect", 6)
            )
        )
        val surround71 = AudioBus(
            "7.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("SideLeft", 3), Pair("SideRight", 4),
                Pair("RearLeft", 5), Pair("RearRight", 6), Pair("LowFrequencyEffect", 7)
            )
        )
    }
}

