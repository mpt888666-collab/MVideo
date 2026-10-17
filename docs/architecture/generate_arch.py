# -*- coding: utf-8 -*-
import os

OUT_DIR = r"C:\Users\mpt\Documents\ChatGPT\My Video Projiect"
HTML = os.path.join(OUT_DIR, "mvideo-architecture.html")
W = 1280
H = 1160

layers = [
    dict(name="客户端层", y=96, h=140, tint="#eff6ff", border="#bfdbfe", accent="#2563eb", boxes=[
        dict(id="qt", title="Qt 客户端", sub="Windows / Linux · OpenGL 渲染", x=190, w=430, h=92),
        dict(id="mobile", title="移动端", sub="iOS / Android（后续）", x=720, w=370, h=92),
    ]),
    dict(name="接入层", y=276, h=140, tint="#fffbeb", border="#fde68a", accent="#d97706", boxes=[
        dict(id="cdn", title="CDN", sub="点播分发 · 多码率", x=130, w=200, h=92),
        dict(id="gw", title="Gateway 网关", sub="HTTP · 鉴权/限流/路由", x=354, w=200, h=92),
        dict(id="conn", title="连接网关", sub="TCP / WebSocket 长连接", x=578, w=200, h=92),
        dict(id="stun", title="STUN / TURN", sub="NAT 穿透", x=802, w=200, h=92),
        dict(id="sfu", title="SFU", sub="实时音视频媒体中转", x=1026, w=200, h=92),
    ]),
    dict(name="业务服务层", y=456, h=140, tint="#ecfdf5", border="#a7f3d0", accent="#059669", boxes=[
        dict(id="auth", title="Auth", sub="认证服务", x=130, w=126, h=92),
        dict(id="user", title="User", sub="用户服务", x=270, w=126, h=92),
        dict(id="follow", title="Follow", sub="关注/粉丝", x=410, w=126, h=92),
        dict(id="social", title="Social", sub="点赞/评论/收藏", x=550, w=126, h=92),
        dict(id="feed", title="Feed", sub="信息流/推荐", x=690, w=126, h=92),
        dict(id="chat", title="Chat/IM", sub="聊天服务", x=830, w=126, h=92),
        dict(id="signaling", title="RTC 信令", sub="通话信令", x=970, w=126, h=92),
        dict(id="media", title="Media", sub="上传/转码/元数据", x=1110, w=126, h=92),
    ]),
    dict(name="异步/数据层", y=636, h=120, tint="#faf5ff", border="#e9d5ff", accent="#7c3aed", boxes=[
        dict(id="ffmpeg", title="FFmpeg 转码", sub="异步任务 · 多码率/封面", x=130, w=330, h=78),
        dict(id="moderation", title="内容审核", sub="机审 + 人审", x=500, w=330, h=78),
        dict(id="rec", title="推荐引擎", sub="召回 / 排序", x=870, w=330, h=78),
    ]),
    dict(name="基础设施层", y=796, h=120, tint="#f8fafc", border="#e2e8f0", accent="#475569", boxes=[
        dict(id="oss", title="对象存储", sub="视频/封面/文件", x=130, w=165, h=78),
        dict(id="mq", title="消息队列", sub="RabbitMQ / Kafka", x=315, w=165, h=78),
        dict(id="reg", title="注册与配置中心", sub="Consul / Nacos", x=500, w=165, h=78),
        dict(id="redis", title="Redis", sub="缓存", x=685, w=165, h=78),
        dict(id="mysql", title="MySQL", sub="分库分表", x=870, w=165, h=78),
        dict(id="obs", title="可观测性", sub="日志/监控/链路", x=1055, w=165, h=78),
    ]),
]

boxes = {}
for L in layers:
    for b in L["boxes"]:
        b["y"] = L["y"] + (L["h"] - b["h"]) // 2
        b["cx"] = b["x"] + b["w"] // 2
        b["top"] = b["y"]
        b["bottom"] = b["y"] + b["h"]
        boxes[b["id"]] = b

C = dict(http="#64748b", tcp="#0891b2", grpc="#059669", mq="#7c3aed", rtc="#ea580c", file="#94a3b8")
edges = []
def E(fr, to, label="", color=C["http"], dash=False, mid=None, manual=None, up=False):
    edges.append(dict(fr=fr, to=to, label=label, color=color, dash=dash, mid=mid, manual=manual, up=up))

