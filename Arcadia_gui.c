#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * FLAT PERSISTABLE GAME STATE (Core Logic)
 * ============================================================ */
#define MAX_USERNAME_LENGTH 32
#define EVENT_LOG_CAPACITY 10
#define EVENT_MESSAGE_LENGTH 128
#define GS_MAX_INVENTORY 32
#define GS_MAX_QUESTS    16
#define GS_MAX_SKILLS     8
#define GS_MAX_SHOP      16
#define GS_STATE_FILE    "game_state.dat"

typedef struct {
    char name[32];
    int value;
    int rarity;
} Item;

typedef struct {
    char title[64];
    char description[128];
} Quest;

typedef struct {
    char skill_name[32];
    int  skill_power;
} FlatSkill;

typedef struct {
    char   username[MAX_USERNAME_LENGTH];
    int    hp, max_hp, attack, defense, gold, level;
    int    room_id;
    Item   inventory[GS_MAX_INVENTORY];
    size_t inv_count;
    Quest  quests[GS_MAX_QUESTS];
    size_t quest_count;
    FlatSkill skills[GS_MAX_SKILLS];
    size_t    skill_count;
    size_t    skill_active;
    Item   shop[GS_MAX_SHOP];
    size_t shop_count;
    char   events[EVENT_LOG_CAPACITY][EVENT_MESSAGE_LENGTH];
    size_t event_count;
    size_t event_head;
    bool   initialized;
} GameState;

typedef struct {
    char name[32];
    int hp;
    int attack;
    bool is_player;
} Combatant;

typedef struct {
    Combatant **slots;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} CombatQueue;

#define ROOM_COUNT 4
static const char *kRoomNames[ROOM_COUNT] = {
  "Sunlit Entrance", "Forgemaster Armory",
  "Whispering Library", "Crystal Sanctum"
};

typedef struct { const char *dir; int dest; } RoomLink;
static const RoomLink kAdj[ROOM_COUNT][4] = {
  {{"N",1},{"E",2},{NULL,-1},{NULL,-1}},
  {{"S",0},{"E",2},{NULL,-1},{NULL,-1}},
  {{"W",1},{"N",3},{NULL,-1},{NULL,-1}},
  {{"S",2},{NULL,-1},{NULL,-1},{NULL,-1}},
};

/* Global Game State */
GameState gs;

/* ---- Event log helper ---- */
static void gsEnqueueEvent(GameState *g, const char *msg) {
    size_t idx;
    if (g == NULL || msg == NULL) return;
    
    if (g->event_count < EVENT_LOG_CAPACITY) {
        idx = (g->event_head + g->event_count) % EVENT_LOG_CAPACITY;
        g->event_count++;
    } else {
        idx = g->event_head;
        g->event_head = (g->event_head + 1U) % EVENT_LOG_CAPACITY;
    }
    strncpy(g->events[idx], msg, EVENT_MESSAGE_LENGTH - 1U);
    g->events[idx][EVENT_MESSAGE_LENGTH - 1U] = '\0';
}

/* ---- Shop Heap ---- */
static void gsShopHeapifyUp(GameState *g, size_t idx) {
    while (idx > 0U) {
        size_t parent = (idx - 1U) / 2U;
        if (g->shop[parent].rarity < g->shop[idx].rarity) {
            Item tmp = g->shop[parent];
            g->shop[parent] = g->shop[idx];
            g->shop[idx] = tmp;
            idx = parent;
        } else break;
    }
}

static void gsShopHeapifyDown(GameState *g, size_t idx) {
    size_t l, r, largest;
    while (true) {
        l = 2U * idx + 1U; r = 2U * idx + 2U; largest = idx;
        if (l < g->shop_count && g->shop[l].rarity > g->shop[largest].rarity) largest = l;
        if (r < g->shop_count && g->shop[r].rarity > g->shop[largest].rarity) largest = r;
        if (largest == idx) break;
        Item tmp = g->shop[idx];
        g->shop[idx] = g->shop[largest];
        g->shop[largest] = tmp;
        idx = largest;
    }
}

static bool gsShopInsert(GameState *g, const Item *item) {
    if (g == NULL || item == NULL || g->shop_count >= GS_MAX_SHOP) return false;
    g->shop[g->shop_count] = *item;
    gsShopHeapifyUp(g, g->shop_count);
    g->shop_count++;
    return true;
}

static bool gsShopExtractMax(GameState *g, Item *out) {
    if (g == NULL || g->shop_count == 0U) return false;
    if (out != NULL) *out = g->shop[0];
    g->shop_count--;
    if (g->shop_count > 0U) {
        g->shop[0] = g->shop[g->shop_count];
        gsShopHeapifyDown(g, 0U);
    }
    return true;
}

