package com.example.breathez   // ← You will change "com.example.breathez" to your own package name

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

class MainActivity : AppCompatActivity() {

    // ─────────────────────────────────────────────────────
    //  Bluetooth SPP (Serial Port Profile) UUID
    //  This is a standard fixed UUID — do NOT change it
    // ─────────────────────────────────────────────────────
    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    // Bluetooth objects
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var inputStream: InputStream? = null
    private var isConnected = false

    // Handler lets background threads safely update the UI
    private val handler = Handler(Looper.getMainLooper())

    // ─────────────────────────────────────────────────────
    //  UI References — these match the IDs in activity_main.xml
    // ─────────────────────────────────────────────────────
    private lateinit var btnConnect: Button
    private lateinit var btnAuto: Button
    private lateinit var btnManual: Button
    private lateinit var btnReset: Button
    private lateinit var btnSetSpeed: Button
    private lateinit var numberPicker: NumberPicker
    private lateinit var tvConnectionStatus: TextView
    private lateinit var tvMode: TextView

    private lateinit var tvBPM: TextView

    private lateinit var tvAvgBreath: TextView

    private lateinit var tvMaxBreath: TextView

    private lateinit var tvMinBreath: TextView
    private lateinit var tvPressure: TextView
    private lateinit var tvSpeedLVL: TextView
    private lateinit var tvPWM: TextView
    private lateinit var tvHumid: TextView
    private lateinit var layoutManualControl: LinearLayout
    private lateinit var tvLog: TextView
    private lateinit var scrollLog: ScrollView


    // ══════════════════════════════════════════════════════
    //  onCreate — called when app starts
    // ══════════════════════════════════════════════════════
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()

