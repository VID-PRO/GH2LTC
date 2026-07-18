package com.tcwl.timecode

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle

class MainActivity : ComponentActivity() {

    private lateinit var bleManager: BleManager

    private val enableBleLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { /* BLE enabled */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        bleManager = BleManager(applicationContext)

        checkPermissions()

        setContent {
            TCWLTheme {
                MainScreen(bleManager)
            }
        }
    }

    override fun onDestroy() {
        bleManager.cleanup()
        super.onDestroy()
    }

    private fun checkPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) {
                requestPermissions(
                    arrayOf(
                        Manifest.permission.BLUETOOTH_SCAN,
                        Manifest.permission.BLUETOOTH_CONNECT,
                        Manifest.permission.ACCESS_FINE_LOCATION,
                    ), 100
                )
            }
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED
            ) {
                requestPermissions(
                    arrayOf(Manifest.permission.ACCESS_FINE_LOCATION), 100
                )
            }
        }
        val btAdapter = BluetoothAdapter.getDefaultAdapter()
        if (btAdapter != null && !btAdapter.isEnabled) {
            enableBleLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
        }
    }
}

@Composable
fun TCWLTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Color(0xFF00BCD4),
            onPrimary = Color.Black,
            background = Color(0xFF121212),
            surface = Color(0xFF1E1E1E),
            onBackground = Color.White,
            onSurface = Color.White,
        ),
        content = content
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(bleManager: BleManager) {
    val timecode by bleManager.timecode.collectAsStateWithLifecycle()
    val connectionState by bleManager.connectionState.collectAsStateWithLifecycle()
    val scannedDevices by bleManager.scannedDevices.collectAsStateWithLifecycle()

    var drawerOpen by remember { mutableStateOf(false) }
    var showJamDialog by remember { mutableStateOf(false) }
    var statusMsg by remember { mutableStateOf<String?>(null) }

    val snackbarHostState = remember { SnackbarHostState() }

    LaunchedEffect(statusMsg) {
        statusMsg?.let {
            snackbarHostState.showSnackbar(it)
            statusMsg = null
        }
    }

    ModalNavigationDrawer(
        drawerState = rememberDrawerState(DrawerValue.Closed).also {
            LaunchedEffect(drawerOpen) { if (drawerOpen) it.open() else it.close() }
        },
        gesturesEnabled = false,
        drawerContent = {
            ModalDrawerSheet {
                ConfigDrawer(
                    bleManager = bleManager,
                    scannedDevices = scannedDevices,
                    connectionState = connectionState,
                    onClose = { drawerOpen = false },
                    onStatus = { statusMsg = it },
                    onShowJam = { showJamDialog = true },
                    currentTimecode = timecode,
                )
            }
        }
    ) {
        Scaffold(
            snackbarHost = { SnackbarHost(snackbarHostState) },
            topBar = {
                TopAppBar(
                    title = { Text(bleManager.deviceName.collectAsStateWithLifecycle().value) },
                    actions = {
                        Text(
                            text = when (connectionState) {
                                ConnectionState.DISCONNECTED -> "Disconnected"
                                ConnectionState.SCANNING -> "Scanning..."
                                ConnectionState.CONNECTING -> "Connecting..."
                                ConnectionState.CONNECTED -> "Connected"
                            },
                            color = when (connectionState) {
                                ConnectionState.CONNECTED -> Color(0xFF4CAF50)
                                else -> Color.Gray
                            },
                            style = MaterialTheme.typography.bodySmall,
                            modifier = Modifier.padding(end = 8.dp)
                        )
                        IconButton(onClick = { drawerOpen = !drawerOpen }) {
                            Text("☰", color = Color.White)
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = MaterialTheme.colorScheme.surface
                    )
                )
            },
            containerColor = MaterialTheme.colorScheme.background
        ) { padding ->
            TimecodeDisplay(
                timecode = timecode,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(padding)
            )
        }
    }

    if (showJamDialog) {
        JamDialog(
            current = timecode,
            onDismiss = { showJamDialog = false },
            onJam = { dd, hh, mm, ss, ff ->
                bleManager.sendConfig("jam", "$dd $hh $mm $ss $ff")
                statusMsg = "Timecode jammed"
                showJamDialog = false
            }
        )
    }
}

@Composable
fun TimecodeDisplay(timecode: Timecode, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .background(Color(0xFF121212))
            .padding(16.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            if (timecode.dd > 0) {
                Text(
                    text = "%02d".format(timecode.dd),
                    color = Color(0xFF888888),
                    fontSize = 24.sp,
                    fontFamily = FontFamily.Monospace,
                )
                Spacer(Modifier.height(8.dp))
            }
            Text(
                text = timecode.display,
                color = Color(0xFF00FF88),
                fontSize = 72.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfigDrawer(
    bleManager: BleManager,
    scannedDevices: List<BleDevice>,
    connectionState: ConnectionState,
    onClose: () -> Unit,
    onStatus: (String) -> Unit,
    onShowJam: () -> Unit,
    currentTimecode: Timecode,
) {
    val connectedName by bleManager.deviceName.collectAsStateWithLifecycle()
    val isClap = connectedName.contains("CLAP", ignoreCase = true)
    val isLtc = connectedName.contains("LTC", ignoreCase = true)
    var selectedFps by remember { mutableStateOf(25) }
    var dropFrame by remember { mutableStateOf(false) }
    var brightness by remember { mutableStateOf(7) }
    var deviceName by remember { mutableStateOf(connectedName) }
    LaunchedEffect(connectedName) { deviceName = connectedName }

    Column(
        modifier = Modifier
            .width(320.dp)
            .padding(16.dp)
            .verticalScroll(rememberScrollState())
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(bottom = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Configuration", style = MaterialTheme.typography.headlineSmall)
            Text(
                "✕",
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface,
                modifier = Modifier.clickable { onClose() }
            )
        }

        // BLE section
        Text("BLE Connection", style = MaterialTheme.typography.titleSmall, color = Color.Gray)
        Spacer(Modifier.height(8.dp))

        if (connectionState == ConnectionState.DISCONNECTED || connectionState == ConnectionState.SCANNING) {
            Button(
                onClick = { bleManager.startScan() },
                enabled = connectionState != ConnectionState.SCANNING,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(if (connectionState == ConnectionState.SCANNING) "Scanning..." else "Scan for Devices")
            }
        } else {
            Button(
                onClick = { bleManager.disconnect() },
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFB00020)),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Disconnect")
            }
        }

        if (scannedDevices.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            Text("Devices:", style = MaterialTheme.typography.bodySmall)
            scannedDevices.forEach { device ->
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp)
                        .clickable { bleManager.connect(device.address, device.name) },
                    shape = RoundedCornerShape(8.dp),
                    color = MaterialTheme.colorScheme.surfaceVariant,
                ) {
                    Text(
                        "${device.name}\n${device.address}",
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.padding(12.dp)
                    )
                }
            }
        }

        Divider(modifier = Modifier.padding(vertical = 16.dp))

        // FPS
        Text("Frame Rate", style = MaterialTheme.typography.titleSmall, color = Color.Gray)
        Spacer(Modifier.height(4.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            listOf(24, 25, 30, 50, 60).forEach { fps ->
                FilterChip(
                    selected = selectedFps == fps,
                    onClick = {
                        selectedFps = fps
                        bleManager.sendConfig("fps", fps.toString())
                        onStatus("FPS set to $fps")
                    },
                    label = { Text("$fps") }
                )
            }
        }

        Spacer(Modifier.height(12.dp))

        // Drop frame
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Drop Frame", modifier = Modifier.weight(1f))
            Switch(
                checked = dropFrame,
                onCheckedChange = {
                    dropFrame = it
                    bleManager.sendConfig("df", if (it) "1" else "0")
                    onStatus("Drop frame ${if (it) "on" else "off"}")
                }
            )
        }

        Spacer(Modifier.height(12.dp))

        // Jam timecode
        Button(onClick = onShowJam, modifier = Modifier.fillMaxWidth()) {
            Text("Jam Timecode")
        }

        Divider(modifier = Modifier.padding(vertical = 16.dp))

        if (isClap) {
            // Brightness — only on CLAP devices with LED matrix
            Text("Brightness: $brightness", style = MaterialTheme.typography.titleSmall, color = Color.Gray)
            Slider(
                value = brightness.toFloat(),
                onValueChange = { brightness = it.toInt() },
                onValueChangeFinished = {
                    bleManager.sendConfig("brightness", brightness.toString())
                },
                valueRange = 0f..15f,
                steps = 14,
            )
            Row(horizontalArrangement = Arrangement.SpaceBetween) {
                Text("0", style = MaterialTheme.typography.bodySmall)
                Text("15", style = MaterialTheme.typography.bodySmall)
            }
            Divider(modifier = Modifier.padding(vertical = 16.dp))
        }

        if (isLtc) {
            // Master/Slave mode switch — only on LTC devices
            Text("Device Mode", style = MaterialTheme.typography.titleSmall, color = Color.Gray)
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = true,
                    onClick = {
                        bleManager.sendConfig("mode", "slave")
                        onStatus("Switching to slave mode...")
                    },
                    label = { Text("Slave") }
                )
                FilterChip(
                    selected = false,
                    onClick = {
                        bleManager.sendConfig("mode", "master")
                        onStatus("Switching to master mode...")
                    },
                    label = { Text("Master") }
                )
            }
            Spacer(Modifier.height(4.dp))
            Text("Changes mode and restarts", style = MaterialTheme.typography.bodySmall, color = Color.Gray)
            Divider(modifier = Modifier.padding(vertical = 16.dp))
        }

        // Device name
        Text("Device Name", style = MaterialTheme.typography.titleSmall, color = Color.Gray)
        Spacer(Modifier.height(4.dp))
        OutlinedTextField(
            value = deviceName,
            onValueChange = { deviceName = it },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(8.dp))
        Button(
            onClick = {
                bleManager.sendConfig("name", deviceName)
                onStatus("Name set")
            },
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Set Name")
        }

        Divider(modifier = Modifier.padding(vertical = 16.dp))

        Button(
            onClick = {
                bleManager.sendConfig("restart", "")
                onStatus("Device restarting")
            },
            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFB00020)),
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Restart Device")
        }

        Spacer(Modifier.height(16.dp))
    }
}

