'use client';
import { useEffect, useRef, useState, useCallback } from 'react';

/* ═══════════════════════════════════════════════════════════════════════
   CONSTANTS
═══════════════════════════════════════════════════════════════════════ */
const TILE   = 40;
const COLS   = 20;
const ROWS   = 13;
const CW     = TILE * COLS;   // 800 px
const CH     = TILE * ROWS;   // 520 px
const PL_R   = 13;            // player radius
const EN_R   = 14;            // enemy radius
const SH_R   = 16;            // shop NPC radius
const PL_SPD = 3.6;
const EN_SPD = 0.75;

/* ═══════════════════════════════════════════════════════════════════════
   TILE SYSTEM
═══════════════════════════════════════════════════════════════════════ */
const FLOOR = 0, WALL = 1, DN = 2, DS = 3, DE = 4, DW = 5;
type TT = 0 | 1 | 2 | 3 | 4 | 5;

function parseTile(c: string): TT {
  switch (c) {
    case '#': return WALL;
    case 'N': return DN;
    case 'S': return DS;
    case 'E': return DE;
    case 'W': return DW;
    default:  return FLOOR;
  }
}
function parseRoom(rows: string[]): TT[][] {
  return rows.map(r => [...r].map(parseTile));
}

/* ═══════════════════════════════════════════════════════════════════════
   ROOM MAPS  (20 wide x 13 tall)
═══════════════════════════════════════════════════════════════════════ */
const ROOM_MAPS: TT[][][] = [

  /* 0 - Sunlit Entrance   exits: N -> Armory, E -> Library */
  parseRoom([
    '##########NN########',
    '#..................#',
    '#....####......#...#',
    '#....#.........#...#',
    '#..................#',
    '#...........####...#',
    '#..................EE',
    '#...........####...EE',
    '#..................#',
    '#....#.........#...#',
    '#....####......#...#',
    '#..................#',
    '####################',
  ]),

  /* 1 - Forgemaster Armory  exits: S -> Entrance, E -> Library */
  parseRoom([
    '####################',
    '#..................#',
    '#..####........#...#',
    '#..#...........#...#',
    '#..................#',
    '#....###...###.....EE',
    '#..................EE',
    '#....###...###.....#',
    '#..................#',
    '#..#...........####.#',
    '#..#...............#',
    '#..................#',
    '##########SS########',
  ]),

  /* 2 - Whispering Library  exits: W -> Armory, N -> Sanctum */
  parseRoom([
    '##########NN########',
    '#..................#',
    '#..#####.....#####.#',
    '#..................#',
    '#..................#',
    'WW.................#',
    '#..................#',
    'WW.................#',
    '#..................#',
    '#..................#',
    '#..#####.....#####.#',
    '#..................#',
    '####################',
  ]),

  /* 3 - Crystal Sanctum   exits: S -> Library */
  parseRoom([
    '####################',
    '#..................#',
    '#..###.......###...#',
    '#..................#',
    '#....#.........#...#',
    '#....##.......##...#',
    '#..................#',
    '#....##.......##...#',
    '#....#.........#...#',
    '#..................#',
    '#..###.......###...#',
    '#..................#',
    '##########SS########',
  ]),
];

/* ═══════════════════════════════════════════════════════════════════════
   ROOM THEMES
═══════════════════════════════════════════════════════════════════════ */
const THEMES = [
  { floor: '#14213d', wall: '#0a1020', wallTop: '#1e3a5f', door: '#2563eb',  glow: '#60a5fa', accent: '#fbbf24', name: 'Sunlit Entrance'    },
  { floor: '#1c0a00', wall: '#0e0500', wallTop: '#4c1010', door: '#dc2626',  glow: '#f87171', accent: '#f97316', name: 'Forgemaster Armory' },
  { floor: '#0c0f2a', wall: '#07080f', wallTop: '#1e1b4b', door: '#7c3aed',  glow: '#a78bfa', accent: '#818cf8', name: 'Whispering Library' },
  { floor: '#001e1e', wall: '#050e0e', wallTop: '#0a3344', door: '#0891b2',  glow: '#22d3ee', accent: '#06b6d4', name: 'Crystal Sanctum'    },
];

/* ═══════════════════════════════════════════════════════════════════════
   DOOR EXIT TABLE
═══════════════════════════════════════════════════════════════════════ */
interface DoorExit { dir: string; tile: TT; destRoom: number; spawnX: number; spawnY: number; }

const ROOM_EXITS: DoorExit[][] = [
  /* 0 */ [
    { dir: 'N', tile: DN, destRoom: 1, spawnX: 9.5 * TILE, spawnY: 10.5 * TILE },
    { dir: 'E', tile: DE, destRoom: 2, spawnX:  1.5 * TILE, spawnY:  6.5 * TILE },
  ],
  /* 1 */ [
    { dir: 'S', tile: DS, destRoom: 0, spawnX: 9.5 * TILE, spawnY:  1.5 * TILE },
    { dir: 'E', tile: DE, destRoom: 2, spawnX:  1.5 * TILE, spawnY:  6.5 * TILE },
  ],
  /* 2 */ [
    { dir: 'W', tile: DW, destRoom: 1, spawnX: 18.5 * TILE, spawnY:  6.5 * TILE },
    { dir: 'N', tile: DN, destRoom: 3, spawnX:  9.5 * TILE, spawnY: 10.5 * TILE },
  ],
  /* 3 */ [
    { dir: 'S', tile: DS, destRoom: 2, spawnX:  9.5 * TILE, spawnY:  1.5 * TILE },
  ],
];

