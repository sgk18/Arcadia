"use client";
import { useEffect, useRef, useState, useCallback } from "react";

/* ─────────────────────────────────────────────────────────── types ── */
interface PlayerStats { username:string; hp:number; max_hp:number; attack:number; defense:number; gold:number; level:number; }
interface ItemEntry   { name:string; value:number; rarity:number; }
interface QuestEntry  { title:string; description:string; }
interface SkillEntry  { name:string; power:number; active:boolean; }
interface GameState   { player:PlayerStats; location:string; exits:string[]; inventory:ItemEntry[]; quests:QuestEntry[]; skills:SkillEntry[]; shop:ItemEntry[]; events:string[]; error:string|null; _state?:Record<string,unknown>; }

async function callGame(command:string,username:string,args:string[]=[],state?:Record<string,unknown>):Promise<GameState> {
  const res = await fetch("/api/game",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({command,username,args,state})});
  return await res.json() as GameState;
}

/* ───────────────────────────────────────────────────── static data ── */
const ROOM_NAMES  = ["Sunlit Entrance","Forgemaster Armory","Whispering Library","Crystal Sanctum"];
const ROOM_ICONS  = ["☀","⚔","📚","💎"];
const ROOM_COORDS = ["79.2 / 44.1","42.1 / 88.3","65.0 / 31.7","51.8 / 77.4"];
const ROOM_POS    = [{x:350,y:360},{x:145,y:175},{x:465,y:268},{x:545,y:100}];
const ROOM_ADJ    = [[{dir:"N",dest:1},{dir:"E",dest:2}],[{dir:"S",dest:0},{dir:"E",dest:2}],[{dir:"W",dest:1},{dir:"N",dest:3}],[{dir:"S",dest:2}]];
const MAP_EDGES:number[][] = [[0,1],[0,2],[1,2],[2,3]];

const RARITY_NAME  = ["","Common","Uncommon","Rare","Epic","Legendary"];
const RARITY_CLASS = ["","r-common","r-uncommon","r-rare","r-epic","r-legendary"];

function getRoomId(loc:string):number { const i=ROOM_NAMES.indexOf(loc); return i>=0?i:0; }
function getTravelDir(from:number,to:number):string|null { return ROOM_ADJ[from]?.find(l=>l.dest===to)?.dir??null; }

/* ──────────────────────────────────────────────────── Spinner ── */
function Spinner(){
  return <svg className="spin" style={{width:13,height:13,flexShrink:0}} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth={2.5}><circle cx={12} cy={12} r={9} strokeOpacity={.25}/><path d="M12 3a9 9 0 0 1 9 9"/></svg>;
}

/* ──────────────────────────────────────────────── SynthwaveHero ── */
function SynthwaveHero(){
  return (
    <svg viewBox="0 0 800 280" style={{display:"block",width:"100%",borderRadius:"8px 8px 0 0"}}>
      <defs>
        <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%"   stopColor="#0a0e1a"/>
          <stop offset="22%"  stopColor="#18092e"/>
          <stop offset="42%"  stopColor="#6b2040"/>
          <stop offset="57%"  stopColor="#c4522a"/>
          <stop offset="65%"  stopColor="#e8813a"/>
          <stop offset="72%"  stopColor="#c4522a"/>
          <stop offset="86%"  stopColor="#3d1030"/>
          <stop offset="100%" stopColor="#0d1117"/>
        </linearGradient>
        <clipPath id="sc"><circle cx="400" cy="142" r="72"/></clipPath>
      </defs>
      <rect width="800" height="280" fill="url(#sky)"/>
      <ellipse cx="400" cy="142" rx="112" ry="90" fill="rgba(232,129,58,.28)"/>
      <circle cx="400" cy="142" r="72" fill="#e07830"/>
      {[0,1,2,3,4,5,6,7].map(i=>(
        <line key={i} x1="328" y1={154+i*9} x2="472" y2={154+i*9}
              stroke="#0d1117" strokeWidth={i<2?3:2} opacity={.5+i*.04} clipPath="url(#sc)"/>
      ))}
      <polygon points="0,232 70,152 155,212 240,132 315,182 375,122 435,175 505,135 575,185 648,140 715,192 800,145 800,280 0,280" fill="#0d2535" opacity=".93"/>
      <polygon points="0,265 55,218 125,250 200,192 272,236 355,182 425,227 495,186 562,232 642,191 722,226 800,196 800,280 0,280" fill="#060f1a"/>
      <g opacity=".3">
        {[0,1,2,3,4].map(i=><line key={i} x1="0" y1={255+i*6} x2="800" y2={255+i*6} stroke="#cc4a2a" strokeWidth=".8"/>)}
        {[-8,-6,-4,-2,0,2,4,6,8].map(i=><line key={i} x1={400+i*55} y1={250} x2={400+i*235} y2={282} stroke="#cc4a2a" strokeWidth=".8"/>)}
      </g>
      <text x="400" y="116" textAnchor="middle" fontFamily="ui-monospace,monospace" fontWeight="900" fontSize="66" fontStyle="italic" letterSpacing="10" fill="none" stroke="#38bdf8" strokeWidth="1.8" opacity=".88">ARCADIA</text>
      <text x="400" y="116" textAnchor="middle" fontFamily="ui-monospace,monospace" fontWeight="900" fontSize="66" fontStyle="italic" letterSpacing="10" fill="#3b82f6" opacity=".45">ARCADIA</text>
      <line x1="330" y1="126" x2="470" y2="126" stroke="#3b82f6" strokeWidth="1.5" opacity=".6"/>
    </svg>
  );
}

