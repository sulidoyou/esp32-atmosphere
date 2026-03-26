#include "bsp_http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "bsp.h"
#include "bsp_magent.h"
#include "audio_player.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const char *TAG = "HTTP";
static const char *OTA_FILE = "/tf/ota_update.bin";

// ============ OTA状态（防止重入）============
static volatile bool s_ota_uploading = false;
static volatile bool s_ota_flashing = false;

// ============ HTML ============
static const char s_html[] =
"<!DOCTYPE html>"
"<html lang=zh>"
"<head>"
"<meta charset=UTF-8>"
"<meta name=viewport content=width=device-width,initial-scale=1.0>"
"<title>ESP32 氛围系统</title>"
"<style>"
"* { margin:0; padding:0; box-sizing:border-box; }"
"body { font-family:Arial; background:#1a1a2e; color:#eee; padding:14px; }"
".c { max-width:540px; margin:0 auto; }"
"h1 { text-align:center; color:#00d4ff; margin-bottom:14px; font-size:20px; }"
".card { background:#16213e; border-radius:12px; padding:13px; margin-bottom:10px; }"
".card h2 { color:#00d4ff; font-size:13px; border-bottom:1px solid #0f3460; padding-bottom:5px; margin-bottom:8px; }"
".row { display:flex; justify-content:space-between; padding:3px 0; font-size:13px; }"
".lb { color:#888; }"
".lv { color:#0f0; font-weight:bold; font-size:12px; }"
".song { color:#00d4ff; font-size:12px; word-break:break-all; margin-top:3px; }"
".g { display:grid; grid-template-columns:1fr 1fr; gap:5px; margin-top:6px; }"
".g3 { display:grid; grid-template-columns:1fr 1fr 1fr; gap:5px; margin-top:6px; }"
"button { padding:8px 4px; border:none; border-radius:8px; font-size:13px; font-weight:bold; cursor:pointer; min-width:44px; }"
"button:active { transform:scale(0.94); }"
".on { background:linear-gradient(135deg,#00ff88,#00d4ff); color:#111; }"
".off { background:#2a2a4a; color:#555; }"
".purple { background:linear-gradient(135deg,#667eea,#764ba2); color:#fff; }"
".pink { background:linear-gradient(135deg,#f093fb,#f5576c); color:#fff; }"
".gray { background:#2a2a4a; color:#888; }"
".green { background:linear-gradient(135deg,#11998e,#38ef7d); color:#fff; }"
".active { box-shadow:0 0 10px rgba(0,212,255,0.5); }"
".vol { display:flex; align-items:center; gap:8px; margin-top:6px; }"
".vol input { flex:1; accent-color:#00d4ff; }"
".vol span { font-size:12px; color:#888; min-width:28px; text-align:right; }"
".ch-row { display:grid; grid-template-columns:1fr 1fr; gap:5px; margin-bottom:4px; }"
".ch-item { display:flex; align-items:center; gap:4px; }"
".ch-item label { font-size:11px; color:#888; min-width:24px; }"
".dur-input { flex:1; background:#0f3460; border:1px solid #1a4a80; border-radius:6px; color:#00d4ff; font-size:12px; padding:4px 6px; text-align:center; min-width:0; }"
".magnet-row { display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:5px; margin-top:8px; }"
".mbtn { width:100%; padding:10px 2px; border:none; border-radius:8px; font-size:12px; font-weight:bold; cursor:pointer; background:#2a2a4a; color:#888; transition:all .15s; }"
".mbtn.fired { background:linear-gradient(135deg,#ff5858,#f09819); color:#fff; transform:scale(0.94); }"
".footer { text-align:center; color:#333; font-size:10px; margin-top:8px; }"
".conn { display:inline-block; width:8px; height:8px; border-radius:50%; background:#555; margin-right:6px; }"
".conn.ok { background:#0f0; }"
".conn.bad { background:#f00; animation:blink .5s infinite; }"
"@keyframes blink { 0%,100%{opacity:1} 50%{opacity:0} }"
".ota-info { font-size:11px; color:#888; margin-bottom:6px; line-height:1.6; }"
".ota-btn { width:100%; padding:10px; background:linear-gradient(135deg,#e67e22,#f39c12); color:#fff; font-size:13px; font-weight:bold; border-radius:8px; border:none; cursor:pointer; }"
".ota-btn:disabled { background:#444; color:#888; cursor:not-allowed; }"
".ota-log { font-size:10px; color:#0f0; margin-top:6px; word-break:break-all; max-height:80px; overflow-y:auto; background:#0a0a1a; border-radius:4px; padding:6px; }"
".ota-progress { width:100%; height:6px; background:#0f3460; border-radius:3px; margin-top:6px; display:none; }"
".ota-progress-bar { height:100%; background:linear-gradient(90deg,#00ff88,#00d4ff); border-radius:3px; width:0%; transition:width .3s; }"
".ota-success { color:#0f0; font-size:12px; text-align:center; margin-top:6px; padding:6px; background:#0a2a0a; border-radius:6px; }"
".ota-error { color:#f55; font-size:12px; text-align:center; margin-top:6px; padding:6px; background:#2a0a0a; border-radius:6px; }"
".fw-btn { width:100%; padding:9px; background:linear-gradient(135deg,#11998e,#38ef7d); color:#fff; font-size:12px; font-weight:bold; border-radius:6px; border:none; cursor:pointer; margin-top:6px; }"
".fw-btn:disabled { background:#333; color:#666; cursor:not-allowed; }"
".fw-status { font-size:11px; color:#555; margin-top:4px; text-align:center; }"
".part-info { font-size:10px; color:#444; margin-top:2px; }"
".drum-row { display:grid; grid-template-columns:1fr 1fr 1fr 1fr 1fr; gap:4px; margin-top:6px; }"
".drum-btn { width:100%; padding:8px 2px; border:none; border-radius:8px; font-size:11px; font-weight:bold; cursor:pointer; background:#1a1a3a; color:#555; transition:all .15s; }"
".drum-btn.active { background:linear-gradient(135deg,#7b2ff7,#f107a3); color:#fff; box-shadow:0 0 12px rgba(123,47,247,0.4); }"
".drum-ctrl { display:flex; align-items:center; gap:6px; margin-top:5px; font-size:12px; color:#666; }"
".drum-ctrl input { flex:1; accent-color:#9b59b6; }"
".drum-ctrl span { min-width:32px; text-align:right; color:#888; }"
".drum-start { width:100%; padding:10px; background:linear-gradient(135deg,#9b59b6,#e74c3c); color:#fff; font-size:13px; font-weight:bold; border-radius:8px; border:none; cursor:pointer; margin-top:6px; }"
".drum-start.running { background:linear-gradient(135deg,#e74c3c,#c0392b); animation:dpulse .6s infinite alternate; }"
"@keyframes dpulse { from{box-shadow:0 0 8px rgba(231,76,60,.4)} to{box-shadow:0 0 20px rgba(231,76,60,.8)} }"
".drum-status { font-size:11px; color:#555; margin-top:4px; text-align:center; }"
"</style>"
"</head>"
"<body>"
"<div class=c>"
"<h1>ESP32 氛围系统</h1>"

