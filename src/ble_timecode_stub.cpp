#include "ble_timecode.h"
#include "config.h"

int bleGetMode() { return BLE_MASTER; }
void bleSetMode(int) {}

void bleTimecodeInit() {}
void bleTimecodeUpdate(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {}
void bleTimecodeSetName(const char *) {}
const char *bleTimecodeGetName() { return ""; }
void bleTimecodeDisconnectAll() {}
void bleTimecodeDisconnectPeer(const char *) {}
uint8_t bleTimecodeConnectedCount() { return 0; }
uint8_t bleTimecodeGetPeers(BlePeerInfo *, uint8_t) { return 0; }

void bleTimecodeSetCallback(BleTimecodeCb) {}
void bleTimecodePoll() {}
bool bleTimecodeConnected() { return false; }
uint8_t bleTimecodeScan(BleScanResult *, uint8_t) { return 0; }
void bleTimecodeSelect(const char *) {}
const char *bleTimecodeSelectedAddress() { return ""; }
const char *bleTimecodeConnectedAddress() { return ""; }
const char *bleTimecodeConnectedName() { return ""; }
