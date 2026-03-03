"use client";

import { useEffect, useRef, useState, useCallback } from "react";

/* ------------------------------------------------------------------ */
/*  Types (mirrors C JSON output)                                       */
/* ------------------------------------------------------------------ */
interface PlayerStats {
  username: string;
  hp: number;
  max_hp: number;
  attack: number;
  defense: number;
  gold: number;
  level: number;
}

interface ItemEntry {
  name: string;
  value: number;
  rarity: number;
}

interface QuestEntry {
  title: string;
  description: string;
}

interface SkillEntry {
  name: string;
  power: number;
  active: boolean;
}

interface GameState {
  player: PlayerStats;
  location: string;
  exits: string[];
  inventory: ItemEntry[];
  quests: QuestEntry[];
  skills: SkillEntry[];
  shop: ItemEntry[];
  events: string[];
  error: string | null;
}

/* ------------------------------------------------------------------ */
/*  API helper                                                          */
/* ------------------------------------------------------------------ */
async function callGame(
  command: string,
  username: string,
  args: string[] = []
): Promise<GameState> {
  const res = await fetch("/api/game", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ command, username, args }),
  });
  return (await res.json()) as GameState;
}

/* ------------------------------------------------------------------ */
/*  Micro components                                                    */
/* ------------------------------------------------------------------ */
function PanelHeader({ label, color = "green" }: { label: string; color?: "green" | "cyan" | "amber" }) {
  const cls =
    color === "cyan" ? "text-cyan-400 glow-cyan"
    : color === "amber" ? "text-amber-400 glow-amber"
    : "text-green-400 glow-green";
  return (
    <h2 className={`font-mono text-xs tracking-widest uppercase mb-2 pb-1 border-b border-current/20 ${cls}`}>
      ▸ {label}
    </h2>
  );
}

function HpBar({ hp, max_hp }: { hp: number; max_hp: number }) {
  const pct = max_hp > 0 ? Math.max(0, Math.min(100, (hp / max_hp) * 100)) : 0;
  const color = pct > 60 ? "#00ff41" : pct > 30 ? "#ffb300" : "#ff1744";
  const glowClass = pct > 60 ? "glow-green" : pct > 30 ? "glow-amber" : "glow-red";
  return (
    <div className="flex items-center gap-2 w-full">
      <div className="flex-1 h-2 bg-[#0f1a0f] rounded-full overflow-hidden border border-[#1a3a1a]">
        <div
          className="h-full rounded-full hp-pulse transition-all duration-500"
          style={{ width: `${pct}%`, background: color, boxShadow: `0 0 6px ${color}` }}
        />
      </div>
      <span className={`font-mono text-xs tabular-nums ${glowClass}`} style={{ color }}>
        {hp}/{max_hp}
      </span>
    </div>
  );
}

const RARITY_LABELS = ["", "Common", "Uncommon", "Rare", "Epic", "Legendary"];
const RARITY_COLORS = ["", "text-slate-400", "text-green-400", "text-blue-400", "text-purple-400", "text-amber-400"];
function RarityBadge({ rarity }: { rarity: number }) {
  return (
    <span className={`font-mono text-[10px] uppercase tracking-wider ${RARITY_COLORS[rarity] ?? "text-slate-400"}`}>
      {RARITY_LABELS[rarity] ?? "???"}
    </span>
  );
}

function Spinner() {
  return (
    <svg className="animate-spin w-3 h-3" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth={2.5}>
      <circle cx={12} cy={12} r={9} strokeOpacity={0.25} />
      <path d="M12 3a9 9 0 0 1 9 9" />
    </svg>
  );
}

function StatRow({ label, value, color }: { label: string; value: number; color: string }) {
  return (
    <div className="flex items-center justify-between rounded px-2 py-1 bg-[#050d05] border border-green-950">
      <span className="text-green-800 text-[10px] uppercase tracking-wider">{label}</span>
      <span className={`font-black text-sm tabular-nums ${color}`}>{value}</span>
    </div>
  );
}

function StatChip({ icon, label, value, color }: { icon: string; label: string; value: number; color: string }) {
  return (
    <div className="flex items-center gap-1">
      <span>{icon}</span>
      <span className="text-green-800 text-[10px] uppercase">{label}</span>
      <span className={`font-black tabular-nums ${color}`}>{value}</span>
    </div>
  );
}