"<div class=card><h2><span class=conn id=conn></span>系统状态</h2>"
"<div class=row><span class=lb>曲目</span><span class=lv id=cur>--/--</span></div>"
"<div class=row><span class=lb>状态</span><span class=lv id=sta>--</span></div>"
"<div class=row><span class=lb>固件</span><span class=lv id=fwver>-</span></div>"
"<div class=song id=song>-</div>"
"<div class=part-info id=partInfo>读取分区信息...</div>"
"</div>"

"<div class=card><h2>音乐控制</h2>"
"<div class=g3>"
"<button class=gray id=bprev onclick=doit('prev')>prev</button>"
"<button class=on id=bplay onclick=doit('play')>play</button>"
"<button class=off id=bpause onclick=doit('pause')>pause</button>"
"</div>"
"<div class=g style=margin-top:5px>"
"<button class=gray onclick=doit('next')>next</button>"
"<button class=gray onclick=doit('stop')>stop</button>"
"</div>"
"<div class=vol>"
"<span>vol</span>"
"<input type=range id=vol min=0 max=100 value=50 oninput=sv(this.value)>"
"<span id=vdis>50</span>"
"</div>"
"</div>"

"<div class=card><h2>Breathing LED</h2>"
"<div class=g>"
"<button class=purple id=b1 onclick=sl('breath')>Breath</button>"
"<button class=pink id=b2 onclick=sl('rainbow')>Rainbow</button>"
"</div>"
"</div>"

