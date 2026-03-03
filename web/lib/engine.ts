/**
 * Arcadia Game Engine — TypeScript port of the C game logic.
 *
 * The engine is pure / stateless: every exported command function
 * receives an EngineState value, mutates a deep-cloned copy, and
 * returns the new state.  No file I/O; callers are responsible for
 * persisting the state (e.g., in localStorage or a DB).
 */

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */
const EVENT_LOG_CAPACITY = 5;
const GS_MAX_INVENTORY   = 32;
const GS_MAX_SKILLS      = 8;
const GS_MAX_SHOP        = 16;

const ROOM_NAMES = [
  "Sunlit Entrance",
  "Forgemaster Armory",
  "Whispering Library",
  "Crystal Sanctum",
] as const;

const ADJ: Array<Array<{ dir: string; dest: number }>> = [
  /* 0 Entrance */ [{ dir: "N", dest: 1 }, { dir: "E", dest: 2 }],
  /* 1 Armory   */ [{ dir: "S", dest: 0 }, { dir: "E", dest: 2 }],
  /* 2 Library  */ [{ dir: "W", dest: 1 }, { dir: "N", dest: 3 }],
  /* 3 Sanctum  */ [{ dir: "S", dest: 2 }],
];

/* ------------------------------------------------------------------ */
/*  Internal State                                                      */
/* ------------------------------------------------------------------ */
export interface ItemData    { name: string; value: number; rarity: number }
export interface QuestData   { title: string; description: string }
export interface SkillData   { name: string; power: number }

export interface EngineState {
  username: string;
  hp: number; max_hp: number;
  attack: number; defense: number;
  gold: number; level: number;
  room_id: number;
  inventory: ItemData[];
  quests: QuestData[];
  skills: SkillData[];
  skill_active: number;
  shop: ItemData[];
  /** Ordered oldest-first; capped at EVENT_LOG_CAPACITY */
  events: string[];
}

