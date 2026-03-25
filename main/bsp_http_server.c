#include "bsp_http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "bsp.h"
#include "bsp_magent.h"
#include "audio_player.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "HTTP";

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
".red { background:linear-gradient(135deg,#ff5858,#f09819); color:#fff; }"
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
"</style>"
"</head>"
"<body>"
"<div class=c>"
"<h1>ESP32 氛围系统</h1>"

"<div class=card><h2><span class=conn id=conn></span>系统状态</h2>"
"<div class=row><span class=lb>曲目</span><span class=lv id=cur>--/--</span></div>"
"<div class=row><span class=lb>状态</span><span class=lv id=sta>--</span></div>"
"<div class=song id=song>-</div>"
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
"<div class=ch-item><label>CH1</label><input class=dur-input id=di0 value=100 type=number min=50 max=10000 placeholder='ms'></div>"
"<div class=ch-item><label>CH2</label><input class=dur-input id=di1 value=100 type=number min=50 max=10000 placeholder='ms'></div>"
"</div>"
"<div class=ch-row>"
"<div class=ch-item><label>CH3</label><input class=dur-input id=di2 value=100 type=number min=50 max=10000 placeholder='ms'></div>"
"<div class=ch-item><label>CH4</label><input class=dur-input id=di3 value=100 type=number min=50 max=10000 placeholder='ms'></div>"
"</div>"
"<div class=magnet-row>"
"<button class=mbtn id=mb0 onclick='fire(0)'>CH1</button>"
"<button class=mbtn id=mb1 onclick='fire(1)'>CH2</button>"
"<button class=mbtn id=mb2 onclick='fire(2)'>CH3</button>"
"<button class=mbtn id=mb3 onclick='fire(3)'>CH4</button>"
"</div>"
"</div>"

"<div class=card><h2>\u1f389 \u6476\u9f13\u63a7\u5236</h2>"
"<div style=margin-top:8px;>"
"<div style=display:flex;gap:8px;flex-wrap:wrap;>"
"<button class=purple id=db1 onclick=setDrumMode(1)>\u9884\u8bbe</button>"
"<button class=purple id=db2 onclick=setDrumMode(2)>\u624b\u52a8</button>"
"<button class=purple id=db3 onclick=setDrumMode(3)>MIC\u540c\u6b65</button>"
"<button class=purple id=db4 onclick=setDrumMode(4)>\u97f3\u4e50\u540c\u6b65</button>"
"</div>"
"<div style=margin-top:8px;>"
"<label style=font-size:11px;color:#888>BPM:</label>"
"<input type=range id=dBpm min=60 max=240 value=120 oninput=setDrumBpm(this.value)>"
"<span id=dBpmVal style=font-size:11px;color:#00d4ff>120</span>"
"</div>"
"<div style=margin-top:6px;>"
"<label style=font-size:11px;color:#888>\u529b\u5ea6:</label>"
"<input type=range id=dVel min=10 max=100 value=70 oninput=setDrumVel(this.value)>"
"<span id=dVelVal style=font-size:11px;color:#00d4ff>70%</span>"
"</div>"
"<div style=margin-top:8px;>"
"<button class=purple id=dr1 onclick=setDrumRhythm(0)>\u5355\u51fb</button>"
"<button class=purple id=dr2 onclick=setDrumRhythm(1)>\u53cc\u51fb</button>"
"<button class=purple id=dr3 onclick=setDrumRhythm(2)>\u6eda\u594f</button>"
"<button class=purple id=dr4 onclick=setDrumRhythm(3)>\u534e\u5c14\u5179</button>"
"<button class=purple id=dr5 onclick=setDrumRhythm(4)>\u6447\u6eda</button>"
"</div>"
"<div style=margin-top:8px;display:flex;gap:8px;align-items:center;>"
"<button class=green onclick=drumStart()>\u25b6 \u5f00\u59cb</button>"
"<button class=red onclick=drumStop()>\u25a0 \u505c\u6b62</button>"
"<span id=drumStatus style=font-size:11px;color:#888>\u5f85\u673a</span>"
"</div>"
"<div style=margin-top:8px;display:grid;grid-template-columns:1fr 1fr;gap:8px;>"
"<button class=mbtn style=background:#e67e22;font-size:16px;font-weight:bold onclick=drumHit(0)>\u5de6\u6476</button>"
"<button class=mbtn style=background:#3498db;font-size:16px;font-weight:bold onclick=drumHit(1)>\u53f3\u6476</button>"
"</div>"
"</div>"

