#pragma once
#include <Arduino.h>
#include <BLEUUID.h>

extern const BLEUUID bleTimecodeServiceUUID;
extern const BLEUUID bleTimecodeCharUUID;

void bleTimecodeInit();

#if defined(BLE_MASTER)
void bleTimecodeUpdate(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);
void bleTimecodeSetName(const char *name);
const char *bleTimecodeGetName();
void bleTimecodeDisconnectAll();
uint8_t bleTimecodeConnectedCount();

#elif defined(BLE_SLAVE)
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
#endif