/* ═══════════════════════════════════════════════════════════════════════
   ENEMY DEFINITIONS
═══════════════════════════════════════════════════════════════════════ */
interface EnemyDef { tx: number; ty: number; name: string; col: string; }

const ENEMY_DEFS: EnemyDef[][] = [
  /* 0 */ [
    { tx: 5,  ty: 8, name: 'Shadow Wisp',   col: '#7c3aed' },
    { tx: 14, ty: 4, name: 'Stone Golem',   col: '#94a3b8' },
  ],
  /* 1 */ [
    { tx: 4,  ty: 3, name: 'Forge Drake',   col: '#ef4444' },
    { tx: 14, ty: 8, name: 'Slag Imp',      col: '#f97316' },
    { tx: 9,  ty: 5, name: 'Ash Knight',    col: '#dc2626' },
  ],
  /* 2 */ [
    { tx: 6,  ty: 5, name: 'Scroll Wraith', col: '#818cf8' },
    { tx: 15, ty: 9, name: 'Ink Phantom',   col: '#6366f1' },
  ],
  /* 3 */ [
    { tx: 6,  ty: 4, name: 'Crystal Golem', col: '#22d3ee' },
    { tx: 13, ty: 8, name: 'Prism Knight',  col: '#06b6d4' },
    { tx: 9,  ty: 6, name: 'Void Wraith',   col: '#67e8f9' },
  ],
];

/* Shop NPC locations per room: [roomId, tileX, tileY] */
const SHOP_SPAWNS: [number, number, number][] = [
  [0, 16, 10],
  [3,  4, 10],
];

/* ═══════════════════════════════════════════════════════════════════════
   RUNTIME ENTITY TYPES
═══════════════════════════════════════════════════════════════════════ */
interface Enemy {
  id: number; x: number; y: number; vx: number; vy: number;
  name: string; col: string; alive: boolean; wanderTimer: number;
}

interface ShopNpc { x: number; y: number; }

interface PlayerInfo {
  username: string; hp: number; max_hp: number;
  attack: number; defense: number; gold: number; level: number;
}
interface ItemInfo  { name: string; value: number; rarity: number; }
interface SkillInfo { name: string; power: number; active: boolean; }
interface ApiResult {
  player: PlayerInfo;
  location: string;
  exits: string[];
  events: string[];
  skills: SkillInfo[];
  shop: ItemInfo[];
  error: string | null;
  _state: unknown;
}

type Overlay = 'none' | 'combat' | 'shop' | 'map';