/* ---- Save/Load ---- */
static void saveGameState(const GameState *g) {
    FILE *f;
    if (g == NULL) return;
    f = fopen(GS_STATE_FILE, "wb");
    if (f != NULL) {
        fwrite(g, sizeof(GameState), 1U, f);
        fclose(f);
    }
}

static bool loadGameState(GameState *g) {
    FILE *f;
    bool found;
    if (g == NULL) return false;
    f = fopen(GS_STATE_FILE, "rb");
    if (f == NULL) return false;
    found = (fread(g, sizeof(GameState), 1U, f) == 1U);
    fclose(f);
    return found;
}

/* ---- Combat Logic ---- */
static bool initCombatQueue(CombatQueue *queue, size_t capacity) {
    queue->slots = (Combatant **)malloc(capacity * sizeof(Combatant *));
    if (queue->slots == NULL) return false;
    queue->capacity = capacity; queue->head = 0U; queue->tail = 0U; queue->count = 0U;
    return true;
}

static void freeCombatQueue(CombatQueue *queue) {
    free(queue->slots);
}

static bool enqueueCombatant(CombatQueue *queue, Combatant *combatant) {
    if (queue->count == queue->capacity) return false;
    queue->slots[queue->tail] = combatant;
    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;
    return true;
}

static Combatant *dequeueCombatant(CombatQueue *queue) {
    Combatant *combatant;
    if (queue->count == 0U) return NULL;
    combatant = queue->slots[queue->head];
    queue->head = (queue->head + 1U) % queue->capacity;
    queue->count--;
    return combatant;
}

static void gsCombat(GameState *g) {
    char msg[EVENT_MESSAGE_LENGTH];
    Combatant player_unit;
    CombatQueue queue;
    int i; /* C89 fix */

    Combatant enemies[3] = {
        {"Goblin", 35, 8, false}, {"Wisp", 28, 10, false}, {"Golem", 45, 6, false}
    };

    strncpy(player_unit.name, g->username, 31); player_unit.name[31] = '\0';
    player_unit.hp = g->hp; player_unit.attack = g->attack; player_unit.is_player = true;

    initCombatQueue(&queue, 8U);
    enqueueCombatant(&queue, &player_unit);
    
    for (i = 0; i < 3; ++i) enqueueCombatant(&queue, &enemies[i]);

    while (player_unit.hp > 0 && (enemies[0].hp > 0 || enemies[1].hp > 0 || enemies[2].hp > 0)) {
        Combatant *actor = dequeueCombatant(&queue);
        if (actor == NULL) break;
        if (actor->hp <= 0) continue;

        if (actor->is_player) {
            int tidx = 3;
            for(i = 0; i < 3; i++) {
                if(enemies[i].hp > 0) { tidx = i; break; }
            }
            if (tidx == 3) { enqueueCombatant(&queue, actor); continue; }
            
            Combatant *tgt = &enemies[tidx];
            tgt->hp -= actor->attack;
            if (tgt->hp < 0) tgt->hp = 0;
            snprintf(msg, sizeof(msg), "%s hit %s (%d dmg)", actor->name, tgt->name, actor->attack);
            gsEnqueueEvent(g, msg);
        } else {
            int dmg = actor->attack - (g->defense / 4);
            if (dmg < 1) dmg = 1;
            player_unit.hp -= dmg;
            if (player_unit.hp < 0) player_unit.hp = 0;
            snprintf(msg, sizeof(msg), "%s hit %s (%d dmg)", actor->name, player_unit.name, dmg);
            gsEnqueueEvent(g, msg);
        }
        if (actor->hp > 0) enqueueCombatant(&queue, actor);
    }

    g->hp = player_unit.hp;
    if (g->hp > 0) {
        g->gold += 25; g->level++;
        gsEnqueueEvent(g, "Victory! +25 Gold.");
    } else {
        g->hp = g->max_hp / 2;
        gsEnqueueEvent(g, "Defeated. Revived at half HP.");
    }
    freeCombatQueue(&queue);
}

/* ============================================================
 * WIN32 GUI IMPLEMENTATION
 * ============================================================ */
#define ID_BTN_INIT 101
#define ID_BTN_MOVE 102
#define ID_BTN_COMBAT 103
#define ID_BTN_SHOP 104
#define ID_BTN_SAVE 105

HWND hStatsText, hLogText, hInvText;

