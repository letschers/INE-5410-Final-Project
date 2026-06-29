#!/usr/bin/env python3
"""
Banquete Viking — gerador de visualização (replay em navegador).

Compila o projeto em modo debug (RELEASE=false), executa ./program capturando a
saída de plog() com carimbo de tempo, reconstrói o estado da mesa (cadeiras +
pratos) e gera um arquivo HTML autocontido (dados embutidos) que reproduz a
execução como uma animação de mesa circular.

NÃO altera nada dentro de vikings/ — apenas lê a saída de debug do programa.

Uso:
    python3 capture.py                      # parâmetros padrão (bons p/ assistir)
    python3 capture.py -v 30 -c 12 -e 500 -p 500
    python3 capture.py --no-build           # não recompila (usa binário existente)

Saída: viking-replay.html  (abra no navegador)
"""

import os
import re
import sys
import json
import time
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
VIKINGS = os.path.normpath(os.path.join(HERE, "..", "vikings"))
PROGRAM = os.path.join(VIKINGS, "program")
OUT_HTML = os.path.join(HERE, "viking-replay.html")

GOD_NAMES = ["BALDR", "LOKI", "VALI", "HODER", "FRIGG", "JORD", "ODIN", "THOR"]
GOD_INDEX = {g: i for i, g in enumerate(GOD_NAMES)}

# ----- regex dos eventos emitidos por plog() ----------------------------------
RE_CREATED = re.compile(r"\[horde\] Viking (\d+) created \(berserker=(\d+), type=(\d+)\)")
RE_EAT_START = re.compile(r"\[viking\] Viking=(\d+) is now eating \(chair=(\d+)\)")
RE_EAT_END = re.compile(r"\[viking\] Viking=(\d+) has finished eating \(chair=(\d+)\)")
RE_PRAY_START = re.compile(r"\[viking\] Viking=(\d+) is now praying to (\w+)")
RE_PRAY_END = re.compile(r"\[viking\] Viking=(\d+) has finished praying to (\w+)")
RE_GRANT = re.compile(r"\[chieftain\] grant seat=(\d+) plates=(\d+),(\d+) berserker=(\d+)")
RE_RUN = re.compile(r"\[viking\] Viking (\d+) is now running!")
RE_VINFO = re.compile(r"Number of vikings\s*:\s*(\d+)")
RE_CINFO = re.compile(r"Number of chairs\s*:\s*(\d+)")
RE_EINFO = re.compile(r"Maximum eat time\s*:\s*(\d+)")
RE_PINFO = re.compile(r"Maximum pray time\s*:\s*(\d+)")