"<div class=card><h2>Magnet (4 Channels)</h2>"
"<div class=ch-row>"
"<div class=ch-item><label>CH1</label><input class=dur-input id=di0 value=100 type=number min=10 max=10000 placeholder=ms></div>"
"<div class=ch-item><label>CH2</label><input class=dur-input id=di1 value=100 type=number min=10 max=10000 placeholder=ms></div>"
"</div>"
"<div class=ch-row>"
"<div class=ch-item><label>CH3</label><input class=dur-input id=di2 value=100 type=number min=10 max=10000 placeholder=ms></div>"
"<div class=ch-item><label>CH4</label><input class=dur-input id=di3 value=100 type=number min=10 max=10000 placeholder=ms></div>"
"</div>"
"<div class=magnet-row>"
"<button class=mbtn id=mb0 onclick=fire(0)>CH1</button>"
"<button class=mbtn id=mb1 onclick=fire(1)>CH2</button>"
"<button class=mbtn id=mb2 onclick=fire(2)>CH3</button>"
"<button class=mbtn id=mb3 onclick=fire(3)>CH4</button>"
"</div>"
"</div>"

"<div class=card><h2>🥁 敲鼓控制</h2>"
"<div class=drum-row>"
"<button class=drum-btn id=dm0 onclick=setDrumMode(0)>关闭</button>"
"<button class=drum-btn id=dm1 onclick=setDrumMode(1)>预设</button>"
"<button class=drum-btn id=dm3 onclick=setDrumMode(3)>麦克风</button>"
"<button class=drum-btn id=dm4 onclick=setDrumMode(4)>音乐</button>"
"</div>"
"<div class=drum-row style='margin-top:4px;'>"
"<button class=drum-btn id=dr0 onclick=setDrumRhythm(0)>单击</button>"
"<button class=drum-btn id=dr1 onclick=setDrumRhythm(1)>双击</button>"
"<button class=drum-btn id=dr2 onclick=setDrumRhythm(2)>滚奏</button>"
"<button class=drum-btn id=dr3 onclick=setDrumRhythm(3)>华尔兹</button>"
"<button class=drum-btn id=dr4 onclick=setDrumRhythm(4)>摇滚</button>"
"</div>"
"<div class=drum-ctrl>"
"<span>BPM</span><input type=range id=drumBpm min=60 max=240 value=120 oninput=drumBpmVal.value=this.value><span id=drumBpmVal>120</span>"
"</div>"
"<div class=drum-ctrl>"
"<span>力度</span><input type=range id=drumVel min=10 max=100 value=70 oninput=drumVelVal.value=this.value><span id=drumVelVal>70</span>"
"</div>"
"<button class=drum-start id=drumStartBtn onclick=toggleDrum()>启动敲鼓</button>"
"<div class=drum-status id=drumStatus>状态: 停止 | 节拍: -- | 力度: --</div>"
"</div>"

"<div class=card><h2>OTA 固件更新</h2>"
"<div class=ota-info id=otaInfo>固件上传至SD卡，验证后安全烧录</div>"
"<input type=file id=fwFile accept='.bin' style=display:none onchange=fwSelect(this)>"
"<button class=ota-btn id=otaBtn onclick=document.getElementById('fwFile').click()>选择固件 (.bin) 上传到SD卡</button>"
"<div class=ota-progress id=otaProgress><div class=ota-progress-bar id=otaBar></div></div>"
"<div class=ota-log id=otaLog>等待选择固件文件...</div>"
"<div class=ota-success id=otaSuccess style=display:none></div>"
"<div class=ota-error id=otaError style=display:none></div>"
"<button class=fw-btn id=fwFlashBtn style=display:none onclick=doFlash()>烧录固件并重启</button>"
"<div class=fw-status id=fwStatus></div>"
"</div>"