E("qt","gw","HTTP/HTTPS", C["http"], mid=268)
E("qt","conn","TCP 长连接", C["tcp"], mid=262)
E("qt","cdn","播放视频", C["file"], mid=256)
E("qt","stun","ICE", C["rtc"], dash=True, mid=262)
E("qt","sfu","RTC 媒体流", C["rtc"], dash=True, mid=268)
E("mobile","gw","", C["http"], mid=280)
E("mobile","conn","", C["tcp"], mid=280)
E("mobile","cdn","", C["file"], mid=280)
E("mobile","sfu","", C["rtc"], dash=True, mid=280)

E("gw","auth","gRPC", C["grpc"], mid=436)
E("gw","user","", C["grpc"], mid=436)
E("gw","follow","", C["grpc"], mid=436)
E("gw","social","", C["grpc"], mid=436)
E("gw","feed","", C["grpc"], mid=436)
E("gw","media","", C["grpc"], mid=436)
E("conn","chat","gRPC", C["grpc"], mid=430)
E("conn","signaling","信令 gRPC", C["grpc"], mid=430)

E("media","ffmpeg","任务投递 MQ", C["mq"], mid=616)
E("media","moderation","审核 MQ", C["mq"], mid=622)
E("feed","rec","特征/调用", C["file"], mid=616)
E("signaling","sfu","分配媒体节点", C["rtc"], dash=True, mid=592, up=True)

E("ffmpeg","oss","回写", C["file"], mid=776)
E("media","oss","上传", C["file"], mid=788)
E("cdn","oss","回源", C["file"], dash=True, manual=("M 230 416 H 108 V 830 H 122", 118, 620))

markers = {"http":C["http"],"tcp":C["tcp"],"grpc":C["grpc"],"mq":C["mq"],"rtc":C["rtc"],"file":C["file"]}
def marker_id(color):
    for k,v in markers.items():
        if v == color:
            return k
    return "file"

def edge_path(e):
    if e["manual"]:
        return e["manual"][0]
    a = boxes[e["fr"]]; b = boxes[e["to"]]
    if e["up"]:
        x1,y1 = a["cx"], a["top"]; x2,y2 = b["cx"], b["bottom"]
    else:
        x1,y1 = a["cx"], a["bottom"]; x2,y2 = b["cx"], b["top"]
    mid = e["mid"] if e["mid"] is not None else (y1+y2)/2
    if abs(x2-x1) < 6:
        return "M %.1f %.1f V %.1f" % (x1, y1, y2)
    return "M %.1f %.1f L %.1f %.1f L %.1f %.1f L %.1f %.1f" % (x1, y1, x1, mid, x2, mid, x2, y2)

def edge_label_pos(e):
    if e["manual"]:
        return e["manual"][1], e["manual"][2]
    a = boxes[e["fr"]]; b = boxes[e["to"]]
    if e["up"]:
        x1,y1 = a["cx"], a["top"]; x2,y2 = b["cx"], b["bottom"]
    else:
        x1,y1 = a["cx"], a["bottom"]; x2,y2 = b["cx"], b["top"]
    mid = e["mid"] if e["mid"] is not None else (y1+y2)/2
    if abs(x2-x1) < 6:
        return x1, (y1+y2)/2 - 6
    return (x1+x2)/2, mid - 7

svg_paths = []
svg_labels = []
for e in edges:
    d = edge_path(e)
    mx, my = edge_label_pos(e)
    dash = ' stroke-dasharray="6 5"' if e["dash"] else ""
    svg_paths.append('<path d="%s" fill="none" stroke="%s" stroke-width="1.6" marker-end="url(#arr-%s)"%s/>' % (d, e["color"], marker_id(e["color"]), dash))
    if e["label"]:
        svg_labels.append('<text x="%.1f" y="%.1f" text-anchor="middle" class="elab">%s</text>' % (mx, my, e["label"]))

def band_html(L):
    p = []
    p.append('<div class="band" style="top:%dpx;height:%dpx;background:%s;border-color:%s;"></div>' % (L["y"], L["h"], L["tint"], L["border"]))
    p.append('<div class="bandtitle" style="top:%dpx;height:%dpx;background:%s;border-right:1px solid %s;color:%s;"><span>%s</span></div>' % (L["y"], L["h"], L["tint"], L["border"], L["accent"], L["name"]))
    for b in L["boxes"]:
        p.append('<div class="box" style="left:%dpx;top:%dpx;width:%dpx;height:%dpx;border-left-color:%s;"><div class="bt">%s</div><div class="bs">%s</div></div>' % (b["x"], b["y"], b["w"], b["h"], L["accent"], b["title"], b["sub"]))
    return "\n".join(p)