/* ------------------------------------------------------------------ */
/*  RetroButton — 3-D press + flash feedback + hotkey badge             */
/* ------------------------------------------------------------------ */
function RetroButton({
  children,
  onClick,
  disabled = false,
  className = "",
  hotkey,
  type = "button",
  style,
}: {
  children: React.ReactNode;
  onClick?: () => void;
  disabled?: boolean;
  className?: string;
  hotkey?: string;
  type?: "button" | "submit";
  style?: React.CSSProperties;
}) {
  const [flashing, setFlashing] = useState(false);

  function handleClick() {
    if (disabled) return;
    if (!flashing) {
      setFlashing(true);
      setTimeout(() => setFlashing(false), 320);
    }
    onClick?.();
  }

  return (
    <button
      type={type}
      disabled={disabled}
      onClick={handleClick}
      style={style}
      className={`btn ${className} ${flashing ? "btn-flashing" : ""}`}
    >
      {children}
      {hotkey && (
        <span className="btn-hotkey">[{hotkey}]</span>
      )}
    </button>
  );
}

/* ------------------------------------------------------------------ */
/*  Login Screen                                                        */
/* ------------------------------------------------------------------ */
function LoginScreen({ onLogin }: { onLogin: (username: string) => void }) {
  const [name, setName] = useState("");
  const [loading, setLoading] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => { inputRef.current?.focus(); }, []);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    const trimmed = name.trim() || "Traveler";
    setLoading(true);
    setErr(null);
    try {
      const gs = await callGame("init", trimmed);
      if (gs.error) { setErr(gs.error); }
      else { onLogin(trimmed); }
    } catch (e) { setErr(String(e)); }
    finally { setLoading(false); }
  }

  return (
    <div className="crt min-h-screen flex items-center justify-center bg-[#0a0c10]">
      <div className="panel p-10 w-full max-w-md text-center space-y-8">
        <div>
          <p className="font-mono text-xs text-green-800 tracking-[0.4em] uppercase mb-3 text-flicker">— System Boot v2.6 —</p>
          <h1 className="font-mono text-5xl font-black text-green-400 tracking-tight">
            <span className="glitch-text">ARCADIA</span>
          </h1>
          <p className="font-mono text-xs text-green-700 mt-2">C-Powered Browser RPG</p>
          <p className="font-mono text-[10px] text-green-900 mt-1 text-flicker tracking-widest uppercase">▸ System Ready ◂</p>
        </div>

        <form onSubmit={handleSubmit} className="space-y-4 text-left">
          <label className="block font-mono text-xs text-green-600 uppercase tracking-widest mb-1">
            Enter your handle
          </label>
          <div className="flex items-center border border-green-900 bg-[#030d03] rounded px-3 py-2 focus-within:border-green-500 focus-within:shadow-[0_0_8px_rgba(0,255,65,0.3)] transition-all">
            <span className="font-mono text-green-500 mr-2">$</span>
            <input
              ref={inputRef}
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="Traveler"
              maxLength={31}
              className="flex-1 bg-transparent font-mono text-green-300 outline-none text-sm placeholder:text-green-900"
            />
            <span className="blink font-mono text-green-400">▌</span>
          </div>

          {err && <p className="font-mono text-xs text-red-400 glow-red">✗ {err}</p>}

          <RetroButton type="submit" disabled={loading} className="btn-green w-full py-3 text-sm">
            {loading ? <Spinner /> : null}
            {loading ? "  Entering Arcadia…" : "Enter the World ▶"}
          </RetroButton>
        </form>

        <p className="font-mono text-[10px] text-green-900">
          New heroes start fresh · Returning heroes load saved state
        </p>
      </div>
    </div>
  );
}