/* ──────────────────────────────────────────────── LoginScreen ── */
function LoginScreen({onLogin}:{onLogin:(n:string)=>void}){
  const [mode,setMode]       = useState<null|"input">(null);
  const [name,setName]       = useState("");
  const [loading,setLoading] = useState(false);
  const [err,setErr]         = useState<string|null>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  useEffect(()=>{if(mode==="input")inputRef.current?.focus();},[mode]);

  async function handleSubmit(e:React.FormEvent){
    e.preventDefault();
    const n=name.trim()||"Traveler";
    setLoading(true);setErr(null);
    try{
      const gs=await callGame("init",n);
      if(gs.error)setErr(gs.error);else onLogin(n);
    }catch(ex){setErr(String(ex));}
    finally{setLoading(false);}
  }

  return(
    <div className="min-h-screen bg-[#0a0e1a] flex flex-col" style={{fontFamily:"ui-monospace,monospace"}}>
      {/* header */}
      <header className="flex items-center justify-between px-5 py-3 border-b border-[#1e2d45]">
        <div className="flex items-center gap-2">
          <div className="w-6 h-6 rounded bg-blue-600 flex items-center justify-center font-black text-white text-xs">A</div>
          <span className="font-bold text-sm text-[#e2e8f0] tracking-wider">ARCADIA OS</span>
        </div>
        <div className="flex items-center gap-3">
          <span className="flex items-center gap-1.5 text-[11px] text-[#64748b]">
            <span className="dot-green"/>SESSION: ACTIVE
          </span>
          <span className="badge">v2.0.4-STABLE</span>
        </div>
      </header>

      <div className="flex-1 flex flex-col items-center justify-center px-4 py-10 gap-5 w-full max-w-md mx-auto">
        {/* hero card */}
        <div className="w-full rounded-xl overflow-hidden border border-[#1e2d45]" style={{boxShadow:"0 0 40px rgba(37,99,235,.1)"}}>
          <SynthwaveHero/>
          <div className="bg-[#111827] px-5 py-3 flex items-center justify-between border-t border-[#1e2d45]">
            <span className="text-[11px] text-[#64748b] uppercase tracking-widest flex items-center gap-2">
              <span className="live-dot"/>SYSTEM ONLINE
            </span>
            <span className="text-[11px] text-[#334155]">©2077 ARCADIA CORP</span>
          </div>
        </div>

        {/* buttons / form */}
        {mode===null?(
          <div className="w-full space-y-2.5">
            <button className="btn-os btn-primary w-full py-3" onClick={()=>setMode("input")}>▶ New Game</button>
            <button className="btn-os btn-secondary w-full py-3 justify-between" onClick={()=>setMode("input")}>
              <span>↺ Load Save</span><span className="badge">ALGO: DIJKSTRA</span>
            </button>
            <button className="btn-os btn-secondary w-full py-3 justify-between" disabled>
              <span>⊞ Leaderboard</span><span className="badge">SEARCH: BINARY</span>
            </button>
          </div>
        ):(
          <form onSubmit={handleSubmit} className="w-full space-y-3">
            <label className="block text-[11px] text-[#64748b] uppercase tracking-widest">Enter access handle</label>
            <div className="flex items-center gap-2 border border-[#1e2d45] bg-[#0f1728] rounded-lg px-4 py-3 focus-within:border-blue-500 transition-colors">
              <span className="text-blue-400 text-sm">$</span>
              <input ref={inputRef} value={name} onChange={e=>setName(e.target.value)}
                     placeholder="Traveler" maxLength={31}
                     className="flex-1 bg-transparent text-[#e2e8f0] text-sm outline-none placeholder:text-[#334155]"/>
            </div>
            {err&&<p className="text-red-400 text-xs flex items-center gap-1">✕ {err}</p>}
            <div className="flex gap-2">
              <button type="button" className="btn-os btn-ghost-os flex-1" onClick={()=>{setMode(null);setErr(null);}}>← Back</button>
              <button type="submit" className="btn-os btn-primary flex-1" disabled={loading}>
                {loading&&<Spinner/>}{loading?"Initializing…":"Enter Arcadia ▶"}
              </button>
            </div>
          </form>
        )}

        <button className="text-[11px] text-[#334155] hover:text-[#64748b] transition-colors bg-transparent border-none cursor-pointer uppercase tracking-widest">
          System Settings
        </button>
      </div>

      <footer className="flex items-center justify-center flex-wrap gap-5 px-5 py-3 border-t border-[#1e2d45] text-[10px] text-[#334155]">
        <span className="flex items-center gap-1.5"><span className="dot-green"/>Kernel Stable</span>
        <span className="flex items-center gap-1.5"><span className="dot-blue"/>DB: Connected</span>
        <span>LATENCY: 12ms • NODE STATUS: HEALTHY</span>
      </footer>
    </div>
  );
}

