#include "webui.h"
#if !defined(BLE_SLAVE) || defined(BLE_MODE_RUNTIME)
#include "logo_data.h"
#endif
#include "ble_timecode.h"

WebUI::WebUI() : _server(80) {
    _apSsid[0] = '\0';
    _apPassword[0] = '\0';
    _staSsid[0] = '\0';
    _staPassword[0] = '\0';
}

// -----------------------------------------------------------------------
// begin — start WiFi AP + HTTP server, then attempt saved STA connection
// -----------------------------------------------------------------------
void WebUI::begin(const char *apSsid, const char *apPassword,
                  const char *staSsid, const char *staPassword) {
    strncpy(_apSsid, apSsid, sizeof(_apSsid) - 1);
    strncpy(_apPassword, apPassword ? apPassword : "", sizeof(_apPassword) - 1);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);
    _apIp = WiFi.softAPIP();
    Serial.print(F("WebUI AP:      "));
    Serial.print(apSsid);
    if (_apIp == IPAddress(0,0,0,0)) {
        Serial.println(F("  IP 0.0.0.0 — AP FAILED!"));
    } else {
        Serial.print(F("  IP "));
        Serial.println(_apIp);
    }

    // --- STA: try saved credentials, then fall back to compile-time defaults ---
    _loadStaCreds();
    if (_staSsid[0] == '\0' && staSsid && staSsid[0]) {
        strncpy(_staSsid, staSsid, sizeof(_staSsid) - 1);
        strncpy(_staPassword, staPassword ? staPassword : "", sizeof(_staPassword) - 1);
    }
    if (_staSsid[0]) {
        Serial.print(F("WebUI STA:     "));
        Serial.print(_staSsid);
        Serial.println(F("  (connecting)"));
        _connectSta(_staSsid, _staPassword);
    } else {
        Serial.println(F("WebUI STA:     (none)"));
    }

    // --- HTTP routes ---
    _server.on("/",          std::bind(&WebUI::handleRoot,   this));
    _server.on("/api/tc",    std::bind(&WebUI::handleApiTc,  this));
    _server.on("/api/config", HTTP_POST, std::bind(&WebUI::handleApiConfig, this));
    _server.on("/api/jam",   HTTP_POST, std::bind(&WebUI::handleApiJam,   this));
    _server.on("/api/brightness", HTTP_ANY, std::bind(&WebUI::handleApiBrightness, this));
    _server.on("/api/matrix",    HTTP_ANY, std::bind(&WebUI::handleApiMatrix,    this));
    _server.on("/api/oled",      HTTP_ANY, std::bind(&WebUI::handleApiOled,      this));
    _server.on("/api/ltc",       HTTP_ANY, std::bind(&WebUI::handleApiLtc,       this));
    _server.on("/api/restart",   HTTP_POST, std::bind(&WebUI::handleApiRestart,   this));
    _server.on("/api/wifi",  HTTP_ANY,  std::bind(&WebUI::handleApiWifi,  this));
    _server.on("/api/ble",   HTTP_ANY,  std::bind(&WebUI::handleApiBle,   this));
    _server.on("/api/mode",  HTTP_ANY,  std::bind(&WebUI::handleApiMode,  this));
#if !defined(BLE_SLAVE) || defined(BLE_MODE_RUNTIME)
    _server.on("/logo.png",            std::bind(&WebUI::handleLogo,    this));
#endif
    _server.onNotFound(      std::bind(&WebUI::handleNotFound, this));

    _server.begin();
    Serial.println(F("WebUI HTTP server started"));

    // Load brightness and matrix state from NVS (read/write to auto-create namespace)
    _prefs.begin("webui", false);
    _brightness = _prefs.getUChar("brightness", 4);
    _matrixEnabled = _prefs.getBool("matrix_en", true);
    _oledEnabled = _prefs.getBool("oled_en", true);
    _ltcEnabled = _prefs.getBool("ltc_en", true);
    _prefs.end();
    if (_brightnessCb) _brightnessCb(_brightness);
    if (_matrixCb) _matrixCb(_matrixEnabled);
    if (_oledCb) _oledCb(_oledEnabled);
    if (_ltcCb) _ltcCb(_ltcEnabled);

    // Mark time so handleClient() can check STA progress non-blockingly
    _staConnectStart = millis();
}

// -----------------------------------------------------------------------
// update — store latest timecode state (call every frame from loop)
// -----------------------------------------------------------------------
void WebUI::update(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff,
                   uint8_t fps, bool dropFrame, bool hdmiLocked,
                   const char *source) {
    _dd = dd; _hh = hh; _mm = mm; _ss = ss; _ff = ff;
    _fps = fps; _dropFrame = dropFrame; _hdmiLocked = hdmiLocked;
    strncpy(_source, source, sizeof(_source) - 1);
    _source[sizeof(_source) - 1] = '\0';
}

// -----------------------------------------------------------------------
// handleClient — must be called regularly from main loop
// -----------------------------------------------------------------------
void WebUI::handleClient() {
    _server.handleClient();

    wl_status_t staStatus = WiFi.status();
    bool staConnected = (staStatus == WL_CONNECTED);

    // STA just connected → grab IP, turn off AP (with debounce)
    if (staConnected && !_staWasConnected) {
        _staIp = WiFi.localIP();
        _apOffSince = millis();
        Serial.print(F("WebUI STA:     connected, IP "));
        Serial.println(_staIp);
        Serial.println(F("WebUI AP:      stopping (STA active)"));
        WiFi.enableAP(false);
    }

    // STA disconnected for >5 s and AP was intentionally turned off → bring it back
    if (!staConnected && _apOffSince && millis() - _apOffSince > 5000) {
        _staIp = IPAddress(0,0,0,0);
        _apOffSince = 0;
        Serial.println(F("WebUI STA:     disconnected, re-enabling AP"));
        WiFi.enableAP(true);
        WiFi.softAP(_apSsid, _apPassword[0] ? _apPassword : nullptr);
    }

    _staWasConnected = staConnected;
}

// =======================================================================
// STA helpers
// =======================================================================

void WebUI::_connectSta(const char *ssid, const char *password) {
    strncpy(_staSsid, ssid, sizeof(_staSsid) - 1);
    strncpy(_staPassword, password ? password : "", sizeof(_staPassword) - 1);
    _staIp = IPAddress(0,0,0,0);
    _staConnectStart = millis();
    WiFi.begin(ssid, password);
}

void WebUI::_saveStaCreds(const char *ssid, const char *password) {
    _prefs.begin("webui", false);
    _prefs.putString("sta_ssid", ssid);
    _prefs.putString("sta_pass", password ? password : "");
    _prefs.end();
    Serial.println(F("WebUI STA:     credentials saved"));
}

