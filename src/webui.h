#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

typedef void (*FpsCallback)(uint8_t fps, bool dropFrame);
typedef void (*JamCallback)(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);
typedef void (*BrightnessCallback)(uint8_t val);
typedef void (*MatrixToggleCallback)(bool enabled);

class WebUI {
public:
    WebUI();

    // Start AP + optional STA, then HTTP server.
    // staSsid == ""  → skip STA (AP-only).
    void begin(const char *apSsid, const char *apPassword = nullptr,
               const char *staSsid = "", const char *staPassword = "");

    void handleClient();

    void update(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff,
                uint8_t fps, bool dropFrame, bool hdmiLocked,
                const char *source);

    void onSetFps(FpsCallback cb) { _fpsCb = cb; }
    void onJamTime(JamCallback cb) { _jamCb = cb; }
    void onSetBrightness(BrightnessCallback cb) { _brightnessCb = cb; }
    void onSetMatrixEnabled(MatrixToggleCallback cb) { _matrixCb = cb; }

    bool matrixEnabled() const { return _matrixEnabled; }
    uint8_t brightness() const { return _brightness; }
    bool autoFps() const { return _autoFps; }

    // Accessors for the config dump / web UI
    IPAddress apIp() const { return _apIp; }
    IPAddress staIp() const { return _staIp; }
    const char *staSsid() const { return _staSsid; }

private:
    WebServer _server;
    Preferences _prefs;

    uint8_t _dd = 0, _hh = 0, _mm = 0, _ss = 0, _ff = 0, _fps = 25;
    uint8_t _brightness = 4;
    bool _dropFrame = false, _hdmiLocked = false, _matrixEnabled = true, _autoFps = false;
    char _source[8] = "FREE";

    FpsCallback _fpsCb = nullptr;
    JamCallback _jamCb = nullptr;
    BrightnessCallback _brightnessCb = nullptr;
    MatrixToggleCallback _matrixCb = nullptr;

    IPAddress _apIp;
    IPAddress _staIp;
    char _apSsid[33];
    char _apPassword[65];
    char _staSsid[33];
    char _staPassword[65];
    unsigned long _staConnectStart = 0;
    bool _staWasConnected = false;
    unsigned long _apOffSince = 0;

    void _connectSta(const char *ssid, const char *password);
    void _saveStaCreds(const char *ssid, const char *password);
    void _loadStaCreds();

    static String _pageHtml();

    void handleRoot();
    void handleApiTc();
    void handleApiConfig();
    void handleApiJam();
    void handleApiBrightness();
    void handleApiMatrix();
    void handleApiWifi();
    void handleApiBle();
#ifndef BLE_SLAVE
    void handleLogo();
#endif
    void handleNotFound();
};