void UpdateUI() {
    char buffer[1024];
    char invBuffer[1024] = "Inventory:\r\n";
    char logBuffer[4096] = "";
    size_t i; /* C89 fix */
    
    // Update Stats
    snprintf(buffer, sizeof(buffer), "Name: %s\r\nLevel: %d\r\nHP: %d/%d\r\nGold: %d\r\nAtk: %d | Def: %d\r\nLocation: %s",
             gs.username[0] ? gs.username : "None", gs.level, gs.hp, gs.max_hp, gs.gold, gs.attack, gs.defense, 
             gs.room_id < ROOM_COUNT ? kRoomNames[gs.room_id] : "Unknown");
    SetWindowText(hStatsText, buffer);

    // Update Inventory
    for(i = 0; i < gs.inv_count; i++) {
        char item[64];
        snprintf(item, sizeof(item), "- %s (Val:%d)\r\n", gs.inventory[i].name, gs.inventory[i].value);
        strcat(invBuffer, item);
    }
    SetWindowText(hInvText, invBuffer);

    // Update Event Log
    for (i = 0U; i < gs.event_count; ++i) {
        size_t idx = (gs.event_head + i) % EVENT_LOG_CAPACITY;
        strcat(logBuffer, gs.events[idx]);
        strcat(logBuffer, "\r\n");
    }
    SetWindowText(hLogText, logBuffer);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
        {
            // Fonts
            HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                                     OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                                     DEFAULT_PITCH | FF_DONTCARE, TEXT("Consolas"));

            // Buttons
            CreateWindow("BUTTON", "Initialize Game", WS_VISIBLE | WS_CHILD, 20, 20, 150, 30, hwnd, (HMENU)ID_BTN_INIT, NULL, NULL);
            CreateWindow("BUTTON", "Explore Next Room", WS_VISIBLE | WS_CHILD, 20, 60, 150, 30, hwnd, (HMENU)ID_BTN_MOVE, NULL, NULL);
            CreateWindow("BUTTON", "Engage Combat", WS_VISIBLE | WS_CHILD, 20, 100, 150, 30, hwnd, (HMENU)ID_BTN_COMBAT, NULL, NULL);
            CreateWindow("BUTTON", "Buy from Shop", WS_VISIBLE | WS_CHILD, 20, 140, 150, 30, hwnd, (HMENU)ID_BTN_SHOP, NULL, NULL);
            CreateWindow("BUTTON", "Save & Exit", WS_VISIBLE | WS_CHILD, 20, 180, 150, 30, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL);

            // Display Panels
            hStatsText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY, 190, 20, 200, 130, hwnd, NULL, NULL, NULL);
            hInvText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY, 400, 20, 200, 190, hwnd, NULL, NULL, NULL);
            hLogText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 190, 220, 410, 150, hwnd, NULL, NULL, NULL);
            
            SendMessage(hStatsText, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hInvText, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hLogText, WM_SETFONT, (WPARAM)hFont, TRUE);

            if(loadGameState(&gs)) {
                gsEnqueueEvent(&gs, "Game Loaded from disk.");
                UpdateUI();
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_BTN_INIT:
                {
                    Item i1 = {"Health Potion", 30, 1};
                    Item i2 = {"Echo Blade", 125, 3};
                    Item i3 = {"Void Crystal", 200, 5};
                    
                    memset(&gs, 0, sizeof(GameState));
                    strcpy(gs.username, "Surya");
                    gs.hp = 100; gs.max_hp = 100; gs.attack = 15; gs.defense = 10; gs.gold = 50; gs.level = 1;
                    
                    gsShopInsert(&gs, &i1);
                    gsShopInsert(&gs, &i2);
                    gsShopInsert(&gs, &i3);
                    
                    gsEnqueueEvent(&gs, "New Game Initialized. Heap loaded.");
                    UpdateUI();
                    break;
                }

                case ID_BTN_MOVE:
                {
                    char movMsg[128];
                    if (gs.username[0] == '\0') break;
                    gs.room_id = (gs.room_id + 1) % ROOM_COUNT; // Simple loop traversal
                    snprintf(movMsg, sizeof(movMsg), "Traversed to %s.", kRoomNames[gs.room_id]);
                    gsEnqueueEvent(&gs, movMsg);
                    UpdateUI();
                    break;
                }

                case ID_BTN_COMBAT:
                    if (gs.username[0] == '\0') break;
                    gsCombat(&gs);
                    UpdateUI();
                    break;

                case ID_BTN_SHOP:
                {
                    Item bought;
                    if (gs.username[0] == '\0') break;
                    if (gsShopExtractMax(&gs, &bought)) {
                        char buyMsg[128];
                        gs.inventory[gs.inv_count++] = bought;
                        snprintf(buyMsg, sizeof(buyMsg), "Bought highest rarity: %s", bought.name);
                        gsEnqueueEvent(&gs, buyMsg);
                    } else {
                        gsEnqueueEvent(&gs, "Shop is empty.");
                    }
                    UpdateUI();
                    break;
                }

                case ID_BTN_SAVE:
                    if (gs.username[0] != '\0') {
                        saveGameState(&gs);
                        MessageBox(hwnd, "Game Saved Successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                        PostQuitMessage(0);
                    }
                    break;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[]  = "ArcadiaWindow";
    HWND hwnd;
    MSG msg = {0};
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0, CLASS_NAME, "Arcadia RPG - Player Hub", 
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX, // Non-resizable
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 420,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