def build_debug():
    env = dict(os.environ, RELEASE="false")
    subprocess.run(["make", "clean"], cwd=VIKINGS, env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    r = subprocess.run(["make"], cwd=VIKINGS, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout)
        sys.exit("Falha ao compilar o projeto em modo debug.")


def run_and_capture(args):
    """Executa o programa e devolve a lista de eventos (com tempo) + metadados."""
    meta = {"horde_size": None, "table_size": None, "max_eat": None, "max_pray": None}
    vikings = {}        # id -> {"berserker":0/1, "type":0/1}
    raw = []            # (t_ms, kind, ...)
    pending = {}        # chair -> fila de pares de pratos concedidos (ground truth)
    t0 = None

    proc = subprocess.Popen([PROGRAM] + args, cwd=VIKINGS,
                            stdout=subprocess.PIPE, text=True, bufsize=1)
    for line in proc.stdout:
        now = time.monotonic()
        line = line.rstrip("\n")

        m = RE_VINFO.search(line)
        if m: meta["horde_size"] = int(m.group(1)); continue
        m = RE_CINFO.search(line)
        if m: meta["table_size"] = int(m.group(1)); continue
        m = RE_EINFO.search(line)
        if m: meta["max_eat"] = int(m.group(1)); continue
        m = RE_PINFO.search(line)
        if m: meta["max_pray"] = int(m.group(1)); continue

        m = RE_CREATED.match(line)
        if m:
            vikings[int(m.group(1))] = {"berserker": int(m.group(2)), "type": int(m.group(3))}
            continue

        # Concessão de cadeira/pratos pelo chieftain (dado exato, não reconstruído).
        # Aparece sempre antes do "is now eating" da mesma cadeira (mesma thread),
        # e cadeiras são exclusivas, então uma fila por cadeira casa exatamente.
        m = RE_GRANT.match(line)
        if m:
            ch = int(m.group(1))
            pending.setdefault(ch, []).append([int(m.group(2)), int(m.group(3))])
            continue

        kind = chair = god = None
        m = RE_EAT_START.match(line)
        if m: kind, vid, chair = "eat_start", int(m.group(1)), int(m.group(2))
        if kind is None:
            m = RE_EAT_END.match(line)
            if m: kind, vid, chair = "eat_end", int(m.group(1)), int(m.group(2))
        if kind is None:
            m = RE_PRAY_START.match(line)
            if m: kind, vid, god = "pray_start", int(m.group(1)), GOD_INDEX[m.group(2)]
        if kind is None:
            m = RE_PRAY_END.match(line)
            if m: kind, vid, god = "pray_end", int(m.group(1)), GOD_INDEX[m.group(2)]
        if kind is None:
            # Thread iniciou: para vikings normais marca a entrada na fila por
            # cadeira (de "running" até "is now eating" o viking espera por lugar).
            m = RE_RUN.match(line)
            if m: kind, vid = "run", int(m.group(1))
        if kind is None:
            continue

        if t0 is None:
            t0 = now
        ev = {"t": round((now - t0) * 1000.0, 1), "kind": kind,
              "v": vid, "chair": chair, "god": god}

        if kind == "eat_start":
            n = meta["table_size"]
            q = pending.get(chair)
            ev["plates"] = q.pop(0) if q else [chair, (chair + 1) % n]
            cands, seen = [], set()
            for c in (chair, (chair - 1 + n) % n, (chair + 1) % n):
                if c not in seen:
                    seen.add(c); cands.append(c)
            ev["cands"] = cands

        raw.append(ev)

    proc.wait()
    return meta, vikings, raw


def repair_timeline(meta, raw):
    """
    Corrige inversões de carimbo de tempo no replay. As linhas "is now eating" /
    "has finished eating" são impressas FORA do lock (viking.c, congelado), então
    o relógio de parede pode, raramente, mostrar uma cadeira/prato sendo reusado
    um instante antes da liberação anterior — algo que o programa jamais permite.

    Reproduz a exclusão real: o início de uso de cada cadeira/prato é empurrado
    para não anteceder a liberação anterior do mesmo recurso. Casos sem inversão
    (a grande maioria) ficam intactos.
    """
    ends = {e["v"]: e["t"] for e in raw if e["kind"] == "eat_end"}
    res_free = {}  # ("c"|"p", idx) -> instante em que o recurso fica livre
    for s in sorted((e for e in raw if e["kind"] == "eat_start"), key=lambda e: e["t"]):
        endt = ends.get(s["v"], s["t"])
        resources = [("c", s["chair"])] + [("p", p) for p in s["plates"]]
        newstart = s["t"]
        for r in resources:
            newstart = max(newstart, res_free.get(r, 0.0))
        s["t"] = min(newstart, endt)  # nunca ultrapassa o próprio fim
        for r in resources:
            res_free[r] = max(res_free.get(r, 0.0), endt)


def main():
    passthrough = sys.argv[1:]
    do_build = True
    if "--no-build" in passthrough:
        do_build = False
        passthrough = [a for a in passthrough if a != "--no-build"]

    # Parâmetros padrão pensados para um replay fluido e legível.
    if not passthrough:
        passthrough = ["-v", "24", "-c", "12", "-e", "500", "-p", "500"]

    if do_build:
        print("Compilando em modo debug (RELEASE=false)...")
        build_debug()
    if not os.path.exists(PROGRAM):
        sys.exit("Binário ./program não encontrado. Rode sem --no-build.")

    print("Executando:", "./program", *passthrough)
    meta, vikings, raw = run_and_capture(passthrough)
    if not raw:
        sys.exit("Nenhum evento capturado. O binário foi compilado com RELEASE=false?")

    repair_timeline(meta, raw)
    # No mesmo instante, liberações (eat_end/pray_end) vêm antes de aquisições
    # (eat_start/pray_start), para o snapshot nunca ver um recurso ocupado por dois.
    order = {"eat_end": 0, "pray_end": 0, "run": 0, "eat_start": 1, "pray_start": 1}
    raw.sort(key=lambda e: (e["t"], order[e["kind"]]))

    n_total = len(vikings)
    n_late = sum(1 for x in vikings.values() if x["type"] == 1)
    counts = [0] * 8
    for ev in raw:
        if ev["kind"] == "pray_start":
            counts[ev["god"]] += 1

    run = {
        "meta": {
            "horde_size": meta["horde_size"], "table_size": meta["table_size"],
            "max_eat": meta["max_eat"], "max_pray": meta["max_pray"],
            "total_vikings": n_total, "late_vikings": n_late,
            "duration": raw[-1]["t"] if raw else 0,
            "gods": GOD_NAMES, "final_counts": counts,
        },
        "vikings": [{"berserker": vikings[i]["berserker"], "type": vikings[i]["type"]}
                    for i in range(n_total)],
        "events": raw,
    }

    html = TEMPLATE.replace("__RUN_DATA__", json.dumps(run))
    with open(OUT_HTML, "w") as f:
        f.write(html)

    print("\nPronto! Abra no navegador:")
    print(" ", OUT_HTML)
    print(f"\n  vikings: {n_total} ({n_total - n_late} no banquete + {n_late} atrasados)"
          f" | cadeiras: {meta['table_size']} | preces: {sum(counts)}")


# =============================================================================
#  Template HTML autocontido. "__RUN_DATA__" é substituído pelo JSON da execução.
# =============================================================================
TEMPLATE = r"""<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<title>Banquete Viking — Replay</title>
<style>
  :root{
    --bg:#14110d; --panel:#1f1a13; --line:#3a2f20; --txt:#e8dcc6;
    --normal:#4a90d9; --berserker:#d9534f; --plate:#e6a23c; --empty:#4d4234;
    --accent:#c8a24b; --good:#5cb85c; --bad:#d9534f; --super:#9b8cff;
  }
  *{box-sizing:border-box}
  html,body{height:100%;margin:0}
  body{background:var(--bg);color:var(--txt);overflow:hidden;
       font-family:"Segoe UI",system-ui,sans-serif;display:flex;flex-direction:column}
  header{flex:0 0 auto;padding:8px 16px;border-bottom:2px solid var(--line);
         display:flex;align-items:baseline;gap:12px;flex-wrap:wrap}
  header h1{font-size:17px;margin:0;color:var(--accent);letter-spacing:.5px;white-space:nowrap}
  header .sub{font-size:12px;opacity:.7}

  .wrap{flex:1 1 auto;min-height:0;display:grid;
        grid-template-columns:minmax(0,1fr) 372px;grid-template-rows:minmax(0,1fr);
        gap:12px;padding:12px}

  .card{background:var(--panel);border:1px solid var(--line);border-radius:10px}
  .tablecard{min-height:0;min-width:0;display:flex;align-items:center;
             justify-content:center;padding:8px;position:relative}
  .tablecard svg{width:100%;height:100%}
  .tablelegend{position:absolute;left:12px;bottom:8px;display:flex;gap:14px;
               font-size:11px;flex-wrap:wrap;opacity:.92}
  .tablelegend span{display:flex;align-items:center;gap:5px}
  .dot{width:11px;height:11px;border-radius:50%;display:inline-block}

  /* coluna lateral: ocupa a altura toda, rola por dentro se faltar espaço */
  .side{min-height:0;display:flex;flex-direction:column;gap:10px;overflow:hidden}
  .side .card{padding:10px 12px}
  .phase{font-size:14px;font-weight:700;letter-spacing:.4px}
  .phase.banquet{color:var(--plate)} .phase.prayers{color:var(--super)}
  .grid2{display:grid;grid-template-columns:1fr 1fr;gap:4px 14px;margin-top:6px;font-size:12px}
  .grid2 .k{opacity:.75} .grid2 b{color:#fff}

  .qhead{display:flex;justify-content:space-between;align-items:baseline;font-size:12px}
  .qhead .qn{color:#fff;font-weight:700;font-variant:tabular-nums}
  .qchips{display:flex;flex-wrap:wrap;gap:3px;margin-top:6px;max-height:54px;overflow:hidden}
  .chip{font-size:10px;line-height:1;padding:3px 5px;border-radius:4px;
        font-variant:tabular-nums;background:#0e0c09;border:1px solid var(--line)}
  .chip.nm{color:#cfe2f6;border-color:#2b4a66}
  .chip.bk{color:#f6cfcf;border-color:#6b2f2f}
  .chip.more{opacity:.7;border-style:dashed}

  .bars{margin-top:4px}
  .god{display:flex;align-items:center;gap:7px;margin:2px 0;font-size:11px}
  .god .name{width:42px;font-variant:tabular-nums}
  .god .track{flex:1;height:14px;background:#0e0c09;border-radius:4px;overflow:hidden;border:1px solid var(--line)}
  .god .fill{height:100%;width:0;background:var(--normal);transition:width .12s linear}
  .god.super .fill{background:var(--super)}
  .god .fill.bad{background:var(--bad)}
  .god .num{width:30px;text-align:right;font-variant:tabular-nums}
  .pairline{height:1px;background:var(--line);margin:5px 0}

  /* feed de decisões do chieftain — cresce e rola por dentro */
  .feedcard{flex:1 1 auto;min-height:80px;display:flex;flex-direction:column;padding:10px 12px}
  .feedcard h3{margin:0 0 6px;font-size:13px;color:var(--accent)}
  #feed{flex:1;overflow-y:auto;font-size:12px;line-height:1.45;padding-right:4px}
  .dec{padding:4px 8px;margin:3px 0;border-radius:6px;border-left:3px solid var(--line);
       background:#181309;animation:pop .25s ease}
  .dec.seat{border-left-color:var(--plate)}
  .dec.god{border-left-color:var(--super)}
  .dec .nm{color:var(--normal)} .dec .bk{color:var(--berserker)}
  .dec .sg{color:var(--super)} .dec .why{opacity:.72}
  @keyframes pop{from{opacity:0;transform:translateY(4px)}to{opacity:1}}

  .controls{flex:0 0 auto;display:flex;align-items:center;gap:12px;
            background:var(--panel);border-top:2px solid var(--line);
            padding:9px 16px;flex-wrap:wrap}
  button{background:var(--accent);color:#241c0a;border:none;border-radius:6px;
         padding:6px 13px;font-weight:700;cursor:pointer;font-size:13px}
  button:hover{filter:brightness(1.1)}
  input[type=range]{accent-color:var(--accent)}
  .clock{font-variant:tabular-nums;font-size:13px;opacity:.85;min-width:120px}
  #scrub{flex:1;min-width:140px}
  .tag{font-size:11px;padding:2px 7px;border-radius:10px;background:#2b2419;border:1px solid var(--line)}
  .chair{stroke:#0c0a07;stroke-width:2}
  .chair.empty{fill:var(--empty)}
  .chair.normal{fill:var(--normal)}
  .chair.berserker{fill:var(--berserker)}
  .chair.eating{stroke:var(--plate);stroke-width:4}
  .clabel{fill:#fff;font-weight:600;text-anchor:middle;dominant-baseline:central;pointer-events:none}
  .plate{fill:#2b2419;stroke:#5a4b34;stroke-width:1.5}
  .plate.used{fill:var(--plate);stroke:#8a6a23}
  .gap-label{fill:var(--accent);text-anchor:middle;opacity:.85}
  .hold{stroke:var(--plate);stroke-width:1.6;opacity:.6}
</style>
</head>
<body>
<header>
  <h1>⚔️ Banquete Viking</h1>
  <span class="sub" id="cfg"></span>
</header>

<div class="wrap">
  <div class="card tablecard">
    <svg id="table" viewBox="0 0 600 600" preserveAspectRatio="xMidYMid meet"></svg>
    <div class="tablelegend">
      <span><i class="dot" style="background:var(--normal)"></i>Normal</span>
      <span><i class="dot" style="background:var(--berserker)"></i>Berserker</span>
      <span><i class="dot" style="background:var(--empty)"></i>Cadeira vazia</span>
      <span><i class="dot" style="background:var(--plate)"></i>Prato em uso</span>
    </div>
  </div>

  <div class="side">
    <div class="card">
      <div class="phase" id="phase">—</div>
      <div class="grid2">
        <span class="k">Comendo agora</span><b id="s-eating">0</b>
        <span class="k">Aguardando lugar</span><b id="s-wait">0</b>
        <span class="k">Já comeram</span><b id="s-done">0</b>
        <span class="k">Rezando agora</span><b id="s-pray">0</b>
        <span class="k">Preces feitas</span><b id="s-prayed">0</b>
        <span class="k">Pratos em uso</span><b id="s-plates">0</b>
      </div>
    </div>

    <div class="card">
      <div class="qhead">
        <span style="color:var(--accent);font-weight:700">⏳ Fila por cadeira</span>
        <span class="qn" id="q-count">0</span>
      </div>
      <div class="qchips" id="q-chips"></div>
      <div style="font-size:10px;opacity:.6;margin-top:4px">
        vikings que já iniciaram mas ainda esperam um lugar (não é FIFO: o monitor
        acorda quem couber primeiro).
      </div>
    </div>

    <div class="card">
      <div style="font-size:12px;color:var(--accent);font-weight:700;margin-bottom:2px">
        Preces aos deuses</div>
      <div class="bars" id="bars"></div>
    </div>

    <div class="card feedcard">
      <h3>🧠 Decisões do chieftain</h3>
      <div id="feed"></div>
    </div>
  </div>
</div>

<div class="controls">
  <button id="play">▶ Play</button>
  <button id="prev" title="Evento anterior">⏮ Passo</button>
  <button id="next" title="Próximo evento">Passo ⏭</button>
  <button id="restart">⟲ Reiniciar</button>
  <span class="clock" id="clock">0.0s / 0.0s</span>
  <input type="range" id="scrub" min="0" max="1000" value="0">
  <span class="tag">Velocidade</span>
  <input type="range" id="speed" min="0.25" max="8" step="0.25" value="1.5" style="width:110px">
  <span class="tag" id="speedval">1.5×</span>
</div>

<script>
const RUN = __RUN_DATA__;
const N = RUN.meta.table_size;
const EV = RUN.events;
const GODS = RUN.meta.gods;
const DUR = Math.max(1, RUN.meta.duration);
const firstPray = (EV.find(e=>e.kind==="pray_start")||{t:DUR}).t;

// Fila por cadeira: do "running" até o "eat_start", o viking normal espera lugar.
// Indexamos por POSIÇÃO no array de eventos (não por tempo): o passo a passo
// aplica exatamente um evento por clique, mesmo quando vários compartilham o
// mesmo carimbo de tempo (ex.: a leva inicial que senta quase ao mesmo tempo).
const runIdx={}, eatIdx={};
for(let i=0;i<EV.length;i++){
  const e=EV[i];
  if(e.kind==="run"){ if(runIdx[e.v]===undefined) runIdx[e.v]=i; }
  else if(e.kind==="eat_start"){ eatIdx[e.v]=i; }
}
const NORMALS=[];
for(let id=0; id<RUN.vikings.length; id++) if(RUN.vikings[id].type===0) NORMALS.push(id);

document.getElementById("cfg").textContent =
  `${RUN.meta.total_vikings} vikings (${RUN.meta.total_vikings-RUN.meta.late_vikings} banquete + `
  + `${RUN.meta.late_vikings} atrasados) · ${N} cadeiras · come≤${RUN.meta.max_eat}ms · reza≤${RUN.meta.max_pray}ms`;

// ---- decisões pré-computadas (reconstruídas dos eventos + tabelas correntes) -
const DECISIONS = (function(){
  const out=[]; const counts=new Array(8).fill(0);
  for(let i=0;i<EV.length;i++){
    const e=EV[i];
    if(e.kind==="eat_start"){
      const t=RUN.vikings[e.v];
      const role = t.berserker ? "<span class=bk>Berserker</span>" : "<span class=nm>Normal</span>";
      const reach = (e.cands||[]).join(", ");
      out.push({evi:i, cls:"seat",
        html:`🪑 <b>Viking ${e.v}</b> ${role} → cadeira <b>${e.chair}</b>`
            +`<div class="why">pegou pratos {${(e.plates||[]).join(", ")}} dos alcançáveis {${reach}};`
            +` vizinhos respeitam a regra do berserker</div>`});
    } else if(e.kind==="pray_start"){
      const g=e.god, c=counts[g];
      let html;
      if(g<6){
        const pr=g^1, cp=counts[pr];
        const lo=Math.min(g,pr), hi=Math.max(g,pr);
        const pair=`${GODS[lo]}/${GODS[hi]}`;
        html=`🙏 <b>Viking ${e.v}</b> → <b>${GODS[g]}</b>`
            +`<div class="why">par ${pair} estava ${counts[lo]}×${counts[hi]} → reforça o menor,`
            +` mantém a diferença ≤ 5%</div>`;
      } else {
        const tn=counts.slice(0,6).reduce((a,b)=>a+b,0);
        const cap=Math.ceil(tn*1.10);
        html=`🙏 <b>Viking ${e.v}</b> → <b>${GODS[g]}</b> <span class=sg>super</span>`
            +`<div class="why">${c+1}/${cap} preces — limite é 10% da soma dos 6 (=${tn})</div>`;
      }
      out.push({evi:i, cls:"god", html});
      counts[g]++;
    }
  }
  return out;
})();

// ---- geometria da mesa (gap/vão no topo) -----------------------------------
const CX=300, CY=312, R_CHAIR=236, R_PLATE=162;
const R_SEAT = Math.max(13, Math.min(30, 150/Math.max(6,N)*2.0));
const R_PD   = Math.max(6, R_SEAT*0.42);
const step = 2*Math.PI/(N+1);          // um slot vira o vão
const ang = i => -Math.PI/2 + step*(i+1);
const pos = (i,r)=>[CX + r*Math.cos(ang(i)), CY + r*Math.sin(ang(i))];

const svg = document.getElementById("table");
const NS = "http://www.w3.org/2000/svg";
function el(tag, attrs){const e=document.createElementNS(NS,tag);
  for(const k in attrs) e.setAttribute(k,attrs[k]); return e;}

// churrasqueira central
svg.appendChild(el("circle",{cx:CX,cy:CY,r:72,fill:"#2a1d10",stroke:"#5a4b34","stroke-width":2}));
const fire=el("text",{x:CX,y:CY+2,"text-anchor":"middle","dominant-baseline":"central","font-size":"38"});
fire.textContent="🔥"; svg.appendChild(fire);
const flbl=el("text",{x:CX,y:CY+38,"text-anchor":"middle","font-size":"11",fill:"#8a7355"});
flbl.textContent="churrasqueira"; svg.appendChild(flbl);

// marcador do vão (entre cadeira N-1 e cadeira 0, no topo)
const [gx,gy]=[CX, CY - R_CHAIR];
svg.appendChild(el("line",{x1:CX,y1:CY-90,x2:gx,y2:gy-12,stroke:"var(--accent)",
  "stroke-width":2,"stroke-dasharray":"4 4",opacity:.6}));
const gl=el("text",{x:CX,y:gy-20,class:"gap-label","font-size":"11"});
gl.textContent="vão (berserker-safe)"; svg.appendChild(gl);

// pratos + cadeiras + ligações
const holdLines=[], plateEls=[], chairEls=[], labelEls=[];
for(let i=0;i<N;i++){
  const ln=el("line",{class:"hold",visibility:"hidden"}); svg.appendChild(ln); holdLines.push(ln);
}
for(let i=0;i<N;i++){
  const [px,py]=pos(i,R_PLATE);
  const p=el("circle",{cx:px,cy:py,r:R_PD,class:"plate"}); svg.appendChild(p); plateEls.push(p);
}
for(let i=0;i<N;i++){
  const [cx,cy]=pos(i,R_CHAIR);
  const c=el("circle",{cx:cx,cy:cy,r:R_SEAT,class:"chair empty"}); svg.appendChild(c); chairEls.push(c);
  const t=el("text",{x:cx,y:cy,class:"clabel","font-size":Math.round(R_SEAT*0.9)});
  svg.appendChild(t); labelEls.push(t);
}

// ---- barras de preces ------------------------------------------------------
const bars=document.getElementById("bars");
const fillEls=[], numEls=[];
const layout=[0,1,-1,2,3,-1,4,5,-2,6,-1,7];
layout.forEach(g=>{
  if(g<0){const d=document.createElement("div");d.className="pairline";bars.appendChild(d);return;}
  const row=document.createElement("div"); row.className="god"+((g>=6)?" super":"");
  const nm=document.createElement("span"); nm.className="name"; nm.textContent=GODS[g];
  const tr=document.createElement("div"); tr.className="track";
  const fl=document.createElement("div"); fl.className="fill"; tr.appendChild(fl);
  const num=document.createElement("span"); num.className="num"; num.textContent="0";
  row.append(nm,tr,num); bars.appendChild(row);
  fillEls[g]=fl; numEls[g]=num;
});

// ---- estado após aplicar os primeiros K eventos ----------------------------
function stateUpTo(K){
  const chair=new Array(N).fill(null);
  const plateUsed=new Array(N).fill(false);
  const counts=new Array(8).fill(0);
  let started=0, finished=0, prayingNow=0, prayed=0;
  for(let i=0;i<K;i++){
    const e=EV[i];
    if(e.kind==="eat_start"){started++; chair[e.chair]={v:e.v,plates:e.plates||[]};
      (e.plates||[]).forEach(p=>plateUsed[p]=true);}
    else if(e.kind==="eat_end"){finished++; const c=chair[e.chair];
      if(c){(c.plates||[]).forEach(p=>plateUsed[p]=false); chair[e.chair]=null;}}
    else if(e.kind==="pray_start"){prayingNow++; counts[e.god]++;}
    else if(e.kind==="pray_end"){prayingNow--; prayed++;}
  }
  return {chair,plateUsed,counts,started,finished,prayingNow,prayed};
}
const vikType=id=>RUN.vikings[id];
const ceilTol=(x,r)=>Math.ceil(x*(1+r)), floorTol=(x,r)=>Math.floor(x*(1-r));
// nº de eventos com t <= clock (deriva K a partir do tempo no play/scrub).
const countLE=clk=>{ let n=0; while(n<EV.length && EV[n].t<=clk) n++; return n; };

const feed=document.getElementById("feed");
let lastFeedK=-1;
function renderFeed(K){
  let n=0; while(n<DECISIONS.length && DECISIONS[n].evi<K) n++;
  if(n===lastFeedK) return;               // evita reflow desnecessário
  lastFeedK=n;
  const recent=DECISIONS.slice(Math.max(0,n-45),n);
  feed.innerHTML=recent.map(d=>`<div class="dec ${d.cls}">${d.html}</div>`).join("");
  feed.scrollTop=feed.scrollHeight;
}

const qCount=document.getElementById("q-count"), qChips=document.getElementById("q-chips");
function renderQueue(K){
  const waiting=[];
  for(const id of NORMALS){
    const ri=runIdx[id];             if(ri===undefined || ri>=K) continue; // ainda não iniciou
    const ei=eatIdx[id];             if(ei!==undefined && ei<K) continue;  // já sentou
    waiting.push(id);
  }
  qCount.textContent=waiting.length;
  const CAP=48, shown=waiting.slice(0,CAP);
  let html=shown.map(id=>`<span class="chip ${RUN.vikings[id].berserker?'bk':'nm'}">${id}</span>`).join("");
  if(waiting.length>CAP) html+=`<span class="chip more">+${waiting.length-CAP}</span>`;
  qChips.innerHTML=html;
  return waiting.length;
}

function render(){
  const st=stateUpTo(K);
  for(let i=0;i<N;i++){
    const occ=st.chair[i], c=chairEls[i], lab=labelEls[i];
    if(occ){const t=vikType(occ.v);
      c.setAttribute("class","chair "+(t.berserker?"berserker":"normal")+" eating");
      lab.textContent=occ.v;
    }else{c.setAttribute("class","chair empty"); lab.textContent="";}
  }
  for(let i=0;i<N;i++)
    plateEls[i].setAttribute("class","plate"+(st.plateUsed[i]?" used":""));
  let li=0;
  for(let i=0;i<N;i++) holdLines[i].setAttribute("visibility","hidden");
  for(let i=0;i<N;i++){
    const occ=st.chair[i]; if(!occ) continue;
    occ.plates.forEach(p=>{ if(li<holdLines.length){
      const ln=holdLines[li++]; const [cx,cy]=pos(i,R_CHAIR), [px,py]=pos(p,R_PLATE);
      ln.setAttribute("x1",cx);ln.setAttribute("y1",cy);
      ln.setAttribute("x2",px);ln.setAttribute("y2",py);
      ln.setAttribute("visibility","visible"); } });
  }
  const ph=document.getElementById("phase");
  if(clock<firstPray){ph.textContent="🍖 BANQUETE — todos comem antes de rezar";ph.className="phase banquet";}
  else{ph.textContent="🙏 PRECES — banquete encerrado";ph.className="phase prayers";}

  document.getElementById("s-eating").textContent=st.started-st.finished;
  document.getElementById("s-wait").textContent=renderQueue(K);
  document.getElementById("s-done").textContent=st.finished;
  document.getElementById("s-pray").textContent=st.prayingNow;
  document.getElementById("s-prayed").textContent=st.prayed;
  document.getElementById("s-plates").textContent=st.plateUsed.filter(Boolean).length;

  const maxc=Math.max(1,...st.counts);
  const totalNormal=st.counts.slice(0,6).reduce((a,b)=>a+b,0);
  for(let g=0;g<8;g++){
    if(!fillEls[g]) continue;
    const c=st.counts[g];
    fillEls[g].style.width=(100*c/maxc)+"%"; numEls[g].textContent=c;
    let bad=false;
    if(g<6){const rc=st.counts[g^1]; if(c>ceilTol(rc,0.05)||c<floorTol(rc,0.05)) bad=true;}
    else{if(c>Math.ceil(totalNormal*1.10)) bad=true;}
    fillEls[g].classList.toggle("bad",bad);
  }
  renderFeed(K);
  document.getElementById("clock").textContent=(clock/1000).toFixed(1)+"s / "+(DUR/1000).toFixed(1)+"s";
  scrub.value=Math.round(1000*clock/DUR);
}

// ---- loop de animação ------------------------------------------------------
// clock = tempo contínuo (play/scrub); K = nº de eventos aplicados, master da
// renderização. No play/scrub, K é derivado do tempo; no passo a passo, K
// avança/recua de 1 em 1 e o tempo acompanha o evento.
let clock=0, K=0, playing=false, speed=1.5, last=null;
const scrub=document.getElementById("scrub");
const playBtn=document.getElementById("play");
const speedSlider=document.getElementById("speed");
function pause(){playing=false;playBtn.textContent="▶ Play";}
function frame(ts){
  if(last==null) last=ts;
  const dt=ts-last; last=ts;
  if(playing){
    clock+=dt*speed;
    if(clock>=DUR){clock=DUR;pause();}
    K=countLE(clock); render();
  }
  requestAnimationFrame(frame);
}
playBtn.onclick=()=>{
  if(K>=EV.length){clock=0;K=0;}                 // recomeça se já terminou
  playing=!playing; playBtn.textContent=playing?"⏸ Pausar":"▶ Play";
};
document.getElementById("restart").onclick=()=>{clock=0;K=0;pause();render();};
scrub.oninput=()=>{clock=DUR*scrub.value/1000;K=countLE(clock);pause();render();};
speedSlider.oninput=()=>{speed=parseFloat(speedSlider.value);
  document.getElementById("speedval").textContent=speed+"×";};
// Passo a passo: aplica/retira EXATAMENTE um evento por clique.
function stepBy(dir){
  K=Math.max(0,Math.min(EV.length,K+dir));
  clock = K>0 ? EV[K-1].t : 0;
  pause(); render();
}
document.getElementById("next").onclick=()=>stepBy(1);
document.getElementById("prev").onclick=()=>stepBy(-1);

render(); requestAnimationFrame(frame);
</script>
</body>
</html>
"""

if __name__ == "__main__":
    main()