"<div class=footer>"ESP32-S3 v1.1</div>"
"</div>"
"<script>"
"var playing=true;"
"var req=null;"
""
"function doit(c){"
"  if(req) req.abort();"
"  req=new AbortController();"
"  fetch('/m?c='+c,{signal:req.signal}).then(function(r){return r.json()}).then(function(d){"
"    upd(d);"
"  }).catch(function(e){});"
"}"
""
"function upd(d){"
"  document.getElementById('sta').textContent=d.sta;"
"  document.getElementById('cur').textContent=d.cur;"
"  document.getElementById('song').textContent=d.song;"
"  document.getElementById('bplay').style.display=d.sta=='playing'?'none':'inline-block';"
"  document.getElementById('bpause').style.display=d.sta=='playing'?'inline-block':'none';"
"  document.getElementById('conn').className='conn ok';"
"}"
""
"function sv(v){"
"  document.getElementById('vdis').textContent=v;"
"  fetch('/v?n='+v).catch(function(e){});"
"}"
""
"function sl(m){"
"  fetch('/l?m='+m).catch(function(e){});"
"  document.getElementById('b1').className=m=='breath'?'purple active':'purple';"
"  document.getElementById('b2').className=m=='rainbow'?'pink active':'pink';"
"}"
""
"function fire(ch){"
"  var b=document.getElementById('mb'+ch);"
"  var ms=parseInt(document.getElementById('di'+ch).value)||100;"
"  b.className='mbtn fired';"
"  fetch('/g?ch='+ch+'\&ms='+ms).catch(function(e){});"
"  setTimeout(function(){b.className='mbtn';},ms+100);"
"}"
""
"// Heartbeat: status only, every 8s"
"function ping(){"
"  fetch('/m?c=info').then(function(r){return r.json()}).then(upd).catch(function(){"
"    document.getElementById('conn').className='conn bad';"
"  });"
"}"
""
"doit('info');"
"document.getElementById('bpause').style.display='none';"
"setInterval(ping,8000);"
"</script>"
"</body></html>";

// ============ 路由 ============
static esp_err_t root_cb(httpd_req_t *req)
{
    httpd_resp_send(req, s_html, strlen(s_html));
    return ESP_OK;
}

static esp_err_t music_cb(httpd_req_t *req)
{
    char buf[48] = {0};
    char cmd[16] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_query_key_value(buf, "c", cmd, sizeof(cmd));

    char sta[16] = "unknown";
    char cur[32] = "--/--";
    char song[64] = "-";

    audio_player_state_t st = audio_player_get_state();
    if (st == AUDIO_PLAYER_STATE_PLAYING) strcpy(sta, "playing");
    else if (st == AUDIO_PLAYER_STATE_PAUSE) strcpy(sta, "paused");
    else strcpy(sta, "idle");

    if (g_file_iterator != NULL) {
        int idx = file_iterator_get_index(g_file_iterator);
        int total = (int)file_iterator_get_count(g_file_iterator);
        char curbuf[32];
        snprintf(curbuf, sizeof(curbuf), "%d/%d", idx + 1, total);
        strncpy(cur, curbuf, sizeof(cur) - 1);
        cur[sizeof(cur) - 1] = 0;
        strncpy(song, music_get_current_name(), sizeof(song) - 1);
        song[sizeof(song) - 1] = 0;
    }

    if (strcmp(cmd, "play") == 0) {
        music_play();
        strcpy(sta, "playing");
    } else if (strcmp(cmd, "pause") == 0) {
        music_pause();
        strcpy(sta, "paused");
    } else if (strcmp(cmd, "stop") == 0) {
        music_pause();
        strcpy(sta, "stopped");
    } else if (strcmp(cmd, "next") == 0) {
        music_next();
        strcpy(sta, "playing");
    } else if (strcmp(cmd, "prev") == 0) {
        music_prev();
        strcpy(sta, "playing");
    }

    char resp[256];
    snprintf(resp, sizeof(resp), "{\"sta\":\"%s\",\"cur\":\"%s\",\"song\":\"%s\"}",
             sta, cur, song);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t led_cb(httpd_req_t *req)
{
    char buf[32] = {0};
    char mode[16] = {0};
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
    char buf[32] = {0};
    char nv[8] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_query_key_value(buf, "n", nv, sizeof(nv));
    int vol = atoi(nv);
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    music_set_volume((uint8_t)vol);
    return ESP_OK;
}

// 电磁铁：支持单个通道触发
static esp_err_t magnet_cb(httpd_req_t *req)
{
    char buf[48] = {0};
    char ch[4] = {0};
    char ms_str[8] = {0};
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
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ============ 启动 ============
void bsp_http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    // 网络优化
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 5;
    cfg.keep_alive_interval = 5;
    cfg.keep_alive_count = 3;
    cfg.backlog_conn = 10;
    cfg.recv_wait_timeout = 30;
    cfg.send_wait_timeout = 30;
    cfg.max_resp_headers = 16;

    httpd_handle_t srv = NULL;
    esp_err_t err = httpd_start(&srv, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "HTTP server started on port 80");

    httpd_uri_t r = {"/", HTTP_GET, root_cb, NULL};
    httpd_uri_t m = {"/m", HTTP_GET, music_cb, NULL};
    httpd_uri_t l = {"/l", HTTP_GET, led_cb, NULL};
    httpd_uri_t v = {"/v", HTTP_GET, vol_cb, NULL};
    httpd_uri_t g = {"/g", HTTP_GET, magnet_cb, NULL};
    httpd_uri_t d = {"/d", HTTP_GET, drum_cb, NULL};

    httpd_register_uri_handler(srv, &r);
    httpd_register_uri_handler(srv, &m);
    httpd_register_uri_handler(srv, &l);
    httpd_register_uri_handler(srv, &v);
    httpd_register_uri_handler(srv, &g);

    ESP_LOGI(TAG, "HTTP server :80");
}