"<div class=footer>ESP32-S3 <span id=ver style=color:#ff9500;font-weight:bold>" APP_VERSION_STR "</span></div>"
"</div>"
"<script>"
"var req=null;"
"var fwReady=false;"
"function doit(c){"
"  if(req)req.abort();"
"  req=new AbortController();"
"  fetch('/m?c='+c,{signal:req.signal}).then(function(r){return r.json()}).then(upd).catch(function(){});"
"}"
"function upd(d){"
"  document.getElementById('sta').textContent=d.sta;"
"  document.getElementById('cur').textContent=d.cur;"
"  document.getElementById('song').textContent=d.song;"
"  if(d.fw){var e=document.getElementById('fwver');if(e)e.textContent=d.fw;}"
"  var bp=document.getElementById('bplay'),bpa=document.getElementById('bpause');"
"  bp.style.display=d.sta=='playing'?'none':'inline-block';"
"  bpa.style.display=d.sta=='playing'?'inline-block':'none';"
"  document.getElementById('conn').className='conn ok';"
"}"
"function sv(v){"
"  document.getElementById('vdis').textContent=v;"
"  fetch('/v?n='+v).catch(function(){});"
"}"
"function sl(m){"
"  fetch('/l?m='+m).catch(function(){});"
"  document.getElementById('b1').className=m=='breath'?'purple active':'purple';"
"  document.getElementById('b2').className=m=='rainbow'?'pink active':'pink';"
"}"
"function fire(ch){"
"  var b=document.getElementById('mb'+ch);"
"  var ms=parseInt(document.getElementById('di'+ch).value)||100;"
"  b.className='mbtn fired';"
"  fetch('/g?ch='+ch+'&ms='+ms).catch(function(){});"
"  setTimeout(function(){b.className='mbtn';},ms+100);"
"}"
"function fwSelect(el){"
"  var f=el.files[0];"
"  if(!f)return;"
"  var log=document.getElementById('otaLog'),"
"      btn=document.getElementById('otaBtn'),"
"      bar=document.getElementById('otaBar'),"
"      prog=document.getElementById('otaProgress'),"
"      succ=document.getElementById('otaSuccess'),"
"      err=document.getElementById('otaError'),"
"      flashBtn=document.getElementById('fwFlashBtn'),"
"      status=document.getElementById('fwStatus');"
"  succ.style.display='none';err.style.display='none';"
"  prog.style.display='block';bar.style.width='0%';"
"  log.textContent='已选: '+f.name+' ('+Math.round(f.size/1024)+' KB)\\n开始上传到SD卡...';"
"  btn.disabled=true;btn.textContent='上传中...';"
"  var xhr=new XMLHttpRequest();"
"  xhr.upload.addEventListener('progress',function(e){"
"    if(e.lengthComputable){"
"      var p=Math.round(e.loaded/e.total*100);"
"      bar.style.width=p+'%';"
"      log.textContent='上传中: '+p+'% ('+Math.round(e.loaded/1024)+'/'+Math.round(e.total/1024)+' KB)';"
"    }"
"  });"
"  xhr.addEventListener('load',function(){"
"    if(xhr.status==200){"
"      log.textContent='上传完成，固件已保存到SD卡';"
"      bar.style.width='100%';"
"      succ.style.display='block';"
"      succ.textContent='固件上传成功！点击下方按钮烧录并重启';"
"      flashBtn.style.display='block';status.textContent='待烧录: '+f.name;"
"      fwReady=true;btn.textContent='上传完成';"
"    }else{"
"      log.textContent='上传失败 HTTP '+xhr.status;"
"      err.style.display='block';err.textContent='上传失败 (HTTP '+xhr.status+')';"
"      prog.style.display='none';btn.disabled=false;btn.textContent='重新选择固件';"
"    }"
"  });"
"  xhr.addEventListener('error',function(){"
"    log.textContent='上传失败，请检查网络';"
"    err.style.display='block';err.textContent='网络错误';"
"    btn.disabled=false;btn.textContent='重新选择固件';prog.style.display='none';"
"  });"
"  xhr.open('POST','/api/ota_upload');xhr.send(f);"
"}"
"function doFlash(){"
"  var log=document.getElementById('otaLog'),"
"      succ=document.getElementById('otaSuccess'),"
"      err=document.getElementById('otaError'),"
"      flashBtn=document.getElementById('fwFlashBtn'),"
"      btn=document.getElementById('otaBtn');"
"  log.textContent='开始烧录...请耐心等待约30秒，不要断电！';"
"  succ.style.display='none';err.style.display='none';"
"  flashBtn.disabled=true;flashBtn.textContent='烧录中...';"  
"  fetch('/api/ota_flash',{method:'POST'}).then(function(r){return r.text()}).then(function(t){"
"    log.textContent='烧录完成: '+t;"
"    succ.style.display='block';succ.textContent='烧录成功！设备将在3秒后重启...';"
"    setTimeout(function(){location.reload();},3000);"
"  }).catch(function(e){"
"    log.textContent='烧录失败: '+e;"
"    err.style.display='block';err.textContent='烧录失败';"
"    flashBtn.disabled=false;flashBtn.textContent='重新烧录';"
"  });"
"}"
"function ping(){"
"  fetch('/m?c=info').then(function(r){return r.json()}).then(upd).catch(function(){"
"    document.getElementById('conn').className='conn bad';"
"  });"
"  fetch('/api/ota_info').then(function(r){return r.text()}).then(function(t){"
"    var e1=document.getElementById('otaInfo');if(e1)e1.innerHTML=t;"
"    var e2=document.getElementById('partInfo');if(e2)e2.innerHTML=t;"
"  }).catch(function(){});"
"}"
"doit('info');"
"document.getElementById('bpause').style.display='none';"
"fetch('/api/version').then(function(r){return r.text()}).then(function(v){"
"  var e=document.getElementById('ver');if(e)e.textContent='v'+v;"
"  var e2=document.getElementById('fwver');if(e2)e2.textContent='v'+v;"
"});"
"setInterval(ping,8000);"
"// ===== 敲鼓控制 ====="
"var drumRunning=false;"
"var drumMode=0, drumRhythm=0;"
"function setDrumMode(m){drumMode=m;for(var i=0;i<5;i++){var b=document.getElementById('dm'+i);if(b)b.className=i===m?'drum-btn active':'drum-btn';}}"
"function setDrumRhythm(r){drumRhythm=r;for(var i=0;i<5;i++){var b=document.getElementById('dr'+i);if(b)b.className=i===r?'drum-btn active':'drum-btn';}}"
"function toggleDrum(){"
"  var bpm=parseInt(document.getElementById('drumBpm').value);"
"  var vel=parseInt(document.getElementById('drumVel').value);"
"  var act=drumRunning?'stop':'start';"
"  drumRunning=!drumRunning;"
"  var btn=document.getElementById('drumStartBtn');"
"  btn.textContent=drumRunning?'停止敲鼓':'启动敲鼓';"
"  btn.className=drumRunning?'drum-start running':'drum-start';"
"  fetch('/drum?m='+drumMode+'&r='+drumRhythm+'&bpm='+bpm+'&vel='+vel+'&a='+act).then(function(r){return r.json()}).then(function(d){"
"    document.getElementById('drumStatus').textContent='状态: '+(d.running?'运行':'停止')+' | 节奏: '+d.rhythm+' | BPM: '+d.bpm+' | 力度: '+d.vel;});"
"}"
"setDrumMode(0);setDrumRhythm(0);"
"</script>"
"</body></html>";