/* ═══════════════════════════════════════════════════════════════════════
   API HELPER
═══════════════════════════════════════════════════════════════════════ */
async function callApi(
  command: string,
  state: unknown,
  args: string[] = [],
  username = '',
): Promise<ApiResult> {
  const body: Record<string, unknown> = { command, args, username };
  if (state) body.state = state;
  const res = await fetch('/api/game', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  return res.json() as Promise<ApiResult>;
}

/* ═══════════════════════════════════════════════════════════════════════
   WORLD HELPERS
═══════════════════════════════════════════════════════════════════════ */
function tileAt(map: TT[][], px: number, py: number): TT {
  const col = Math.floor(px / TILE);
  const row = Math.floor(py / TILE);
  if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return WALL;
  return map[row][col];
}

function isDoorTile(t: TT): boolean { return t === DN || t === DS || t === DE || t === DW; }

function spawnEnemies(roomId: number): Enemy[] {
  return ENEMY_DEFS[roomId].map((d, i) => ({
    id: i,
    x: (d.tx + 0.5) * TILE, y: (d.ty + 0.5) * TILE,
    vx: 0, vy: 0,
    name: d.name, col: d.col,
    alive: true,
    wanderTimer: 60 + Math.random() * 120,
  }));
}

function buildShopNpcs(roomId: number): ShopNpc[] {
  return SHOP_SPAWNS
    .filter(([r]) => r === roomId)
    .map(([, tx, ty]) => ({ x: (tx + 0.5) * TILE, y: (ty + 0.5) * TILE }));
}

function dist2d(ax: number, ay: number, bx: number, by: number): number {
  return Math.sqrt((ax - bx) ** 2 + (ay - by) ** 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   CANVAS DRAW HELPERS
═══════════════════════════════════════════════════════════════════════ */
function drawRoom(ctx: CanvasRenderingContext2D, map: TT[][], th: typeof THEMES[0], t: number) {
  for (let row = 0; row < ROWS; row++) {
    for (let col = 0; col < COLS; col++) {
      const tile = map[row][col];
      const x = col * TILE, y = row * TILE;

      if (tile === WALL) {
        ctx.fillStyle = th.wall;
        ctx.fillRect(x, y, TILE, TILE);
        ctx.fillStyle = th.wallTop;
        ctx.fillRect(x, y, TILE, 8);
      } else if (tile === FLOOR) {
        ctx.fillStyle = th.floor;
        ctx.fillRect(x, y, TILE, TILE);
        ctx.strokeStyle = 'rgba(255,255,255,0.025)';
        ctx.lineWidth = 1;
        ctx.strokeRect(x + 0.5, y + 0.5, TILE - 1, TILE - 1);
      } else {
        /* door tile */
        ctx.fillStyle = th.floor;
        ctx.fillRect(x, y, TILE, TILE);
        ctx.fillStyle = th.door + '44';
        ctx.fillRect(x, y, TILE, TILE);
        const pulse = 0.5 + 0.5 * Math.sin(t * 0.05);
        ctx.globalAlpha = 0.4 + 0.4 * pulse;
        ctx.fillStyle = th.glow;
        ctx.fillRect(x + TILE * 0.35, y + TILE * 0.35, TILE * 0.3, TILE * 0.3);
        ctx.globalAlpha = 1;
      }
    }
  }
}

function drawGlowCircle(
  ctx: CanvasRenderingContext2D,
  x: number, y: number, r: number,
  bodyCol: string, glowCol: string,
  label?: string,
) {
  const g = ctx.createRadialGradient(x, y, 0, x, y, r * 2.8);
  g.addColorStop(0, glowCol + '88');
  g.addColorStop(1, 'transparent');
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(x, y, r * 2.8, 0, Math.PI * 2);
  ctx.fill();

  ctx.fillStyle = bodyCol;
  ctx.beginPath();
  ctx.arc(x, y, r, 0, Math.PI * 2);
  ctx.fill();

  ctx.strokeStyle = glowCol;
  ctx.lineWidth = 2;
  ctx.stroke();

  if (label) {
    ctx.fillStyle = '#cbd5e1';
    ctx.font = '9px ui-monospace,monospace';
    ctx.textAlign = 'center';
    ctx.fillText(label, x, y - r - 5);
    ctx.textAlign = 'left';
  }
}

function drawPlayer(ctx: CanvasRenderingContext2D, x: number, y: number, th: typeof THEMES[0]) {
  drawGlowCircle(ctx, x, y, PL_R, '#3b82f6', th.glow);
  ctx.fillStyle = '#fff';
  ctx.beginPath();
  ctx.arc(x, y - 5, 3, 0, Math.PI * 2);
  ctx.fill();
}

function drawShopNpc(ctx: CanvasRenderingContext2D, npc: ShopNpc, frame: number) {
  const pulse = 0.75 + 0.25 * Math.sin(frame * 0.05);
  ctx.globalAlpha = pulse;
  drawGlowCircle(ctx, npc.x, npc.y, SH_R, '#92400e', '#fde68a', 'SHOP  [E]');
  ctx.globalAlpha = 1;
}

/* ═══════════════════════════════════════════════════════════════════════
   LOGIN SCREEN
═══════════════════════════════════════════════════════════════════════ */
function LoginScreen({ onLogin }: { onLogin: (u: string) => void }) {
  const [name, setName] = useState('');
  const [busy, setBusy] = useState(false);

  function submit() {
    if (busy) return;
    setBusy(true);
    onLogin(name.trim() || 'Traveler');
  }

  return (
    <div className="min-h-screen flex flex-col items-center justify-center"
         style={{ background: '#0a0e1a', fontFamily: 'ui-monospace,monospace' }}>
      <div className="mb-10 text-center select-none">
        <p className="text-[11px] tracking-[0.5em] text-[#2563eb] mb-3 uppercase">Neural Protocol Active</p>
        <h1 className="text-6xl font-black tracking-widest text-white mb-2"
            style={{ textShadow: '0 0 40px #2563eb, 0 0 80px #2563eb55' }}>
          ARCADIA
        </h1>
        <p className="text-[#475569] text-xs tracking-[0.3em] uppercase">2D Dungeon Explorer</p>
      </div>

      <div className="w-80 p-6 rounded-xl border" style={{ background: '#111827', borderColor: '#1e2d45' }}>
        <p className="text-[10px] tracking-widest text-[#64748b] uppercase mb-4">Enter Designation</p>
        <input
          className="w-full bg-[#0a0e1a] border border-[#1e2d45] rounded-lg px-4 py-3 text-white
                     text-sm outline-none focus:border-[#2563eb] transition-colors mb-4"
          placeholder="Traveler"
          value={name}
          onChange={e => setName(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && submit()}
          autoFocus
        />
        <button
          onClick={submit} disabled={busy}
          className="w-full py-3 rounded-lg text-sm font-bold tracking-widest uppercase transition-all"
          style={{ background: busy ? '#1e3a5f' : '#2563eb', color: '#fff',
                   opacity: busy ? 0.7 : 1, cursor: busy ? 'not-allowed' : 'pointer' }}>
          {busy ? 'Initialising...' : 'Enter Arcadia'}
        </button>
        <p className="text-center text-[#334155] text-[10px] mt-4 tracking-wide">
          WASD / Arrows to move &nbsp;|&nbsp; E to interact
        </p>
      </div>
    </div>
  );
}

/* ═══════════════════════════════════════════════════════════════════════
   HUD
═══════════════════════════════════════════════════════════════════════ */
function Hud({
  player, roomName, events, skills, onMap, onLogout,
}: {
  player: PlayerInfo; roomName: string; events: string[];
  skills: SkillInfo[]; onMap: () => void; onLogout: () => void;
}) {
  const hpPct = Math.max(0, player.hp / player.max_hp) * 100;
  const active = skills.find(s => s.active);

  return (
    <div className="pointer-events-none absolute inset-0" style={{ fontFamily: 'ui-monospace,monospace' }}>
      {/* top bar */}
      <div className="pointer-events-auto absolute top-0 left-0 right-0 flex items-center gap-3 px-4 py-2"
           style={{ background: 'rgba(10,14,26,0.88)', borderBottom: '1px solid #1e2d45' }}>
        <span className="text-[10px] tracking-widest text-[#2563eb] uppercase">{roomName}</span>
        <span className="text-[#1e2d45]">|</span>

        <div className="flex items-center gap-2">
          <span className="text-[10px] text-[#64748b]">HP</span>
          <div className="w-28 h-2 rounded-full" style={{ background: '#1e2d45' }}>
            <div className="h-2 rounded-full transition-all duration-300"
                 style={{
                   width: `${hpPct}%`,
                   background: hpPct > 50 ? '#22c55e' : hpPct > 25 ? '#f59e0b' : '#ef4444',
                 }} />
          </div>
          <span className="text-[10px] text-[#94a3b8]">{player.hp}/{player.max_hp}</span>
        </div>

        <span className="text-[10px] text-[#f59e0b] ml-1">&#9670; {player.gold}g</span>
        <span className="text-[10px] text-[#60a5fa]">Lv.{player.level}</span>
        {active && <span className="text-[10px] text-[#a78bfa]">&#9889; {active.name}</span>}

        <div className="ml-auto flex gap-3">
          <button className="pointer-events-auto text-[10px] text-[#475569] hover:text-[#94a3b8] transition-colors"
                  onClick={onMap}>MAP</button>
          <button className="pointer-events-auto text-[10px] text-[#475569] hover:text-[#ef4444] transition-colors"
                  onClick={onLogout}>EXIT</button>
        </div>
      </div>

      {/* event log bottom-left */}
      {events.length > 0 && (
        <div className="absolute bottom-0 left-0 p-3 max-w-xs"
             style={{ background: 'rgba(10,14,26,0.72)' }}>
          {events.slice(-3).map((e, i, arr) => (
            <p key={i} className="text-[10px] text-[#64748b] leading-relaxed"
               style={{ opacity: 0.4 + 0.6 * ((i + 1) / arr.length) }}>
              &rsaquo; {e}
            </p>
          ))}
        </div>
      )}

      {/* hints bottom-right */}
      <div className="absolute bottom-2 right-3 text-[9px] text-[#1e3a5f] text-right space-y-px">
        <div>WASD / Arrows &middot; Move</div>
        <div>Walk into a door to travel &middot; E near &#9670; to shop</div>
      </div>
    </div>
  );
}

/* ═══════════════════════════════════════════════════════════════════════
   COMBAT OVERLAY
═══════════════════════════════════════════════════════════════════════ */
function CombatOverlay({
  enemyName, player, events, busy, onAttack, onFlee,
}: {
  enemyName: string; player: PlayerInfo; events: string[];
  busy: boolean; onAttack: () => void; onFlee: () => void;
}) {
  const hpPct = Math.max(0, player.hp / player.max_hp) * 100;
  return (
    <div className="absolute inset-0 flex items-center justify-center z-40"
         style={{ background: 'rgba(0,0,0,0.84)', fontFamily: 'ui-monospace,monospace' }}>
      <div className="w-96 rounded-2xl p-6 border" style={{ background: '#0d1520', borderColor: '#1e3a5f' }}>
        <div className="text-center mb-5">
          <p className="text-[10px] tracking-widest text-[#ef4444] uppercase mb-1">Combat Engaged</p>
          <h2 className="text-xl font-bold text-white">{enemyName}</h2>
        </div>

        <div className="mb-4">
          <div className="flex justify-between text-[10px] text-[#64748b] mb-1">
            <span>{player.username}</span>
            <span>{player.hp} / {player.max_hp} HP</span>
          </div>
          <div className="h-3 rounded-full" style={{ background: '#1e2d45' }}>
            <div className="h-3 rounded-full transition-all"
                 style={{
                   width: `${hpPct}%`,
                   background: hpPct > 50 ? '#22c55e' : hpPct > 25 ? '#f59e0b' : '#ef4444',
                 }} />
          </div>
        </div>

        <div className="rounded-lg p-3 mb-4 min-h-16 space-y-1"
             style={{ background: '#0a0e1a', border: '1px solid #1e2d45' }}>
          {events.slice(-4).map((e, i) => (
            <p key={i} className="text-[10px] text-[#94a3b8] leading-relaxed">&rsaquo; {e}</p>
          ))}
          {busy && <p className="text-[10px] text-[#2563eb] animate-pulse">Calculating...</p>}
        </div>

        <div className="flex gap-3">
          <button onClick={onAttack} disabled={busy}
                  className="flex-1 py-3 rounded-lg font-bold text-sm tracking-widest uppercase transition-all"
                  style={{ background: busy ? '#1e2d45' : '#dc2626', color: '#fff',
                           cursor: busy ? 'not-allowed' : 'pointer', opacity: busy ? 0.6 : 1 }}>
            Attack
          </button>
          <button onClick={onFlee} disabled={busy}
                  className="flex-1 py-3 rounded-lg font-bold text-sm tracking-widest uppercase"
                  style={{ background: '#1e2d45', color: '#94a3b8',
                           cursor: busy ? 'not-allowed' : 'pointer', opacity: busy ? 0.6 : 1 }}>
            Flee
          </button>
        </div>
      </div>
    </div>
  );
}

/* ═══════════════════════════════════════════════════════════════════════
   SHOP OVERLAY
═══════════════════════════════════════════════════════════════════════ */
const RARITY_LABELS = ['', 'Common', 'Uncommon', 'Rare', 'Epic', 'Legendary'];
const RARITY_COLORS = ['', '#94a3b8', '#4ade80', '#818cf8', '#f59e0b', '#f43f5e'];

function ShopOverlay({
  shop, player, busy, lastEvent, onBuy, onClose,
}: {
  shop: ItemInfo[]; player: PlayerInfo; busy: boolean;
  lastEvent: string; onBuy: () => void; onClose: () => void;
}) {
  const top = shop[0];
  const canBuy = !!top && player.gold >= top.value && !busy;
  return (
    <div className="absolute inset-0 flex items-center justify-center z-40"
         style={{ background: 'rgba(0,0,0,0.84)', fontFamily: 'ui-monospace,monospace' }}>
      <div className="w-96 rounded-2xl p-6 border" style={{ background: '#0d1520', borderColor: '#1e3a5f' }}>
        <div className="flex items-center justify-between mb-5">
          <div>
            <p className="text-[10px] tracking-widest text-[#f59e0b] uppercase mb-0.5">Merchant</p>
            <h2 className="text-lg font-bold text-white">Item Shop</h2>
          </div>
          <div className="text-right">
            <p className="text-[10px] text-[#64748b]">Your gold</p>
            <p className="text-[#f59e0b] font-bold">&#9670; {player.gold}</p>
          </div>
        </div>

        {top ? (
          <div className="rounded-xl p-4 mb-3 border" style={{ background: '#0a0e1a', borderColor: '#1e3a5f' }}>
            <div className="flex items-start justify-between">
              <div>
                <p className="text-white font-bold text-sm">{top.name}</p>
                <p className="text-[10px] mt-0.5"
                   style={{ color: RARITY_COLORS[top.rarity] ?? '#94a3b8' }}>
                  {RARITY_LABELS[top.rarity] ?? 'Unknown'} Rarity
                </p>
              </div>
              <span className="text-[#f59e0b] font-bold">&#9670; {top.value}</span>
            </div>
          </div>
        ) : (
          <div className="rounded-xl p-4 mb-3 text-center text-[#475569] text-sm"
               style={{ background: '#0a0e1a' }}>
            Shop is empty
          </div>
        )}

        {shop.length > 1 && (
          <div className="rounded-lg overflow-hidden mb-3" style={{ border: '1px solid #1e2d45' }}>
            {shop.slice(1, 5).map((it, i) => (
              <div key={i} className="flex items-center justify-between px-3 py-2"
                   style={{ borderBottom: i < Math.min(shop.length - 2, 3) ? '1px solid #0f1728' : 'none' }}>
                <span className="text-[11px] text-[#64748b]">{it.name}</span>
                <span className="text-[11px] text-[#475569]">&#9670; {it.value}</span>
              </div>
            ))}
          </div>
        )}

        {lastEvent && <p className="text-[10px] text-[#22c55e] mb-3">&rsaquo; {lastEvent}</p>}

        <div className="flex gap-3">
          <button onClick={onBuy} disabled={!canBuy}
                  className="flex-1 py-3 rounded-lg font-bold text-sm tracking-widest uppercase transition-all"
                  style={{ background: canBuy ? '#d97706' : '#1e2d45', color: '#fff',
                           cursor: canBuy ? 'pointer' : 'not-allowed', opacity: canBuy ? 1 : 0.5 }}>
            {busy ? '...' : 'Buy Item'}
          </button>
          <button onClick={onClose}
                  className="flex-1 py-3 rounded-lg font-bold text-sm tracking-widest uppercase"
                  style={{ background: '#1e2d45', color: '#94a3b8', cursor: 'pointer' }}>
            Leave
          </button>
        </div>
      </div>
    </div>
  );
}

/* ═══════════════════════════════════════════════════════════════════════
   MINI-MAP OVERLAY
═══════════════════════════════════════════════════════════════════════ */
const MAP_POS = [
  { id: 0, label: 'Entrance', icon: 'E', x: 40,  y: 90  },
  { id: 1, label: 'Armory',   icon: 'A', x: 40,  y: 30  },
  { id: 2, label: 'Library',  icon: 'L', x: 130, y: 90  },
  { id: 3, label: 'Sanctum',  icon: 'S', x: 130, y: 30  },
];
const MAP_EDGES = [[0, 1], [0, 2], [1, 2], [2, 3]];

function MapOverlay({ current, onClose }: { current: number; onClose: () => void }) {
  return (
    <div className="absolute inset-0 flex items-center justify-center z-40"
         style={{ background: 'rgba(0,0,0,0.84)', fontFamily: 'ui-monospace,monospace' }}>
      <div className="w-72 rounded-2xl p-6 border" style={{ background: '#0d1520', borderColor: '#1e3a5f' }}>
        <div className="flex items-center justify-between mb-5">
          <h2 className="text-sm font-bold tracking-widest text-white uppercase">World Map</h2>
          <button onClick={onClose} className="text-[#64748b] hover:text-white text-xl leading-none">&#xd7;</button>
        </div>

        <svg viewBox="0 0 180 130" className="w-full" style={{ height: 130 }}>
          {MAP_EDGES.map(([a, b], i) => {
            const pa = MAP_POS[a], pb = MAP_POS[b];
            return <line key={i} x1={pa.x} y1={pa.y} x2={pb.x} y2={pb.y}
                         stroke="#1e3a5f" strokeWidth={1.5} strokeDasharray="4 3" />;
          })}
          {MAP_POS.map(p => (
            <g key={p.id}>
              {p.id === current && (
                <circle cx={p.x} cy={p.y} r={16} fill="none"
                        stroke="#2563eb" strokeWidth={1.5} opacity={0.5} />
              )}
              <circle cx={p.x} cy={p.y} r={11}
                      fill={p.id === current ? '#1e3a5f' : '#111827'}
                      stroke={p.id === current ? '#3b82f6' : '#1e2d45'} strokeWidth={1.5} />
              <text x={p.x} y={p.y + 4} textAnchor="middle" fontSize="9"
                    fontFamily="ui-monospace,monospace"
                    fill={p.id === current ? '#60a5fa' : '#475569'}>{p.icon}</text>
              <text x={p.x} y={p.y + 22} textAnchor="middle" fontSize="7"
                    fontFamily="ui-monospace,monospace"
                    fill={p.id === current ? '#93c5fd' : '#334155'}>{p.label}</text>
            </g>
          ))}
        </svg>

        <button onClick={onClose}
                className="w-full py-2 rounded-lg text-sm transition-colors"
                style={{ background: '#1e2d45', color: '#64748b' }}>
          Back to Game
        </button>
      </div>
    </div>
  );
}

/* ═══════════════════════════════════════════════════════════════════════
   MAIN GAME CANVAS COMPONENT
═══════════════════════════════════════════════════════════════════════ */
function ArcadiaGame({ initialGs, username }: { initialGs: ApiResult; username: string }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  /* mutable world (game-loop owned, never trigger re-render) */
  const W = useRef({
    px:       9.5 * TILE,
    py:       6.5 * TILE,
    enemies:  spawnEnemies(0),
    shops:    buildShopNpcs(0),
    roomId:   (initialGs._state as { room_id?: number }).room_id ?? 0,
    keys:     new Set<string>(),
    paused:   false,
    cooldown: 0,
    frame:    0,
    animId:   0,
    apiState: initialGs._state,
  });

  /* react state driving HUD + overlays */
  const [gs,        setGs]        = useState<ApiResult>(initialGs);
  const [overlay,   setOverlay]   = useState<Overlay>('none');
  const [fightName, setFightName] = useState('');
  const [busy,      setBusy]      = useState(false);
  const [shopTxt,   setShopTxt]   = useState('');

  /* keep overlay ref in sync so game loop can read it without stale closure */
  const overlayRef = useRef<Overlay>('none');
  useEffect(() => {
    overlayRef.current = overlay;
    W.current.paused   = overlay !== 'none';
  }, [overlay]);

  /* ── combat ── */
  const handleAttack = useCallback(async () => {
    setBusy(true);
    const res = await callApi('combat', W.current.apiState);
    W.current.apiState = res._state;
    setGs(res);
    setBusy(false);

    if (res.player.hp <= 0) { setOverlay('none'); return; }

    const last = res.events[res.events.length - 1] ?? '';
    if (/defeat|fled|victory/i.test(last)) {
      /* mark this enemy as dead in the world */
      const en = W.current.enemies.find(e => e.alive && e.name === fightName);
      if (en) en.alive = false;
      setOverlay('none');
      W.current.cooldown = 90;
    }
  }, [fightName]);

  const handleFlee = useCallback(() => {
    setOverlay('none');
    W.current.cooldown = 180;
  }, []);

  /* ── shop ── */
  const handleBuy = useCallback(async () => {
    setBusy(true);
    const res = await callApi('buy', W.current.apiState);
    W.current.apiState = res._state;
    setGs(res);
    setShopTxt(res.events[res.events.length - 1] ?? '');
    setBusy(false);
  }, []);

  /* ── room travel ── */
  const doTravel = useCallback(async (dir: string, dest: number, sx: number, sy: number) => {
    if (W.current.paused) return;
    W.current.paused = true;
    const res = await callApi('move', W.current.apiState, [dir]);
    W.current.apiState = res._state;
    W.current.roomId   = dest;
    W.current.px       = sx;
    W.current.py       = sy;
    W.current.enemies  = spawnEnemies(dest);
    W.current.shops    = buildShopNpcs(dest);
    W.current.cooldown = 90;
    setGs(res);
    W.current.paused = overlayRef.current !== 'none';
  }, []);

  /* ── logout ── */
  const handleLogout = useCallback(() => {
    localStorage.removeItem(`arcadia_${username}`);
    window.location.reload();
  }, [username]);

  /* ════════════════════════════════ GAME LOOP ════════════════════════════ */
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d')!;

    /* key handlers */
    function onKeyDown(e: KeyboardEvent) {
      W.current.keys.add(e.key);
      if (!W.current.paused && (e.key === 'e' || e.key === 'E')) {
        for (const npc of W.current.shops) {
          if (dist2d(W.current.px, W.current.py, npc.x, npc.y) < 80) {
            setShopTxt('');
            setOverlay('shop');
            break;
          }
        }
      }
    }
    function onKeyUp(e: KeyboardEvent) { W.current.keys.delete(e.key); }
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup',   onKeyUp);

    function tick(frame: number) {
      W.current.animId = requestAnimationFrame(() => tick(frame + 1));
      W.current.frame  = frame;
      const w   = W.current;
      const map = ROOM_MAPS[w.roomId];
      const th  = THEMES[w.roomId];

      /* ─── update ─── */
      if (!w.paused) {
        if (w.cooldown > 0) w.cooldown--;

        /* movement */
        const k = w.keys;
        let dx = 0, dy = 0;
        if (k.has('w') || k.has('W') || k.has('ArrowUp'))    dy -= PL_SPD;
        if (k.has('s') || k.has('S') || k.has('ArrowDown'))  dy += PL_SPD;
        if (k.has('a') || k.has('A') || k.has('ArrowLeft'))  dx -= PL_SPD;
        if (k.has('d') || k.has('D') || k.has('ArrowRight')) dx += PL_SPD;

        const cr = PL_R - 2;  /* collision radius (slightly smaller for feel) */

        if (dx !== 0) {
          const tx = w.px + dx + (dx > 0 ? cr : -cr);
          if (!isDoorTile(tileAt(map, tx, w.py - cr)) && tileAt(map, tx, w.py - cr) !== WALL &&
              !isDoorTile(tileAt(map, tx, w.py + cr)) && tileAt(map, tx, w.py + cr) !== WALL) {
            w.px += dx;
          } else if (isDoorTile(tileAt(map, tx, w.py))) {
            w.px += dx;  /* allow walking into door tiles */
          }
        }
        if (dy !== 0) {
          const ty = w.py + dy + (dy > 0 ? cr : -cr);
          if (!isDoorTile(tileAt(map, w.px - cr, ty)) && tileAt(map, w.px - cr, ty) !== WALL &&
              !isDoorTile(tileAt(map, w.px + cr, ty)) && tileAt(map, w.px + cr, ty) !== WALL) {
            w.py += dy;
          } else if (isDoorTile(tileAt(map, w.px, ty))) {
            w.py += dy;
          }
        }

        w.px = Math.max(PL_R, Math.min(CW - PL_R, w.px));
        w.py = Math.max(PL_R, Math.min(CH - PL_R, w.py));

        /* door transition */
        const pt = tileAt(map, w.px, w.py);
        if (isDoorTile(pt) && w.cooldown === 0) {
          for (const exit of ROOM_EXITS[w.roomId]) {
            if (exit.tile === pt) {
              w.cooldown = 999; /* prevent re-triggering while async travels */
              void doTravel(exit.dir, exit.destRoom, exit.spawnX, exit.spawnY);
              break;
            }
          }
        }

        /* enemy wander + combat trigger */
        for (const en of w.enemies) {
          if (!en.alive) continue;

          en.wanderTimer--;
          if (en.wanderTimer <= 0) {
            const angle = Math.random() * Math.PI * 2;
            en.vx = Math.cos(angle) * EN_SPD;
            en.vy = Math.sin(angle) * EN_SPD;
            en.wanderTimer = 60 + Math.random() * 120;
          }

          /* enemy collision with walls */
          const ex2 = en.x + en.vx, ey2 = en.y + en.vy;
          const er = EN_R - 2;
          if (tileAt(map, ex2 + er, en.y) !== WALL && tileAt(map, ex2 - er, en.y) !== WALL)
            en.x += en.vx; else en.vx *= -1;
          if (tileAt(map, en.x, ey2 + er) !== WALL && tileAt(map, en.x, ey2 - er) !== WALL)
            en.y += en.vy; else en.vy *= -1;

          en.x = Math.max(TILE + EN_R, Math.min(CW - TILE - EN_R, en.x));
          en.y = Math.max(TILE + EN_R, Math.min(CH - TILE - EN_R, en.y));

          /* player vs enemy */
          if (w.cooldown === 0 && dist2d(w.px, w.py, en.x, en.y) < PL_R + EN_R + 2) {
            w.cooldown = 120;
            setFightName(en.name);
            setOverlay('combat');
          }
        }
      }

      /* ─── draw ─── */
      drawRoom(ctx, map, th, frame);

      /* shop NPCs */
      for (const npc of w.shops) drawShopNpc(ctx, npc, frame);

      /* enemies */
      for (const en of w.enemies) {
        if (!en.alive) continue;
        drawGlowCircle(ctx, en.x, en.y, EN_R, en.col, en.col, en.name);
        /* eye */
        ctx.fillStyle = '#fff';
        ctx.beginPath(); ctx.arc(en.x, en.y - 4, 3, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = '#000';
        ctx.beginPath(); ctx.arc(en.x, en.y - 4, 1.5, 0, Math.PI * 2); ctx.fill();
      }

      /* player */
      drawPlayer(ctx, w.px, w.py, th);

      /* room name watermark */
      ctx.save();
      ctx.fillStyle = 'rgba(255,255,255,0.035)';
      ctx.font = 'bold 52px ui-monospace,monospace';
      ctx.textAlign = 'center';
      ctx.fillText(th.name.toUpperCase(), CW / 2, CH / 2 + 18);
      ctx.restore();
    }

    W.current.animId = requestAnimationFrame(() => tick(0));
    return () => {
      cancelAnimationFrame(W.current.animId);
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup',   onKeyUp);
    };
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const dead = gs.player.hp <= 0;

  return (
    <div className="min-h-screen flex items-center justify-center" style={{ background: '#000' }}>
      <div className="relative shadow-2xl" style={{ width: CW, height: CH }}>
        <canvas ref={canvasRef} width={CW} height={CH} className="block"
                style={{ imageRendering: 'pixelated' }} />

        {/* HUD */}
        {!dead && (
          <Hud
            player={gs.player}
            roomName={THEMES[W.current.roomId]?.name ?? gs.location}
            events={gs.events}
            skills={gs.skills}
            onMap={() => setOverlay('map')}
            onLogout={handleLogout}
          />
        )}

        {/* GAME OVER */}
        {dead && (
          <div className="absolute inset-0 flex flex-col items-center justify-center z-50"
               style={{ background: 'rgba(0,0,0,0.92)', fontFamily: 'ui-monospace,monospace' }}>
            <p className="text-[10px] tracking-widest text-[#ef4444] uppercase mb-3">Connection Lost</p>
            <h2 className="text-4xl font-black text-white mb-2">YOU FELL</h2>
            <p className="text-[#475569] text-sm mb-8">{gs.player.username} has been defeated.</p>
            <button onClick={handleLogout}
                    className="px-8 py-3 rounded-lg font-bold tracking-widest uppercase text-white text-sm"
                    style={{ background: '#dc2626', cursor: 'pointer' }}>
              Restart
            </button>
          </div>
        )}

        {/* COMBAT */}
        {overlay === 'combat' && (
          <CombatOverlay
            enemyName={fightName} player={gs.player}
            events={gs.events} busy={busy}
            onAttack={handleAttack} onFlee={handleFlee}
          />
        )}

        {/* SHOP */}
        {overlay === 'shop' && (
          <ShopOverlay
            shop={gs.shop} player={gs.player}
            busy={busy} lastEvent={shopTxt}
            onBuy={handleBuy} onClose={() => setOverlay('none')}
          />
        )}

        {/* MAP */}
        {overlay === 'map' && (
          <MapOverlay current={W.current.roomId} onClose={() => setOverlay('none')} />
        )}
      </div>
    </div>
  );
}

/* ═══════════════════════════════════════════════════════════════════════
   ROOT
═══════════════════════════════════════════════════════════════════════ */
export default function Page() {
  const [screen,  setScreen]  = useState<'login' | 'game'>('login');
  const [initGs,  setInitGs]  = useState<ApiResult | null>(null);
  const [uname,   setUname]   = useState('');

  const handleLogin = useCallback(async (u: string) => {
    const key = `arcadia_${u}`;
    let res: ApiResult;
    try {
      const saved = localStorage.getItem(key);
      res = saved ? (JSON.parse(saved) as ApiResult) : await callApi('init', null, [], u);
    } catch {
      res = await callApi('init', null, [], u);
    }
    localStorage.setItem(key, JSON.stringify(res));
    setUname(u);
    setInitGs(res);
    setScreen('game');
  }, []);

  if (screen === 'game' && initGs) {
    return <ArcadiaGame initialGs={initGs} username={uname} />;
  }
  return <LoginScreen onLogin={handleLogin} />;
}