void WebUI::_loadStaCreds() {
    _prefs.begin("webui", false);
    String ssid = _prefs.getString("sta_ssid", "");
    String pass = _prefs.getString("sta_pass", "");
    _prefs.end();
    if (ssid.length()) {
        strncpy(_staSsid, ssid.c_str(), sizeof(_staSsid) - 1);
        strncpy(_staPassword, pass.c_str(), sizeof(_staPassword) - 1);
    } else {
        _staSsid[0] = '\0';
        _staPassword[0] = '\0';
    }
}

// =======================================================================
// HTTP handlers
// =======================================================================

// -----------------------------------------------------------------------
// GET /api/tc  — JSON timecode + status snapshot
// -----------------------------------------------------------------------
void WebUI::handleApiTc() {
    char body[256];
    snprintf(body, sizeof(body),
        "{\"tc\":\"%02u:%02u:%02u:%02u:%02u\","
        "\"fps\":%u,\"df\":%s,\"locked\":%s,\"source\":\"%s\","
        "\"auto\":%s,\"matrix\":%s,\"oled\":%s,\"ltc\":%s}",
        _dd, _hh, _mm, _ss, _ff,
        _fps, _dropFrame ? "true" : "false",
        _hdmiLocked ? "true" : "false",
        _source,
        _autoFps ? "true" : "false",
        _matrixEnabled ? "true" : "false",
        _oledEnabled ? "true" : "false",
        _ltcEnabled ? "true" : "false");
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// POST /api/config  — set frame rate and/or drop frame
// -----------------------------------------------------------------------
void WebUI::handleApiConfig() {
    String fpsStr = _server.arg("fps");
    String dfStr  = _server.arg("df");

    uint8_t fps = _fps;
    bool df = _dropFrame;

    if (fpsStr.length()) {
        int v = fpsStr.toInt();
        if (v == 0) {
            _autoFps = true;
            fps = 25;
        } else if (v == 24 || v == 25 || v == 30 || v == 50 || v == 60) {
            _autoFps = false;
            fps = v;
        }
        _fps = fps;
    }
    if (dfStr.length()) { df = (dfStr.toInt() != 0); _dropFrame = df; }

    if (_fpsCb) _fpsCb(fps, df);

    char body[48];
    snprintf(body, sizeof(body), "{\"ok\":true,\"auto\":%s}", _autoFps ? "true" : "false");
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// POST /api/jam  — jam / set timecode
// -----------------------------------------------------------------------
void WebUI::handleApiJam() {
    uint8_t dd = (uint8_t)constrain(_server.arg("dd").toInt(), 0, 99);
    uint8_t hh = (uint8_t)constrain(_server.arg("hh").toInt(), 0, 23);
    uint8_t mm = (uint8_t)constrain(_server.arg("mm").toInt(), 0, 59);
    uint8_t ss = (uint8_t)constrain(_server.arg("ss").toInt(), 0, 59);
    uint8_t ff = (uint8_t)constrain(_server.arg("ff").toInt(), 0, _fps - 1);

    if (_jamCb) _jamCb(dd, hh, mm, ss, ff);

    _server.send(200, "application/json", "{\"ok\":true}");
}

// -----------------------------------------------------------------------
// GET/POST /api/brightness  — matrix LED brightness 0-15
// -----------------------------------------------------------------------
void WebUI::handleApiBrightness() {
    if (_server.method() == HTTP_POST) {
        uint8_t val = (uint8_t)constrain(_server.arg("val").toInt(), 0, 15);
        _brightness = val;

        _prefs.begin("webui", false);
        _prefs.putUChar("brightness", val);
        _prefs.end();

        if (_brightnessCb) _brightnessCb(val);
    }

    char body[48];
    snprintf(body, sizeof(body), "{\"val\":%u}", _brightness);
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// GET/POST /api/matrix  — enable/disable LED matrix display
// -----------------------------------------------------------------------
void WebUI::handleApiMatrix() {
    if (_server.method() == HTTP_POST) {
        String enStr = _server.arg("en");
        if (enStr.length()) {
            _matrixEnabled = (enStr.toInt() != 0);
            _prefs.begin("webui", false);
            _prefs.putBool("matrix_en", _matrixEnabled);
            _prefs.end();
            if (_matrixCb) _matrixCb(_matrixEnabled);
        }
    }

    char body[48];
    snprintf(body, sizeof(body), "{\"en\":%s}", _matrixEnabled ? "true" : "false");
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// GET/POST /api/oled  — enable/disable OLED display
// -----------------------------------------------------------------------
void WebUI::handleApiOled() {
    if (_server.method() == HTTP_POST) {
        String enStr = _server.arg("en");
        if (enStr.length()) {
            _oledEnabled = (enStr.toInt() != 0);
            _prefs.begin("webui", false);
            _prefs.putBool("oled_en", _oledEnabled);
            _prefs.end();
            if (_oledCb) _oledCb(_oledEnabled);
        }
    }

    char body[48];
    snprintf(body, sizeof(body), "{\"en\":%s}", _oledEnabled ? "true" : "false");
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// GET/POST /api/ltc  — enable/disable LTC audio output
// -----------------------------------------------------------------------
void WebUI::handleApiLtc() {
    if (_server.method() == HTTP_POST) {
        String enStr = _server.arg("en");
        if (enStr.length()) {
            _ltcEnabled = (enStr.toInt() != 0);
            _prefs.begin("webui", false);
            _prefs.putBool("ltc_en", _ltcEnabled);
            _prefs.end();
            if (_ltcCb) _ltcCb(_ltcEnabled);
        }
    }

    char body[48];
    snprintf(body, sizeof(body), "{\"en\":%s}", _ltcEnabled ? "true" : "false");
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// POST /api/restart  — restart the ESP
// -----------------------------------------------------------------------
void WebUI::handleApiRestart() {
    _server.send(200, "application/json", "{\"ok\":true}");
    delay(100);
    ESP.restart();
}

// -----------------------------------------------------------------------
// GET/POST /api/wifi  — STA connection management
//   GET  → {"ssid":"...","ip":"...","rssi":0,"status":"..."}
//   POST → body: ssid=xxx&password=yyy  → {"ok":true}
// -----------------------------------------------------------------------
void WebUI::handleApiWifi() {
    if (_server.method() == HTTP_POST) {
        String ssid = _server.arg("ssid");
        String pass = _server.arg("password");

        // Empty ssid + empty password → disconnect and clear saved creds
        if (ssid.length() == 0 && pass.length() == 0) {
            WiFi.disconnect();
            _staSsid[0] = '\0';
            _staPassword[0] = '\0';
            _staIp = IPAddress(0,0,0,0);
            _prefs.begin("webui", false);
            _prefs.remove("sta_ssid");
            _prefs.remove("sta_pass");
            _prefs.end();
            Serial.println(F("WebUI STA:     credentials cleared"));
            _server.send(200, "application/json", "{\"ok\":true}");
            return;
        }

        if (ssid.length() == 0) {
            _server.send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
            return;
        }

        // Save credentials and attempt connection
        _saveStaCreds(ssid.c_str(), pass.c_str());
        _connectSta(ssid.c_str(), pass.c_str());

        _server.send(200, "application/json", "{\"ok\":true}");
        return;
    }

    // GET — return current STA status
    wl_status_t status = WiFi.status();
    const char *statusStr;
    switch (status) {
        case WL_CONNECTED:      statusStr = "connected";    break;
        case WL_DISCONNECTED:   statusStr = "disconnected"; break;
        case WL_NO_SSID_AVAIL:  statusStr = "no_ssid";      break;
        case WL_CONNECT_FAILED: statusStr = "auth_fail";    break;
        case WL_IDLE_STATUS:    statusStr = "connecting";   break;
        default:                statusStr = "idle";          break;
    }

    char body[256];
    snprintf(body, sizeof(body),
        "{\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"status\":\"%s\"}",
        _staSsid,
        _staIp.toString().c_str(),
        status == WL_CONNECTED ? WiFi.RSSI() : 0,
        statusStr);
    _server.send(200, "application/json", body);
}

// -----------------------------------------------------------------------
// GET/POST /api/ble  — BLE status and control
// -----------------------------------------------------------------------
void WebUI::handleApiBle() {
    if (bleGetMode() == BLE_MODE_MASTER) {
        if (_server.method() == HTTP_POST) {
            String action = _server.arg("action");

            if (action == "setname") {
                String name = _server.arg("name");
                if (name.length() > 0 && name.length() <= 32) {
                    bleTimecodeSetName(name.c_str());
                    _server.send(200, "application/json",
                        "{\"ok\":true,\"reboot\":true}");
                    return;
                }
                _server.send(400, "application/json",
                    "{\"ok\":false,\"error\":\"invalid name\"}");
                return;
            }

            if (action == "disconnect") {
                bleTimecodeDisconnectAll();
                _server.send(200, "application/json", "{\"ok\":true}");
                return;
            }

            if (action == "disconnect_peer") {
                String addr = _server.arg("address");
                if (addr.length()) {
                    bleTimecodeDisconnectPeer(addr.c_str());
                    _server.send(200, "application/json", "{\"ok\":true}");
                    return;
                }
                _server.send(400, "application/json",
                    "{\"ok\":false,\"error\":\"address required\"}");
                return;
            }

            _server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"unknown action\"}");
            return;
        }

        // GET — return master name + peer list
        BlePeerInfo peerBuf[8];
        uint8_t n = bleTimecodeGetPeers(peerBuf, 8);
        String json = "{\"name\":\"" + String(bleTimecodeGetName()) + "\",\"peers\":[";
        for (uint8_t i = 0; i < n; i++) {
            if (i) json += ',';
            json += "{\"addr\":\"" + String(peerBuf[i].address) + "\",\"name\":\"" + String(peerBuf[i].name) + "\"}";
        }
        json += "]}";
        _server.send(200, "application/json", json);
    } else {
        // Slave mode
        if (_server.method() == HTTP_POST) {
            String action = _server.arg("action");

            if (action == "setname") {
                String name = _server.arg("name");
                if (name.length() > 0 && name.length() <= 32) {
                    bleTimecodeSetName(name.c_str());
                    _server.send(200, "application/json",
                        "{\"ok\":true,\"reboot\":true}");
                    return;
                }
                _server.send(400, "application/json",
                    "{\"ok\":false,\"error\":\"invalid name\"}");
                return;
            }

            if (action == "scan") {
                BleScanResult results[10];
                uint8_t count = bleTimecodeScan(results, 10);
                String json = "{\"count\":" + String(count) + ",\"devices\":[";
                for (uint8_t i = 0; i < count; i++) {
                    if (i) json += ',';
                    json += "{\"name\":\"" + String(results[i].name) +
                            "\",\"address\":\"" + String(results[i].address) + "\"}";
                }
                json += "]}";
                _server.send(200, "application/json", json);
                return;
            }

            if (action == "select") {
                String address = _server.arg("address");
                if (address.length()) {
                    bleTimecodeSelect(address.c_str());
                    _server.send(200, "application/json", "{\"ok\":true}");
                    return;
                }
                _server.send(400, "application/json",
                    "{\"ok\":false,\"error\":\"address required\"}");
                return;
            }

            _server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"unknown action\"}");
            return;
        }

        // GET
        char body[512];
        snprintf(body, sizeof(body),
            "{\"connected\":%s,\"selected\":\"%s\",\"connected_addr\":\"%s\",\"connected_name\":\"%s\",\"name\":\"%s\"}",
            bleTimecodeConnected() ? "true" : "false",
            bleTimecodeSelectedAddress(),
            bleTimecodeConnected() ? bleTimecodeConnectedAddress() : "",
            bleTimecodeConnected() ? bleTimecodeConnectedName() : "",
            bleTimecodeGetName());
        _server.send(200, "application/json", body);
    }
}

// -----------------------------------------------------------------------
// GET/POST /api/mode  — get/set device mode (master/slave)
// -----------------------------------------------------------------------
void WebUI::handleApiMode() {
#if defined(BLE_MODE_RUNTIME)
    if (_server.method() == HTTP_POST) {
        String modeStr = _server.arg("mode");
        int newMode = modeStr.toInt();
        if (newMode == BLE_MODE_MASTER || newMode == BLE_MODE_SLAVE) {
            bleSetMode(newMode);
            _server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
            return;
        }
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid mode\"}");
        return;
    }
    int mode = bleGetMode();
    char body[64];
    snprintf(body, sizeof(body), "{\"mode\":%d,\"modeName\":\"%s\"}",
             mode, mode == BLE_MODE_MASTER ? "master" : "slave");
    _server.send(200, "application/json", body);
#else
#if defined(BLE_MASTER)
    _server.send(200, "application/json", "{\"mode\":1,\"modeName\":\"master\",\"locked\":true}");
#else
    _server.send(200, "application/json", "{\"mode\":2,\"modeName\":\"slave\",\"locked\":true}");
#endif
#endif
}

#if !defined(BLE_SLAVE) || defined(BLE_MODE_RUNTIME)
// -----------------------------------------------------------------------
// GET /logo.png  — serve the logo image from PROGMEM
// -----------------------------------------------------------------------
void WebUI::handleLogo() {
#if defined(BLE_MODE_RUNTIME)
    if (bleGetMode() != BLE_MODE_MASTER) { handleNotFound(); return; }
#endif
    _server.setContentLength(logo_png_len);
    _server.send(200, "image/png", "");
    _server.sendContent_P((PGM_P)logo_png, logo_png_len);
}
#endif

// -----------------------------------------------------------------------
// GET /  — serve the full web app page (with runtime IPs injected)
// -----------------------------------------------------------------------
void WebUI::handleRoot() {
    String html = _pageHtml();
    html.replace("__AP_SSID__", _apSsid);
    html.replace("__AP_IP__",   _apIp.toString());
    if (_staSsid[0] && _staIp != IPAddress(0,0,0,0)) {
        html.replace("__STA_SSID__", _staSsid);
        html.replace("__STA_IP__",   _staIp.toString());
    } else {
        html.replace("__STA_SSID__", "—");
        html.replace("__STA_IP__",   "—");
    }
    _server.send(200, "text/html", html);
}

// -----------------------------------------------------------------------
// 404 handler
// -----------------------------------------------------------------------
void WebUI::handleNotFound() {
    _server.send(404, "text/plain", "Not found");
}

// =======================================================================
// Embedded HTML/CSS/JS — DJI Nucleus-inspired fullscreen timecode UI
// =======================================================================

String WebUI::_pageHtml() {
    // Part 1: everything before the BLE drawer-section content
    String html = F(R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>)rawliteral");
    html += bleTimecodeGetName();
    html += F(R"rawliteral(</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}

html,body{
  width:100%;height:100%;
  background:#0a0a0a;
  color:#00ffbb;
  font-family:'Consolas','Courier New',monospace;
  overflow:hidden;
  display:flex;flex-direction:column
}

/* ── status row ── */
.status-dot{
  width:10px;height:10px;border-radius:50%;
  flex-shrink:0;
  transition:background .15s
}
.status-dot.hdmi{background:#00ff44;box-shadow:0 0 8px #00ff44}
.status-dot.rtc{background:#ffaa00;box-shadow:0 0 8px #ffaa00}
.status-dot.free{background:#555;box-shadow:0 0 8px #555}
#source-label{
  text-transform:uppercase;letter-spacing:.1em;
  color:#666;flex-shrink:0
}
.spacer{flex:1}
.fps-badge{
  background:#1a1a1a;border-radius:4px;padding:2px 8px;
  font-size:clamp(10px,2vw,14px);color:#888;flex-shrink:0
}

/* ── bar height (header & footer match) ── */
:root{
  --bar-height:clamp(56px,12vw,100px)
}

/* ── header (logo + status in one row, top of page) ── */
.header{
  display:flex;align-items:center;
  height:var(--bar-height);
  padding:0 16px;
  flex-shrink:0;
  font-size:clamp(10px,2.5vw,16px);
  color:#555;
  position:relative;
  -webkit-user-select:none;user-select:none
}
.header-left{
  display:flex;align-items:center;gap:clamp(6px,1.5vw,14px)
}
.header-right{
  margin-left:auto;
  display:flex;align-items:center
}
.header-brand{
  position:absolute;left:50%;transform:translateX(-50%);
  color:#00ffbb;font-weight:700;font-size:clamp(14px,3vw,22px);
  text-decoration:none;letter-spacing:2px;
  top:50%;transform:translate(-50%,-50%)
}

/* ── timecode area (fills all space between status-bar and bottom) ── */
.tc-area{
  flex:1;display:flex;flex-direction:column;
  align-items:center;
  padding:0;
  -webkit-user-select:none;user-select:none
}

/* ── timecode wrapper (centered in remaining space) ── */
.tc-wrap{
  flex:1;display:flex;flex-direction:column;
  align-items:center;justify-content:center;
  padding:0 16px
}
.tc-label{
  font-size:clamp(10px,2.5vw,16px);
  color:#333;letter-spacing:.3em;text-transform:uppercase;
  margin-bottom:clamp(2px,.5vw,8px)
}
#tc-display{
  font-size:clamp(2.8rem,22vw,12rem);
  font-weight:700;
  letter-spacing:.04em;
  color:#00ffbb;
  text-shadow:0 0 30px rgba(0,255,187,.25);
  line-height:1;
  transition:color .2s
}
#tc-display.hdmi{color:#00ffbb;text-shadow:0 0 30px rgba(0,255,187,.25)}
#tc-display.rtc{color:#ffaa00;text-shadow:0 0 30px rgba(255,170,0,.2)}
#tc-display.free{color:#555;text-shadow:none}

/* ── footer (gear button centered) ── */
.footer{
  height:var(--bar-height);
  display:flex;align-items:center;justify-content:center;
  width:100%;
  flex-shrink:0;
  margin-top:auto
}
.settings-btn{
  width:44px;height:44px;border-radius:50%;
  background:#1a1a1a;border:1px solid #333;
  color:#555;font-size:22px;
  display:flex;align-items:center;justify-content:center;
  cursor:pointer;transition:.2s;
  -webkit-user-select:none;user-select:none
}
.settings-btn:hover{background:#222;color:#00ffbb;border-color:#00ffbb}
.settings-btn.active{background:#222;color:#00ffbb;border-color:#00ffbb}

/* ── settings panel ── */
.settings-panel{
  position:fixed;bottom:0;left:0;right:0;
  background:#111;border-top:1px solid #222;
  padding:20px 24px 28px;
  transform:translateY(100%);transition:transform .3s ease;
  z-index:12;
  max-height:70vh;overflow-y:auto
}
.settings-panel.open{transform:translateY(0)}
.settings-title{
  color:#444;font-size:11px;text-transform:uppercase;
  letter-spacing:.15em;margin-bottom:16px
}
.panel-close{
  position:absolute;top:12px;right:12px;
  width:36px;height:36px;border-radius:50%;
  background:transparent;border:1px solid #333;
  color:#555;font-size:16px;
  display:flex;align-items:center;justify-content:center;
  cursor:pointer;transition:.2s;
  -webkit-user-select:none;user-select:none
}
.panel-close:hover{color:#00ffbb;border-color:#00ffbb}
.setting-row{
  display:flex;align-items:center;gap:12px;
  margin-bottom:14px;flex-wrap:wrap
}
.setting-label{
  color:#666;font-size:clamp(11px,2vw,13px);
  min-width:70px;text-transform:uppercase;letter-spacing:.08em
}
.fps-group{display:flex;gap:6px;flex-wrap:wrap}
.fps-btn{
  background:#1a1a1a;border:1px solid #333;border-radius:4px;
  color:#888;padding:6px 14px;
  font-family:inherit;font-size:clamp(12px,2vw,14px);
  cursor:pointer;transition:.15s
}
.fps-btn:hover{border-color:#555;color:#aaa}
.fps-btn.active{background:#0a2a1a;border-color:#00ffbb;color:#00ffbb}

/* toggle switch */
.toggle-track{
  display:inline-flex;align-items:center;gap:8px;
  cursor:pointer;-webkit-user-select:none;user-select:none
}
.toggle-track input{display:none}
.toggle-switch{
  width:40px;height:22px;border-radius:11px;
  background:#222;position:relative;transition:.2s
}
.toggle-switch::after{
  content:'';position:absolute;top:2px;left:2px;
  width:18px;height:18px;border-radius:50%;
  background:#555;transition:.2s
}
.toggle-track input:checked+.toggle-switch{background:#0a3a2a}
.toggle-track input:checked+.toggle-switch::after{
  background:#00ffbb;transform:translateX(18px)
}
.toggle-label{color:#888;font-size:clamp(11px,2vw,13px)}

/* jam section */
.jam-row{display:flex;align-items:center;gap:6px;flex-wrap:wrap}
.jam-row input{
  width:clamp(36px,8vw,52px);padding:6px 4px;text-align:center;
  background:#1a1a1a;border:1px solid #333;border-radius:4px;
  color:#00ffbb;font-family:inherit;font-size:clamp(13px,2.5vw,18px)
}
.jam-row input:focus{outline:none;border-color:#00ffbb}
.jam-sep{color:#444;font-size:clamp(13px,2.5vw,18px)}
.jam-btn{
  background:#0a2a1a;border:1px solid #00ffbb;border-radius:4px;
  color:#00ffbb;padding:6px 16px;
  font-family:inherit;font-size:clamp(11px,2vw,13px);
  cursor:pointer;transition:.15s;text-transform:uppercase
}
.jam-btn:hover{background:#0f3f2a}

/* ── WiFi config section ── */
.wifi-section{
  margin-top:12px;padding-top:12px;border-top:1px solid #1a1a1a
}
.wifi-status{
  font-size:clamp(9px,1.5vw,11px);
  color:#444;margin-bottom:10px
}
.wifi-status span{color:#666}
.wifi-form{display:flex;flex-direction:column;gap:8px}
.wifi-form input{
  padding:8px 10px;
  background:#1a1a1a;border:1px solid #333;border-radius:4px;
  color:#00ffbb;font-family:inherit;font-size:clamp(12px,2vw,14px)
}
.wifi-form input:focus{outline:none;border-color:#00ffbb}
.wifi-form input::placeholder{color:#444}
.wifi-form .row{display:flex;gap:8px;align-items:center}
.wifi-form .row input{flex:1}
.wifi-connect-btn{
  background:#0a2a1a;border:1px solid #00ffbb;border-radius:4px;
  color:#00ffbb;padding:8px 20px;
  font-family:inherit;font-size:clamp(11px,2vw,13px);
  cursor:pointer;transition:.15s;text-transform:uppercase;
  white-space:nowrap
}
.wifi-connect-btn:hover{background:#0f3f2a}
.wifi-forget-btn{
  background:transparent;border:1px solid #441a1a;border-radius:4px;
  color:#884444;padding:8px 14px;
  font-family:inherit;font-size:clamp(10px,1.5vw,12px);
  cursor:pointer;transition:.15s;text-transform:uppercase
}
.wifi-forget-btn:hover{background:#1a0a0a;border-color:#664444}
.wifi-msg{
  font-size:clamp(9px,1.5vw,11px);margin-top:4px;min-height:1.2em
}
.wifi-msg.ok{color:#00aa44}
.wifi-msg.err{color:#aa4444}

/* restart button */
.restart-btn{
  display:block;width:100%;margin-top:16px;
  background:transparent;border:1px solid #441a1a;border-radius:4px;
  color:#884444;padding:10px 14px;
  font-family:inherit;font-size:clamp(11px,1.5vw,13px);
  cursor:pointer;transition:.15s;text-transform:uppercase
}
.restart-btn:hover{background:#1a0a0a;border-color:#664444}

/* brightness slider */
.brightness-row{display:flex;align-items:center;gap:10px}
.brightness-row input[type=range]{
  -webkit-appearance:none;appearance:none;
  width:140px;height:6px;
  background:#222;border-radius:3px;outline:none
}
.brightness-row input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;appearance:none;
  width:20px;height:20px;border-radius:50%;
  background:#00ffbb;border:none;cursor:pointer
}

/* BLE config in drawer */
.ble-drawer-section{
  margin-top:8px;padding-top:12px;border-top:1px solid #1a1a1a;
  color:#444;font-size:clamp(9px,1.5vw,11px)
}
.ble-drawer-section .settings-title{margin-bottom:10px}
.ble-status{font-size:clamp(9px,1.5vw,11px);color:#444;margin-bottom:10px}
.ble-status span{color:#666}
.ble-form{display:flex;flex-direction:column;gap:8px}
.ble-form input{
  padding:8px 10px;
  background:#1a1a1a;border:1px solid #333;border-radius:4px;
  color:#00ffbb;font-family:inherit;font-size:clamp(12px,2vw,14px);flex:1
}
.ble-form input:focus{outline:none;border-color:#00ffbb}
.ble-form input::placeholder{color:#444}
.ble-form .row{display:flex;gap:8px;align-items:center}
.ble-form button{
  background:#0a2a1a;border:1px solid #00ffbb;border-radius:4px;
  color:#00ffbb;padding:8px 14px;
  font-family:inherit;font-size:clamp(11px,2vw,13px);
  cursor:pointer;transition:.15s;text-transform:uppercase;
  white-space:nowrap
}
.ble-form button:hover{background:#0f3f2a}
.ble-form button:disabled{opacity:.4;cursor:default}
.ble-msg{font-size:clamp(9px,1.5vw,11px);margin-top:4px;min-height:1.2em}
.ble-device{
  display:flex;gap:8px;align-items:center;
  padding:6px 0;border-bottom:1px solid #1a1a1a;
  font-size:clamp(11px,1.8vw,13px)
}
.ble-device span{flex:1}
.ble-addr{color:#555;font-size:clamp(9px,1.3vw,11px);font-family:monospace}
.ble-device button{
  background:transparent;border:1px solid #1a4a2a;border-radius:4px;
  color:#00aa55;padding:4px 10px;
  font-family:inherit;font-size:clamp(10px,1.5vw,12px);
  cursor:pointer
}
.ble-device button:hover{background:#0a2a1a}
.ble-subtitle{
  color:#555;font-size:clamp(10px,1.5vw,12px);
  text-transform:uppercase;letter-spacing:.1em;
  margin:8px 0 4px
}
.ble-peer{
  display:flex;gap:16px;align-items:center;
  padding:4px 0;font-size:clamp(11px,1.8vw,13px)
}
.ble-peer span{flex:1}
.ble-peer button{
  background:transparent;border:1px solid #441a1a;border-radius:4px;
  color:#884444;padding:3px 10px;
  font-family:inherit;font-size:clamp(9px,1.3vw,11px);
  cursor:pointer
}
.ble-peer button:hover{background:#1a0a0a;border-color:#664444}
}
.brightness-val{
  color:#00ffbb;font-size:clamp(12px,2vw,14px);
  min-width:20px;text-align:center
}

/* ── bottom info (always visible) ── */
.wifi-info{
  margin-top:8px;padding-top:12px;border-top:1px solid #1a1a1a;
  color:#444;font-size:clamp(9px,1.5vw,11px)
}
.wifi-info span{color:#666}
</style>
</head>
<body>

<div class="header">
  <div class="header-left">
    <span class="status-dot hdmi" id="status-dot"></span>
    <span id="source-label">HDMI</span>
  </div>
  <a class="header-brand" href="https://www.vid-prot.de" target="_blank" rel="noopener">VID-PRO</a>
  <div class="header-right">
    <span class="fps-badge" id="fps-badge">25 fps</span>
  </div>
</div>

<div class="tc-area">
  <div class="tc-wrap">
    <div class="tc-label">Timecode</div>
    <div id="tc-display" class="hdmi">00:00:00:00:00</div>
  </div>
  <div class="footer">
    <div class="settings-btn" id="settings-btn" onclick="toggleSettings()">&#9881;</div>
  </div>
</div>

<div class="settings-panel" id="settings-panel">
  <div class="panel-close" onclick="toggleSettings()">&#10005;</div>
  <div class="settings-title">Generator Settings</div>

  <div class="setting-row"> 
    <span class="setting-label">Frame Rate</span>
    <div class="fps-group" id="fps-group">
      <button class="fps-btn" data-fps="0">Auto</button>
      <button class="fps-btn" data-fps="24">24</button>
      <button class="fps-btn active" data-fps="25">25</button>
      <button class="fps-btn" data-fps="30">30</button>
      <button class="fps-btn" data-fps="50">50</button>
      <button class="fps-btn" data-fps="60">60</button>
    </div>
  </div>

  <div class="setting-row">
    <span class="setting-label">Drop Frame</span>
    <label class="toggle-track">
      <input type="checkbox" id="df-toggle">
      <span class="toggle-switch"></span>
      <span class="toggle-label" id="df-label">Off</span>
    </label>
  </div>

  <div class="setting-row">
    <span class="setting-label">Brightness</span>
    <div class="brightness-row">
      <input type="range" min="0" max="15" value="4" id="brightness-slider">
      <span class="brightness-val" id="brightness-val">4</span>
    </div>
  </div>

  <div class="setting-row">
    <span class="setting-label">Matrix</span>
    <label class="toggle-track">
      <input type="checkbox" id="matrix-toggle" checked>
      <span class="toggle-switch"></span>
      <span class="toggle-label" id="matrix-label">On</span>
    </label>
  </div>

)rawliteral");
#ifndef BLE_CLAP
    html += F(R"rawliteral(
  <div class="setting-row">
    <span class="setting-label">OLED</span>
    <label class="toggle-track">
      <input type="checkbox" id="oled-toggle" checked>
      <span class="toggle-switch"></span>
      <span class="toggle-label" id="oled-label">On</span>
    </label>
  </div>

  <div class="setting-row">
    <span class="setting-label">LTC Out</span>
    <label class="toggle-track">
      <input type="checkbox" id="ltc-toggle" checked>
      <span class="toggle-switch"></span>
      <span class="toggle-label" id="ltc-label">On</span>
    </label>
  </div>
)rawliteral");
#endif
    html += F(R"rawliteral(
  <div class="setting-row">
    <span class="setting-label">Jam Time</span>
    <div class="jam-row">
      <input type="number" min="0" max="99" id="jam-dd" value="0">
      <span class="jam-sep">:</span>
      <input type="number" min="0" max="23" id="jam-hh" value="0">
      <span class="jam-sep">:</span>
      <input type="number" min="0" max="59" id="jam-mm" value="0">
      <span class="jam-sep">:</span>
      <input type="number" min="0" max="59" id="jam-ss" value="0">
      <span class="jam-sep">:</span>
      <input type="number" min="0" max="99" id="jam-ff" value="0">
      <button class="jam-btn" onclick="jamTime()">Set</button>
    </div>
  </div>

  <!-- WiFi Config -->
  <div class="wifi-section">
    <div class="settings-title">WiFi Network</div>
    <div class="wifi-status">
      STA: <span id="wifi-ssid">—</span>
      &nbsp;|&nbsp; <span id="wifi-ip">—</span>
      &nbsp;|&nbsp; RSSI <span id="wifi-rssi">0</span>
    </div>
    <div class="wifi-form">
      <input type="text" id="wifi-ssid-input" placeholder="Network name (SSID)">
      <div class="row">
        <input type="password" id="wifi-pass-input" placeholder="Password">
        <button class="wifi-connect-btn" onclick="wifiConnect()">Connect</button>
      </div>
      <div class="row">
        <button class="wifi-forget-btn" onclick="wifiForget()">Forget</button>
        <div class="wifi-msg" id="wifi-msg"></div>
      </div>
    </div>
  </div>

  <div class="wifi-info">
    AP <span>__AP_SSID__</span> &nbsp;|&nbsp; <span>__AP_IP__</span><br>
    STA <span>__STA_SSID__</span> &nbsp;|&nbsp; <span>__STA_IP__</span>
  </div>

  <button class="restart-btn" onclick="restartEsp()">Restart Device</button>
)rawliteral");
#if BLE_MASTER
    html += F(R"rawliteral(
  <div class="ble-drawer-section" id="ble-section-master">
    <div class="settings-title">BLE Setup &mdash; Master</div>
    <div class="ble-form">
      <div class="row">
        <input type="text" id="ble-name-input" value="" maxlength="32">
        <button onclick="bleSetName()">Save Name</button>
        <button class="wifi-forget-btn" onclick="bleDisconnectAll()">Disconnect All</button>
      </div>
      <div id="ble-peers"></div>
      <div class="ble-msg" id="ble-msg-master"></div>
    </div>
  </div>
)rawliteral");
#elif BLE_SLAVE
    html += F(R"rawliteral(
  <div class="ble-drawer-section" id="ble-section-slave">
    <div class="settings-title">BLE Setup &mdash; Slave</div>
    <div class="ble-form">
      <div class="row">
        <input type="text" id="ble-slave-name-input" value="" maxlength="32">
        <button onclick="bleSetName()">Save Name</button>
      </div>
    </div>
    <div class="ble-status">
      Status: <span id="ble-status">—</span>
    </div>
    <div class="ble-form">
      <button onclick="bleScan()" id="ble-scan-btn">Scan</button>
      <div class="ble-msg" id="ble-msg-slave"></div>
    </div>
    <div id="ble-results"></div>
  </div>
)rawliteral");
#endif

    // Part 3: close settings-panel and common JS
    html += String(F(R"rawliteral(</div>

<script>
(function(){
  // ── DOM refs ──
  var tcEl=document.getElementById('tc-display');
  var dotEl=document.getElementById('status-dot');
  var srcEl=document.getElementById('source-label');
  var fpsEl=document.getElementById('fps-badge');
  var dfChk=document.getElementById('df-toggle');
  var dfLbl=document.getElementById('df-label');
  var matrixToggle=document.getElementById('matrix-toggle');
  var matrixLabel=document.getElementById('matrix-label');
)rawliteral"));
#ifndef BLE_CLAP
    html += String(F(R"rawliteral(
  var oledToggle=document.getElementById('oled-toggle');
  var oledLabel=document.getElementById('oled-label');
  var ltcToggle=document.getElementById('ltc-toggle');
  var ltcLabel=document.getElementById('ltc-label');
)rawliteral"));
#endif
    html += String(F(R"rawliteral(
  // WiFi DOM refs
  var wifiSsidEl=document.getElementById('wifi-ssid');
  var wifiIpEl=document.getElementById('wifi-ip');
  var wifiRssiEl=document.getElementById('wifi-rssi');

  // ── poll /api/tc every ~100 ms ──
  function pollTc(){
    var x=new XMLHttpRequest();
    x.open('GET','/api/tc',true);
    x.onload=function(){
      if(x.status!==200)return;
      try{
        var d=JSON.parse(x.responseText);
        tcEl.textContent=d.tc;
        fpsEl.textContent=d.auto?'AUTO ('+d.fps+' fps)':d.fps+' fps';
        tcEl.className=d.source.toLowerCase();
        dotEl.className='status-dot '+d.source.toLowerCase();
        srcEl.textContent=d.source;
        document.querySelectorAll('#fps-group .fps-btn').forEach(function(b){
          b.classList.toggle('active',
            (d.auto && parseInt(b.dataset.fps)===0) ||
            (!d.auto && parseInt(b.dataset.fps)===d.fps));
        });
        if(dfChk.checked!==d.df){
          dfChk.checked=d.df;
          dfLbl.textContent=d.df?'On':'Off';
        }
        if(matrixToggle.checked!==d.matrix){
          matrixToggle.checked=d.matrix;
          matrixLabel.textContent=d.matrix?'On':'Off';
        }
        if(oledToggle && oledToggle.checked!==d.oled){
          oledToggle.checked=d.oled;
          oledLabel.textContent=d.oled?'On':'Off';
        }
        if(ltcToggle && ltcToggle.checked!==d.ltc){
          ltcToggle.checked=d.ltc;
          ltcLabel.textContent=d.ltc?'On':'Off';
        }
      }catch(e){}
    };
    x.send();
  }
  setInterval(pollTc,100);
  pollTc();

  // ── poll /api/wifi every 2 seconds ──
  function pollWifi(){
    var x=new XMLHttpRequest();
    x.open('GET','/api/wifi',true);
    x.onload=function(){
      if(x.status!==200)return;
      try{
        var d=JSON.parse(x.responseText);
        wifiSsidEl.textContent=d.ssid||'—';
        wifiIpEl.textContent=d.ip||'—';
        wifiRssiEl.textContent=d.rssi||'0';
      }catch(e){}
    };
    x.send();
  }
  setInterval(pollWifi,2000);
  pollWifi();

  // ── Brightness slider ──
  var brightnessSlider=document.getElementById('brightness-slider');
  var brightnessVal=document.getElementById('brightness-val');
  function loadBrightness(){
    var x=new XMLHttpRequest();
    x.open('GET','/api/brightness',true);
    x.onload=function(){
      if(x.status!==200)return;
      try{
        var d=JSON.parse(x.responseText);
        brightnessSlider.value=d.val;
        brightnessVal.textContent=d.val;
      }catch(e){}
    };
    x.send();
  }
  loadBrightness();
  var brightnessPending=null;
  brightnessSlider.addEventListener('input',function(){
    brightnessVal.textContent=this.value;
    if(brightnessPending)clearTimeout(brightnessPending);
    var v=this.value;
    brightnessPending=setTimeout(function(){
      var x=new XMLHttpRequest();
      x.open('POST','/api/brightness',true);
      x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
      x.send('val='+v);
    },50);
  });
  brightnessSlider.addEventListener('change',function(){
    if(brightnessPending)clearTimeout(brightnessPending);
    var x=new XMLHttpRequest();
    x.open('POST','/api/brightness',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.send('val='+this.value);
  });

  // ── Matrix toggle ──
  matrixToggle.addEventListener('change',function(){
    var en=this.checked?1:0;
    matrixLabel.textContent=this.checked?'On':'Off';
    var x=new XMLHttpRequest();
    x.open('POST','/api/matrix',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.send('en='+en);
  });

  // ── OLED toggle ──
  if(typeof oledToggle !== 'undefined') oledToggle.addEventListener('change',function(){
    var en=this.checked?1:0;
    oledLabel.textContent=this.checked?'On':'Off';
    var x=new XMLHttpRequest();
    x.open('POST','/api/oled',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.send('en='+en);
  });

  // ── LTC Out toggle ──
  if(typeof ltcToggle !== 'undefined') ltcToggle.addEventListener('change',function(){
    var en=this.checked?1:0;
    ltcLabel.textContent=this.checked?'On':'Off';
    var x=new XMLHttpRequest();
    x.open('POST','/api/ltc',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.send('en='+en);
  });

  // ── Restart button ──
  window.restartEsp=function(){
    if(!confirm('Restart device?'))return;
    var x=new XMLHttpRequest();
    x.open('POST','/api/restart',true);
    x.send();
  };

  // ── FPS buttons ──
  document.querySelectorAll('#fps-group .fps-btn').forEach(function(btn){
    btn.addEventListener('click',function(){
      var fps=parseInt(this.dataset.fps);
      var df=dfChk.checked?1:0;
      var x=new XMLHttpRequest();
      x.open('POST','/api/config',true);
      x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
      x.onload=function(){if(x.status===200)pollTc();};
      x.send('fps='+fps+'&df='+df);
    });
  });

  // ── Drop frame toggle ──
  dfChk.addEventListener('change',function(){
    var active=document.querySelector('#fps-group .fps-btn.active');
    var fps=active?parseInt(active.dataset.fps):25;
    var df=this.checked?1:0;
    dfLbl.textContent=this.checked?'On':'Off';
    var x=new XMLHttpRequest();
    x.open('POST','/api/config',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.send('fps='+fps+'&df='+df);
  });

  // ── Jam input helpers ──
  ['jam-dd','jam-hh','jam-mm','jam-ss','jam-ff'].forEach(function(id){
    var el=document.getElementById(id);
    el.addEventListener('input',function(){
      var min=parseInt(this.min),max=parseInt(this.max);
      var v=parseInt(this.value);
      if(!isNaN(v)){
        if(v<min)this.value=min;
        if(v>max)this.value=max;
      }
    });
  });
})();

function toggleSettings(){
  var p=document.getElementById('settings-panel');
  var b=document.getElementById('settings-btn');
  p.classList.toggle('open');
  b.classList.toggle('active');
}

function jamTime(){
  var dd=parseInt(document.getElementById('jam-dd').value)||0;
  var hh=parseInt(document.getElementById('jam-hh').value)||0;
  var mm=parseInt(document.getElementById('jam-mm').value)||0;
  var ss=parseInt(document.getElementById('jam-ss').value)||0;
  var ff=parseInt(document.getElementById('jam-ff').value)||0;
  var x=new XMLHttpRequest();
  x.open('POST','/api/jam',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){if(x.status===200)pollTc();};
  x.send('dd='+dd+'&hh='+hh+'&mm='+mm+'&ss='+ss+'&ff='+ff);
}

function wifiConnect(){
  var ssid=document.getElementById('wifi-ssid-input').value.trim();
  var pass=document.getElementById('wifi-pass-input').value;
  var msg=document.getElementById('wifi-msg');
  if(!ssid){msg.textContent='Enter a network name';msg.className='wifi-msg err';return;}
  msg.textContent='Connecting...';msg.className='wifi-msg ok';
  var x=new XMLHttpRequest();
  x.open('POST','/api/wifi',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){
    if(x.status===200){
      msg.textContent='Saved — connecting...';msg.className='wifi-msg ok';
      setTimeout(pollWifi,500);
    }else{
      try{
        var d=JSON.parse(x.responseText);
        msg.textContent=d.error||'Failed';
      }catch(e){msg.textContent='Request failed';}
      msg.className='wifi-msg err';
    }
  };
  x.onerror=function(){msg.textContent='No response from device';msg.className='wifi-msg err';};
  x.send('ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass));
}

function wifiForget(){
  var msg=document.getElementById('wifi-msg');
  var x=new XMLHttpRequest();
  x.open('POST','/api/wifi',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){
    if(x.status===200){
      msg.textContent='Credentials cleared';msg.className='wifi-msg ok';
      document.getElementById('wifi-ssid-input').value='';
      document.getElementById('wifi-pass-input').value='';
      setTimeout(pollWifi,500);
    }else{msg.textContent='Failed to clear';msg.className='wifi-msg err';}
  };
  x.onerror=function(){msg.textContent='No response';msg.className='wifi-msg err';};
  x.send('ssid=&password=');
}
)rawliteral"));

    // Part 4: BLE JS (unified for all builds)
    html += String(F(R"rawliteral(
// ===== BLE functions =====
function bleSetName(){
  var el=document.getElementById('ble-name-input')||document.getElementById('ble-slave-name-input');
  var msgEl=document.getElementById('ble-msg-master')||document.getElementById('ble-msg-slave');
  var n=el?el.value:'';
  if(!n)return;
  var x=new XMLHttpRequest();
  x.open('POST','/api/ble',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){
    if(x.status==200){
      var d=JSON.parse(x.responseText);
      if(d.reboot){
        if(msgEl)msgEl.textContent='Saved. Rebooting...';
        setTimeout(function(){location.reload()},2000);
      } else if(msgEl)msgEl.textContent='OK';
    } else {
      try{if(msgEl)msgEl.textContent=JSON.parse(x.responseText).error}
      catch(e){if(msgEl)msgEl.textContent='Error'}
    }
  };
  x.send('action=setname&name='+encodeURIComponent(n));
}

function bleDisconnectAll(){
  var msgEl=document.getElementById('ble-msg-master');
  var x=new XMLHttpRequest();
  x.open('POST','/api/ble',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){if(x.status==200){
    if(msgEl)msgEl.textContent='Disconnected all';
    pollBleMaster();
  }};
  x.send('action=disconnect');
}

function bleDisconnectPeer(addr){
  var msgEl=document.getElementById('ble-msg-master');
  var x=new XMLHttpRequest();
  x.open('POST','/api/ble',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){if(x.status==200){
    if(msgEl)msgEl.textContent='Disconnected '+addr;
    pollBleMaster();
  }};
  x.send('action=disconnect_peer&address='+encodeURIComponent(addr));
}

function pollBleMaster(){
  var el=document.getElementById('ble-peers');
  if(!el)return;
  var x=new XMLHttpRequest();
  x.open('GET','/api/ble',true);
  x.onload=function(){
    if(x.status!=200)return;
    try{var d=JSON.parse(x.responseText);
    var h='';
    if(d.peers&&d.peers.length){
      h='<div class="ble-subtitle">Connected Peers</div>';
      for(var i=0;i<d.peers.length;i++){
        h+='<div class="ble-peer">';
        h+='<span>'+(d.peers[i].name||d.peers[i].addr)+'</span>';
        h+='<button onclick="bleDisconnectPeer(\''+d.peers[i].addr+'\')">Disconnect</button>';
        h+='</div>';
      }
    } else {
      h='<div class="ble-subtitle" style="color:#444">No slaves connected</div>';
    }
    el.innerHTML=h;
    }catch(e){}
  };
  x.send();
}

function bleScan(){
  var btn=document.getElementById('ble-scan-btn');
  var msgEl=document.getElementById('ble-msg-slave');
  var resultsEl=document.getElementById('ble-results');
  if(btn){btn.disabled=true;btn.textContent='Scanning...';}
  if(msgEl)msgEl.textContent='';
  if(resultsEl)resultsEl.innerHTML='';
  var x=new XMLHttpRequest();
  x.open('POST','/api/ble',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){
    if(btn){btn.disabled=false;btn.textContent='Scan';}
    if(x.status!=200)return;
    var d=JSON.parse(x.responseText);
    var h='';
    for(var i=0;i<d.count;i++){
      h+='<div class="ble-device">';
      h+='<span>'+d.devices[i].name+'</span>';
      h+='<button onclick="bleSelect(\''+d.devices[i].address+'\')">Connect</button>';
      h+='</div>';
    }
    if(resultsEl)resultsEl.innerHTML=h||'<div class="ble-device">No masters found</div>';
    pollBleSlave();
  };
  x.send('action=scan');
}

function bleSelect(addr){
  var msgEl=document.getElementById('ble-msg-slave');
  var x=new XMLHttpRequest();
  x.open('POST','/api/ble',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){if(x.status==200){
    if(msgEl)msgEl.textContent='Connecting to '+addr+'...';
    pollBleSlave();
    setTimeout(function(){location.reload()},3000);
  }};
  x.send('action=select&address='+encodeURIComponent(addr));
}

function pollBleSlave(){
  var el=document.getElementById('ble-status');
  var nameEl=document.getElementById('ble-slave-name-input');
  if(!el)return;
  var x=new XMLHttpRequest();
  x.open('GET','/api/ble',true);
  x.onload=function(){
    if(x.status!=200)return;
    try{var d=JSON.parse(x.responseText);
    el.textContent=d.connected?'Connected ('+d.connected_name+')':'Disconnected';
    if(nameEl&&d.name)nameEl.value=d.name;
    }catch(e){}
  };
  x.send();
}

// ===== Polling =====
)rawliteral"));
#if BLE_MASTER
  html += F("setInterval(pollBleMaster,3000);\n");
#elif BLE_SLAVE
  html += F("setInterval(pollBleSlave,3000);\n");
#endif
html += String(F(R"rawliteral(
// Load initial BLE name and status
pollBleSlave();
)rawliteral"));

    // Part 5: closing tags
    html += String(F(R"rawliteral(</script>
</body>
</html>)rawliteral"));
    return html;
}
