#include "ble_timecode.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAdvertising.h>
#include <BLEUUID.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <string>
#include <cstring>

static Preferences blePrefs;
static const char *NVS_NS = "ble";

// 128-bit custom service / characteristic UUIDs
const BLEUUID bleTimecodeServiceUUID(std::string("9a6f0001-5c9a-4b3e-8a2c-f12345678901"));
const BLEUUID bleTimecodeCharUUID(std::string("9a6f0002-5c9a-4b3e-8a2c-f12345678901"));

// =========================================================================
// BLE Master — advertises timecode service, sends notifications per frame
// =========================================================================
#if defined(BLE_MASTER)

static BLEServer *server = nullptr;
static BLECharacteristic *tcChar = nullptr;
static bool deviceConnected = false;
static char bleName[33] = "TC-LTC-MASTER";

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *) override {
        deviceConnected = true;
    }
    void onDisconnect(BLEServer *) override {
        deviceConnected = false;
        BLEDevice::getAdvertising()->start();
    }
};

const char *bleTimecodeGetName() {
    return bleName;
}

void bleTimecodeSetName(const char *name) {
    strncpy(bleName, name, sizeof(bleName) - 1);
    bleName[sizeof(bleName) - 1] = '\0';
    blePrefs.begin(NVS_NS, false);
    blePrefs.putString("name", bleName);
    blePrefs.end();
}

void bleTimecodeDisconnectAll() {
    if (!server) return;
    std::map<uint16_t, conn_status_t> peers = server->getPeerDevices(false);
    for (auto &p : peers) {
        server->disconnect(p.first);
    }
}

uint8_t bleTimecodeConnectedCount() {
    return server ? server->getConnectedCount() : 0;
}

void bleTimecodeInit() {
    blePrefs.begin(NVS_NS, true);
    String saved = blePrefs.getString("name", "TC-LTC-MASTER");
    blePrefs.end();
    strncpy(bleName, saved.c_str(), sizeof(bleName) - 1);
    bleName[sizeof(bleName) - 1] = '\0';

    BLEDevice::init(bleName);
    server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService *svc = server->createService(bleTimecodeServiceUUID);
    tcChar = svc->createCharacteristic(
        bleTimecodeCharUUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    tcChar->addDescriptor(new BLE2902());
    uint8_t init[5] = {0, 0, 0, 0, 0};
    tcChar->setValue(init, 5);
    svc->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(bleTimecodeServiceUUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
}

void bleTimecodeUpdate(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    if (!deviceConnected) return;
    uint8_t data[5] = {dd, hh, mm, ss, ff};
    tcChar->setValue(data, 5);
    tcChar->notify();
}

// =========================================================================
// BLE Slave — scans for master, connects, subscribes to notifications
// =========================================================================
#elif defined(BLE_SLAVE)

static BLEClient *client = nullptr;
static BLERemoteCharacteristic *remoteChar = nullptr;
static BleTimecodeCb timecodeCb = nullptr;
static bool connected = false;
static unsigned long lastScan = 0;
static char selectedAddr[18] = "";
static char connectedAddr[18] = "";
static BleScanResult cachedResults[10];
static uint8_t cachedCount = 0;
static bool scanPending = false;

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient *) override {
        connected = true;
    }
    void onDisconnect(BLEClient *) override {
        connected = false;
        client = nullptr;
        remoteChar = nullptr;
        connectedAddr[0] = '\0';
    }
};

static void notifyCallback(BLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
    if (len >= 5 && timecodeCb) {
        timecodeCb(data[0], data[1], data[2], data[3], data[4]);
    }
}

static bool tryConnect(BLEAdvertisedDevice &dev) {
    client = BLEDevice::createClient();
    client->setClientCallbacks(new ClientCallbacks());
    if (!client->connect(&dev)) {
        delete client;
        client = nullptr;
        return false;
    }
    delay(500);
    BLERemoteService *svc = client->getService(bleTimecodeServiceUUID);
    if (!svc) {
        client->disconnect();
        delete client;
        client = nullptr;
        return false;
    }
    delay(200);
    remoteChar = svc->getCharacteristic(bleTimecodeCharUUID);
    if (!remoteChar || !remoteChar->canNotify()) {
        client->disconnect();
        delete client;
        client = nullptr;
        return false;
    }
    delay(200);
    remoteChar->registerForNotify(notifyCallback);
    connected = true;

    // Save connected address
    BLEAddress addr = dev.getAddress();
    snprintf(connectedAddr, sizeof(connectedAddr),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             (*addr.getNative())[5], (*addr.getNative())[4],
             (*addr.getNative())[3], (*addr.getNative())[2],
             (*addr.getNative())[1], (*addr.getNative())[0]);
    return true;
}

