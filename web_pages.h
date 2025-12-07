#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include "config.h"

// Modern GitHub-style with icons and polished design
inline String getHtmlHeader(String title) {
  String html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>)HTML";
  html += title;
  html += R"HTML(</title>
<style>
:root{--c-bg:#0d1117;--c-bg2:#161b22;--c-border:#30363d;--c-text:#e6edf3;--c-text2:#8d96a0;--c-blue:#58a6ff;--c-green:#3fb950;--c-red:#f85149;--c-orange:#d29922;--c-purple:#a371f7}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Noto Sans,Helvetica,Arial,sans-serif;background:var(--c-bg);color:var(--c-text);font-size:14px;-webkit-font-smoothing:antialiased}
.app{max-width:500px;margin:0 auto;padding:12px}

/* Header */
.hdr{background:var(--c-bg2);border:1px solid var(--c-border);border-radius:12px;padding:16px;margin-bottom:12px}
.hdr-top{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px}
.brand{display:flex;align-items:center;gap:10px}
.brand-icon{width:36px;height:36px;background:linear-gradient(135deg,var(--c-blue),var(--c-purple));border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:18px}
.brand-text h1{font-size:16px;font-weight:600;color:var(--c-text)}
.brand-text span{font-size:11px;color:var(--c-text2)}
.badge{padding:5px 10px;border-radius:20px;font-size:11px;font-weight:600;text-transform:uppercase;letter-spacing:0.5px}
.badge-idle{background:rgba(141,150,160,0.15);color:var(--c-text2)}
.badge-atk{background:rgba(248,81,73,0.15);color:var(--c-red)}
.badge-evil{background:rgba(163,113,247,0.15);color:var(--c-purple)}

/* Stats */
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;padding:16px 0;border-top:1px solid var(--c-border);border-bottom:1px solid var(--c-border);margin-bottom:12px}
.stat{text-align:center}
.stat-val{font-size:28px;font-weight:700;color:var(--c-blue)}
.stat-lbl{font-size:11px;color:var(--c-text2);margin-top:2px}

/* Toolbar */
.tools{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}

/* Card */
.card{background:var(--c-bg2);border:1px solid var(--c-border);border-radius:12px;overflow:hidden;margin-bottom:12px}
.card-h{display:flex;align-items:center;gap:8px;padding:12px 16px;background:rgba(255,255,255,0.02);border-bottom:1px solid var(--c-border);font-weight:600;font-size:13px}
.card-h .icon{opacity:0.7}

/* Tabs */
.tabs{display:grid;grid-template-columns:1fr 1fr}
.tab{padding:14px;text-align:center;font-weight:500;color:var(--c-text2);cursor:pointer;border-bottom:2px solid transparent;transition:all 0.15s;display:flex;align-items:center;justify-content:center;gap:6px}
.tab:hover{color:var(--c-text);background:rgba(255,255,255,0.02)}
.tab.active{color:var(--c-text);border-bottom-color:var(--c-orange)}
.tab-body{display:none;padding:16px}
.tab-body.active{display:block}

/* Table */
.tbl-wrap{border:1px solid var(--c-border);border-radius:8px;overflow:hidden}
.tbl-scroll{max-height:240px;overflow-y:auto}
.tbl-scroll::-webkit-scrollbar{width:6px}
.tbl-scroll::-webkit-scrollbar-thumb{background:var(--c-border);border-radius:3px}
table{width:100%;border-collapse:collapse;font-size:11px}
thead{position:sticky;top:0;background:var(--c-bg2);z-index:1}
th{text-align:left;padding:6px 8px;font-weight:500;color:var(--c-text2);font-size:10px;border-bottom:1px solid var(--c-border);white-space:nowrap}
td{padding:6px 8px;border-bottom:1px solid rgba(48,54,61,0.5);white-space:nowrap}
tr:last-child td{border-bottom:none}
tr:hover td{background:rgba(56,139,253,0.04)}
.chk{width:14px;height:14px;accent-color:var(--c-blue)}
.ssid{font-weight:500;max-width:100px;overflow:hidden;text-overflow:ellipsis}
.hidden{color:var(--c-text2);font-style:italic}
.mono{font-family:ui-monospace,monospace;font-size:10px;color:var(--c-text2)}
.sig-g{color:var(--c-green)}
.sig-m{color:var(--c-orange)}
.sig-b{color:var(--c-red)}
.b5{color:var(--c-blue);font-weight:600}
.b2{color:var(--c-purple)}
.sel-all{padding:6px 8px;background:var(--c-bg);display:flex;align-items:center;gap:6px;font-size:11px;color:var(--c-text2);border-bottom:1px solid var(--c-border)}