// ============ 路由回调 ============
static esp_err_t root_cb(httpd_req_t *req)
{
    httpd_resp_send(req, s_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t music_cb(httpd_req_t *req)
{
    char buf[48] = {0}, cmd[16] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_query_key_value(buf, "c", cmd, sizeof(cmd));

    char sta[16] = "unknown", cur[32] = "--/--", song[64] = "-";

    audio_player_state_t st = audio_player_get_state();
    if (st == AUDIO_PLAYER_STATE_PLAYING) strcpy(sta, "playing");
    else if (st == AUDIO_PLAYER_STATE_PAUSE) strcpy(sta, "paused");
    else strcpy(sta, "idle");

    if (g_file_iterator != NULL) {
        int idx = file_iterator_get_index(g_file_iterator);
        int total = (int)file_iterator_get_count(g_file_iterator);
        snprintf(cur, sizeof(cur), "%d/%d", idx + 1, total);
        strncpy(song, music_get_current_name(), sizeof(song) - 1);
        song[sizeof(song) - 1] = 0;
    }

    if (strcmp(cmd, "play") == 0) { music_play(); strcpy(sta, "playing"); }
    else if (strcmp(cmd, "pause") == 0) { music_pause(); strcpy(sta, "paused"); }
    else if (strcmp(cmd, "stop") == 0) { music_pause(); strcpy(sta, "stopped"); }
    else if (strcmp(cmd, "next") == 0) { music_next(); strcpy(sta, "playing"); }
    else if (strcmp(cmd, "prev") == 0) { music_prev(); strcpy(sta, "playing"); }

    char resp[256];
    snprintf(resp, sizeof(resp), "{\"sta\":\"%s\",\"cur\":\"%s\",\"song\":\"%s\"}", sta, cur, song);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t led_cb(httpd_req_t *req)
{
    char buf[32] = {0}, mode[16] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_query_key_value(buf, "m", mode, sizeof(mode));
    extern void breathing_led_set_mode(uint8_t m);
    if (strcmp(mode, "breath") == 0) breathing_led_set_mode(0);
    else if (strcmp(mode, "rainbow") == 0) breathing_led_set_mode(1);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":1}", 13);
    return ESP_OK;
}

static esp_err_t vol_cb(httpd_req_t *req)
{
    char buf[32] = {0}, nv[8] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_query_key_value(buf, "n", nv, sizeof(nv));
    int vol = atoi(nv);
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    music_set_volume((uint8_t)vol);
    char resp[32];
    int len = snprintf(resp, sizeof(resp), "{\"vol\":%d}", vol);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, len);
    return ESP_OK;
}

static esp_err_t magnet_cb(httpd_req_t *req)
{
    char buf[48] = {0}, ch[4] = {0}, ms_str[8] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_query_key_value(buf, "ch", ch, sizeof(ch));
    httpd_query_key_value(buf, "ms", ms_str, sizeof(ms_str));

    int channel = atoi(ch);
    if (channel < 0) channel = 0;
    if (channel > 3) channel = 3;
    int ms = atoi(ms_str);
    if (ms < 10) ms = 10;
    if (ms > 10000) ms = 10000;

    extern void magent_fire_ch_with_ms(int ch, int ms);
    magent_fire_ch_with_ms(channel, ms);

    char resp[48];
    snprintf(resp, sizeof(resp), "{\"ok\":1,\"ch\":%d,\"ms\":%d}", channel, ms);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t drum_cb(httpd_req_t *req)
{
    char buf[64] = {0}, ms[8] = {0}, rs[8] = {0}, bpm_s[8] = {0}, vel_s[8] = {0}, act[8] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    int m = atoi(httpd_query_key_value(buf, "m", ms, sizeof(ms)) == ESP_OK ? ms : "0");
    int r = atoi(httpd_query_key_value(buf, "r", rs, sizeof(rs)) == ESP_OK ? rs : "0");
    int bpm = atoi(httpd_query_key_value(buf, "bpm", bpm_s, sizeof(bpm_s)) == ESP_OK ? bpm_s : "120");
    int vel = atoi(httpd_query_key_value(buf, "vel", vel_s, sizeof(vel_s)) == ESP_OK ? vel_s : "70");
    httpd_query_key_value(buf, "a", act, sizeof(act));

    extern void bsp_drum_set_mode(drum_mode_t mode);
    extern void bsp_drum_set_bpm(uint8_t b);
    extern void bsp_drum_set_velocity(uint8_t v);
    extern void bsp_drum_set_rhythm(rhythm_type_t rt);
    extern void bsp_drum_start(void);
    extern void bsp_drum_stop(void);
    extern void bsp_drum_get_info(drum_info_t *info);

    drum_mode_t modes[5] = {DRUM_MODE_NONE, DRUM_MODE_PRESET, DRUM_MODE_MANUAL, DRUM_MODE_MIC_SYNC, DRUM_MODE_MUSIC_SYNC};
    if (m < 0 || m > 4) m = 0;
    rhythm_type_t rhythms[5] = {RHYTHM_SINGLE, RHYTHM_DOUBLE, RHYTHM_ROLL, RHYTHM_WALTZ, RHYTHM_ROCK};
    if (r < 0 || r > 4) r = 0;
    if (bpm < 60) bpm = 60;
    if (bpm > 240) bpm = 240;
    if (vel < 10) vel = 10;
    if (vel > 100) vel = 100;

    bsp_drum_set_mode(modes[m]);
    bsp_drum_set_rhythm(rhythms[r]);
    bsp_drum_set_bpm(bpm);
    bsp_drum_set_velocity(vel);

    bool running = false;
    if (strcmp(act, "start") == 0) {
        bsp_drum_start();
        running = true;
    } else if (strcmp(act, "stop") == 0) {
        bsp_drum_stop();
        running = false;
    } else {
        // 查询状态
        drum_info_t info;
        bsp_drum_get_info(&info);
        running = info.running;
    }

    const char *rhythm_names[5] = {"单击","双击","滚奏","华尔兹","摇滚"};
    char resp[128];
    int len = snprintf(resp, sizeof(resp),
        "{\"running\":%s,\"mode\":%d,\"rhythm\":\"%s\",\"bpm\":%d,\"vel\":%d}",
        running ? "true" : "false", m, rhythm_names[r], bpm, vel);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, len);
    return ESP_OK;
}

static esp_err_t version_cb(httpd_req_t *req)
{
    httpd_resp_send(req, APP_VERSION_STR, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// OTA分区信息
static esp_err_t ota_info_cb(httpd_req_t *req)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    char info[384];
    if (next == NULL) {
        snprintf(info, sizeof(info),
                 "当前: <b>%s</b><br>"
                 "<span style=color:#f55>⚠️ 单分区模式，不支持OTA升级<br>"
                 "如需OTA，请使用双分区配置（需4MB Flash）</span>",
                 run ? run->label : "?");
    } else {
        snprintf(info, sizeof(info),
                 "当前: <b>%s</b> @ 0x%" PRIx32 "<br>"
                 "OTA目标: <b>%s</b> @ 0x%" PRIx32 "<br>"
                 "<span style=color:#e67e22>固件上传到SD卡，再烧录到OTA分区</span>",
                 run ? run->label : "?",
                 run ? (uint32_t)run->address : 0,
                 next->label,
                 (uint32_t)next->address);
    }
    httpd_resp_send(req, info, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 上传固件到SD卡（防重入）
static esp_err_t ota_upload_cb(httpd_req_t *req)
{
    // 防重入：正在上传中则拒绝
    if (s_ota_uploading) {
        httpd_resp_send(req, "Upload already in progress", -1);
        return ESP_FAIL;
    }
    s_ota_uploading = true;

    size_t content_len = req->content_len;
    if (content_len > 2 * 1024 * 1024) {
        s_ota_uploading = false;
        httpd_resp_send(req, "File too large (max 2MB)", -1);
        return ESP_FAIL;
    }

    // 删除旧文件（如果存在）
    unlink(OTA_FILE);

    int fd = open(OTA_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s", OTA_FILE);
        s_ota_uploading = false;
        httpd_resp_send(req, "SD card write error", -1);
        return ESP_FAIL;
    }

    char buf[4096];
    size_t received = 0;
    bool error = false;

    while (received < content_len) {
        int ret = httpd_req_recv(req, buf, sizeof(buf));
        if (ret < 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "OTA recv error: %d", ret);
            error = true;
            break;
        }
        if (write(fd, buf, ret) != ret) {
            ESP_LOGE(TAG, "SD write error");
            error = true;
            break;
        }
        received += ret;
    }

    close(fd);

    if (error) {
        unlink(OTA_FILE);
        s_ota_uploading = false;
        httpd_resp_send(req, "Upload interrupted", -1);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA firmware saved: %d bytes", received);
    s_ota_uploading = false;
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// 烧录固件（从SD卡到OTA分区）
static esp_err_t ota_flash_cb(httpd_req_t *req)
{
    (void)req;  // unused

    if (s_ota_flashing) {
        httpd_resp_send(req, "Flash already in progress", -1);
        return ESP_FAIL;
    }
    s_ota_flashing = true;

    // 检查OTA分区是否存在
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition - single app partition table");
        s_ota_flashing = false;
        httpd_resp_send(req, "No OTA partition available (need dual-partition config for OTA)", -1);
        return ESP_FAIL;
    }

    // 检查固件文件
    struct stat st;
    if (stat(OTA_FILE, &st) != 0) {
        ESP_LOGE(TAG, "No firmware file on SD card");
        s_ota_flashing = false;
        httpd_resp_send(req, "No firmware on SD card", -1);
        return ESP_FAIL;
    }

    size_t fw_size = st.st_size;
    ESP_LOGI(TAG, "OTA: %d bytes, target partition: %s @ 0x%" PRIx32,
             fw_size, update_partition->label, (uint32_t)update_partition->address);

    // 验证固件最小大小
    size_t header_size = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    if (fw_size < header_size) {
        ESP_LOGE(TAG, "File too small: %d < %d", fw_size, header_size);
        s_ota_flashing = false;
        httpd_resp_send(req, "Invalid firmware file (too small)", -1);
        return ESP_FAIL;
    }

    int fd = open(OTA_FILE, O_RDONLY);
    if (fd < 0) {
        s_ota_flashing = false;
        httpd_resp_send(req, "Cannot read SD firmware", -1);
        return ESP_FAIL;
    }

    // 读取并验证固件头部
    char header[header_size];
    if (read(fd, header, header_size) != (ssize_t)header_size) {
        close(fd);
        s_ota_flashing = false;
        httpd_resp_send(req, "Cannot read firmware header", -1);
        return ESP_FAIL;
    }

    esp_app_desc_t new_app_info;
    memcpy(&new_app_info,
           &header[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
           sizeof(esp_app_desc_t));
    ESP_LOGI(TAG, "OTA: new firmware version: %.32s", new_app_info.version);

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "OTA: running firmware version: %.32s", running_app_info.version);
        if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
            ESP_LOGW(TAG, "Same version, skipping");
            close(fd);
            s_ota_flashing = false;
            httpd_resp_send(req, "Same version as running - no update needed", -1);
            return ESP_FAIL;
        }
    }

    // 开始OTA写入OTA分区（不是running分区！）
    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        close(fd);
        s_ota_flashing = false;
        httpd_resp_send(req, "esp_ota_begin failed", -1);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA: writing to partition %s", update_partition->label);

    // 分块复制（跳过固件头）
    char chunk[8192];
    size_t total_written = 0;
    size_t header_skipped = 0;

    while (1) {
        ssize_t r = read(fd, chunk, sizeof(chunk));
        if (r == 0) break;
        if (r < 0) {
            ESP_LOGE(TAG, "SD read error");
            esp_ota_abort(ota_handle);
            close(fd);
            s_ota_flashing = false;
            httpd_resp_send(req, "SD read error", -1);
            return ESP_FAIL;
        }

        // 跳过第一次读的头部
        const uint8_t *write_ptr = (const uint8_t *)chunk;
        size_t write_len = r;

        if (header_skipped == 0) {
            size_t skip = header_size < (size_t)r ? header_size : (size_t)r;
            write_ptr += skip;
            write_len -= skip;
            header_skipped = 1;
            // 如果剩余为0，继续读下一个块
            if (write_len == 0) {
                total_written += r;  // 记录但不用来写
                continue;
            }
        }

        err = esp_ota_write(ota_handle, write_ptr, write_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            close(fd);
            s_ota_flashing = false;
            httpd_resp_send(req, "Flash write failed", -1);
            return ESP_FAIL;
        }
        total_written += r;
    }

    close(fd);
    ESP_LOGI(TAG, "OTA: wrote %d bytes, calling esp_ota_end", total_written);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed");
            s_ota_flashing = false;
            httpd_resp_send(req, "Firmware validation failed - corrupted", -1);
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
            s_ota_flashing = false;
            httpd_resp_send(req, "OTA end failed", -1);
        }
        return ESP_FAIL;
    }

    // 设置启动分区为新固件
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        s_ota_flashing = false;
        httpd_resp_send(req, "Set boot partition failed", -1);
        return ESP_FAIL;
    }

    // 删除SD卡固件文件
    unlink(OTA_FILE);

    ESP_LOGI(TAG, "OTA: success! Rebooting...");
    s_ota_flashing = false;
    httpd_resp_send(req, "OTA OK - rebooting", -1);

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;  // Will not reach here
}

// ============ 启动 ============
void bsp_http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;

    httpd_handle_t srv = NULL;
    esp_err_t err = httpd_start(&srv, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: %s", esp_err_to_name(err));
        return;
    }

    httpd_uri_t uris[] = {
        {"/",             HTTP_GET,  root_cb},
        {"/m",            HTTP_GET,  music_cb},
        {"/l",            HTTP_GET,  led_cb},
        {"/v",            HTTP_GET,  vol_cb},
        {"/g",            HTTP_GET,  magnet_cb},
        {"/drum",         HTTP_GET,  drum_cb},
        {"/api/version",  HTTP_GET,  version_cb},
        {"/api/ota_info", HTTP_GET,  ota_info_cb},
        {"/api/ota_upload", HTTP_POST, ota_upload_cb},
        {"/api/ota_flash",  HTTP_POST, ota_flash_cb},
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        if (httpd_register_uri_handler(srv, &uris[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI: %s", uris[i].uri);
        }
    }

    ESP_LOGI(TAG, "HTTP server :80");
}
