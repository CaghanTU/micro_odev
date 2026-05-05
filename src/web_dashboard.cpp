#include "web_dashboard.h"

#include <WebServer.h>
#include <WiFi.h>

#include "logger.h"

namespace {
WebServer server(80);
SystemData *systemData = nullptr;

String webNotice;

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (uint16_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    switch (c) {
      case '&': escaped += F("&amp;"); break;
      case '<': escaped += F("&lt;"); break;
      case '>': escaped += F("&gt;"); break;
      case '"': escaped += F("&quot;"); break;
      case '\'': escaped += F("&#39;"); break;
      default: escaped += c; break;
    }
  }
  return escaped;
}

bool parseBoundedFloat(const String &raw, float minValue, float maxValue, float &outValue) {
  if (raw.length() == 0) {
    return false;
  }

  char *endPtr = nullptr;
  const float parsed = strtof(raw.c_str(), &endPtr);
  if (endPtr == raw.c_str() || *endPtr != '\0') {
    return false;
  }

  if (parsed < minValue || parsed > maxValue) {
    return false;
  }

  outValue = parsed;
  return true;
}

void redirectToRoot() {
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

String formatFloatOrDash(float value, const char *unit, bool valid) {
  if (!valid) {
    return String("--") + unit;
  }

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.1f%s", value, unit);
  return String(buffer);
}

void handleRoot() {
  if (systemData == nullptr) {
    server.send(503, "text/plain", "System data unavailable");
    return;
  }

  const SystemData &data = *systemData;

  String html;
  html.reserve(6500);
  html += F("<!doctype html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Calibration Cabinet</title>");
  html += F("<style>body{font-family:Arial,sans-serif;margin:24px;background:#f5f7fa;color:#18202a}");
  html += F("h1{font-size:24px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}");
  html += F(".card{background:white;border:1px solid #d8dee9;border-radius:8px;padding:14px}");
  html += F(".label{font-size:13px;color:#667085}.value{font-size:28px;font-weight:700;margin-top:4px}");
  html += F("table{width:100%;border-collapse:collapse;background:white}td,th{border-bottom:1px solid #e4e7ec;padding:8px;text-align:left}");
  html += F("input{box-sizing:border-box;width:100%;font-size:18px;padding:9px;margin:6px 0 10px;border:1px solid #b8c0cc;border-radius:6px}");
  html += F("button{font-size:16px;padding:10px 14px;border:0;border-radius:6px;background:#175cd3;color:white}");
  html += F(".alarm{color:#b42318}.notice{background:#ecfdf3;border:1px solid #abefc6;padding:10px;border-radius:6px}</style>");
  html += F("<script>setTimeout(function(){if(!document.querySelector('input:focus'))location.reload();},");
  html += String(WEB_REFRESH_INTERVAL_SECONDS * 1000UL);
  html += F(");</script></head><body>");
  html += F("<h1>Thermometer Calibration Cabinet</h1><div class='grid'>");

  html += F("<div class='card'><div class='label'>Temperature</div><div class='value'>");
  html += htmlEscape(formatFloatOrDash(data.currentTemperatureC, " C", data.hasValidSensorReading));
  html += F("</div></div>");

  html += F("<div class='card'><div class='label'>Humidity</div><div class='value'>");
  html += htmlEscape(formatFloatOrDash(data.currentHumidityRh, "%", data.hasValidSensorReading));
  html += F("</div></div>");

  html += F("<div class='card'><div class='label'>Temperature Setpoint</div><div class='value'>");
  html += String(data.temperatureSetpointC, 0);
  html += F(" C</div></div>");

  html += F("<div class='card'><div class='label'>Humidity Setpoint</div><div class='value'>");
  html += String(data.humiditySetpointRh, 0);
  html += F("%</div></div>");

  html += F("</div><div class='card' style='margin-top:12px'><div class='label'>State</div><div class='value ");
  html += data.alarmActive ? F("alarm") : F("");
  html += F("'>");
  html += htmlEscape(String(stateToString(data.state)));
  if (data.alarmActive) {
    html += F(" - ");
    html += htmlEscape(String(alarmReasonToString(data.alarmReason)));
  }
  html += F("</div><p>");
  html += htmlEscape(data.lastMessage);
  if (!data.controlEnabled && !data.alarmActive) {
    html += F("<br>Control waiting for setpoint input");
  }
  html += F("</p></div>");

  if (webNotice.length() > 0) {
    html += F("<div class='notice' style='margin-top:12px'>");
    html += htmlEscape(webNotice);
    html += F("</div>");
  }

  html += F("<div class='card' style='margin-top:12px'><h2>Setpoints</h2>");
  html += F("<form method='POST' action='/setpoints'>");
  html += F("<label class='label' for='temp'>Temperature setpoint (0-50 C)</label>");
  html += F("<input id='temp' name='temp' inputmode='decimal' type='number' min='");
  html += String(MIN_TEMPERATURE_SETPOINT_C, 0);
  html += F("' max='");
  html += String(MAX_TEMPERATURE_SETPOINT_C, 0);
  html += F("' step='1' value='");
  html += String(data.temperatureSetpointC, 0);
  html += F("'>");
  html += F("<label class='label' for='humidity'>Humidity setpoint (20-100%)</label>");
  html += F("<input id='humidity' name='humidity' inputmode='decimal' type='number' min='");
  html += String(MIN_HUMIDITY_SETPOINT_RH, 0);
  html += F("' max='");
  html += String(MAX_HUMIDITY_SETPOINT_RH, 0);
  html += F("' step='1' value='");
  html += String(data.humiditySetpointRh, 0);
  html += F("'>");
  html += F("<button type='submit'>Update Setpoints</button></form></div>");

  html += F("<h2>Last 10 Log Entries</h2><table><tr><th>Time</th><th>Message</th></tr>");
  const uint8_t count = loggerCount();
  for (uint8_t i = 0; i < count; i++) {
    const LogEntry entry = loggerGet(i);
    html += F("<tr><td>");
    html += htmlEscape(loggerFormatTimestamp(entry.secondsSinceBoot));
    html += F("</td><td>");
    html += htmlEscape(entry.message);
    html += F("</td></tr>");
  }
  html += F("</table></body></html>");

  server.send(200, "text/html", html);
}

void handleSetpoints() {
  if (systemData == nullptr) {
    server.send(503, "text/plain", "System data unavailable");
    return;
  }

  SystemData &data = *systemData;
  float newTemperature = data.temperatureSetpointC;
  float newHumidity = data.humiditySetpointRh;

  const bool tempOk = parseBoundedFloat(server.arg("temp"),
                                        MIN_TEMPERATURE_SETPOINT_C,
                                        MAX_TEMPERATURE_SETPOINT_C,
                                        newTemperature);
  const bool humidityOk = parseBoundedFloat(server.arg("humidity"),
                                            MIN_HUMIDITY_SETPOINT_RH,
                                            MAX_HUMIDITY_SETPOINT_RH,
                                            newHumidity);

  if (!tempOk || !humidityOk) {
    webNotice = "Invalid web setpoint rejected";
    loggerAdd(data, webNotice);
    redirectToRoot();
    return;
  }

  data.temperatureSetpointC = newTemperature;
  data.humiditySetpointRh = newHumidity;
  data.controlEnabled = true;
  webNotice = "Web setpoints updated";
  loggerAdd(data, "Web SP T:" + String(newTemperature, 0) +
                 "C H:" + String(newHumidity, 0) + "%");
  redirectToRoot();
}
}

void webDashboardInit(SystemData &data) {
  systemData = &data;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

  server.on("/", handleRoot);
  server.on("/setpoints", HTTP_POST, handleSetpoints);
  server.begin();

  IPAddress ip = WiFi.softAPIP();
  loggerAdd(data, "AP " + ip.toString());
}

void webDashboardHandle() {
  server.handleClient();
}
