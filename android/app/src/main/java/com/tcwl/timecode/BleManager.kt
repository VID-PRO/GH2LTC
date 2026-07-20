package com.tcwl.timecode

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.UUID

class BleManager(private val context: Context) {

    companion object {
        val SERVICE_UUID = UUID.fromString("9a6f0001-5c9a-4b3e-8a2c-f12345678901")
        val TC_CHAR_UUID = UUID.fromString("9a6f0002-5c9a-4b3e-8a2c-f12345678901")
        val NAME_CHAR_UUID = UUID.fromString("9a6f0003-5c9a-4b3e-8a2c-f12345678901")
        val CONFIG_CHAR_UUID = UUID.fromString("9a6f0004-5c9a-4b3e-8a2c-f12345678901")
        val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val bluetoothManager: BluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _timecode = MutableStateFlow(Timecode())
    val timecode: StateFlow<Timecode> = _timecode

    private val _scannedDevices = MutableStateFlow<List<BleDevice>>(emptyList())
    val scannedDevices: StateFlow<List<BleDevice>> = _scannedDevices

    private val _deviceName = MutableStateFlow("TC-WL-HDMI")
    val deviceName: StateFlow<String> = _deviceName

    private val _deviceState = MutableStateFlow<Map<String, String>>(emptyMap())
    val deviceState: StateFlow<Map<String, String>> = _deviceState

    val timecodeChannel = Channel<Timecode>(Channel.BUFFERED)

    private var bluetoothGatt: BluetoothGatt? = null
    private var tcCharacteristic: BluetoothGattCharacteristic? = null
    private var configCharacteristic: BluetoothGattCharacteristic? = null

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            if (device.name != null) {
                val existing = _scannedDevices.value.toMutableList()
                if (existing.none { it.address == device.address }) {
                    existing.add(BleDevice(device.address, device.name ?: "Unknown"))
                    _scannedDevices.value = existing
                }
            }
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.CONNECTING
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    _connectionState.value = ConnectionState.DISCONNECTED
                    bluetoothGatt = null
                    tcCharacteristic = null
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val service = gatt.getService(SERVICE_UUID) ?: run {
                gatt.disconnect()
                return
            }
            tcCharacteristic = service.getCharacteristic(TC_CHAR_UUID)
            val nameChar = service.getCharacteristic(NAME_CHAR_UUID)
            configCharacteristic = service.getCharacteristic(CONFIG_CHAR_UUID)

            if (tcCharacteristic != null) {
                val cccd = tcCharacteristic!!.getDescriptor(CCCD_UUID)
                gatt.setCharacteristicNotification(tcCharacteristic!!, true)
                cccd?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(cccd)
            }

            nameChar?.let {
                it.value = "TC-WL-Android".toByteArray()
                it.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                gatt.writeCharacteristic(it)
            }

            // Read device state (wifi, ssid, etc.)
            configCharacteristic?.let {
                gatt.readCharacteristic(it)
            }
            _connectionState.value = ConnectionState.CONNECTED
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            if (characteristic.uuid == TC_CHAR_UUID) {
                val tc = Timecode.fromBytes(value)
                _timecode.value = tc
                timecodeChannel.trySend(tc)
            }
        }

        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
            status: Int,
        ) {
            if (characteristic.uuid == CONFIG_CHAR_UUID) {
                val raw = String(value, Charsets.UTF_8).trimEnd('\u0000')
                val state = raw.split('|').mapNotNull { kv ->
                    val parts = kv.split('=', limit = 2)
                    if (parts.size == 2) parts[0] to parts[1] else null
                }.toMap()
                _deviceState.value = state
            }
        }
    }

    fun startScan() {
        if (bluetoothAdapter?.isEnabled != true) return
        _scannedDevices.value = emptyList()
        _connectionState.value = ConnectionState.SCANNING

        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(SERVICE_UUID))
            .build()

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bluetoothAdapter.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)

        android.os.Handler(context.mainLooper).postDelayed({
            stopScan()
        }, 5000)
    }

    fun stopScan() {
        bluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback)
        if (_connectionState.value == ConnectionState.SCANNING) {
            _connectionState.value = ConnectionState.DISCONNECTED
        }
    }

    fun connect(deviceAddress: String, deviceName: String = "") {
        stopScan()
        _connectionState.value = ConnectionState.CONNECTING
        if (deviceName.isNotEmpty()) _deviceName.value = deviceName
        val device = bluetoothAdapter?.getRemoteDevice(deviceAddress) ?: return
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
    }

    fun disconnect() {
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        tcCharacteristic = null
        configCharacteristic = null
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    fun readState() {
        val gatt = bluetoothGatt ?: return
        val char = configCharacteristic ?: return
        gatt.readCharacteristic(char)
    }

    fun sendConfig(cmd: String, value: String) {
        val gatt = bluetoothGatt ?: return
        val char = configCharacteristic ?: return
        char.value = "$cmd:$value".toByteArray()
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        gatt.writeCharacteristic(char)
    }

    fun cleanup() {
        disconnect()
    }
}