/* Form */
.form-group{margin-bottom:12px}
.form-group label{display:block;font-size:12px;color:var(--c-text2);margin-bottom:6px}
select{width:100%;padding:10px 12px;background:var(--c-bg);border:1px solid var(--c-border);color:var(--c-text);border-radius:8px;font-size:13px;cursor:pointer}
select:focus{outline:none;border-color:var(--c-blue);box-shadow:0 0 0 3px rgba(88,166,255,0.15)}

/* Buttons */
.btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:10px 16px;font-size:13px;font-weight:500;border-radius:8px;border:none;cursor:pointer;transition:all 0.1s}
.btn:active{transform:scale(0.98)}
.btn-block{width:100%}
.btn-green{background:var(--c-green);color:#fff}
.btn-red{background:var(--c-red);color:#fff}
.btn-ghost{background:transparent;border:1px solid var(--c-border);color:var(--c-text)}
.btn-ghost:hover{background:var(--c-border)}

/* Password */
.pwd{background:var(--c-bg);border:1px solid var(--c-green);border-radius:8px;padding:12px;margin-bottom:8px}
.pwd-h{display:flex;align-items:center;gap:6px;font-size:11px;color:var(--c-green);margin-bottom:4px}
.pwd-t{font-family:ui-monospace,monospace;font-size:14px;word-break:break-all}

/* Evil Active */
.evil-on{background:rgba(163,113,247,0.08);border:1px solid var(--c-purple);border-radius:12px;padding:20px;text-align:center;margin-bottom:16px}
.evil-on h3{color:var(--c-purple);font-size:16px;display:flex;align-items:center;justify-content:center;gap:8px;margin-bottom:6px}
.evil-on p{color:var(--c-text2);font-size:13px}
.evil-on strong{color:var(--c-text)}
</style>
</head>
<body>
<div class="app">
)HTML";
  return html;
}

inline String getHtmlFooter() {
  return R"HTML(
</div>
<script>
function tab(t){document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));document.querySelectorAll('.tab-body').forEach(e=>e.classList.remove('active'));document.querySelector('[data-t="'+t+'"]').classList.add('active');document.getElementById(t).classList.add('active')}
function all(c){document.querySelectorAll('.chk:not([id])').forEach(e=>e.checked=c.checked)}
function toggle(id){var x=document.getElementById(id);if(x.style.display==="none"){x.style.display="block"}else{x.style.display="none"}}
</script>
</body>
</html>)HTML";
}

inline String getStatusBadge(DeviceState state) {
  switch (state) {
    case STATE_IDLE: return "<span class='badge badge-idle'>● Idle</span>";
    case STATE_SCANNING: return "<span class='badge badge-idle'>◌ Scanning</span>";
    case STATE_ATTACK: return "<span class='badge badge-atk'>● Attacking</span>";
    case STATE_EVIL_TWIN: return "<span class='badge badge-evil'>● Evil Twin</span>";
    default: return "<span class='badge badge-idle'>-</span>";
  }
}

inline String getSigClass(int rssi) {
  if (rssi >= -50) return "sig-g";
  if (rssi >= -70) return "sig-m";
  return "sig-b";
}

#endif