        linkViews()
        setupNumberPicker()
        setupButtons()
        setControlsEnabled(false)   // disable until connected
        layoutManualControl.visibility = View.GONE   // hide manual panel initially
    }


    // ──────────────────────────────────────────────────────
    //  Link every UI element by their XML id
    // ──────────────────────────────────────────────────────
    private fun linkViews() {
        btnConnect          = findViewById(R.id.btnConnect)
        btnAuto             = findViewById(R.id.btnAuto)
        btnManual           = findViewById(R.id.btnManual)
        btnReset            = findViewById(R.id.btnReset)
        btnSetSpeed         = findViewById(R.id.btnSetSpeed)
        numberPicker        = findViewById(R.id.numberPicker)
        tvConnectionStatus  = findViewById(R.id.tvConnectionStatus)
        tvMode              = findViewById(R.id.tvMode)
        tvBPM               = findViewById(R.id.tvBPM)
        tvAvgBreath         = findViewById(R.id.tvAvgBreath)
        tvMaxBreath         = findViewById(R.id.tvMaxBreath)
        tvMinBreath         = findViewById(R.id.tvMinBreath)
        tvPressure          = findViewById(R.id.tvPressure)
        tvSpeedLVL          = findViewById(R.id.tvSpeedLVL)
        tvPWM               = findViewById(R.id.tvPWM)
        tvHumid             = findViewById(R.id.tvHumid)
        layoutManualControl = findViewById(R.id.layoutManualControl)
        tvLog               = findViewById(R.id.tvLog)
        scrollLog           = findViewById(R.id.scrollLog)
    }


    // ──────────────────────────────────────────────────────
    //  Number Picker — speed selector 0 to 10
    // ──────────────────────────────────────────────────────
    private fun setupNumberPicker() {
        numberPicker.minValue = 0
        numberPicker.maxValue = 10
        numberPicker.value    = 5          // default to mid
        numberPicker.wrapSelectorWheel = false
    }


    // ──────────────────────────────────────────────────────
    //  Button Listeners
    // ──────────────────────────────────────────────────────
    private fun setupButtons() {

        // Connect / Disconnect toggle
        btnConnect.setOnClickListener {
            if (isConnected) disconnect()
            else connectToDevice()
        }

        // AUTO mode
        btnAuto.setOnClickListener {
            sendCommand("A")
            layoutManualControl.visibility = View.GONE
        }

        // MANUAL mode
        btnManual.setOnClickListener {
            sendCommand("M")
            layoutManualControl.visibility = View.VISIBLE
        }

        // Reset — goes back to mode selection screen
        btnReset.setOnClickListener {
            sendCommand("R")
            layoutManualControl.visibility = View.GONE
        }

        // Set Speed — sends the current NumberPicker value
        btnSetSpeed.setOnClickListener {
            val speed = numberPicker.value
            sendCommand(speed.toString())
        }
    }


    // ══════════════════════════════════════════════════════
    //  CONNECT — finds "BreathEZ" in paired devices and connects
    // ══════════════════════════════════════════════════════
    private fun connectToDevice() {

        // Android 12+ needs BLUETOOTH_CONNECT permission at runtime
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(
                        Manifest.permission.BLUETOOTH_CONNECT,
                        Manifest.permission.BLUETOOTH_SCAN
                    ),
                    REQUEST_BLUETOOTH_PERMISSION
                )
                return
            }
        }

        // Search for "BreathEZ" in already-paired devices
        val pairedDevices: Set<BluetoothDevice>? = bluetoothAdapter?.bondedDevices
        var targetDevice: BluetoothDevice? = null

        pairedDevices?.forEach { device ->
            if (device.name == "BreathEZ") {
                targetDevice = device
            }
        }

        if (targetDevice == null) {
            setStatus("'BreathEZ' not found. Pair it in phone Bluetooth settings first.", false)
            appendLog(">> Tip: Go to Phone Settings → Bluetooth → Pair new device → select BreathEZ")
            return
        }

        setStatus("Connecting...", false)
        appendLog(">> Trying to connect to BreathEZ...")

        // Connection must happen on a background thread — never on the main thread
        Thread {
            try {
                val socket = targetDevice!!.createRfcommSocketToServiceRecord(SPP_UUID)
                bluetoothAdapter?.cancelDiscovery()   // stop scanning to save battery during connect
                socket.connect()                      // blocks until connected or throws

                // Store references
                bluetoothSocket = socket
                outputStream    = socket.outputStream
                inputStream     = socket.inputStream
                isConnected     = true

                // Back on main thread — update UI
                handler.post {
                    setStatus("Connected to BreathEZ ✓", true)
                    btnConnect.text = "Disconnect"
                    setControlsEnabled(true)
                    appendLog(">> Connected!")
                }

                startReadingLoop()   // start listening for incoming data

            } catch (e: IOException) {
                handler.post {
                    setStatus("Connection failed.", false)
                    appendLog(">> Error: ${e.message}")
                }
            }
        }.start()
    }


    // ══════════════════════════════════════════════════════
    //  READ LOOP — runs in background, receives ESP data line by line
    // ══════════════════════════════════════════════════════
    private fun startReadingLoop() {
        Thread {
            val buffer        = ByteArray(1024)
            val lineBuilder   = StringBuilder()

            while (isConnected) {
                try {
                    val bytesRead = inputStream!!.read(buffer)
                    val chunk     = String(buffer, 0, bytesRead)
                    lineBuilder.append(chunk)

                    // Extract complete lines (ESP sends \n at end of each line)
                    while (lineBuilder.contains('\n')) {
                        val idx  = lineBuilder.indexOf('\n')
                        val line = lineBuilder.substring(0, idx).trim()
                        lineBuilder.delete(0, idx + 1)

                        if (line.isNotEmpty()) {
                            handler.post { processIncomingLine(line) }
                        }
                    }

                } catch (e: IOException) {
                    if (isConnected) {
                        handler.post {
                            setStatus("Connection lost.", false)
                            appendLog(">> Connection dropped.")
                            disconnect()
                        }
                    }
                    break
                }
            }
        }.start()
    }


    // ══════════════════════════════════════════════════════
    //  PROCESS LINE — parse telemetry and update display cards
    //
    //  ESP sends lines like:
    //  Mode: AUTO, Paper Sensor: 1523, Temp: 25.3C, Pressure: 1013.2hPa,
    //  Speed LVL: 5/10, PWM: 1537, Humid Status: OFF
    // ══════════════════════════════════════════════════════
    private fun processIncomingLine(line: String) {

        // Show raw line in log
        appendLog(line)

        // Only parse telemetry lines that start with "State:"
        if (!line.startsWith("State:")) return

        try {
            // Split on the pipe separator the ESP uses
            val parts = line.split("|")

            for (part in parts) {
                val t = part.trim()

                when {
                    // State:MANUAL   or   State:COLLECT  etc.
                    t.startsWith("State:") -> {
                        tvMode.text = "Mode: " + t.substringAfter("State:").trim()
                    }

                    // BPM:  12.3
                    t.startsWith("BPM:") -> {
                        val bpm = t.substringAfter("BPM:").trim()
                        tvBPM.text = "BPM: $bpm"
                    }

                    // Avg: 4.88s Min: 3.20s Max: 6.10s  (all in one segment)
                    t.startsWith("Avg:") -> {
                        // Extract avg, min, max from the same pipe segment
                        val avgMatch = Regex("Avg:\\s*([\\d.]+)s").find(t)
                        val minMatch = Regex("Min:\\s*([\\d.]+)s").find(t)
                        val maxMatch = Regex("Max:\\s*([\\d.]+)s").find(t)

                        tvAvgBreath.text = "Avg Breath Interval: " +
                                (avgMatch?.groupValues?.get(1) ?: "—") + " s"
                        tvMinBreath.text = "Min Breath Interval: " +
                                (minMatch?.groupValues?.get(1) ?: "—") + " s"
                        tvMaxBreath.text = "Max Breath Interval: " +
                                (maxMatch?.groupValues?.get(1) ?: "—") + " s"
                    }

                    // Press:  8.25 cmH2O
                    t.startsWith("Press:") -> {
                        val press = t.substringAfter("Press:").trim()
                        tvPressure.text = "Pressure: $press"
                    }

                    // Fan: 5/10 PWM:1537
                    t.startsWith("Fan:") -> {
                        val fanMatch = Regex("Fan:\\s*([\\d]+/10)").find(t)
                        val pwmMatch = Regex("PWM:\\s*(\\d+)").find(t)

                        tvSpeedLVL.text = "Speed Level: " +
                                (fanMatch?.groupValues?.get(1) ?: "—")
                        tvPWM.text = "PWM: " +
                                (pwmMatch?.groupValues?.get(1) ?: "—") + " µs"
                    }

                    // Humid:OFF  or  Humid:ON
                    t.startsWith("Humid:") -> {
                        val humid = t.substringAfter("Humid:").trim()
                        tvHumid.text = "Humidifier: $humid"
                        tvHumid.setTextColor(
                            if (humid == "ON") Color.parseColor("#4CAF50")
                            else Color.parseColor("#AAAAAA")
                        )
                    }
                }
            }

        } catch (e: Exception) {
            // Parsing failed — line is already visible in log
        }
    }


    // ══════════════════════════════════════════════════════
    //  SEND COMMAND — writes a string to ESP over Bluetooth
    // ══════════════════════════════════════════════════════
    private fun sendCommand(command: String) {
        if (!isConnected) {
            appendLog(">> Cannot send — not connected.")
            return
        }
        try {
            outputStream?.write((command + "\n").toByteArray())
            appendLog(">> Sent: $command")
        } catch (e: IOException) {
            appendLog(">> Send failed: ${e.message}")
        }
    }


    // ══════════════════════════════════════════════════════
    //  DISCONNECT — cleanly closes all streams and socket
    // ══════════════════════════════════════════════════════
    private fun disconnect() {
        isConnected = false
        try {
            inputStream?.close()
            outputStream?.close()
            bluetoothSocket?.close()
        } catch (e: IOException) { /* ignore close errors */ }

        bluetoothSocket = null
        outputStream    = null
        inputStream     = null

        setStatus("Disconnected", false)
        btnConnect.text = "Connect to BreathEZ"
        setControlsEnabled(false)
        layoutManualControl.visibility = View.GONE
        appendLog(">> Disconnected.")
    }


    // ──────────────────────────────────────────────────────
    //  Helper — enable/disable control buttons
    // ──────────────────────────────────────────────────────
    private fun setControlsEnabled(enabled: Boolean) {
        btnAuto.isEnabled      = enabled
        btnManual.isEnabled    = enabled
        btnReset.isEnabled     = enabled
        btnSetSpeed.isEnabled  = enabled
        numberPicker.isEnabled = enabled
    }

    // ──────────────────────────────────────────────────────
    //  Helper — update status bar text and color
    // ──────────────────────────────────────────────────────
    private fun setStatus(message: String, connected: Boolean) {
        tvConnectionStatus.text = message
        tvConnectionStatus.setBackgroundColor(
            if (connected) Color.parseColor("#388E3C")   // green
            else           Color.parseColor("#C62828")   // red
        )
    }

    // ──────────────────────────────────────────────────────
    //  Helper — append a line to the log box, keep last 30 lines
    // ──────────────────────────────────────────────────────
    private fun appendLog(message: String) {
        val existing = tvLog.text.toString()
        val lines    = existing.split("\n").takeLast(30)
        tvLog.text   = (lines + message).joinToString("\n")
        // Auto-scroll log to bottom
        scrollLog.post { scrollLog.fullScroll(View.FOCUS_DOWN) }
    }


    // ──────────────────────────────────────────────────────
    //  Permission result callback (Android 12+)
    // ──────────────────────────────────────────────────────
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_BLUETOOTH_PERMISSION) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                connectToDevice()   // retry after permission granted
            } else {
                appendLog(">> Bluetooth permission denied. Cannot connect.")
            }
        }
    }

    // Close everything when app is closed
    override fun onDestroy() {
        super.onDestroy()
        disconnect()
    }

    companion object {
        private const val REQUEST_BLUETOOTH_PERMISSION = 1
    }
}