@Composable
fun JamDialog(
    current: Timecode,
    onDismiss: () -> Unit,
    onJam: (dd: Int, hh: Int, mm: Int, ss: Int, ff: Int) -> Unit,
) {
    var dd by remember { mutableStateOf(current.dd.toString().padStart(2, '0')) }
    var hh by remember { mutableStateOf(current.hh.toString().padStart(2, '0')) }
    var mm by remember { mutableStateOf(current.mm.toString().padStart(2, '0')) }
    var ss by remember { mutableStateOf(current.ss.toString().padStart(2, '0')) }
    var ff by remember { mutableStateOf(current.ff.toString().padStart(2, '0')) }

    fun digitOnly(s: String, max: Int): String {
        val digits = s.filter { it.isDigit() }.take(2)
        val num = digits.toIntOrNull() ?: return digits
        return if (num > max) max.toString().padStart(2, '0') else digits
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Jam Timecode") },
        text = {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TcField("DD", dd) { dd = digitOnly(it, 99) }
                    Text(":", style = MaterialTheme.typography.headlineSmall)
                    TcField("HH", hh) { hh = digitOnly(it, 23) }
                    Text(":", style = MaterialTheme.typography.headlineSmall)
                    TcField("MM", mm) { mm = digitOnly(it, 59) }
                    Text(":", style = MaterialTheme.typography.headlineSmall)
                    TcField("SS", ss) { ss = digitOnly(it, 59) }
                    Text(".", style = MaterialTheme.typography.headlineSmall)
                    TcField("FF", ff) { ff = digitOnly(it, 59) }
                }
                Spacer(Modifier.height(8.dp))
                Text(
                    "Current: ${current.displayWithDays}",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color.Gray
                )
            }
        },
        confirmButton = {
            TextButton(onClick = {
                onJam(
                    dd.toIntOrNull() ?: current.dd,
                    hh.toIntOrNull() ?: current.hh,
                    mm.toIntOrNull() ?: current.mm,
                    ss.toIntOrNull() ?: current.ss,
                    ff.toIntOrNull() ?: current.ff,
                )
            }) { Text("Jam") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        }
    )
}

@Composable
private fun TcField(label: String, value: String, onValueChange: (String) -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(label, style = MaterialTheme.typography.bodySmall)
        OutlinedTextField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier.width(56.dp),
            singleLine = true,
            textStyle = MaterialTheme.typography.bodyLarge.copy(
                fontFamily = FontFamily.Monospace,
                textAlign = TextAlign.Center
            ),
        )
    }
}
