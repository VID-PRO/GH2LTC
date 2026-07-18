package com.tcwl.timecode

data class Timecode(
    val dd: Int = 0,
    val hh: Int = 0,
    val mm: Int = 0,
    val ss: Int = 0,
    val ff: Int = 0,
) {
    val display: String
        get() = "%02d:%02d:%02d.%02d".format(hh, mm, ss, ff)

    val displayWithDays: String
        get() = "%02d %02d:%02d:%02d.%02d".format(dd, hh, mm, ss, ff)

    companion object {
        fun fromBytes(data: ByteArray): Timecode {
            if (data.size < 5) return Timecode()
            return Timecode(
                dd = data[0].toInt() and 0xFF,
                hh = data[1].toInt() and 0xFF,
                mm = data[2].toInt() and 0xFF,
                ss = data[3].toInt() and 0xFF,
                ff = data[4].toInt() and 0xFF,
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