legend_rows = [
    (C["http"], "HTTP / HTTPS", False),
    (C["tcp"], "TCP 长连接", False),
    (C["grpc"], "gRPC 内部服务", False),
    (C["mq"], "消息队列 MQ", False),
    (C["rtc"], "WebRTC / RTC", True),
    (C["file"], "文件流 / 回源", False),
]
legend_html = ""
for col, txt, d in legend_rows:
    dash = ' stroke-dasharray="6 5"' if d else ""
    legend_html += '<div class="lg"><svg width="34" height="14" viewBox="0 0 34 14"><line x1="0" y1="7" x2="34" y2="7" stroke="%s" stroke-width="2"%s/></svg><span>%s</span></div>' % (col, dash, txt)

marker_defs = "".join('<marker id="arr-%s" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 L3,5 Z" fill="%s"/></marker>' % (k, v) for k, v in markers.items())

html = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; }
  html, body { margin: 0; padding: 0; background: #ffffff; }
  body { font-family: "Microsoft YaHei", "PingFang SC", "Segoe UI", sans-serif; color: #0f172a; }
  #canvas { position: relative; width: %(W)dpx; height: %(H)dpx; background: #ffffff; overflow: hidden; }
  .hd { position: absolute; top: 0; left: 0; width: %(W)dpx; height: 84px; background: linear-gradient(90deg,#1e293b,#334155); color:#fff; padding: 14px 30px; }
  .hd h1 { margin: 0; font-size: 24px; letter-spacing: 1px; }
  .hd .sub { margin-top: 4px; font-size: 12.5px; color:#cbd5e1; }
  .hd .tag { position:absolute; right: 28px; top: 30px; background:#0ea5e9; color:#fff; font-size: 12px; padding: 4px 12px; border-radius: 999px; }
  .band { position: absolute; left: 0; width: %(W)dpx; border: 1px solid; border-radius: 12px; }
  .bandtitle { position: absolute; left: 0; width: 96px; border-radius: 12px 0 0 12px; display: flex; align-items: center; justify-content: center; }
  .bandtitle span { writing-mode: vertical-rl; font-size: 15px; font-weight: 700; letter-spacing: 6px; }
  .box { position: absolute; background: #ffffff; border: 1px solid #e2e8f0; border-left: 4px solid; border-radius: 8px; box-shadow: 0 1px 3px rgba(15,23,42,.08); display: flex; flex-direction: column; justify-content: center; padding: 8px 10px; }
  .bt { font-size: 13px; font-weight: 700; color: #0f172a; line-height: 1.2; }
  .bs { font-size: 11px; color: #64748b; margin-top: 3px; line-height: 1.35; }
  svg.links { position: absolute; left: 0; top: 0; width: %(W)dpx; height: %(H)dpx; pointer-events: none; }
  .elab { font-size: 10.5px; fill: #334155; paint-order: stroke; stroke: #ffffff; stroke-width: 3px; stroke-linejoin: round; }
  .legend { position: absolute; left: 30px; top: 940px; width: 1220px; display: flex; flex-wrap: wrap; gap: 8px 22px; align-items: center; }
  .legend .ttl { font-size: 12px; font-weight: 700; color:#334155; margin-right: 6px; }
  .lg { display: flex; align-items: center; gap: 6px; font-size: 12px; color:#475569; }
  .note { position: absolute; left: 30px; top: 1058px; width: 1220px; font-size: 12px; color: #64748b; line-height: 1.7; border-top: 1px dashed #cbd5e1; padding-top: 10px; }
</style>
</head>
<body>
<div id="canvas">
  <div class="hd"><h1>MVideo 系统架构图</h1><div class="sub">短视频点播(VoD) + 实时音视频(RTC) · 目标版</div><div class="tag">v1.0</div></div>
  %(bands)s
  <svg class="links" viewBox="0 0 %(W)d %(H)d" xmlns="http://www.w3.org/2000/svg"><defs>%(markers)s</defs>%(paths)s%(labels)s</svg>
  <div class="legend"><span class="ttl">协议图例</span>%(legend)s</div>
  <div class="note">说明：所有业务服务统一注册到「注册与配置中心」，读写 Redis / MySQL 并上报「可观测性」。媒体上传链路：客户端 → Media → 对象存储 / FFmpeg（经消息队列转码）→ 对象存储 → CDN 分发。实时音视频链路：客户端 → 连接网关 → RTC 信令 → SFU，媒体流经 STUN/TURN 穿透后走 WebRTC(UDP)。</div>
</div>
</body>
</html>
""" % dict(W=W, H=H, bands="".join(band_html(L) for L in layers), markers=marker_defs, paths="".join(svg_paths), labels="".join(svg_labels), legend=legend_html)

with open(HTML, "w", encoding="utf-8") as f:
    f.write(html)

print("wrote", HTML)
print("boxes", len(boxes), "edges", len(edges))
