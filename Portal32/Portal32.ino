#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

/**
 * Portal32 - ESP32 Configuration / Provisioning Portal
 * 
 * Features:
 * - AP Mode for initial setup (192.168.4.1)
 * - STA Mode for normal operation
 * - Captive Portal redirection
 * - Preferences (NVS) storage for credentials
 * - Responsive Web UI (HTML/CSS/JS)
 * - JSON API for scanning and status
 */

// Configuration
const char* AP_SSID = "Portal32";
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// Global Objects
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// State Variables
String ssid = "";
String password = "";
String device_name = "Portal32-Device";
bool isAPMode = false;
unsigned long lastStatusCheck = 0;

// --- HTML / CSS / JS UI ---

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Portal32 Setup</title>
    <style>
        :root {
            --primary: #007aff;
            --bg: #f5f5f7;
            --card: #ffffff;
            --text: #1d1d1f;
            --secondary: #86868b;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg);
            color: var(--text);
            margin: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            width: 90%;
            max-width: 400px;
            background: var(--card);
            padding: 2rem;
            border-radius: 18px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.05);
        }
        h1 { font-size: 24px; font-weight: 600; margin-bottom: 1.5rem; text-align: center; }
        .form-group { margin-bottom: 1.2rem; }
        label { display: block; font-size: 14px; color: var(--secondary); margin-bottom: 0.4rem; }
        input, select {
            width: 100%;
            padding: 12px;
            border: 1px solid #d2d2d7;
            border-radius: 8px;
            font-size: 16px;
            box-sizing: border-box;
            outline: none;
            transition: border-color 0.2s;
        }
        input:focus { border-color: var(--primary); }
        .btn {
            width: 100%;
            padding: 14px;
            background: var(--primary);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: 500;
            cursor: pointer;
            margin-top: 1rem;
        }
        .btn:disabled { background: #b1b1b1; }
        .status-msg { margin-top: 1rem; font-size: 14px; text-align: center; padding: 10px; border-radius: 8px; display: none; }
        .success { background: #e8f5e9; color: #2e7d32; }
        .error { background: #ffebee; color: #c62828; }
        .scan-btn { font-size: 12px; color: var(--primary); cursor: pointer; float: right; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Portal32</h1>
        <div class="form-group">
            <span class="scan-btn" onclick="scanWifi()">Refresh Scan</span>
            <label for="ssid">WiFi Network (SSID)</label>
            <select id="ssid_select" onchange="document.getElementById('ssid').value = this.value">
                <option value="">Scanning...</option>
            </select>
            <input type="hidden" id="ssid">
        </div>
        <div class="form-group">
            <label for="pass">Password</label>
            <input type="password" id="pass" placeholder="Enter WiFi Password">
        </div>
        <div class="form-group">
            <label for="name">Device Name</label>
            <input type="text" id="name" placeholder="Portal32-Device">
        </div>
        <button class="btn" id="saveBtn" onclick="saveConfig()">Save & Connect</button>
        <div id="status" class="status-msg"></div>
    </div>

    <script>
        async function scanWifi() {
            const select = document.getElementById('ssid_select');
            select.innerHTML = '<option>Scanning...</option>';
            try {
                const res = await fetch('/scan');
                const networks = await res.json();
                select.innerHTML = '<option value="">Select a network</option>';
                networks.forEach(net => {
                    const opt = document.createElement('option');
                    opt.value = net.ssid;
                    opt.textContent = `${net.ssid} (${net.rssi}dBm)`;
                    select.appendChild(opt);
                });
            } catch (e) {
                console.error("Scan failed", e);
            }
        }

        async function saveConfig() {
            const ssid = document.getElementById('ssid').value;
            const pass = document.getElementById('pass').value;
            const name = document.getElementById('name').value;
            const status = document.getElementById('status');
            const btn = document.getElementById('saveBtn');

            if (!ssid) { alert("Please select a WiFi network"); return; }

            status.style.display = 'block';
            status.textContent = "Saving...";
            status.className = "status-msg";
            btn.disabled = true;

            try {
                const formData = new URLSearchParams();
                formData.append('ssid', ssid);
                formData.append('password', pass);
                formData.append('device_name', name);

                const res = await fetch('/save', {
                    method: 'POST',
                    body: formData
                });

                if (res.ok) {
                    status.textContent = "Success! Rebooting ESP32. Please reconnect to your network.";
                    status.className = "status-msg success";
                } else {
                    throw new Error("Save failed");
                }
            } catch (e) {
                status.textContent = "Error saving settings.";
                status.className = "status-msg error";
                btn.disabled = false;
            }
        }

        window.onload = scanWifi;
    </script>
</body>
</html>
)=====";

// --- Function Prototypes ---
void setupAP();
void setupSTA();
void loadSettings();
void saveSettings(String s, String p, String n);
void handleRoot();
void handleScan();
void handleSave();
void handleStatus();
void handleNotFound();

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[SYSTEM] Portal32 Starting...");

    loadSettings();

    if (ssid.length() > 0) {
        Serial.printf("[WIFI] Attempting STA connection to: %s\n", ssid.c_str());
        setupSTA();
    } else {
        Serial.println("[WIFI] No credentials found. Starting AP...");
        setupAP();
    }
}

void loop() {
    if (isAPMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
    
    // Check connection status periodically if not in AP mode
    if (!isAPMode && WiFi.status() != WL_CONNECTED && millis() - lastStatusCheck > 30000) {
        Serial.println("[WIFI] Connection lost. Re-evaluating...");
        if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
            setupAP();
        }
        lastStatusCheck = millis();
    }
    
    yield();
}

// --- Persistence ---

void loadSettings() {
    preferences.begin("portal32", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    device_name = preferences.getString("device_name", "Portal32-Device");
    preferences.end();
}

void saveSettings(String s, String p, String n) {
    preferences.begin("portal32", false);
    preferences.putString("ssid", s);
    preferences.putString("password", p);
    preferences.putString("device_name", n);
    preferences.end();
}

// --- WiFi Management ---

void setupAP() {
    isAPMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);

    Serial.print("[WIFI] AP Mode started. SSID: ");
    Serial.println(AP_SSID);
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Start DNS Server for Captive Portal
    dnsServer.start(DNS_PORT, "*", apIP);

    // Setup Web Server Routes
    server.on("/", handleRoot);
    server.on("/scan", handleScan);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/status", handleStatus);
    server.onNotFound(handleRoot); // Redirect anything else to root for captive portal
    server.begin();
    
    Serial.println("[HTTP] Web Server started");
}

void setupSTA() {
    isAPMode = false;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(device_name.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.print("[WIFI] Connecting");
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected!");
        Serial.print("[WIFI] IP Address: ");
        Serial.println(WiFi.localIP());
        
        if (MDNS.begin(device_name.c_str())) {
            Serial.println("[MDNS] Started: " + device_name + ".local");
        }

        server.on("/", handleRoot);
        server.on("/status", handleStatus);
        server.on("/scan", handleScan); // Allow re-scanning even in STA
        server.begin();
        Serial.println("[HTTP] Web Server started in STA mode");
    } else {
        Serial.println("\n[WIFI] Connection failed. Falling back to AP...");
        setupAP();
    }
}

// --- Handlers ---

void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void handleScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encrypted\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        json += "}";
        if (i < n - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleSave() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "Missing Parameters");
        return;
    }

    String s = server.arg("ssid");
    String p = server.arg("password");
    String n = server.arg("device_name");
    
    if (n.length() == 0) n = "Portal32-Device";

    Serial.println("[SYSTEM] New configuration received. Saving...");
    saveSettings(s, p, n);

    server.send(200, "application/json", "{\"status\":\"ok\"}");
    
    delay(1000);
    Serial.println("[SYSTEM] Rebooting...");
    ESP.restart();
}

void handleStatus() {
    String json = "{";
    json += "\"status\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "idle") + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"device\":\"" + device_name + "\"";
    json += "}";
    server.send(200, "application/json", json);
}