/* ──────────────────────────────────────────────── WorldMapSVG ── */
function WorldMapSVG({gs,onTravel,busy}:{gs:GameState;onTravel:(dir:string)=>void;busy:boolean}){
  // `clicked` = user click-locked a destination; `hovered` = transient mouse-over
  const [clicked,setClicked] = useState<number|null>(null);
  const [hovered,setHovered] = useState<number|null>(null);
  const currentId   = getRoomId(gs.location);
  const adjacentIds = ROOM_ADJ[currentId]?.map(l=>l.dest)??[];

  // reset selection whenever the room changes
  useEffect(()=>{ setClicked(null); setHovered(null); },[gs.location]);

  // active selection: click takes priority over hover
  const selected = clicked ?? hovered;

  function handleClick(id:number){
    if(id===currentId||!adjacentIds.includes(id))return;
    // toggle click lock
    setClicked(prev=>prev===id?null:id);
  }

  function handleCancel(){
    setClicked(null);
    setHovered(null);
  }

  const dest = (selected!==null&&adjacentIds.includes(selected))
    ? {id:selected,name:ROOM_NAMES[selected],dir:getTravelDir(currentId,selected)}
    : null;

  return(
    <div className="flex-1 flex flex-col" style={{minHeight:0}}>
      {/* breadcrumb */}
      <div className="flex items-center justify-between mb-3 shrink-0">
        <p className="text-[11px] text-[#64748b] uppercase tracking-widest">
          <span className="text-[#334155]">World</span>
          <span className="mx-2 text-[#1e2d45]">/</span>
          <span className="text-[#60a5fa]">Adjacency Graph</span>
        </p>
        <div className="flex items-center gap-2 bg-[#0f1728] border border-[#1e2d45] rounded-lg px-3 py-1.5">
          <span className="text-[#334155] text-xs">⌕</span>
          <span className="text-[#334155] text-[11px]">Search Rooms…</span>
        </div>
      </div>

      {/* map */}
      <div className="relative flex-1 card-dark overflow-hidden" style={{minHeight:320}}>
        <svg viewBox="0 0 700 430" className="w-full h-full" style={{minHeight:320}}>
          {MAP_EDGES.map(([a,b],i)=>{
            const pa=ROOM_POS[a],pb=ROOM_POS[b];
            const live=(a===currentId||b===currentId);
            return <line key={i} x1={pa.x} y1={pa.y} x2={pb.x} y2={pb.y}
              stroke={live?"#1e3a5f":"#151f35"} strokeWidth={live?1.5:1}
              strokeDasharray="6 4" opacity={live?.9:.45}/>;
          })}
          {ROOM_NAMES.map((_,id)=>{
            const p=ROOM_POS[id], cur=id===currentId, adj=adjacentIds.includes(id);
            const isClicked=id===clicked, isHovered=id===hovered&&!isClicked;
            const sel=isClicked||isHovered;
            return(
              <g key={id} style={{cursor:adj?"pointer":"default"}}>
                {cur&&<circle cx={p.x} cy={p.y} r="40" fill="none" stroke="#2563eb" strokeWidth="1" className="map-node-ring" style={{pointerEvents:"none"}}/>}
                {isClicked&&<circle cx={p.x} cy={p.y} r={30} fill="none" stroke="#3b82f6" strokeWidth="1.5" strokeDasharray="4 3" className="map-node-ring" style={{pointerEvents:"none"}}/>}
                <circle cx={p.x} cy={p.y} r={cur?28:adj?21:16}
                  fill={cur?"#1e3a5f":isClicked?"#1a3060":isHovered?"#162540":"#0f1728"}
                  stroke={cur?"#3b82f6":isClicked?"#60a5fa":isHovered?"#2563eb":adj?"#1e3a5f":"#151f35"}
                  strokeWidth={cur?2.5:isClicked?2.5:isHovered?2:1.5}
                  style={{pointerEvents:"none"}}/>
                <text x={p.x} y={p.y+1} textAnchor="middle" dominantBaseline="middle"
                  fontSize={cur?17:12}
                  fill={cur?"#60a5fa":sel?"#93c5fd":adj?"#475569":"#2d3d55"}
                  style={{pointerEvents:"none"}}>{ROOM_ICONS[id]}</text>
                <text x={p.x} y={p.y+(cur?40:29)} textAnchor="middle"
                  fontSize="9" fontFamily="ui-monospace,monospace" fontWeight={cur?"700":sel?"600":"400"}
                  fill={cur?"#60a5fa":sel?"#93c5fd":adj?"#475569":"#2d3d55"}
                  style={{pointerEvents:"none"}}>
                  {ROOM_NAMES[id].toUpperCase()}
                </text>
                {/* fixed-size transparent hit area — never resizes, so no twitching */}
                <circle cx={p.x} cy={p.y} r={44} fill="transparent" stroke="none"
                  onClick={()=>handleClick(id)}
                  onMouseEnter={()=>{if(adj&&!cur)setHovered(id);}}
                  onMouseLeave={()=>setHovered(null)}/>
              </g>
            );
          })}
        </svg>
        {/* zoom controls (decorative) */}
        <div className="absolute bottom-4 left-4 flex flex-col gap-1.5">
          {["+","−","⊙"].map(s=>(
            <div key={s} className="card w-8 h-8 flex items-center justify-center text-[#64748b] text-sm hover:text-[#94a3b8] transition-colors cursor-pointer select-none">{s}</div>
          ))}
        </div>
      </div>

      {/* travel panel */}
      {dest?.dir&&(
        <div className="card p-4 mt-3 border-[#1e3a5f]">
          <div className="flex items-center justify-between mb-3">
            <div>
              <p className="text-[9px] text-[#64748b] uppercase tracking-widest mb-0.5">Destination</p>
              <p className="text-blue-400 font-bold text-sm">{dest.name}</p>
            </div>
            <div className="text-right">
              <p className="text-[9px] text-[#64748b] uppercase tracking-widest mb-0.5">Direction</p>
              <p className="text-[#e2e8f0] font-bold text-sm">{dest.dir}</p>
            </div>
          </div>
          <div className="flex gap-2">
            <button className="btn-os btn-ghost-os flex-1 text-xs py-2" onClick={handleCancel}>Cancel</button>
            <button className="btn-os btn-primary flex-1 text-xs py-2" disabled={busy}
                    onClick={()=>{onTravel(dest.dir!);handleCancel();}}>
              {busy?<Spinner/>:null} Initiate Travel
            </button>
          </div>
        </div>
      )}

      <div className="flex items-center gap-4 mt-3 text-[10px] text-[#334155] shrink-0">
        <span className="flex items-center gap-1.5"><span className="dot-green" style={{width:5,height:5}}/>System: Nominal</span>
        <span>Grid Latency: 4ms</span>
        <span className="ml-auto text-[#1e2d45]">ARC_OS v4.2.0 • STABLE</span>
      </div>
    </div>
  );
}