void bleTimecodeInit() {
    BLEDevice::init("TC-LTC-SLAVE");
    blePrefs.begin(NVS_NS, true);
    String saved = blePrefs.getString("master", "");
    blePrefs.end();
    if (saved.length()) {
        strncpy(selectedAddr, saved.c_str(), sizeof(selectedAddr) - 1);
        selectedAddr[sizeof(selectedAddr) - 1] = '\0';
    }
}

void bleTimecodeSetCallback(BleTimecodeCb cb) {
    timecodeCb = cb;
}

void bleTimecodePoll() {
    if (connected) return;

    unsigned long now = millis();
    if (now - lastScan < 5000) return;
    lastScan = now;

    if (client) {
        client->disconnect();
        delete client;
        client = nullptr;
        remoteChar = nullptr;
    }

    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(nullptr);
    scan->setActiveScan(true);
    scan->setInterval(200);
    scan->setWindow(100);
    BLEScanResults results = scan->start(3, false);

    for (int i = 0; i < results.getCount(); i++) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        if (!dev.haveServiceUUID() || !dev.isAdvertisingService(bleTimecodeServiceUUID))
            continue;

        // If a specific master is selected, only connect to that one
        if (selectedAddr[0]) {
            BLEAddress addr = dev.getAddress();
            char addrStr[18];
            snprintf(addrStr, sizeof(addrStr),
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     (*addr.getNative())[5], (*addr.getNative())[4],
                     (*addr.getNative())[3], (*addr.getNative())[2],
                     (*addr.getNative())[1], (*addr.getNative())[0]);
            if (strcmp(addrStr, selectedAddr) != 0) continue;
        }

        if (tryConnect(dev)) break;
    }
    scan->clearResults();
}

bool bleTimecodeConnected() {
    return connected;
}

uint8_t bleTimecodeScan(BleScanResult *results, uint8_t maxResults) {
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(nullptr);
    scan->setActiveScan(true);
    scan->setInterval(200);
    scan->setWindow(100);
    BLEScanResults scanResults = scan->start(3, false);

    uint8_t count = 0;
    for (int i = 0; i < scanResults.getCount() && count < maxResults; i++) {
        BLEAdvertisedDevice dev = scanResults.getDevice(i);
        if (!dev.haveServiceUUID() || !dev.isAdvertisingService(bleTimecodeServiceUUID))
            continue;

        BLEAddress addr = dev.getAddress();
        snprintf(results[count].address, sizeof(results[count].address),
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 (*addr.getNative())[5], (*addr.getNative())[4],
                 (*addr.getNative())[3], (*addr.getNative())[2],
                 (*addr.getNative())[1], (*addr.getNative())[0]);

        const char *dn = dev.getName().c_str();
        if (dn && dn[0]) {
            strncpy(results[count].name, dn, sizeof(results[count].name) - 1);
            results[count].name[sizeof(results[count].name) - 1] = '\0';
        } else {
            snprintf(results[count].name, sizeof(results[count].name),
                     "TC-MASTER-%s", results[count].address + 9);
        }
        count++;
    }
    scan->clearResults();
    return count;
}

void bleTimecodeSelect(const char *address) {
    strncpy(selectedAddr, address, sizeof(selectedAddr) - 1);
    selectedAddr[sizeof(selectedAddr) - 1] = '\0';

    blePrefs.begin(NVS_NS, false);
    blePrefs.putString("master", selectedAddr);
    blePrefs.end();

    if (client) {
        client->disconnect();
        delete client;
        client = nullptr;
        remoteChar = nullptr;
    }
    connected = false;
    lastScan = 0;
}

const char *bleTimecodeSelectedAddress() {
    return selectedAddr;
}

const char *bleTimecodeConnectedAddress() {
    return connectedAddr;
}

#endif
