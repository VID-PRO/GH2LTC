#pragma once
#include <Arduino.h>

#ifdef SOC_BLE_SUPPORTED
#include <BLEUUID.h>
extern const BLEUUID bleTimecodeServiceUUID;
extern const BLEUUID bleTimecodeCharUUID;
extern const BLEUUID bleTimecodeNameCharUUID;
#endif

// Runtime mode management (always available)
int bleGetMode();
void bleSetMode(int mode);
#define BLE_MODE_MASTER 1
#define BLE_MODE_SLAVE  2

void bleTimecodeInit();

// Master-mode functions (no-ops in slave mode)
void bleTimecodeUpdate(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);
void bleTimecodeSetName(const char *name);
const char *bleTimecodeGetName();
void bleTimecodeDisconnectAll();
void bleTimecodeDisconnectPeer(const char *address);
uint8_t bleTimecodeConnectedCount();

typedef struct {
    char address[18];
    char name[33];
    uint16_t connId;
} BlePeerInfo;

uint8_t bleTimecodeGetPeers(BlePeerInfo *peers, uint8_t maxPeers);

// Slave-mode functions (no-ops in master mode)
typedef void (*BleTimecodeCb)(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);
void bleTimecodeSetCallback(BleTimecodeCb cb);
void bleTimecodePoll();
bool bleTimecodeConnected();

typedef struct {
    char name[33];
    char address[18];
} BleScanResult;

uint8_t bleTimecodeScan(BleScanResult *results, uint8_t maxResults);
void bleTimecodeSelect(const char *address);
const char *bleTimecodeSelectedAddress();
const char *bleTimecodeConnectedAddress();
const char *bleTimecodeConnectedName();