/* ------------------------------------------------------------------ */
/*  API Output Shape  (matches what page.tsx expects)                  */
/* ------------------------------------------------------------------ */
export interface GameOutput {
  player: {
    username: string; hp: number; max_hp: number;
    attack: number; defense: number; gold: number; level: number;
  };
  location: string;
  exits: string[];
  inventory: ItemData[];
  quests: QuestData[];
  skills: Array<SkillData & { active: boolean }>;
  shop: ItemData[];
  events: string[];
  error: string | null;
  _state: EngineState;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */
function clone<T>(v: T): T { return JSON.parse(JSON.stringify(v)) as T; }

function enqueueEvent(gs: EngineState, msg: string): void {
  if (gs.events.length >= EVENT_LOG_CAPACITY) gs.events.shift();
  gs.events.push(msg);
}

function stateToOutput(gs: EngineState, error: string | null = null): GameOutput {
  const roomId = gs.room_id >= 0 && gs.room_id < ROOM_NAMES.length
    ? gs.room_id : 0;

  return {
    player: {
      username: gs.username,
      hp: gs.hp, max_hp: gs.max_hp,
      attack: gs.attack, defense: gs.defense,
      gold: gs.gold, level: gs.level,
    },
    location: ROOM_NAMES[roomId],
    exits: ADJ[roomId].map(l => l.dir),
    inventory: gs.inventory,
    quests: gs.quests,
    skills: gs.skills.map((s, i) => ({ ...s, active: i === gs.skill_active })),
    shop: gs.shop,
    events: [...gs.events],
    error,
    _state: gs,
  };
}

/* ------------------------------------------------------------------ */
/*  Seed new game                                                       */
/* ------------------------------------------------------------------ */
function seedNewGame(gs: EngineState): void {
  gs.inventory = [
    { name: "Solar Tonic",  value: 40,  rarity: 1 },
    { name: "Echo Blade",   value: 125, rarity: 3 },
    { name: "Prism Aegis",  value: 90,  rarity: 2 },
  ];
  gs.quests = [
    { title: "Beacon", description: "Restore the lighthouse flames" },
    { title: "Relic",  description: "Retrieve the crystal heart" },
  ];
  gs.skills = [
    { name: "Arcane Bolt",  power: 40 },
    { name: "Shadow Dash",  power: 25 },
    { name: "Iron Bastion", power: 55 },
  ];
  gs.skill_active = 0;
  // Shop — max-heap on rarity; store pre-sorted desc so first item is always max
  gs.shop = [
    { name: "Void Crystal",   value: 200, rarity: 5 },
    { name: "Mana Shard",     value: 80,  rarity: 3 },
    { name: "Steel Ingot",    value: 50,  rarity: 2 },
    { name: "Health Potion",  value: 30,  rarity: 1 },
  ];
}

/* ------------------------------------------------------------------ */
/*  Shop helpers (max-heap by rarity)                                  */
/* ------------------------------------------------------------------ */
function shopHeapifyUp(shop: ItemData[], idx: number): void {
  while (idx > 0) {
    const parent = Math.floor((idx - 1) / 2);
    if (shop[parent].rarity < shop[idx].rarity) {
      [shop[parent], shop[idx]] = [shop[idx], shop[parent]];
      idx = parent;
    } else break;
  }
}

function shopHeapifyDown(shop: ItemData[], idx: number): void {
  const n = shop.length;
  while (true) {
    const l = 2 * idx + 1, r = 2 * idx + 2;
    let largest = idx;
    if (l < n && shop[l].rarity > shop[largest].rarity) largest = l;
    if (r < n && shop[r].rarity > shop[largest].rarity) largest = r;
    if (largest === idx) break;
    [shop[idx], shop[largest]] = [shop[largest], shop[idx]];
    idx = largest;
  }
}

function shopExtractMax(shop: ItemData[]): ItemData | null {
  if (shop.length === 0) return null;
  const top = shop[0];
  const last = shop.pop()!;
  if (shop.length > 0) { shop[0] = last; shopHeapifyDown(shop, 0); }
  return top;
}

/* ------------------------------------------------------------------ */
/*  Combat                                                              */
/* ------------------------------------------------------------------ */
function doCombat(gs: EngineState): void {
  type Combatant = { name: string; hp: number; attack: number; isPlayer: boolean };

  const player: Combatant = {
    name: gs.username, hp: gs.hp, attack: gs.attack, isPlayer: true,
  };
  const enemies: Combatant[] = [
    { name: "Goblin Marauder", hp: 35, attack: 8,  isPlayer: false },
    { name: "Crystal Wisp",    hp: 28, attack: 10, isPlayer: false },
    { name: "Stone Bulwark",   hp: 45, attack: 6,  isPlayer: false },
  ];

  const queue: Combatant[] = [player, ...enemies];

  while (player.hp > 0 && enemies.some(e => e.hp > 0)) {
    const actor = queue.shift();
    if (!actor) break;
    if (actor.hp <= 0) continue;

    if (actor.isPlayer) {
      const tgt = enemies.find(e => e.hp > 0);
      if (!tgt) { queue.push(actor); continue; }
      const dmg = actor.attack;
      tgt.hp = Math.max(0, tgt.hp - dmg);
      enqueueEvent(gs, `${actor.name} strikes ${tgt.name} for ${dmg} damage.`);
      if (tgt.hp === 0) enqueueEvent(gs, `${tgt.name} is defeated!`);
    } else {
      const dmg = Math.max(1, actor.attack - Math.floor(gs.defense / 4));
      player.hp = Math.max(0, player.hp - dmg);
      enqueueEvent(gs, `${actor.name} claws ${player.name} for ${dmg} damage.`);
    }

    if (actor.hp > 0) queue.push(actor);
  }

  gs.hp = Math.min(player.hp, gs.max_hp);

  if (gs.hp > 0) {
    gs.gold += 25;
    if (gs.level < 99) gs.level++;
    enqueueEvent(gs, `Victory! +25 gold. Level ${gs.level}.`);
  } else {
    enqueueEvent(gs, "You have fallen in battle.");
    gs.hp = Math.max(1, Math.floor(gs.max_hp / 2));
    enqueueEvent(gs, `Revived at ${gs.hp} HP.`);
  }
}

/* ------------------------------------------------------------------ */
/*  Exported command functions                                          */
/* ------------------------------------------------------------------ */

/** init — create or load a game for the given username. */
export function cmdInit(username: string): GameOutput {
  const gs: EngineState = {
    username: username || "Traveler",
    hp: 100, max_hp: 100,
    attack: 15, defense: 10,
    gold: 125, level: 1,
    room_id: 0,
    inventory: [], quests: [], skills: [], skill_active: 0,
    shop: [], events: [],
  };
  seedNewGame(gs);
  enqueueEvent(gs, "A new hero emerges from the mists of Arcadia.");
  return stateToOutput(gs);
}

/** combat — run a combat round. */
export function cmdCombat(prev: EngineState): GameOutput {
  const gs = clone(prev);
  doCombat(gs);
  return stateToOutput(gs);
}

/** move — travel in a direction ("N","S","E","W"). */
export function cmdMove(prev: EngineState, dir: string): GameOutput {
  if (!dir) return stateToOutput(clone(prev), "No direction provided.");
  const upper = dir.toUpperCase();
  const links = ADJ[prev.room_id] ?? [];
  const link  = links.find(l => l.dir === upper);
  if (!link)  return stateToOutput(clone(prev), "No passage in that direction.");

  const gs = clone(prev);
  const destName = ROOM_NAMES[link.dest] ?? "Unknown";
  enqueueEvent(gs, `Traveled ${upper} to ${destName}.`);
  gs.room_id = link.dest;
  return stateToOutput(gs);
}

/** buy — extract the highest-rarity item from the shop. */
export function cmdBuy(prev: EngineState): GameOutput {
  const gs = clone(prev);
  const item = shopExtractMax(gs.shop);
  if (!item) return stateToOutput(gs, "Shop is empty.");
  if (gs.gold < item.value) {
    // put it back
    gs.shop.push(item);
    shopHeapifyUp(gs.shop, gs.shop.length - 1);
    return stateToOutput(gs, `Not enough gold. Need ${item.value}g, have ${gs.gold}g.`);
  }
  if (gs.inventory.length >= GS_MAX_INVENTORY) {
    gs.shop.push(item);
    shopHeapifyUp(gs.shop, gs.shop.length - 1);
    return stateToOutput(gs, "Inventory is full.");
  }
  gs.gold -= item.value;
  gs.inventory.push(item);
  enqueueEvent(gs, `Bought ${item.name} for ${item.value}g. Gold: ${gs.gold}.`);
  return stateToOutput(gs);
}

/** skill rotate — advance the active skill ring index. */
export function cmdSkillRotate(prev: EngineState): GameOutput {
  if (prev.skills.length === 0)
    return stateToOutput(clone(prev), "No skills available.");
  const gs = clone(prev);
  gs.skill_active = (gs.skill_active + 1) % gs.skills.length;
  enqueueEvent(gs, `Active skill: ${gs.skills[gs.skill_active].name}.`);
  return stateToOutput(gs);
}

/** skill learn — add a new skill to the ring. */
export function cmdSkillLearn(prev: EngineState, name: string, power: number): GameOutput {
  if (!name) return stateToOutput(clone(prev), "Skill name required.");
  if (prev.skills.length >= GS_MAX_SKILLS)
    return stateToOutput(clone(prev), "Skill ring is full.");
  const gs = clone(prev);
  gs.skills.push({ name: name.slice(0, 31), power });
  enqueueEvent(gs, `Learned skill: ${name} (Power ${power}).`);
  return stateToOutput(gs);
}