/* ──────────────────────────────────────── PlayerStatsView (ARCADIA OS) ── */
function PlayerStatsView({gs,busy}:{gs:GameState;busy:boolean}){
  const {player} = gs;
  const hpPct    = player.max_hp>0?Math.round((player.hp/player.max_hp)*100):0;
  const hpColor  = hpPct>60?"#22c55e":hpPct>30?"#f59e0b":"#ef4444";

  const metrics = [
    {label:"STRENGTH",   value:player.attack*5,  max:100, color:"#3b82f6",  desc:"Sub-dermal hydraulic reinforcements active."},
    {label:"AGILITY",    value:Math.min(100,player.defense*7+22),max:100,color:"#22c55e",desc:"Synaptic response time: 0.04ms."},
    {label:"INTELLIGENCE",value:Math.min(100,player.level*2+50), max:100,color:"#a855f7",desc:"Secondary processing core enabled."},
  ] as const;

  const neuralLinks = [
    {icon:"👁",label:"Enhanced Optical Overlay",desc:"Project tactical data and enemy health bars directly onto the retina.",status:"ACTIVE",statusClass:"badge-green"},
    {icon:"⚡",label:"Adrenaline Governor",     desc:"Reduces stamina drain by 25% during high-intensity combat encounters.",status:"PASSIVE",statusClass:"badge"},
    {icon:"🔒",label:"Deep-Net Breach",         desc:"Requires Level 50. Bypass advanced security nodes from distance.",status:"LOCKED",statusClass:"badge-red",locked:true},
  ];

  return(
    <div className="flex-1 flex flex-col gap-4">
      {/* subject card */}
      <div className="card p-5 flex items-center gap-5">
        <div className="w-20 h-20 rounded-lg border-2 border-blue-500 bg-[#0f1728] flex items-center justify-center text-3xl flex-shrink-0 relative" style={{boxShadow:"0 0 16px rgba(59,130,246,.25)"}}>
          {(player.username[0]??"T").toUpperCase()}
          <span className="absolute bottom-1 right-1 w-3 h-3 rounded-full bg-green-400 border-2 border-[#0f1728]"/>
        </div>
        <div className="flex-1">
          <div className="flex items-center gap-3 mb-2">
            <h2 className="text-xl font-bold text-[#e2e8f0]">Subject #{player.username}</h2>
            <span className="badge">LEVEL {player.level} OPERATIVE</span>
          </div>
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
            {[
              {label:"NEURAL SYNC",value:`${hpPct}.${(hpPct%10).toString().padStart(1,"0")}%`,color:"#60a5fa"},
              {label:"CLASS",      value:"Infiltrator",   color:"#e2e8f0"},
              {label:"REGION",     value:gs.location,    color:"#e2e8f0"},
              {label:"KARMA",      value:player.gold>200?"Benevolent":player.gold>100?"Neutral":"Chaotic",color:"#fbbf24"},
            ].map(r=>(
              <div key={r.label}>
                <p className="text-[10px] text-[#64748b] uppercase tracking-widest">{r.label}</p>
                <p className="font-bold text-sm mt-0.5" style={{color:r.color}}>{r.value}</p>
              </div>
            ))}
          </div>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4 flex-1">
        {/* Cognitive metrics */}
        <div className="card p-5 flex flex-col gap-4">
          <div className="flex items-center justify-between">
            <p className="text-[10px] text-[#64748b] uppercase tracking-widest">⚙ Cognitive &amp; Physical Metrics</p>
            <span className="badge-green text-[9px]">LIVE UPDATES</span>
          </div>
          <div className="space-y-5">
            {metrics.map(m=>{
              const pct=Math.round((m.value/m.max)*100);
              return(
                <div key={m.label}>
                  <div className="flex justify-between mb-1.5">
                    <span className="text-xs font-bold text-[#94a3b8] tracking-widest">{m.label}</span>
                    <span className="text-xs font-bold tabular-nums" style={{color:m.color}}>{m.value}<span className="text-[#334155]">/{m.max}</span></span>
                  </div>
                  <div className="bar-wrap">
                    <div className="bar-fill" style={{width:pct+"%",background:m.color}}/>
                  </div>
                  <p className="text-[10px] text-[#475569] mt-1">{m.desc}</p>
                </div>
              );
            })}
          </div>
          {/* HP */}
          <div className="card-dark rounded-lg px-4 py-3 flex items-center justify-between gap-4">
            <div>
              <p className="text-[9px] text-[#64748b] uppercase tracking-widest mb-1">HP</p>
              <div className="hp-bar-wrap w-32"><div className="hp-bar-fill" style={{width:hpPct+"%",background:hpColor}}/></div>
            </div>
            <span className="text-sm font-bold tabular-nums text-[#e2e8f0]">{player.hp}<span className="text-[#334155]">/{player.max_hp}</span></span>
          </div>
        </div>

        {/* Neural links */}
        <div className="card p-5 flex flex-col gap-4">
          <div className="flex items-center justify-between">
            <p className="text-[10px] text-[#64748b] uppercase tracking-widest">✦ Neural Links</p>
          </div>
          <div className="grid grid-cols-2 gap-3 flex-1">
            {neuralLinks.map((nl,i)=>(
              <div key={i} className={"neural-card"+(nl.locked?" opacity-60":"")}>
                <div className="flex items-center justify-between">
                  <span className="text-2xl">{nl.icon}</span>
                  <span className={nl.statusClass}>{nl.status}</span>
                </div>
                <p className="text-xs font-bold text-[#e2e8f0] mt-1">{nl.label}</p>
                <p className="text-[10px] text-[#64748b] leading-relaxed">{nl.desc}</p>
              </div>
            ))}
            <button className="neural-card-add" disabled={busy}>
              <span className="text-2xl">⊕</span>
              <span className="text-[11px] font-semibold uppercase tracking-widest">Install New Link</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

/* ──────────────────────────────────────────── Activity Log view ── */
function ActivityLogView({gs,dispatch,busy}:{gs:GameState;dispatch:(c:string,a?:string[])=>void;busy:boolean}){
  const {player} = gs;
  const hpPct    = player.max_hp>0?Math.round((player.hp/player.max_hp)*100):0;
  const active   = gs.skills.find(s=>s.active);

  const evtIcons = (ev:string)=>{
    if(ev.startsWith("Victory")||ev.includes("defeated"))return{icon:"✓",bg:"bg-green-500"};
    if(ev.includes("fallen")||ev.includes("claws"))      return{icon:"⚠",bg:"bg-red-500"};
    if(ev.includes("Traveled"))                          return{icon:"→",bg:"bg-blue-500"};
    if(ev.includes("Purchased"))                         return{icon:"◈",bg:"bg-purple-500"};
    if(ev.includes("skill")||ev.includes("Learned"))     return{icon:"⚡",bg:"bg-yellow-500"};
    return{icon:"●",bg:"bg-[#334155]"};
  };

  return(
    <div className="flex-1 flex gap-4">
      {/* feed */}
      <div className="flex-1 flex flex-col gap-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <p className="text-sm font-bold text-[#e2e8f0]">Recent Buffer Records</p>
            <span className="badge-green flex items-center gap-1.5"><span className="live-dot"/>LIVE FEED</span>
          </div>
          <div className="flex gap-1">
            {["All","Combat","Loot","Milestones"].map(t=>(
              <button key={t} className={"btn-os py-1 px-3 text-[10px] "+(t==="All"?"btn-primary":"btn-ghost-os")} disabled>{t}</button>
            ))}
          </div>
        </div>

        {gs.events.length===0
          ? <div className="card-dark rounded-lg p-6 text-center text-[#334155] text-sm">No recent activity.</div>
          : <div className="space-y-2">
              {[...gs.events].reverse().map((ev,i)=>{
                const {icon,bg}=evtIcons(ev);
                const ago=["2M","6M","13M","1H","8H"];
                return(
                  <div key={i} className="evt-card">
                    <div className={`w-8 h-8 rounded-full ${bg} flex items-center justify-center text-white text-sm flex-shrink-0`}>{icon}</div>
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center justify-between gap-2">
                        <p className="text-[#e2e8f0] text-sm font-semibold truncate">{ev}</p>
                        <span className="badge text-[9px] shrink-0">{ago[i]??""} AGO</span>
                      </div>
                      {ev.includes("Victory")&&<p className="text-[11px] text-[#64748b] mt-1">⚡ +{player.gold}g &nbsp;❤ {player.hp} HP</p>}
                    </div>
                  </div>
                );
              })}
            </div>
        }
        <button className="btn-os btn-ghost-os w-full text-xs py-2.5 mt-auto">↓ Load Older History</button>
      </div>

      {/* right panel */}
      <div className="w-60 shrink-0 flex flex-col gap-4">
        {/* session summary */}
        <div className="card p-4">
          <p className="text-[10px] text-[#64748b] uppercase tracking-widest mb-3">⊡ Session Summary</p>
          {[
            {label:"Time Played",   value:"2h 45m",        color:"text-[#e2e8f0]"},
            {label:"Total XP Gained",value:"+"+player.gold*148,color:"text-green-400"},
            {label:"Enemies Slain", value:String(player.level*3), color:"text-[#e2e8f0]"},
            {label:"Rare Items",    value:String(gs.inventory.filter(x=>x.rarity>=3).length), color:"text-blue-400"},
          ].map(r=>(
            <div key={r.label} className="flex justify-between py-2 border-b border-[#1a2a3f] last:border-0">
              <span className="text-[11px] text-[#64748b]">{r.label}</span>
              <span className={"text-[11px] font-bold "+r.color}>{r.value}</span>
            </div>
          ))}
          <div className="mt-3 flex gap-1">
            {[["#22c55e","COMBAT"],["#f59e0b","LOOT"],["#a855f7","LEVEL"]].map(([c,l])=>(
              <div key={l} className="flex-1 text-center">
                <div className="h-1.5 rounded-full mb-1" style={{background:c}}/>
                <p className="text-[8px] text-[#334155] uppercase">{l}</p>
              </div>
            ))}
          </div>
        </div>

        {/* combat button */}
        <div className="card p-4 flex flex-col gap-3">
          <p className="text-[10px] text-[#64748b] uppercase tracking-widest">⚔ Combat</p>
          {active&&(
            <div className="card-dark rounded-lg px-3 py-2 flex items-center justify-between">
              <span className="text-[10px] text-blue-400">Active:</span>
              <span className="text-[11px] font-bold text-[#e2e8f0]">{active.name}</span>
              <span className="text-[10px] text-[#64748b]">{active.power} PWR</span>
            </div>
          )}
          <div className="bar-wrap-sm">
            <div className="bar-fill" style={{width:hpPct+"%",background:hpPct>60?"#22c55e":hpPct>30?"#f59e0b":"#ef4444"}}/>
          </div>
          <p className="text-[10px] text-[#334155]">HP: {player.hp}/{player.max_hp}</p>
          <button className="btn-os btn-danger w-full py-2.5" disabled={busy} onClick={()=>dispatch("combat")}>
            {busy?<><Spinner/><span>Processing…</span></>:<span>⚔ Engage Combat</span>}
          </button>
          <button className="btn-os btn-secondary w-full text-xs py-2" disabled={busy||gs.skills.length===0} onClick={()=>dispatch("skill",["rotate"])}>⟳ Rotate Skill</button>
        </div>

        {/* upgrade hint */}
        {gs.shop.length>0&&(
          <div className="card p-4 flex flex-col gap-2 border-[#1e3a5f]" style={{background:"rgba(30,58,95,.15)"}}>
            <p className="text-[10px] text-[#64748b] uppercase tracking-widest">Upgrade Hint</p>
            <p className="text-[11px] text-[#94a3b8]">Best available: <span className="text-[#60a5fa] font-bold">{gs.shop[0].name}</span></p>
            <p className="text-[10px] text-[#64748b]">{RARITY_NAME[gs.shop[0].rarity]??""} · {gs.shop[0].value}g</p>
          </div>
        )}
      </div>
    </div>
  );
}

/* ──────────────────────────────────────────── QuestsView ── */
function QuestsView({gs,dispatch,busy}:{gs:GameState;dispatch:(c:string,a?:string[])=>void;busy:boolean}){
  const active=gs.skills.find(s=>s.active);
  return(
    <div className="flex-1 grid grid-cols-1 lg:grid-cols-2 gap-4">
      {/* quests */}
      <div className="card p-5 flex flex-col gap-4">
        <p className="text-[10px] text-[#64748b] uppercase tracking-widest">Active Quests</p>
        {gs.quests.length===0
          ?<p className="text-[#334155] text-sm">No active quests.</p>
          :<ul className="space-y-3">
            {gs.quests.map((q,i)=>(
              <li key={i} className="border-l-2 border-blue-800 pl-3">
                <p className="text-[#e2e8f0] text-sm font-semibold">{q.title}</p>
                <p className="text-[#64748b] text-xs mt-0.5">{q.description}</p>
              </li>
            ))}
          </ul>
        }
      </div>

      {/* skills + combat */}
      <div className="flex flex-col gap-4">
        <div className="card p-5 flex flex-col gap-3">
          <p className="text-[10px] text-[#64748b] uppercase tracking-widest">Skill Ring</p>
          <ul className="space-y-1.5">
            {gs.skills.map((s,i)=>(
              <li key={i} className={"flex items-center justify-between rounded-md px-3 py-2 text-sm "+(s.active?"bg-blue-900/30 border border-blue-800/50 text-blue-300":"text-[#475569]")}>
                <span className="flex items-center gap-2">{s.active&&<span className="text-blue-400 text-xs">▶</span>}{s.name}</span>
                <span className="text-xs tabular-nums">{s.power} PWR</span>
              </li>
            ))}
          </ul>
          <button className="btn-os btn-secondary w-full text-xs py-2" disabled={busy||gs.skills.length===0} onClick={()=>dispatch("skill",["rotate"])}>⟳ Rotate Skill</button>
        </div>
        <div className="card p-5 flex flex-col gap-3">
          <p className="text-[10px] text-[#64748b] uppercase tracking-widest">Combat</p>
          {active&&<div className="flex items-center gap-2 p-2 card-dark rounded-md"><span className="text-blue-400 text-xs">Active:</span><span className="text-[#e2e8f0] text-sm font-semibold">{active.name}</span><span className="text-[#64748b] text-xs ml-auto">{active.power} PWR</span></div>}
          <button className="btn-os btn-danger w-full py-3" disabled={busy} onClick={()=>dispatch("combat")}>
            {busy?<><Spinner/><span>Processing…</span></>:<span>⚔ Engage Combat</span>}
          </button>
        </div>
      </div>
    </div>
  );
}

/* ──────────────────────────────────────────── InventoryView ── */
function InventoryView({gs,dispatch,busy}:{gs:GameState;dispatch:(c:string,a?:string[])=>void;busy:boolean}){
  return(
    <div className="flex-1 grid grid-cols-1 lg:grid-cols-2 gap-4">
      {/* inventory */}
      <div className="card p-5 flex flex-col gap-4">
        <p className="text-[10px] text-[#64748b] uppercase tracking-widest">Inventory ({gs.inventory.length})</p>
        {gs.inventory.length===0
          ?<p className="text-[#334155] text-sm">Empty.</p>
          :<ul className="space-y-2 overflow-y-auto" style={{maxHeight:380}}>
            {gs.inventory.map((item,i)=>(
              <li key={i} className="flex items-center justify-between card-dark rounded-lg px-3 py-2.5">
                <div>
                  <p className="text-[#e2e8f0] text-sm font-semibold">{item.name}</p>
                  <p className={"text-[10px] mt-0.5 "+(RARITY_CLASS[item.rarity]??"r-common")}>{RARITY_NAME[item.rarity]??""}</p>
                </div>
                <span className="text-[#64748b] text-xs tabular-nums">{item.value}g</span>
              </li>
            ))}
          </ul>
        }
      </div>

      {/* shop */}
      <div className="card p-5 flex flex-col gap-4">
        <div className="flex items-center justify-between">
          <p className="text-[10px] text-[#64748b] uppercase tracking-widest">Shop — Max Heap</p>
          <button className="btn-os btn-primary text-xs py-1.5 px-4" disabled={busy||gs.shop.length===0} onClick={()=>dispatch("buy")}>Buy Best</button>
        </div>
        {gs.shop.length===0
          ?<p className="text-[#334155] text-sm">Shop is empty.</p>
          :<ul className="space-y-2">
            {gs.shop.map((item,i)=>(
              <li key={i} className={"flex items-center justify-between rounded-lg px-3 py-2.5 "+(i===0?"bg-blue-900/20 border border-blue-800/40":"card-dark")}>
                <div>
                  <div className="flex items-center gap-1.5">{i===0&&<span className="text-yellow-400 text-[10px]">★</span>}<p className="text-[#e2e8f0] text-sm">{item.name}</p></div>
                  <p className={"text-[10px] mt-0.5 "+(RARITY_CLASS[item.rarity]??"r-common")}>{RARITY_NAME[item.rarity]??""}</p>
                </div>
                <span className="text-[#64748b] text-xs">{item.value}g</span>
              </li>
            ))}
          </ul>
        }
      </div>
    </div>
  );
}

/* ──────────────────────────────────────── Leaderboard view ── */
function LeaderboardView({gs}:{gs:GameState}){
  const fakeRows = [
    {rank:1,name:"ZeroIndex_00",id:"88219",level:99,score:14205,wr:98.2},
    {rank:2,name:"Logarithm_X", id:"44012",level:92,score:12880,wr:94.5},
    {rank:3,name:"BinarySearcher",id:"12998",level:88,score:11450,wr:92.1},
    {rank:4,name:"PivotElement",id:"33120",level:85,score:9940, wr:89.9},
    {rank:5,name:"DivideAndConquer",id:"77102",level:81,score:8200,wr:85.4},
  ];
  const rankClass=(r:number)=>r===1?"rank-gold":r===2?"rank-silver":r===3?"rank-bronze":"rank-default";

  const [filter,setFilter]=useState<"all"|"season">("season");

  return(
    <div className="flex-1 flex flex-col gap-4">
      {/* header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-[#e2e8f0]">Global Rankings</h2>
          <p className="text-[11px] text-[#64748b] flex items-center gap-1.5 mt-0.5"><span className="text-blue-400">⚡</span>Optimized by Binary Search Algorithm for real-time sorting</p>
        </div>
        <div className="flex gap-2">
          <button className={"btn-os py-2 px-4 text-xs "+(filter==="all"?"btn-secondary":"btn-primary")}   onClick={()=>setFilter("all")}>All Time</button>
          <button className={"btn-os py-2 px-4 text-xs "+(filter==="season"?"btn-primary":"btn-secondary")} onClick={()=>setFilter("season")}>Season 12</button>
        </div>
      </div>

      {/* search */}
      <div className="flex items-center gap-3 card-dark rounded-lg px-4 py-3">
        <span className="text-[#334155]">⌕</span>
        <span className="text-[#334155] text-sm flex-1">Binary Search: [Low, High] indexing for player name…</span>
        <span className="badge text-[9px]">CMD + K</span>
      </div>

      {/* table */}
      <div className="card overflow-hidden">
        <div className="grid text-[10px] text-[#64748b] uppercase tracking-wider px-5 py-3 border-b border-[#1e2d45]"
             style={{gridTemplateColumns:"60px 1fr 110px 160px 100px"}}>
          <span>Rank</span><span>Player Name</span><span>Level</span><span>Achievement Score</span><span>Win Rate</span>
        </div>
        {fakeRows.map(row=>(
          <div key={row.rank} className="grid items-center px-5 py-4 border-b border-[#1a2a3f] last:border-0 hover:bg-[#111827] transition-colors"
               style={{gridTemplateColumns:"60px 1fr 110px 160px 100px"}}>
            <span className={"w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold "+rankClass(row.rank)}>{row.rank}</span>
            <div className="flex items-center gap-3">
              <div className="w-8 h-8 rounded-full bg-[#0f1728] border border-[#1e2d45] flex items-center justify-center text-sm">
                {row.name[0]}
              </div>
              <div>
                <p className="text-[#e2e8f0] text-sm font-semibold">{row.name}</p>
                <p className="text-[#334155] text-[10px]">ID: #{row.id}</p>
              </div>
            </div>
            <span className="text-blue-400 text-sm font-bold">Level {row.level}</span>
            <span className="flex items-center gap-1.5 text-sm font-bold text-[#e2e8f0]">
              <span className="text-blue-400 text-xs">●</span>{row.score.toLocaleString()}
            </span>
            <span className="text-[#e2e8f0] text-sm">{row.wr}%</span>
          </div>
        ))}
      </div>

      {/* you */}
      <div className="card-dark rounded-lg px-5 py-3 flex items-center gap-4 border border-blue-800/30">
        <span className="badge-blue">You</span>
        <span className="text-[#e2e8f0] text-sm font-bold">{gs.player.username}</span>
        <span className="text-[#64748b] text-xs">Level {gs.player.level}</span>
        <span className="text-[#64748b] text-xs ml-auto">Score: {gs.player.gold*100}</span>
      </div>

      <div className="text-[10px] text-center text-[#334155]">
        Showing 1–5 of 24,052 players &nbsp;•&nbsp; Algorithm: Binary Search O(log n) &nbsp;•&nbsp; Last Sorted: just now
      </div>
    </div>
  );
}

/* ──────────────────────────────────────────────── Main game ── */
export default function ArcadiaGame(){
  const [username,setUsername] = useState<string|null>(null);
  const [gs,setGs]             = useState<GameState|null>(null);
  const [busy,setBusy]         = useState(false);
  const [screenFlash,setScreenFlash] = useState(false);
  const [activeTab,setActiveTab]     = useState<"map"|"log"|"quests"|"inventory"|"leaderboard"|"rankings">("map");
  const logRef = useRef<HTMLDivElement>(null);

  useEffect(()=>{if(logRef.current)logRef.current.scrollTop=logRef.current.scrollHeight;},[gs?.events]);

  const dispatch = useCallback(async(command:string,args:string[]=[])=>{
    if(!username||busy)return;
    setBusy(true);
    try{
      const next=await callGame(command,username,args,gs?._state as Record<string,unknown>|undefined);
      setGs(next);
      if(command==="combat"){setScreenFlash(true);setTimeout(()=>setScreenFlash(false),500);}
      try{localStorage.setItem("arcadia_"+username,JSON.stringify(next));}catch{/* ok */}
    }catch{/* ok */}
    finally{setBusy(false);}
  },[username,busy,gs]);

  function handleLogin(name:string){
    setUsername(name);
    try{
      const s=localStorage.getItem("arcadia_"+name);
      if(s){const p=JSON.parse(s) as GameState;if(p?.player){setGs(p);return;}}
    }catch{/* ok */}
    setBusy(true);
    callGame("init",name)
      .then(s=>{setGs(s);try{localStorage.setItem("arcadia_"+name,JSON.stringify(s));}catch{/* ok */}})
      .finally(()=>setBusy(false));
  }

  if(!username)return <LoginScreen onLogin={handleLogin}/>;
  if(!gs)return(
    <div className="min-h-screen bg-[#0a0e1a] flex items-center justify-center" style={{fontFamily:"ui-monospace,monospace"}}>
      <div className="flex items-center gap-3 text-[#64748b] text-sm"><Spinner/><span>Loading world data…</span></div>
    </div>
  );

  const {player,location} = gs;
  const hpPct    = player.max_hp>0?Math.round((player.hp/player.max_hp)*100):0;
  const hpColor  = hpPct>60?"#22c55e":hpPct>30?"#f59e0b":"#ef4444";
  const xpPct    = Math.min(100,((player.level-1)/99)*100);
  const currentRoomId = getRoomId(location);

  const TABS:[string,typeof activeTab][] = [
    ["Map",         "map"],
    ["Player",      "log"],
    ["Activity Log","leaderboard"],
    ["Rankings",    "rankings"],
    ["Quests",      "quests"],
    ["Inventory",   "inventory"],
  ];

  return(
    <div className="min-h-screen bg-[#0a0e1a] flex flex-col select-none" style={{fontFamily:"ui-monospace,monospace"}}>
      {screenFlash&&<div className="screen-flash"/>}

      {/* ── top header ── */}
      <header className="flex items-center justify-between px-5 py-3 border-b border-[#1e2d45] shrink-0 bg-[#0d1117]">
        <div className="flex items-center gap-3">
          <div className="w-6 h-6 rounded bg-blue-600 flex items-center justify-center font-black text-white text-xs">A</div>
          <span className="font-bold text-sm text-[#e2e8f0] tracking-wider">ARCADIA TERMINAL</span>
        </div>
        <nav className="flex items-center gap-1 sm:gap-5">
          {TABS.map(([label,tab])=>(
            <button key={tab} onClick={()=>setActiveTab(tab)} className={"nav-tab"+(activeTab===tab?" active":"")}>
              {label}
            </button>
          ))}
          <div className="w-7 h-7 rounded-md card flex items-center justify-center text-[#64748b] hover:text-[#94a3b8] transition-colors cursor-pointer text-sm">⚙</div>
          <div className="w-7 h-7 rounded-full bg-blue-700 flex items-center justify-center font-bold text-white text-xs cursor-pointer">
            {(player.username[0]??"T").toUpperCase()}
          </div>
        </nav>
      </header>

      <div className="flex flex-1 overflow-hidden">
        {/* ── sidebar ── */}
        <aside className="w-52 shrink-0 border-r border-[#1e2d45] flex flex-col p-4 gap-3 overflow-y-auto bg-[#0d1117]">
          {/* avatar */}
          <div className="flex flex-col items-center gap-2 pb-3 border-b border-[#1e2d45]">
            <div className="w-14 h-14 rounded-full bg-blue-900/50 border-2 border-blue-700 flex items-center justify-center font-black text-blue-300 text-2xl"
                 style={{boxShadow:"0 0 12px rgba(59,130,246,.25)"}}>
              {(player.username[0]??"T").toUpperCase()}
            </div>
            <div className="text-center">
              <p className="text-[#e2e8f0] font-bold text-sm">{player.username}</p>
              <span className="badge text-[9px] py-0 mt-0.5 inline-block">LEVEL {player.level} OPERATIVE</span>
            </div>
            <div className="w-full hp-bar-wrap"><div className="xp-bar-fill" style={{width:xpPct+"%"}}/></div>
          </div>

          {/* nav */}
          <nav className="flex flex-col gap-1">
            <p className="text-[9px] text-[#334155] uppercase tracking-widest px-3 pt-1 mb-0.5">Navigation</p>
            {([
              ["🗺 World Map","map"],
              ["⚙ Player Stats","log"],
              ["📋 Activity Log","leaderboard"],
              ["🏆 Rankings","rankings"],
              ["📜 Quests","quests"],
              ["🎒 Inventory","inventory"],
            ] as [string,typeof activeTab][]).map(([label,tab])=>(
              <button key={tab} onClick={()=>setActiveTab(tab)} className={"sidebar-item"+(activeTab===tab?" active":"")}>
                {label}
              </button>
            ))}
            <div className="border-t border-[#1e2d45] my-1"/>
            <button className="sidebar-item" disabled={busy} onClick={()=>dispatch("combat")}>⚔ Combat</button>
          </nav>

          {/* stats */}
          <div className="mt-auto flex flex-col gap-2 pt-3 border-t border-[#1e2d45]">
            <div className="flex items-center justify-between text-[10px] text-[#64748b] uppercase">
              <span>HP</span><span className="text-[#e2e8f0] tabular-nums">{player.hp}/{player.max_hp}</span>
            </div>
            <div className="hp-bar-wrap"><div className="hp-bar-fill" style={{width:hpPct+"%",background:hpColor}}/></div>
            <div className="grid grid-cols-2 gap-1.5 mt-1">
              {([["ATK",player.attack],["DEF",player.defense],["GOLD",player.gold],["LVL",player.level]] as [string,number][]).map(([l,v])=>(
                <div key={l} className="card-dark rounded px-2 py-1.5 text-center">
                  <p className="text-[#e2e8f0] font-bold text-sm tabular-nums">{v}</p>
                  <p className="text-[#475569] text-[9px] uppercase">{l}</p>
                </div>
              ))}
            </div>
            <div className="card-dark rounded-lg px-3 py-2">
              <p className="text-[9px] text-[#475569] uppercase tracking-widest mb-0.5">Current Sector</p>
              <p className="text-[#60a5fa] text-xs font-semibold">{location}</p>
              <p className="text-[10px] text-[#334155] mt-0.5">{ROOM_COORDS[currentRoomId]??""}</p>
            </div>
            <button className="btn-os btn-ghost-os w-full text-xs py-2" onClick={()=>{setUsername(null);setGs(null);}}>← Logout</button>
          </div>
        </aside>

        {/* ── main content ── */}
        <main className="flex-1 overflow-y-auto p-5 flex flex-col gap-4 bg-[#0a0e1a]">
          {activeTab==="map"&&<WorldMapSVG gs={gs} onTravel={dir=>dispatch("move",[dir])} busy={busy}/>}

          {activeTab==="log"&&(
            <>
              <p className="text-[10px] text-[#64748b] uppercase tracking-widest shrink-0 flex items-center gap-2">
                <span className="text-[#334155]">World</span><span className="text-[#1e2d45]">/</span><span className="text-[#60a5fa]">Player Stats</span>
              </p>
              <PlayerStatsView gs={gs} busy={busy}/>
            </>
          )}

          {activeTab==="leaderboard"&&(
            <>
              <p className="text-[10px] text-[#64748b] uppercase tracking-widest shrink-0 flex items-center gap-2">
                <span className="text-[#334155]">World</span><span className="text-[#1e2d45]">/</span><span className="text-[#60a5fa]">Activity Log</span>
              </p>
              <ActivityLogView gs={gs} dispatch={dispatch} busy={busy}/>
            </>
          )}

          {activeTab==="rankings"&&(
            <>
              <p className="text-[10px] text-[#64748b] uppercase tracking-widest shrink-0 flex items-center gap-2">
                <span className="text-[#334155]">World</span><span className="text-[#1e2d45]">/</span><span className="text-[#60a5fa]">Global Rankings</span>
              </p>
              <LeaderboardView gs={gs}/>
            </>
          )}

          {activeTab==="quests"&&(
            <>
              <p className="text-[10px] text-[#64748b] uppercase tracking-widest shrink-0 flex items-center gap-2">
                <span className="text-[#334155]">World</span><span className="text-[#1e2d45]">/</span><span className="text-[#60a5fa]">Quests &amp; Skills</span>
              </p>
              <QuestsView gs={gs} dispatch={dispatch} busy={busy}/>
            </>
          )}

          {activeTab==="inventory"&&(
            <>
              <p className="text-[10px] text-[#64748b] uppercase tracking-widest shrink-0 flex items-center gap-2">
                <span className="text-[#334155]">World</span><span className="text-[#1e2d45]">/</span><span className="text-[#60a5fa]">Inventory &amp; Shop</span>
              </p>
              <InventoryView gs={gs} dispatch={dispatch} busy={busy}/>
            </>
          )}

          {/* system status log */}
          {gs.events.length>0&&(
            <div ref={logRef} className="card-dark rounded-lg p-4 shrink-0 space-y-1 overflow-y-auto" style={{maxHeight:90}}>
              <p className="text-[9px] text-[#475569] uppercase tracking-widest mb-2">System Log</p>
              {gs.events.map((ev,i)=>{
                const _i=i;void _i;const last=i===gs.events.length-1;
                const cls=ev.startsWith("Victory")?"text-yellow-400":ev.includes("fallen")?"text-red-400":last?"text-[#93c5fd]":"text-[#475569]";
                return<p key={i} className={"text-[11px] "+cls}><span className="text-[#2d3d55] mr-2">{String(i+1).padStart(2,"0")}.</span>{ev}</p>;
              })}
            </div>
          )}
        </main>
      </div>

      {/* ── footer ── */}
      <footer className="flex items-center justify-between px-5 py-2 border-t border-[#1e2d45] text-[10px] text-[#334155] bg-[#0d1117] shrink-0">
        <div className="flex items-center gap-4">
          <span className="flex items-center gap-1.5"><span className="dot-green"/>Algorithm: Binary Search O(log n)</span>
          <span className="hidden sm:flex items-center gap-1.5"><span className="dot-blue"/>Last Sorted: 12ms ago</span>
        </div>
        <div className="flex items-center gap-1.5"><span className="dot-green"/>Node Status: Healthy</div>
      </footer>
    </div>
  );
}