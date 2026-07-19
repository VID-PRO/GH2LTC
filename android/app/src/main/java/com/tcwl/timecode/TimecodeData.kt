package com.tcwl.timecode

data class Timecode(
    val dd: Int = 0,
    val hh: Int = 0,
    val mm: Int = 0,
    val ss: Int = 0,
    val ff: Int = 0,
    val lockState: Int = 0,
    val fps: Int = 0,
    val autoFps: Boolean = false,
    val isMaster: Boolean = false,
    val batteryPct: Int = 0,
    val flags: Int = 0,
) {
    val display: String
        get() = "%02d:%02d:%02d.%02d".format(hh, mm, ss, ff)

    val displayWithDays: String
        get() = "%02d %02d:%02d:%02d.%02d".format(dd, hh, mm, ss, ff)

    val lockChar: Char
        get() = when (lockState) {
            1 -> 'H'
            2 -> 'R'
            3 -> 'B'
            else -> 'F'
        }

    val runtimeText: String
        get() {
            if (batteryPct > 100) return "--"
            val remMin = batteryPct * 600 / 100
            val hrs = remMin / 60
            return if (hrs >= 1) "${hrs}h" else "${remMin}m"
        }

    val ltcModeText: String
        get() = when ((flags shr 2) and 0x03) {
            1 -> "I"
            2 -> "B"
            else -> "O"
        }

    companion object {
        fun fromBytes(data: ByteArray): Timecode {
            if (data.size < 5) return Timecode()
            val flagsVal = if (data.size >= 8) data[7].toInt() and 0xFF else 0
            return Timecode(
                dd = data[0].toInt() and 0xFF,
                hh = data[1].toInt() and 0xFF,
                mm = data[2].toInt() and 0xFF,
                ss = data[3].toInt() and 0xFF,
                ff = data[4].toInt() and 0xFF,
                lockState = if (data.size >= 6) data[5].toInt() and 0xFF else 0,
                fps = if (data.size >= 7) data[6].toInt() and 0xFF else 0,
                autoFps = (flagsVal and 0x01) != 0,
                isMaster = (flagsVal and 0x02) != 0,
                batteryPct = if (data.size >= 9) data[8].toInt() and 0xFF else 0,
                flags = flagsVal,
            )
        }
    }
}

enum class ConnectionState {
    DISCONNECTED, SCANNING, CONNECTING, CONNECTED
}

data class BleDevice(
    val address: String,
    val name: String,
)