/* ================================================================== */
/*  Main Game Dashboard                                                 */
/* ================================================================== */
export default function ArcadiaGame() {
  const [username, setUsername] = useState<string | null>(null);
  const [gs, setGs] = useState<GameState | null>(null);
  const [busy, setBusy] = useState(false);
  const [lastError, setLastError] = useState<string | null>(null);
  const [screenFlash, setScreenFlash] = useState(false);
  const logRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [gs?.events]);

  const dispatch = useCallback(
    async (command: string, args: string[] = []) => {
      if (!username || busy) return;
      setBusy(true);
      setLastError(null);
      try {
        const next = await callGame(command, username, args);
        if (next.error) setLastError(next.error);
        setGs(next);
      } catch (e) { setLastError(String(e)); }
      finally { setBusy(false); }
    },
    [username, busy]
  );

  function triggerFlash() {
    setScreenFlash(true);
    setTimeout(() => setScreenFlash(false), 500);
  }

  /* Keyboard shortcuts */
  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      if (!gs || busy) return;
      const tag = (e.target as HTMLElement).tagName;
      if (tag === "INPUT" || tag === "TEXTAREA") return;
      const k = e.key.toLowerCase();
      if (k === "c")                             { dispatch("combat"); triggerFlash(); }
      else if (k === "b")                        { dispatch("buy"); }
      else if (k === "r")                        { dispatch("skill", ["rotate"]); }
      else if (k === "arrowup"    || k === "w")  { if (gs.exits.includes("N")) dispatch("move", ["N"]); }
      else if (k === "arrowdown"  || k === "s")  { if (gs.exits.includes("S")) dispatch("move", ["S"]); }
      else if (k === "arrowright" || k === "d")  { if (gs.exits.includes("E")) dispatch("move", ["E"]); }
      else if (k === "arrowleft"  || k === "a")  { if (gs.exits.includes("W")) dispatch("move", ["W"]); }
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [gs, busy, dispatch]);

  function handleLogin(name: string) {
    setUsername(name);
    setBusy(true);
    callGame("init", name)
      .then((s) => setGs(s))
      .catch((e) => setLastError(String(e)))
      .finally(() => setBusy(false));
  }

  if (!username) return <LoginScreen onLogin={handleLogin} />;

  if (!gs) {
    return (
      <div className="crt min-h-screen flex items-center justify-center bg-[#0a0c10]">
        <p className="font-mono text-green-400 glow-green text-sm animate-pulse">Loading world data…</p>
      </div>
    );
  }

  const { player, location, exits, inventory, quests, skills, shop, events } = gs;
  const hpPct = player.max_hp > 0 ? (player.hp / player.max_hp) * 100 : 0;

  return (
    <div className="crt min-h-screen bg-[#0a0c10] flex flex-col font-mono select-none">
      {screenFlash && <div className="screen-flash" />}

      {/* ── TOP STATUS BAR ──────────────────────────────────────────── */}
      <header className="sticky top-0 z-10 border-b border-green-950 bg-[#050a05]/95 backdrop-blur px-4 py-2">
        <div className="max-w-[1400px] mx-auto flex flex-wrap items-center gap-4">
          <span className="text-green-400 glow-green font-black text-lg tracking-widest mr-2">ARCADIA</span>

          <div className="flex items-center gap-1.5 text-xs">
            <span className="text-green-700">◈</span>
            <span className="text-green-300 font-bold">{player.username}</span>
            <span className="text-green-800 ml-2">LVL</span>
            <span className="text-amber-400 glow-amber font-black">{player.level}</span>
          </div>

          <div className="flex items-center gap-2 min-w-[200px]">
            <span className="text-[10px] text-green-700 uppercase tracking-widest">HP</span>
            <HpBar hp={player.hp} max_hp={player.max_hp} />
          </div>

          <div className="flex gap-4 text-xs ml-auto">
            <StatChip icon="⚔" label="ATK" value={player.attack} color="text-red-400" />
            <StatChip icon="🛡" label="DEF" value={player.defense} color="text-cyan-400" />
            <StatChip icon="◈" label="GOLD" value={player.gold} color="text-amber-400" />
          </div>

          {busy && (
            <div className="flex items-center gap-1.5 text-xs text-green-600">
              <Spinner /><span>Processing…</span>
            </div>
          )}
        </div>
      </header>

      {/* ── MAIN GRID ───────────────────────────────────────────────── */}
      <main className="flex-1 max-w-[1400px] mx-auto w-full px-4 py-4
                       grid grid-cols-1 lg:grid-cols-[280px_1fr_280px] gap-4">

        {/* ── LEFT ── */}
        <div className="flex flex-col gap-4">

          {/* Location + compass */}
          <div className="panel p-4">
            <PanelHeader label="Location" color="cyan" />
            <p className="text-cyan-300 glow-cyan text-sm font-bold mb-3">{location}</p>
            <p className="text-[10px] text-green-700 uppercase tracking-widest mb-2">Exits</p>
            <div
              className="grid grid-cols-3 grid-rows-3 gap-1 w-24 mx-auto"
              style={{ gridTemplateAreas: `'. n .' 'w . e' '. s .'` }}
            >
              {(["N","E","S","W"] as const).map((dir) => {
                const areaMap: Record<string,string> = { N:"n", S:"s", E:"e", W:"w" };
                const keyHint: Record<string,string> = { N:"W", S:"S", E:"D", W:"A" };
                const avail = exits.includes(dir);
                return (
                  <RetroButton
                    key={dir}
                    style={{ gridArea: areaMap[dir] }}
                    disabled={!avail || busy}
                    onClick={() => dispatch("move", [dir])}
                    className={`btn-compass text-xs py-1.5 px-2 ${avail ? "btn-cyan" : "btn-ghost opacity-25"}`}
                    hotkey={avail ? keyHint[dir] : undefined}
                  >
                    {dir}
                  </RetroButton>
                );
              })}
              <div style={{ gridArea: "2 / 2 / 3 / 3" }}
                   className="flex items-center justify-center text-green-900 text-xs">✦</div>
            </div>
          </div>

          {/* Quest Log */}
          <div className="panel-amber p-4 flex-1">
            <PanelHeader label="Quest Log" color="amber" />
            {quests.length === 0 ? (
              <p className="text-amber-900 text-xs">No active quests.</p>
            ) : (
              <ul className="space-y-2">
                {quests.map((q, i) => (
                  <li key={i} className="border-l-2 border-amber-900 pl-2">
                    <p className="text-amber-400 text-xs font-bold">{q.title}</p>
                    <p className="text-amber-800 text-[10px] leading-tight mt-0.5">{q.description}</p>
                  </li>
                ))}
              </ul>
            )}
          </div>

          {/* Skill Ring */}
          <div className="panel-cyan p-4">
            <PanelHeader label="Skill Ring" color="cyan" />
            <ul className="space-y-1 mb-3">
              {skills.map((s, i) => (
                <li key={i} className={`flex items-center justify-between rounded px-2 py-1 text-xs transition-all
                  ${s.active ? "bg-cyan-950/60 border border-cyan-700 text-cyan-300 glow-cyan" : "text-green-800"}`}>
                  <span className="flex items-center gap-1.5">
                    {s.active && <span className="text-cyan-400">▶</span>}
                    <span>{s.name}</span>
                  </span>
                  <span className={`text-[10px] tabular-nums ${s.active ? "text-cyan-400" : "text-green-900"}`}>
                    PWR {s.power}
                  </span>
                </li>
              ))}
            </ul>
            <RetroButton
              disabled={busy || skills.length === 0}
              onClick={() => dispatch("skill", ["rotate"])}
              className="btn-cyan w-full text-[11px] py-1.5"
              hotkey="R"
            >
              ↻ Rotate Active Skill
            </RetroButton>
          </div>
        </div>

        {/* ── CENTER ── */}
        <div className="flex flex-col gap-4">

          {/* Event Log terminal */}
          <div className="panel-cyan p-4 flex flex-col" style={{ minHeight: "300px" }}>
            <PanelHeader label="Combat / Event Log" color="cyan" />
            <div ref={logRef} className="flex-1 overflow-y-auto space-y-1 pr-1" style={{ maxHeight: "260px" }}>
              {events.length === 0 ? (
                <p className="text-cyan-900 text-xs">No events recorded.</p>
              ) : (
                events.map((ev, i) => {
                  const isLatest  = i === events.length - 1;
                  const isVictory = ev.startsWith("Victory");
                  const isDeath   = ev.includes("fallen");
                  const isRevive  = ev.startsWith("Revived");
                  const cls = isVictory ? "text-amber-400 glow-amber"
                            : isDeath   ? "text-red-500 glow-red"
                            : isRevive  ? "text-cyan-400"
                            : isLatest  ? "text-cyan-300"
                            : "text-cyan-800";
                  return (
                    <div key={i} className={`font-mono text-xs leading-snug ${cls}`}>
                      <span className="text-cyan-900 mr-1.5 select-none">{String(i + 1).padStart(2, "0")}.</span>
                      {ev}
                      {isLatest && <span className="blink ml-1 text-cyan-400">▌</span>}
                    </div>
                  );
                })
              )}
            </div>
          </div>

          {/* Error banner */}
          {lastError && (
            <div className="panel p-3 border-red-900 bg-red-950/20">
              <p className="text-red-400 text-xs glow-red"><span className="font-bold">✗ ERROR: </span>{lastError}</p>
            </div>
          )}

          {/* Primary action buttons */}
          <div className="panel p-4">
            <PanelHeader label="Actions" color="green" />
            <div className="grid grid-cols-2 gap-2">
              <RetroButton
                disabled={busy}
                onClick={() => { dispatch("combat"); triggerFlash(); }}
                className="btn-red col-span-2 py-3 text-sm"
                hotkey="C"
              >
                {busy ? <Spinner /> : "⚔"}
                {busy ? "  Simulating…" : "  Engage Combat"}
              </RetroButton>

              <RetroButton
                disabled={busy || shop.length === 0}
                onClick={() => dispatch("buy")}
                className="btn-amber"
                hotkey="B"
              >
                🛒&nbsp;Buy Best Item
              </RetroButton>

              <RetroButton
                disabled={busy || skills.length === 0}
                onClick={() => dispatch("skill", ["rotate"])}
                className="btn-cyan"
                hotkey="R"
              >
                ↻&nbsp;Rotate Skill
              </RetroButton>

              {(["N","S","E","W"] as const).map((dir) => {
                const keyHint: Record<string,string> = { N:"W", S:"S", E:"D", W:"A" };
                const avail = exits.includes(dir);
                return (
                  <RetroButton
                    key={dir}
                    disabled={busy || !avail}
                    onClick={() => dispatch("move", [dir])}
                    className={avail ? "btn-green" : "btn-ghost"}
                    hotkey={avail ? keyHint[dir] : undefined}
                  >
                    Move {dir}
                  </RetroButton>
                );
              })}
            </div>

            {/* HP summary */}
            <div className="mt-3 space-y-1">
              <div className="flex justify-between text-[10px] text-green-800 uppercase tracking-widest">
                <span>Health</span><span>{Math.round(hpPct)}%</span>
              </div>
              <HpBar hp={player.hp} max_hp={player.max_hp} />
            </div>
          </div>

          {/* Item Shop */}
          <div className="panel-amber p-4">
            <PanelHeader label="Item Shop — Priority Queue (Max-Heap)" color="amber" />
            {shop.length === 0 ? (
              <p className="text-amber-900 text-xs">Shop is empty.</p>
            ) : (
              <ul className="space-y-1">
                {shop.map((item, i) => (
                  <li key={i} className={`flex items-center justify-between rounded px-2 py-1 text-xs
                    ${i === 0 ? "bg-amber-950/50 border border-amber-800 text-amber-300" : "text-amber-900"}`}>
                    <span className="flex items-center gap-1.5">
                      {i === 0 && <span className="text-amber-400 text-[10px]">★</span>}
                      <span>{item.name}</span>
                    </span>
                    <div className="flex items-center gap-2">
                      <span className="text-[10px] tabular-nums">{item.value}g</span>
                      <RarityBadge rarity={item.rarity} />
                    </div>
                  </li>
                ))}
              </ul>
            )}
            <p className="text-[10px] text-amber-900 mt-2">
              ★ = heap top (highest rarity) · Buy always extracts best first
            </p>
          </div>
        </div>

        {/* ── RIGHT ── */}
        <div className="flex flex-col gap-4">

          {/* Hero card */}
          <div className="panel p-4">
            <PanelHeader label="Hero Profile" color="green" />
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-green-300 font-black text-base glow-green">{player.username}</p>
                  <p className="text-green-700 text-[10px] uppercase tracking-widest">Level {player.level} Adventurer</p>
                </div>
                <div className="text-right">
                  <p className="text-amber-400 glow-amber font-black text-2xl tabular-nums">{player.gold}</p>
                  <p className="text-amber-800 text-[10px] uppercase tracking-widest">Gold</p>
                </div>
              </div>
              <div className="grid grid-cols-2 gap-1.5 text-xs">
                <StatRow label="Attack"  value={player.attack}  color="text-red-400" />
                <StatRow label="Defense" value={player.defense} color="text-cyan-400" />
                <StatRow label="HP"      value={player.hp}      color="text-green-400" />
                <StatRow label="Max HP"  value={player.max_hp}  color="text-green-700" />
              </div>
            </div>
          </div>

          {/* Inventory */}
          <div className="panel p-4 flex-1">
            <PanelHeader label={`Inventory (${inventory.length})`} color="green" />
            {inventory.length === 0 ? (
              <p className="text-green-800 text-xs">Inventory is empty.</p>
            ) : (
              <ul className="space-y-1 overflow-y-auto" style={{ maxHeight: "320px" }}>
                {inventory.map((item, i) => (
                  <li key={i}
                      className="flex flex-col rounded px-2 py-1.5 bg-[#050d05] border border-green-950 hover:border-green-800 transition-colors">
                    <div className="flex items-center justify-between">
                      <span className="text-green-300 text-xs font-semibold">{item.name}</span>
                      <span className="text-green-700 text-[10px] tabular-nums">{item.value}g</span>
                    </div>
                    <RarityBadge rarity={item.rarity} />
                  </li>
                ))}
              </ul>
            )}
          </div>

          {/* Logout */}
          <RetroButton
            onClick={() => { setUsername(null); setGs(null); setLastError(null); }}
            className="btn-ghost w-full text-[11px]"
          >
            ← Logout
          </RetroButton>
        </div>
      </main>

      {/* ── FOOTER ── */}
      <footer className="border-t border-green-950 px-4 py-2 text-center">
        <p className="font-mono text-[10px] text-green-900">
          Arcadia Engine · C Binary Backend via Child Process API · Next.js 15 · Tailwind CSS
        </p>
      </footer>
    </div>
  );
}
