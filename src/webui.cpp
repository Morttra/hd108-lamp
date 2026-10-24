#include "webui.h"

#include <WebServer.h>

#include "config.h"

namespace webui {
namespace {

WebServer server(80);
Api g_api;

const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HD108 Lamp</title>
<style>
body{font-family:system-ui,sans-serif;background:#141414;color:#eee;margin:0 auto;padding:24px;max-width:480px}
h1{font-size:1.3rem;font-weight:600}
button{display:block;width:100%;padding:12px;margin:6px 0;font-size:1rem;border-radius:10px;border:1px solid #444;background:#222;color:#eee;cursor:pointer}
button.on{background:#2a6df4;border-color:#2a6df4}
.row{display:flex;gap:8px;align-items:center}
.row button{flex:1;width:auto;margin:0;padding:10px 6px}
.row label{flex:1;display:flex;align-items:center;justify-content:space-between;gap:8px}
input[type=range]{width:100%}
input[type=color]{width:52px;height:34px;border:1px solid #444;border-radius:8px;background:#222;padding:2px}
input[type=number]{padding:10px;border-radius:10px;border:1px solid #444;background:#222;color:#eee}
p{margin:14px 0 6px}
</style></head><body>
<h1>HD108 Lamp</h1>
<div id="modes"></div>
<p id="gainl">Brightness</p>
<div class="row">
<input type="range" id="gain" min="0" max="31" value="10" style="flex:1">
<input type="color" id="color" title="Solid color">
</div>
<p>Reading light</p>
<button id="ron">Reading light</button>
<p id="rposl">Position</p>
<input type="range" id="rpos" min="0" max="339" value="0">
<p id="rgainl">Reading brightness</p>
<input type="range" id="rgain" min="1" max="31" value="24">
<p>Aurora palette</p>
<div class="row" id="pals"></div>
<p id="aspdl">Aurora speed</p>
<input type="range" id="aspd" min="10" max="300" step="10" value="100">
<p>Horror colors</p>
<div class="row">
<label>Flashes <input type="color" id="hstrobe"></label>
<label>Background <input type="color" id="hbg"></label>
</div>
<p>Dinner timer (minutes)</p>
<div class="row">
<input type="number" id="tmin" min="1" max="720" value="15" style="flex:1">
<button id="tstart">Start timer</button>
</div>
<p>Metronome (BPM)</p>
<div class="row">
<input type="number" id="bpm" min="30" max="240" value="120" style="width:90px">
<input type="range" id="bpmr" min="30" max="240" value="120" style="flex:1">
</div>
<button id="bell" style="margin-top:18px">&#128276; Test doorbell alert</button>
<script>
const $=id=>document.getElementById(id);
const modes=$('modes'),gain=$('gain'),gainl=$('gainl'),color=$('color');
const pals=$('pals'),aspd=$('aspd'),aspdl=$('aspdl');
const hstrobe=$('hstrobe'),hbg=$('hbg');
const ron=$('ron'),rpos=$('rpos'),rposl=$('rposl'),rgain=$('rgain'),rgainl=$('rgainl');
const tmin=$('tmin'),bpm=$('bpm'),bpmr=$('bpmr');
async function st(){render(await(await fetch('/api/status')).json())}
function buttons(host,names,active,url){
  host.innerHTML='';
  names.forEach((n,i)=>{
    const b=document.createElement('button');
    b.textContent=n;
    if(i===active)b.className='on';
    b.onclick=async()=>{await fetch(url+i);st()};
    host.appendChild(b);
  });
}
function render(s){
  buttons(modes,s.effects,s.mode,'/api/mode?i=');
  gainl.textContent=`Brightness ${s.gain}/31`;
  gain.value=s.gain;
  color.value=s.color;
  buttons(pals,s.aurora.palettes,s.aurora.palette,'/api/aurora?palette=');
  aspd.value=s.aurora.speed;
  aspdl.textContent=`Aurora speed ${s.aurora.speed}%`;
  hstrobe.value=s.horror.strobe;
  hbg.value=s.horror.bg;
  ron.className=s.reading.on?'on':'';
  rpos.max=s.leds-1;rpos.value=s.reading.pos;
  rposl.textContent=`Position ${s.reading.pos}`;
  rgain.value=s.reading.gain;
  rgainl.textContent=`Reading brightness ${s.reading.gain}/31`;
  if(s.timer)tmin.value=s.timer;
  if(s.bpm){bpm.value=s.bpm;bpmr.value=s.bpm}
}
const hex=el=>el.value.slice(1);
gain.onchange=async()=>{await fetch('/api/gain?v='+gain.value);st()};
color.onchange=async()=>{await fetch('/api/color?hex='+hex(color));st()};
aspd.oninput=()=>{aspdl.textContent=`Aurora speed ${aspd.value}%`};
aspd.onchange=async()=>{await fetch('/api/aurora?speed='+aspd.value);st()};
hstrobe.onchange=async()=>{await fetch('/api/horror?strobe='+hex(hstrobe));st()};
hbg.onchange=async()=>{await fetch('/api/horror?bg='+hex(hbg));st()};
ron.onclick=async()=>{await fetch('/api/reading?on='+(ron.className?0:1));st()};
// Aim live while dragging (at most every 100 ms, not saved), save on release.
let aimT=null,aimV=null;
function aim(v){aimV=v;if(aimT)return;aimT=setTimeout(()=>{aimT=null;fetch('/api/reading?live=1&pos='+aimV)},100)}
rpos.oninput=()=>{rposl.textContent=`Position ${rpos.value}`;aim(rpos.value)};
rpos.onchange=async()=>{await fetch('/api/reading?pos='+rpos.value);st()};
rgain.oninput=()=>{rgainl.textContent=`Reading brightness ${rgain.value}/31`};
rgain.onchange=async()=>{await fetch('/api/reading?gain='+rgain.value);st()};
$('tstart').onclick=async()=>{await fetch('/api/timer?min='+tmin.value);st()};
const sendBpm=async v=>{await fetch('/api/bpm?v='+v);st()};
bpm.onchange=()=>sendBpm(bpm.value);
bpmr.oninput=()=>{bpm.value=bpmr.value};
bpmr.onchange=()=>sendBpm(bpmr.value);
$('bell').onclick=()=>{fetch('/api/bell')};
st();
</script></body></html>
)HTML";

String hexColor(uint32_t rgb) {
  char hex[8];
  snprintf(hex, sizeof(hex), "#%06x", (unsigned)(rgb & 0xFFFFFF));
  return String(hex);
}

// Parses exactly six hex digits (no '#') into 0xRRGGBB.
bool parseHex(const String& s, uint32_t& rgb) {
  if (s.length() != 6) return false;
  char* end = nullptr;
  rgb = (uint32_t)strtoul(s.c_str(), &end, 16);
  return end != nullptr && *end == '\0';
}

void appendNames(String& json, size_t n,
                 const std::function<const char*(size_t)>& name) {
  for (size_t i = 0; i < n; ++i) {
    if (i) json += ',';
    json += '"';
    json += name(i);
    json += '"';
  }
}

void sendStatus() {
  String json = "{\"timer\":" + String(g_api.timerMinutes()) +
                ",\"bpm\":" + String(g_api.bpm()) +
                ",\"mode\":" + String(g_api.mode()) +
                ",\"gain\":" + String(g_api.gain()) + ",\"color\":\"" +
                hexColor(g_api.solidColor()) + "\",\"effects\":[";
  appendNames(json, g_api.effectCount(), g_api.effectName);
  json += "],\"aurora\":{\"palette\":" + String(g_api.auroraPalette()) +
          ",\"speed\":" + String(g_api.auroraSpeed()) + ",\"palettes\":[";
  appendNames(json, g_api.auroraPaletteCount(), g_api.auroraPaletteName);
  json += "]},\"horror\":{\"strobe\":\"" + hexColor(g_api.horrorStrobe()) +
          "\",\"bg\":\"" + hexColor(g_api.horrorBackground()) + "\"}";
  json += ",\"leds\":" + String(LED_COUNT) +
          ",\"reading\":{\"on\":" + (g_api.readingOn() ? "true" : "false") +
          ",\"pos\":" + String(g_api.readingPosition()) +
          ",\"gain\":" + String(g_api.readingGain()) + "}}";
  server.send(200, "application/json", json);
}

void handleMode() {
  if (!server.hasArg("i")) {
    server.send(400, "application/json", "{\"error\":\"missing i\"}");
    return;
  }
  const long i = server.arg("i").toInt();
  if (i < 0 || (size_t)i >= g_api.effectCount()) {
    server.send(400, "application/json", "{\"error\":\"bad mode\"}");
    return;
  }
  g_api.setMode((size_t)i);
  sendStatus();
}

void handleGain() {
  if (!server.hasArg("v")) {
    server.send(400, "application/json", "{\"error\":\"missing v\"}");
    return;
  }
  long v = server.arg("v").toInt();
  if (v < 0) v = 0;
  if (v > 31) v = 31;
  g_api.setGain((uint8_t)v);
  sendStatus();
}

void handleColor() {
  uint32_t rgb;
  if (!server.hasArg("hex") || !parseHex(server.arg("hex"), rgb)) {
    server.send(400, "application/json", "{\"error\":\"bad hex\"}");
    return;
  }
  g_api.setColor((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
  sendStatus();
}

void handleBpm() {
  if (!server.hasArg("v")) {
    server.send(400, "application/json", "{\"error\":\"missing v\"}");
    return;
  }
  long v = server.arg("v").toInt();
  if (v < 30) v = 30;
  if (v > 240) v = 240;
  g_api.setBpm((uint16_t)v);
  sendStatus();
}

void handleAurora() {
  bool any = false;
  if (server.hasArg("palette")) {
    const long p = server.arg("palette").toInt();
    if (p < 0 || (size_t)p >= g_api.auroraPaletteCount()) {
      server.send(400, "application/json", "{\"error\":\"bad palette\"}");
      return;
    }
    g_api.setAuroraPalette((size_t)p);
    any = true;
  }
  if (server.hasArg("speed")) {
    long v = server.arg("speed").toInt();
    if (v < 10) v = 10;
    if (v > 300) v = 300;
    g_api.setAuroraSpeed((uint16_t)v);
    any = true;
  }
  if (!any) {
    server.send(400, "application/json",
                "{\"error\":\"missing palette or speed\"}");
    return;
  }
  sendStatus();
}

void handleHorror() {
  bool any = false;
  uint32_t rgb;
  if (server.hasArg("strobe")) {
    if (!parseHex(server.arg("strobe"), rgb)) {
      server.send(400, "application/json", "{\"error\":\"bad strobe\"}");
      return;
    }
    g_api.setHorrorStrobe(rgb);
    any = true;
  }
  if (server.hasArg("bg")) {
    if (!parseHex(server.arg("bg"), rgb)) {
      server.send(400, "application/json", "{\"error\":\"bad bg\"}");
      return;
    }
    g_api.setHorrorBackground(rgb);
    any = true;
  }
  if (!any) {
    server.send(400, "application/json",
                "{\"error\":\"missing strobe or bg\"}");
    return;
  }
  sendStatus();
}

void handleReading() {
  bool any = false;
  if (server.hasArg("pos")) {
    const long p = server.arg("pos").toInt();
    if (p < 0 || p >= LED_COUNT) {
      server.send(400, "application/json", "{\"error\":\"bad pos\"}");
      return;
    }
    g_api.setReadingPosition((uint16_t)p, !server.hasArg("live"));
    any = true;
  }
  if (server.hasArg("gain")) {
    long g = server.arg("gain").toInt();
    if (g < 1) g = 1;
    if (g > 31) g = 31;
    g_api.setReadingGain((uint8_t)g);
    any = true;
  }
  if (server.hasArg("on")) {  // last, so an explicit on/off wins
    g_api.setReadingOn(server.arg("on").toInt() != 0);
    any = true;
  }
  if (!any) {
    server.send(400, "application/json",
                "{\"error\":\"missing on, pos or gain\"}");
    return;
  }
  sendStatus();
}

void handleTimer() {
  if (!server.hasArg("min")) {
    server.send(400, "application/json", "{\"error\":\"missing min\"}");
    return;
  }
  const long m = server.arg("min").toInt();
  if (m < 1 || m > 720) {
    server.send(400, "application/json", "{\"error\":\"bad min\"}");
    return;
  }
  g_api.startTimer((uint32_t)m);
  sendStatus();
}

}  // namespace

void begin(const Api& api) {
  g_api = api;
  server.on("/", []() { server.send_P(200, "text/html", kIndexHtml); });
  server.on("/api/status", sendStatus);
  server.on("/api/mode", handleMode);
  server.on("/api/gain", handleGain);
  server.on("/api/timer", handleTimer);
  server.on("/api/color", handleColor);
  server.on("/api/bpm", handleBpm);
  server.on("/api/aurora", handleAurora);
  server.on("/api/horror", handleHorror);
  server.on("/api/reading", handleReading);
  server.on("/api/bell", []() {
    g_api.doorbell();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
}

void handle() { server.handleClient(); }

}  // namespace webui
