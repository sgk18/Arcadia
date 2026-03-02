#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "raylib.h"

#define ANSI_CLEAR_SCREEN "\033[2J\033[H"
#define ANSI_COLOR_BORDER "\033[1;36m"
#define ANSI_COLOR_TITLE "\033[1;33m"
#define ANSI_COLOR_SUCCESS "\033[1;32m"
#define ANSI_COLOR_WARNING "\033[1;31m"
#define ANSI_COLOR_RESET "\033[0m"
#define MAX_USERNAME_LENGTH 32
#define MAX_INPUT_LENGTH 128
#define PLAYER_DATA_FILE "player_data.dat"
#define LEADERBOARD_SIZE 10
#define EVENT_LOG_CAPACITY 5
#define EVENT_MESSAGE_LENGTH 128
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

/* --------------------------------- Structs --------------------------------- */
typedef struct
{
    char username[MAX_USERNAME_LENGTH];
    int hp;
    int max_hp;
    int attack;
    int defense;
    int gold;
    int level;
} Player;

typedef struct
{
    char name[32];
    int value;
    int rarity;
} Item;

typedef struct InventoryNode
{
    Item item;
    struct InventoryNode *prev;
    struct InventoryNode *next;
} InventoryNode;

typedef struct
{
    InventoryNode *head;
    InventoryNode *tail;
    size_t count;
} Inventory;

typedef struct
{
    char title[64];
    char description[128];
} Quest;

typedef struct QuestNode
{
    Quest quest;
    struct QuestNode *next;
} QuestNode;

typedef struct
{
    QuestNode *head;
    size_t count;
} QuestLog;

typedef struct
{
    char username[MAX_USERNAME_LENGTH];
    int level;
} LeaderboardEntry;

typedef struct
{
    LeaderboardEntry entries[LEADERBOARD_SIZE];
    size_t count;
} Leaderboard;

typedef enum
{
    MAIN_MENU = 0,
    INVENTORY_MENU,
    QUEST_MENU,
    LEADERBOARD_MENU,
    WORLD_MENU,
    COMBAT_MENU,
    BESTIARY_MENU
} MenuState;

typedef struct
{
    MenuState *states;
    size_t count;
    size_t capacity;
} MenuStack;

typedef struct Edge Edge;

typedef struct Room
{
    int id;
    char name[32];
    Edge *edges;
} Room;

struct Edge
{
    Room *destination;
    char direction[8];
    Edge *next;
};

typedef struct
{
    Room **rooms;
    size_t count;
} WorldGraph;

typedef struct
{
    char name[32];
    int hp;
    int attack;
    bool is_player;
} Combatant;

typedef struct
{
    Combatant **slots;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} CombatQueue;

typedef struct
{
    char **messages;
    size_t capacity;
    size_t count;
    size_t head;
} EventLog;

typedef struct MonsterInfo
{
    int id;
    char name[32];
    char weakness[64];
    struct MonsterInfo *left;
    struct MonsterInfo *right;
} MonsterInfo;

typedef struct
{
    Rectangle bounds;
    const char *label;
} TitleButton;

typedef struct
{
    unsigned long username_hash;
    Player player;
} PlayerRecord;

typedef enum
{
    SCREEN_LOGO = 0,
    SCREEN_TITLE_MENU,
    SCREEN_GAMEPLAY
} GameScreen;

/* --------------------------- Utility Declarations -------------------------- */
static void readLine(char *buffer, size_t length);
static int readInt(void);
static void pauseScreen(void);
static void toUpperString(char *text);
static void clearScreen(void);
static void drawBox(int width, int height);
static void initTitleButtons(int screenWidth, int screenHeight);
static void DrawTitleScreen(void);
static void DrawGameplayScreen(const Player *player, bool viewingLeaderboard, const Leaderboard *board);
static void runTextInterface(MenuStack *stack,
                             Player *player,
                             Inventory *inventory,
                             QuestLog *quests,
                             Leaderboard *board,
                             WorldGraph *world,
                             Room **current_room,
                             EventLog *events,
                             MonsterInfo *bestiary_root);

static const size_t TITLE_BUTTON_COUNT = 3U;
static TitleButton gTitleButtons[3];

/* ----------------------------- Menu Stack Logic ---------------------------- */
static bool initMenuStack(MenuStack *stack)
{
    if (stack == NULL)
    {
        return false;
    }

    stack->count = 0U;
    stack->capacity = 8U;
    stack->states = (MenuState *)malloc(stack->capacity * sizeof(MenuState));

    if (stack->states == NULL)
    {
        stack->capacity = 0U;
        return false;
    }

    return true;
}

static void freeMenuStack(MenuStack *stack)
{
    if (stack == NULL)
    {
        return;
    }

    free(stack->states);
    stack->states = NULL;
    stack->count = 0U;
    stack->capacity = 0U;
}

static bool pushMenuState(MenuStack *stack, MenuState state)
{
    if (stack == NULL)
    {
        return false;
    }

    if (stack->count == stack->capacity)
    {
        size_t new_capacity = stack->capacity * 2U;
        MenuState *resized = (MenuState *)realloc(stack->states, new_capacity * sizeof(MenuState));

        if (resized == NULL)
        {
            return false;
        }

        stack->states = resized;
        stack->capacity = new_capacity;
    }

    stack->states[stack->count++] = state;
    return true;
}

static bool popMenuState(MenuStack *stack, MenuState *state)
{
    if (stack == NULL || stack->count == 0U)
    {
        return false;
    }

    stack->count--;

    if (state != NULL)
    {
        *state = stack->states[stack->count];
    }

    return true;
}

static bool peekMenuState(const MenuStack *stack, MenuState *state)
{
    if (stack == NULL || stack->count == 0U || state == NULL)
    {
        return false;
    }

    *state = stack->states[stack->count - 1U];
    return true;
}

/* --------------------------- Inventory Management -------------------------- */
static void initInventory(Inventory *inventory)
{
    if (inventory == NULL)
    {
        return;
    }

    inventory->head = NULL;
    inventory->tail = NULL;
    inventory->count = 0U;
}

static bool addItemToInventory(Inventory *inventory, const Item *item)
{
    if (inventory == NULL || item == NULL)
    {
        return false;
    }

    InventoryNode *node = (InventoryNode *)malloc(sizeof(InventoryNode));
    if (node == NULL)
    {
        return false;
    }

    node->item = *item;
    node->prev = inventory->tail;
    node->next = NULL;

    if (inventory->tail != NULL)
    {
        inventory->tail->next = node;
    }
    else
    {
        inventory->head = node;
    }

    inventory->tail = node;
    inventory->count++;
    return true;
}

static bool useItemFromInventory(Inventory *inventory, const char *name)
{
    if (inventory == NULL || name == NULL)
    {
        return false;
    }

    InventoryNode *current = inventory->head;
    while (current != NULL)
    {
        if (strncmp(current->item.name, name, sizeof(current->item.name)) == 0)
        {
            if (current->prev != NULL)
            {
                current->prev->next = current->next;
            }
            else
            {
                inventory->head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->prev = current->prev;
            }
            else
            {
                inventory->tail = current->prev;
            }

            free(current);
            inventory->count--;
            return true;
        }
        current = current->next;
    }

    return false;
}

static void printInventoryForward(const Inventory *inventory)
{
    printf("%sInventory (forward traversal):%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    if (inventory == NULL || inventory->head == NULL)
    {
        printf("  [Empty]\n");
        return;
    }

    const InventoryNode *current = inventory->head;
    while (current != NULL)
    {
        printf("  %-16s | Value %3d | Rarity %d\n",
               current->item.name,
               current->item.value,
               current->item.rarity);
        current = current->next;
    }
}

static void printInventoryBackward(const Inventory *inventory)
{
    printf("%sInventory (reverse traversal):%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    if (inventory == NULL || inventory->tail == NULL)
    {
        printf("  [Empty]\n");
        return;
    }

    const InventoryNode *current = inventory->tail;
    while (current != NULL)
    {
        printf("  %-16s | Value %3d | Rarity %d\n",
               current->item.name,
               current->item.value,
               current->item.rarity);
        current = current->prev;
    }
}

static void freeInventory(Inventory *inventory)
{
    if (inventory == NULL)
    {
        return;
    }

    InventoryNode *current = inventory->head;
    while (current != NULL)
    {
        InventoryNode *next = current->next;
        free(current);
        current = next;
    }

    inventory->head = NULL;
    inventory->tail = NULL;
    inventory->count = 0U;
}

/* ----------------------------- Quest Log System ----------------------------- */
static void initQuestLog(QuestLog *log)
{
    if (log == NULL)
    {
        return;
    }

    log->head = NULL;
    log->count = 0U;
}

static bool addQuest(QuestLog *log, const char *title, const char *description)
{
    if (log == NULL || title == NULL || description == NULL)
    {
        return false;
    }

    QuestNode *node = (QuestNode *)malloc(sizeof(QuestNode));
    if (node == NULL)
    {
        return false;
    }

    strncpy(node->quest.title, title, sizeof(node->quest.title) - 1U);
    node->quest.title[sizeof(node->quest.title) - 1U] = '\0';
    strncpy(node->quest.description, description, sizeof(node->quest.description) - 1U);
    node->quest.description[sizeof(node->quest.description) - 1U] = '\0';

    node->next = log->head;
    log->head = node;
    log->count++;
    return true;
}

static bool completeQuest(QuestLog *log, const char *title)
{
    if (log == NULL || title == NULL)
    {
        return false;
    }

    QuestNode *current = log->head;
    QuestNode *previous = NULL;

    while (current != NULL)
    {
        if (strncmp(current->quest.title, title, sizeof(current->quest.title)) == 0)
        {
            if (previous == NULL)
            {
                log->head = current->next;
            }
            else
            {
                previous->next = current->next;
            }

            free(current);
            if (log->count > 0U)
            {
                log->count--;
            }
            return true;
        }

        previous = current;
        current = current->next;
    }

    return false;
}

static void printQuestLog(const QuestLog *log)
{
    printf("%sActive Quests:%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    if (log == NULL || log->head == NULL)
    {
        printf("  No quests tracked.\n");
        return;
    }

    const QuestNode *current = log->head;
    while (current != NULL)
    {
        printf("  %-20s | %s\n", current->quest.title, current->quest.description);
        current = current->next;
    }
}

static void freeQuestLog(QuestLog *log)
{
    if (log == NULL)
    {
        return;
    }

    QuestNode *current = log->head;
    while (current != NULL)
    {
        QuestNode *next = current->next;
        free(current);
        current = next;
    }

    log->head = NULL;
    log->count = 0U;
}

/* ---------------------------- Leaderboard Engine --------------------------- */
static void initLeaderboard(Leaderboard *board)
{
    if (board == NULL)
    {
        return;
    }

    board->count = 0U;
}

static void sortLeaderboard(Leaderboard *board)
{
    if (board == NULL)
    {
        return;
    }

    bool swapped = true;
    while (swapped)
    {
        swapped = false;
        for (size_t i = 0U; i + 1U < board->count; ++i)
        {
            if (board->entries[i].level < board->entries[i + 1U].level)
            {
                LeaderboardEntry temp = board->entries[i];
                board->entries[i] = board->entries[i + 1U];
                board->entries[i + 1U] = temp;
                swapped = true;
            }
        }
    }
}

static void addLeaderboardEntry(Leaderboard *board, const char *username, int level)
{
    if (board == NULL || username == NULL)
    {
        return;
    }

    if (board->count < LEADERBOARD_SIZE)
    {
        strncpy(board->entries[board->count].username, username, MAX_USERNAME_LENGTH - 1U);
        board->entries[board->count].username[MAX_USERNAME_LENGTH - 1U] = '\0';
        board->entries[board->count].level = level;
        board->count++;
    }
    else
    {
        size_t lowest_index = 0U;
        for (size_t i = 1U; i < board->count; ++i)
        {
            if (board->entries[i].level < board->entries[lowest_index].level)
            {
                lowest_index = i;
            }
        }

        if (level > board->entries[lowest_index].level)
        {
            strncpy(board->entries[lowest_index].username, username, MAX_USERNAME_LENGTH - 1U);
            board->entries[lowest_index].username[MAX_USERNAME_LENGTH - 1U] = '\0';
            board->entries[lowest_index].level = level;
        }
    }

    sortLeaderboard(board);
}

static void displayLeaderboard(const Leaderboard *board)
{
    clearScreen();
    drawBox(60, 4);
    printf("%sArcadia Leaderboard%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);

    if (board == NULL || board->count == 0U)
    {
        printf("  No entries recorded.\n");
        return;
    }

    for (size_t i = 0U; i < board->count; ++i)
    {
        printf("  %zu) %-20s | Level %d\n", i + 1U, board->entries[i].username, board->entries[i].level);
    }
}

/* --------------------------- Player File Handling -------------------------- */
static unsigned long hashUsername(const char *username)
{
    unsigned long hash = 5381UL;
    int c = 0;

    while (username != NULL && (c = *username++) != '\0')
    {
        hash = ((hash << 5) + hash) + (unsigned long)c;
    }

    return hash;
}

static bool savePlayer(const Player *player)
{
    if (player == NULL)
    {
        return false;
    }

    FILE *file = fopen(PLAYER_DATA_FILE, "rb+");
    if (file == NULL)
    {
        file = fopen(PLAYER_DATA_FILE, "wb+");
        if (file == NULL)
        {
            return false;
        }
    }

    PlayerRecord record;
    unsigned long target_hash = hashUsername(player->username);

    while (fread(&record, sizeof(PlayerRecord), 1U, file) == 1U)
    {
        if (record.username_hash == target_hash && strncmp(record.player.username, player->username, MAX_USERNAME_LENGTH) == 0)
        {
            long position = (long)(ftell(file) - (long)sizeof(PlayerRecord));
            if (position < 0)
            {
                fclose(file);
                return false;
            }

            record.player = *player;
            record.username_hash = target_hash;

            if (fseek(file, position, SEEK_SET) != 0)
            {
                fclose(file);
                return false;
            }

            if (fwrite(&record, sizeof(PlayerRecord), 1U, file) != 1U)
            {
                fclose(file);
                return false;
            }

            fflush(file);
            fclose(file);
            return true;
        }
    }

    record.player = *player;
    record.username_hash = target_hash;

    if (fwrite(&record, sizeof(PlayerRecord), 1U, file) != 1U)
    {
        fclose(file);
        return false;
    }

    fflush(file);
    fclose(file);
    return true;
}

static bool loadPlayer(const char *username, Player *out_player)
{
    if (username == NULL || out_player == NULL)
    {
        return false;
    }

    FILE *file = fopen(PLAYER_DATA_FILE, "rb");
    if (file == NULL)
    {
        return false;
    }

    PlayerRecord record;
    unsigned long target_hash = hashUsername(username);

    while (fread(&record, sizeof(PlayerRecord), 1U, file) == 1U)
    {
        if (record.username_hash == target_hash && strncmp(record.player.username, username, MAX_USERNAME_LENGTH) == 0)
        {
            *out_player = record.player;
            fclose(file);
            return true;
        }
    }

    fclose(file);
    return false;
}

/* ----------------------------- Event Log Queue ----------------------------- */
static bool initEventLog(EventLog *log, size_t capacity)
{
    if (log == NULL || capacity == 0U)
    {
        return false;
    }

    log->messages = (char **)malloc(capacity * sizeof(char *));
    if (log->messages == NULL)
    {
        return false;
    }

    for (size_t i = 0U; i < capacity; ++i)
    {
        log->messages[i] = (char *)malloc(EVENT_MESSAGE_LENGTH);
        if (log->messages[i] == NULL)
        {
            for (size_t j = 0U; j < i; ++j)
            {
                free(log->messages[j]);
            }
            free(log->messages);
            log->messages = NULL;
            return false;
        }
        log->messages[i][0] = '\0';
    }

    log->capacity = capacity;
    log->count = 0U;
    log->head = 0U;
    return true;
}

static void resetEventLog(EventLog *log)
{
    if (log == NULL || log->messages == NULL)
    {
        return;
    }

    for (size_t i = 0U; i < log->capacity; ++i)
    {
        log->messages[i][0] = '\0';
    }

    log->count = 0U;
    log->head = 0U;
}

static void enqueueEvent(EventLog *log, const char *message)
{
    if (log == NULL || log->messages == NULL || message == NULL)
    {
        return;
    }

    size_t index = 0U;
    if (log->count < log->capacity)
    {
        index = (log->head + log->count) % log->capacity;
        log->count++;
    }
    else
    {
        index = log->head;
        log->head = (log->head + 1U) % log->capacity;
    }

    strncpy(log->messages[index], message, EVENT_MESSAGE_LENGTH - 1U);
    log->messages[index][EVENT_MESSAGE_LENGTH - 1U] = '\0';
}

static void printEventLog(const EventLog *log)
{
    printf("%sRecent Combat Log:%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    if (log == NULL || log->messages == NULL || log->count == 0U)
    {
        printf("  No events recorded.\n");
        return;
    }

    for (size_t i = 0U; i < log->count; ++i)
    {
        size_t index = (log->head + i) % log->capacity;
        printf("  %s\n", log->messages[index]);
    }
}

static void freeEventLog(EventLog *log)
{
    if (log == NULL || log->messages == NULL)
    {
        return;
    }

    for (size_t i = 0U; i < log->capacity; ++i)
    {
        free(log->messages[i]);
    }

    free(log->messages);
    log->messages = NULL;
    log->capacity = 0U;
    log->count = 0U;
    log->head = 0U;
}

/* ------------------------------- World Graph ------------------------------- */
static void initWorldGraph(WorldGraph *graph)
{
    if (graph == NULL)
    {
        return;
    }

    graph->rooms = NULL;
    graph->count = 0U;
}

static void freeWorldGraph(WorldGraph *graph);

static Room *createRoom(int id, const char *name)
{
    Room *room = (Room *)malloc(sizeof(Room));
    if (room == NULL)
    {
        return NULL;
    }

    room->id = id;
    strncpy(room->name, name, sizeof(room->name) - 1U);
    room->name[sizeof(room->name) - 1U] = '\0';
    room->edges = NULL;
    return room;
}

static bool appendRoom(WorldGraph *graph, Room *room)
{
    if (graph == NULL || room == NULL)
    {
        return false;
    }

    Room **resized = (Room **)realloc(graph->rooms, (graph->count + 1U) * sizeof(Room *));
    if (resized == NULL)
    {
        return false;
    }

    graph->rooms = resized;
    graph->rooms[graph->count++] = room;
    return true;
}

static bool addEdge(Room *from, Room *to, const char *direction)
{
    if (from == NULL || to == NULL || direction == NULL)
    {
        return false;
    }

    Edge *edge = (Edge *)malloc(sizeof(Edge));
    if (edge == NULL)
    {
        return false;
    }

    edge->destination = to;
    strncpy(edge->direction, direction, sizeof(edge->direction) - 1U);
    edge->direction[sizeof(edge->direction) - 1U] = '\0';
    edge->next = from->edges;
    from->edges = edge;
    return true;
}

static Room *moveToRoom(Room *current, const char *direction)
{
    if (current == NULL || direction == NULL)
    {
        return NULL;
    }

    Edge *edge = current->edges;
    while (edge != NULL)
    {
        if (strncmp(edge->direction, direction, sizeof(edge->direction)) == 0)
        {
            return edge->destination;
        }
        edge = edge->next;
    }

    return NULL;
}

static void describeRoom(const Room *room)
{
    if (room == NULL)
    {
        printf("Room data unavailable.\n");
        return;
    }

    printf("%sYou stand in %s.%s\n", ANSI_COLOR_TITLE, room->name, ANSI_COLOR_RESET);
    printf("  Exits: ");
    Edge *edge = room->edges;
    if (edge == NULL)
    {
        printf("None\n");
        return;
    }

    while (edge != NULL)
    {
        printf("%s ", edge->direction);
        edge = edge->next;
    }
    printf("\n");
}

static bool buildDefaultWorld(WorldGraph *graph)
{
    if (graph == NULL)
    {
        return false;
    }

    Room *entrance = createRoom(0, "Sunlit Entrance");
    Room *armory = createRoom(1, "Forgemaster Armory");
    Room *library = createRoom(2, "Whispering Library");
    Room *sanctum = createRoom(3, "Crystal Sanctum");

    if (entrance == NULL || armory == NULL || library == NULL || sanctum == NULL)
    {
        free(entrance);
        free(armory);
        free(library);
        free(sanctum);
        return false;
    }

    if (!appendRoom(graph, entrance) ||
        !appendRoom(graph, armory) ||
        !appendRoom(graph, library) ||
        !appendRoom(graph, sanctum))
    {
        freeWorldGraph(graph);
        return false;
    }

    addEdge(entrance, armory, "N");
    addEdge(armory, entrance, "S");
    addEdge(armory, library, "E");
    addEdge(library, armory, "W");
    addEdge(library, sanctum, "N");
    addEdge(sanctum, library, "S");
    addEdge(entrance, library, "E");
    addEdge(library, entrance, "W");

    return true;
}

static void freeWorldGraph(WorldGraph *graph)
{
    if (graph == NULL || graph->rooms == NULL)
    {
        return;
    }

    for (size_t i = 0U; i < graph->count; ++i)
    {
        if (graph->rooms[i] == NULL)
        {
            continue;
        }

        Edge *edge = graph->rooms[i]->edges;
        while (edge != NULL)
        {
            Edge *next = edge->next;
            free(edge);
            edge = next;
        }

        free(graph->rooms[i]);
    }

    free(graph->rooms);
    graph->rooms = NULL;
    graph->count = 0U;
}

/* --------------------------- Combat Queue & Logic -------------------------- */
static bool initCombatQueue(CombatQueue *queue, size_t capacity)
{
    if (queue == NULL || capacity == 0U)
    {
        return false;
    }

    queue->slots = (Combatant **)malloc(capacity * sizeof(Combatant *));
    if (queue->slots == NULL)
    {
        return false;
    }

    queue->capacity = capacity;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    return true;
}

static void freeCombatQueue(CombatQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    free(queue->slots);
    queue->slots = NULL;
    queue->capacity = 0U;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
}

static bool enqueueCombatant(CombatQueue *queue, Combatant *combatant)
{
    if (queue == NULL || combatant == NULL || queue->count == queue->capacity)
    {
        return false;
    }

    queue->slots[queue->tail] = combatant;
    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;
    return true;
}

static Combatant *dequeueCombatant(CombatQueue *queue)
{
    if (queue == NULL || queue->count == 0U)
    {
        return NULL;
    }

    Combatant *combatant = queue->slots[queue->head];
    queue->head = (queue->head + 1U) % queue->capacity;
    queue->count--;
    return combatant;
}

static bool enemiesRemain(const Combatant *enemies, size_t count)
{
    for (size_t i = 0U; i < count; ++i)
    {
        if (enemies[i].hp > 0)
        {
            return true;
        }
    }
    return false;
}

static size_t nextEnemyIndex(Combatant *enemies, size_t count)
{
    for (size_t i = 0U; i < count; ++i)
    {
        if (enemies[i].hp > 0)
        {
            return i;
        }
    }
    return count;
}

static void startCombat(Player *player, EventLog *log)
{
    if (player == NULL || log == NULL)
    {
        return;
    }

    CombatQueue queue;
    if (!initCombatQueue(&queue, 6U))
    {
        enqueueEvent(log, "Combat queue initialization failed.");
        return;
    }

    Combatant player_unit;
    strncpy(player_unit.name, player->username, sizeof(player_unit.name) - 1U);
    player_unit.name[sizeof(player_unit.name) - 1U] = '\0';
    player_unit.hp = player->hp;
    player_unit.attack = player->attack;
    player_unit.is_player = true;

    Combatant enemies[3] = {
        {"Goblin Marauder", 35, 8, false},
        {"Crystal Wisp", 28, 10, false},
        {"Stone Bulwark", 45, 6, false}};

    enqueueCombatant(&queue, &player_unit);
    for (size_t i = 0U; i < 3U; ++i)
    {
        enqueueCombatant(&queue, &enemies[i]);
    }

    char message[EVENT_MESSAGE_LENGTH];

    while (player_unit.hp > 0 && enemiesRemain(enemies, 3U))
    {
        Combatant *actor = dequeueCombatant(&queue);
        if (actor == NULL)
        {
            break;
        }

        if (actor->hp <= 0)
        {
            continue;
        }

        if (actor->is_player)
        {
            size_t index = nextEnemyIndex(enemies, 3U);
            if (index == 3U)
            {
                enqueueCombatant(&queue, actor);
                continue;
            }

            Combatant *target = &enemies[index];
            int damage = actor->attack;
            target->hp -= damage;
            if (target->hp < 0)
            {
                target->hp = 0;
            }

            snprintf(message, sizeof(message), "%s strikes %s for %d damage.",
                     actor->name,
                     target->name,
                     damage);
            enqueueEvent(log, message);

            if (target->hp == 0)
            {
                snprintf(message, sizeof(message), "%s is defeated!", target->name);
                enqueueEvent(log, message);
            }
        }
        else
        {
            int damage = actor->attack - (player->defense / 4);
            if (damage < 1)
            {
                damage = 1;
            }

            player_unit.hp -= damage;
            if (player_unit.hp < 0)
            {
                player_unit.hp = 0;
            }

            snprintf(message, sizeof(message), "%s claws %s for %d damage.",
                     actor->name,
                     player_unit.name,
                     damage);
            enqueueEvent(log, message);
        }

        if (actor->hp > 0)
        {
            enqueueCombatant(&queue, actor);
        }
    }

    player->hp = player_unit.hp;
    if (player->hp > player->max_hp)
    {
        player->hp = player->max_hp;
    }

    if (player->hp > 0)
    {
        player->gold += 25;
        if (player->level < 99)
        {
            player->level++;
        }
        snprintf(message, sizeof(message), "Victory! +25 gold. Level %d.", player->level);
        enqueueEvent(log, message);
    }
    else
    {
        enqueueEvent(log, "You have fallen in battle.");
        player->hp = player->max_hp / 2;
        if (player->hp == 0)
        {
            player->hp = 1;
        }
        snprintf(message, sizeof(message), "Revived at %d HP.", player->hp);
        enqueueEvent(log, message);
    }

    freeCombatQueue(&queue);
}

/* -------------------------------- Bestiary BST ----------------------------- */
static MonsterInfo *insertMonster(MonsterInfo *root, int id, const char *name, const char *weakness)
{
    if (name == NULL || weakness == NULL)
    {
        return root;
    }

    if (root == NULL)
    {
        MonsterInfo *node = (MonsterInfo *)malloc(sizeof(MonsterInfo));
        if (node == NULL)
        {
            return NULL;
        }

        node->id = id;
        strncpy(node->name, name, sizeof(node->name) - 1U);
        node->name[sizeof(node->name) - 1U] = '\0';
        strncpy(node->weakness, weakness, sizeof(node->weakness) - 1U);
        node->weakness[sizeof(node->weakness) - 1U] = '\0';
        node->left = NULL;
        node->right = NULL;
        return node;
    }

    if (id < root->id)
    {
        MonsterInfo *child = insertMonster(root->left, id, name, weakness);
        if (child != NULL)
        {
            root->left = child;
        }
    }
    else if (id > root->id)
    {
        MonsterInfo *child = insertMonster(root->right, id, name, weakness);
        if (child != NULL)
        {
            root->right = child;
        }
    }
    else
    {
        strncpy(root->name, name, sizeof(root->name) - 1U);
        root->name[sizeof(root->name) - 1U] = '\0';
        strncpy(root->weakness, weakness, sizeof(root->weakness) - 1U);
        root->weakness[sizeof(root->weakness) - 1U] = '\0';
    }

    return root;
}

static const MonsterInfo *searchMonster(const MonsterInfo *root, int id)
{
    if (root == NULL)
    {
        return NULL;
    }

    if (id < root->id)
    {
        return searchMonster(root->left, id);
    }
    if (id > root->id)
    {
        return searchMonster(root->right, id);
    }
    return root;
}

static void printBestiaryInOrder(const MonsterInfo *root)
{
    if (root == NULL)
    {
        return;
    }

    printBestiaryInOrder(root->left);
    printf("  ID %3d | %-16s | Weakness: %s\n", root->id, root->name, root->weakness);
    printBestiaryInOrder(root->right);
}

static void freeBestiary(MonsterInfo *root)
{
    if (root == NULL)
    {
        return;
    }

    freeBestiary(root->left);
    freeBestiary(root->right);
    free(root);
}

/* ----------------------------- Input / UI Utils ---------------------------- */
static void readLine(char *buffer, size_t length)
{
    if (buffer == NULL || length == 0U)
    {
        return;
    }

    if (fgets(buffer, (int)length, stdin) != NULL)
    {
        size_t newline = strcspn(buffer, "\n");
        buffer[newline] = '\0';
    }
    else
    {
        buffer[0] = '\0';
    }
}

static int readInt(void)
{
    char buffer[MAX_INPUT_LENGTH];
    readLine(buffer, sizeof(buffer));
    return (int)strtol(buffer, NULL, 10);
}

static void pauseScreen(void)
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    char buffer[MAX_INPUT_LENGTH];
    readLine(buffer, sizeof(buffer));
}

static void toUpperString(char *text)
{
    if (text == NULL)
    {
        return;
    }

    for (size_t i = 0U; text[i] != '\0'; ++i)
    {
        if (text[i] >= 'a' && text[i] <= 'z')
        {
            text[i] = (char)(text[i] - ('a' - 'A'));
        }
    }
}

static void clearScreen(void)
{
    printf("%s", ANSI_CLEAR_SCREEN);
    fflush(stdout);
}

static void drawBox(int width, int height)
{
    if (width < 2 || height < 2)
    {
        return;
    }

    printf("%s", ANSI_COLOR_BORDER);

    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            bool is_top_or_bottom = (row == 0) || (row == height - 1);
            bool is_side = (col == 0) || (col == width - 1);

            if (is_top_or_bottom)
            {
                putchar((col == 0 || col == width - 1) ? '+' : '-');
            }
            else if (is_side)
            {
                putchar('|');
            }
            else
            {
                putchar(' ');
            }
        }
        putchar('\n');
    }

    printf("%s", ANSI_COLOR_RESET);
    fflush(stdout);
}

/* --------------------------- Raylib UI Helpers ---------------------------- */
static void initTitleButtons(int screenWidth, int screenHeight)
{
    const char *labels[TITLE_BUTTON_COUNT] = {"Start Game", "Leaderboard", "Exit"};
    float button_width = 240.0f;
    float button_height = 54.0f;
    float spacing = 18.0f;
    float origin_x = ((float)screenWidth - button_width) * 0.5f;
    float origin_y = (float)screenHeight * 0.45f;

    for (size_t i = 0U; i < TITLE_BUTTON_COUNT; ++i)
    {
        gTitleButtons[i].bounds = (Rectangle){origin_x,
                                              origin_y + ((button_height + spacing) * (float)i),
                                              button_width,
                                              button_height};
        gTitleButtons[i].label = labels[i];
    }
}

static void DrawTitleScreen(void)
{
    Vector2 mouse = GetMousePosition();
    DrawText("Arcadia RPG", 220, 140, 48, RAYWHITE);
    DrawText("Systems Online", 260, 200, 22, LIGHTGRAY);

    for (size_t i = 0U; i < TITLE_BUTTON_COUNT; ++i)
    {
        bool hovered = CheckCollisionPointRec(mouse, gTitleButtons[i].bounds);
        Color fill = hovered ? SKYBLUE : (Color){25, 25, 25, 230};
        Color outline = hovered ? RAYWHITE : SKYBLUE;

        DrawRectangleRec(gTitleButtons[i].bounds, fill);
        DrawRectangleLinesEx(gTitleButtons[i].bounds, 2.0f, outline);

        int font_size = 20;
        int text_width = MeasureText(gTitleButtons[i].label, font_size);
        float text_x = gTitleButtons[i].bounds.x + (gTitleButtons[i].bounds.width - (float)text_width) * 0.5f;
        float text_y = gTitleButtons[i].bounds.y + (gTitleButtons[i].bounds.height - (float)font_size) * 0.5f;
        DrawText(gTitleButtons[i].label, (int)text_x, (int)text_y, font_size, hovered ? DARKGRAY : RAYWHITE);
    }
}

static void DrawGameplayScreen(const Player *player, bool viewingLeaderboard, const Leaderboard *board)
{
    DrawText("Arcadia Operations Console", 170, 40, 28, RAYWHITE);

    if (player != NULL)
    {
        DrawText(TextFormat("Adventurer: %s", player->username), 60, 120, 22, RAYWHITE);
        DrawText(TextFormat("HP: %d/%d", player->hp, player->max_hp), 60, 150, 20, RAYWHITE);
        DrawText(TextFormat("ATK: %d", player->attack), 60, 180, 20, LIGHTGRAY);
        DrawText(TextFormat("DEF: %d", player->defense), 60, 210, 20, LIGHTGRAY);
        DrawText(TextFormat("Gold: %d", player->gold), 60, 240, 20, SKYBLUE);
        DrawText(TextFormat("Level: %d", player->level), 60, 270, 20, SKYBLUE);
    }

    DrawRectangleLines(50, 320, 700, 120, SKYBLUE);
    DrawText("Press ENTER to launch the full console interface", 70, 350, 18, RAYWHITE);
    DrawText("Press ESC to return to the title menu", 70, 380, 18, LIGHTGRAY);

    if (viewingLeaderboard && board != NULL && board->count > 0U)
    {
        DrawText("Top Arcadians", 500, 120, 22, SKYBLUE);
        size_t lines = (board->count < 5U) ? board->count : 5U;
        for (size_t i = 0U; i < lines; ++i)
        {
            DrawText(TextFormat("%2zu) %-12s  Lv %d", i + 1U, board->entries[i].username, board->entries[i].level),
                     480,
                     160 + (int)(i * 28),
                     20,
                     RAYWHITE);
        }
    }
    else
    {
        DrawText("Start a session to sync quests, inventory, and combat.", 60, 450, 18, LIGHTGRAY);
    }
}

/* ------------------------------ Data Seeding ------------------------------- */
static void seedDemoData(Player *player, Inventory *inventory, QuestLog *quests, Leaderboard *board, MonsterInfo **bestiary_root)
{
    if (inventory != NULL)
    {
        Item tonic = {"Solar Tonic", 40, 1};
        Item blade = {"Echo Blade", 125, 3};
        Item aegis = {"Prism Aegis", 90, 2};
        addItemToInventory(inventory, &tonic);
        addItemToInventory(inventory, &blade);
        addItemToInventory(inventory, &aegis);
    }

    if (quests != NULL)
    {
        addQuest(quests, "Beacon", "Restore the lighthouse flames");
        addQuest(quests, "Relic", "Retrieve the crystal heart");
    }

    if (board != NULL)
    {
        addLeaderboardEntry(board, "Azura", 12);
        addLeaderboardEntry(board, "Darius", 15);
        addLeaderboardEntry(board, "Nyx", 11);
        if (player != NULL)
        {
            addLeaderboardEntry(board, player->username, player->level);
        }
    }

    if (bestiary_root != NULL)
    {
        MonsterInfo *root = *bestiary_root;
        MonsterInfo *inserted = insertMonster(root, 101, "Goblin Scout", "Fire");
        if (inserted != NULL)
        {
            root = inserted;
        }
        inserted = insertMonster(root, 150, "Cave Bat", "Wind");
        if (inserted != NULL)
        {
            root = inserted;
        }
        inserted = insertMonster(root, 220, "Stone Golem", "Lightning");
        if (inserted != NULL)
        {
            root = inserted;
        }
        inserted = insertMonster(root, 310, "Crystal Hydra", "Frost");
        if (inserted != NULL)
        {
            root = inserted;
        }
        *bestiary_root = root;
    }
}

/* ------------------------------ Menu Handlers ------------------------------ */
static bool handleMainMenu(MenuStack *stack, Player *player, Inventory *inventory, QuestLog *quests, Leaderboard *board, WorldGraph *world, Room **current_room)
{
    if (stack == NULL || player == NULL)
    {
        return false;
    }

    clearScreen();
    drawBox(80, 5);
    size_t inventory_count = (inventory != NULL) ? inventory->count : 0U;
    size_t quest_count = (quests != NULL) ? quests->count : 0U;
    size_t room_count = (world != NULL) ? world->count : 0U;
    const char *location = (current_room != NULL && *current_room != NULL) ? (*current_room)->name : "Unknown";

    printf("%sArcadia Command Nexus%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    printf("Adventurer: %s | HP %d/%d | Gold %d | Level %d\n",
           player->username,
           player->hp,
           player->max_hp,
           player->gold,
           player->level);
    printf("Inventory %zu items | Quests %zu | Realms %zu | Location: %s\n",
           inventory_count,
           quest_count,
           room_count,
           location);

    printf("\n1) Inventory\n2) Quest Log\n3) Leaderboard\n4) Explore World\n5) Engage Combat\n6) Bestiary\n7) Save & Exit\n> ");
    int choice = readInt();

    switch (choice)
    {
        case 1:
            pushMenuState(stack, INVENTORY_MENU);
            break;
        case 2:
            pushMenuState(stack, QUEST_MENU);
            break;
        case 3:
            pushMenuState(stack, LEADERBOARD_MENU);
            break;
        case 4:
            pushMenuState(stack, WORLD_MENU);
            break;
        case 5:
            pushMenuState(stack, COMBAT_MENU);
            break;
        case 6:
            pushMenuState(stack, BESTIARY_MENU);
            break;
        case 7:
            printf("Saving progress...\n");
            savePlayer(player);
            return false;
        default:
            printf("Invalid option.\n");
            pauseScreen();
            break;
    }

    return true;
}

static void handleInventoryMenu(MenuStack *stack, Inventory *inventory)
{
    if (stack == NULL || inventory == NULL)
    {
        popMenuState(stack, NULL);
        return;
    }

    bool stay = true;
    while (stay)
    {
        clearScreen();
        drawBox(70, 4);
        printInventoryForward(inventory);
        printf("\n1) Add Item\n2) Use Item\n3) Reverse Traverse\n4) Back\n> ");
        int choice = readInt();

        switch (choice)
        {
            case 1:
            {
                Item item;
                printf("Item name: ");
                readLine(item.name, sizeof(item.name));
                printf("Value: ");
                item.value = readInt();
                printf("Rarity (1-5): ");
                item.rarity = readInt();
                if (addItemToInventory(inventory, &item))
                {
                    printf("%sItem stored.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
                }
                else
                {
                    printf("%sUnable to store item.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
                }
                pauseScreen();
                break;
            }
            case 2:
            {
                char target[32];
                printf("Use which item? ");
                readLine(target, sizeof(target));
                if (useItemFromInventory(inventory, target))
                {
                    printf("%sItem used.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
                }
                else
                {
                    printf("%sItem not found.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
                }
                pauseScreen();
                break;
            }
            case 3:
                clearScreen();
                drawBox(70, 4);
                printInventoryBackward(inventory);
                pauseScreen();
                break;
            case 4:
                stay = false;
                popMenuState(stack, NULL);
                break;
            default:
                printf("Invalid option.\n");
                pauseScreen();
                break;
        }
    }
}

static void handleQuestMenu(MenuStack *stack, QuestLog *quests)
{
    if (stack == NULL || quests == NULL)
    {
        popMenuState(stack, NULL);
        return;
    }

    bool stay = true;
    while (stay)
    {
        clearScreen();
        drawBox(70, 4);
        printQuestLog(quests);
        printf("\n1) Add Quest\n2) Complete Quest\n3) Back\n> ");
        int choice = readInt();

        switch (choice)
        {
            case 1:
            {
                char title[64];
                char description[128];
                printf("Quest title: ");
                readLine(title, sizeof(title));
                printf("Description: ");
                readLine(description, sizeof(description));
                if (addQuest(quests, title, description))
                {
                    printf("%sQuest added.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
                }
                else
                {
                    printf("%sUnable to add quest.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
                }
                pauseScreen();
                break;
            }
            case 2:
            {
                char title[64];
                printf("Quest to complete: ");
                readLine(title, sizeof(title));
                if (completeQuest(quests, title))
                {
                    printf("%sQuest completed.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
                }
                else
                {
                    printf("%sQuest not found.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
                }
                pauseScreen();
                break;
            }
            case 3:
                stay = false;
                popMenuState(stack, NULL);
                break;
            default:
                printf("Invalid option.\n");
                pauseScreen();
                break;
        }
    }
}

static void handleLeaderboardMenu(MenuStack *stack, Leaderboard *board)
{
    if (stack == NULL || board == NULL)
    {
        popMenuState(stack, NULL);
        return;
    }

    bool stay = true;
    while (stay)
    {
        displayLeaderboard(board);
        printf("\n1) Add Entry\n2) Back\n> ");
        int choice = readInt();

        switch (choice)
        {
            case 1:
            {
                char username[MAX_USERNAME_LENGTH];
                printf("Champion name: ");
                readLine(username, sizeof(username));
                printf("Level: ");
                int level = readInt();
                addLeaderboardEntry(board, username, level);
                printf("%sEntry recorded.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
                pauseScreen();
                break;
            }
            case 2:
                stay = false;
                popMenuState(stack, NULL);
                break;
            default:
                printf("Invalid option.\n");
                pauseScreen();
                break;
        }
    }
}

static void handleWorldMenu(MenuStack *stack, Room **current_room)
{
    if (stack == NULL || current_room == NULL || *current_room == NULL)
    {
        printf("World map unavailable.\n");
        pauseScreen();
        popMenuState(stack, NULL);
        return;
    }

    bool stay = true;
    while (stay)
    {
        clearScreen();
        drawBox(70, 4);
        describeRoom(*current_room);
        printf("\nChoose direction (N/S/E/W) or B to return: ");
        char buffer[MAX_INPUT_LENGTH];
        readLine(buffer, sizeof(buffer));
        toUpperString(buffer);

        if (buffer[0] == '\0')
        {
            continue;
        }

        if (buffer[0] == 'B')
        {
            stay = false;
            popMenuState(stack, NULL);
            break;
        }

        char direction[2];
        direction[0] = buffer[0];
        direction[1] = '\0';
        Room *next = moveToRoom(*current_room, direction);
        if (next != NULL)
        {
            *current_room = next;
            printf("%sYou travel %c into %s.%s\n",
                   ANSI_COLOR_SUCCESS,
                   direction[0],
                   (*current_room)->name,
                   ANSI_COLOR_RESET);
        }
        else
        {
            printf("%sNo passage in that direction.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
        }
        pauseScreen();
    }
}

static void handleCombatMenu(MenuStack *stack, Player *player, EventLog *events)
{
    if (stack == NULL || player == NULL || events == NULL)
    {
        popMenuState(stack, NULL);
        return;
    }

    clearScreen();
    drawBox(70, 4);
    printf("%sCombat Simulation Initiated%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    resetEventLog(events);
    startCombat(player, events);
    printEventLog(events);
    pauseScreen();
    popMenuState(stack, NULL);
}

static void handleBestiaryMenu(MenuStack *stack, MonsterInfo *root)
{
    if (stack == NULL)
    {
        return;
    }

    bool stay = true;
    while (stay)
    {
        clearScreen();
        drawBox(70, 4);
        printf("%sBestiary Archives%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
        printf("1) View All\n2) Search by ID\n3) Back\n> ");
        int choice = readInt();

        switch (choice)
        {
            case 1:
                if (root == NULL)
                {
                    printf("No discoveries logged.\n");
                }
                else
                {
                    printBestiaryInOrder(root);
                }
                pauseScreen();
                break;
            case 2:
            {
                printf("Enter monster ID: ");
                int id = readInt();
                const MonsterInfo *info = searchMonster(root, id);
                if (info != NULL)
                {
                    printf("ID %d | %s | Weakness: %s\n", info->id, info->name, info->weakness);
                }
                else
                {
                    printf("%sUnknown entry.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
                }
                pauseScreen();
                break;
            }
            case 3:
                stay = false;
                popMenuState(stack, NULL);
                break;
            default:
                printf("Invalid option.\n");
                pauseScreen();
                break;
        }
    }
}

/* ------------------------------ Player Login ------------------------------- */
static void loginOrCreatePlayer(Player *player)
{
    if (player == NULL)
    {
        return;
    }

    char username[MAX_USERNAME_LENGTH];
    printf("Enter your Arcadian handle: ");
    readLine(username, sizeof(username));
    if (username[0] == '\0')
    {
        strncpy(username, "Traveler", sizeof(username) - 1U);
        username[sizeof(username) - 1U] = '\0';
    }

    if (loadPlayer(username, player))
    {
        printf("%sWelcome back, %s.%s\n", ANSI_COLOR_SUCCESS, player->username, ANSI_COLOR_RESET);
    }
    else
    {
        strncpy(player->username, username, MAX_USERNAME_LENGTH - 1U);
        player->username[MAX_USERNAME_LENGTH - 1U] = '\0';
        player->hp = 100;
        player->max_hp = 100;
        player->attack = 15;
        player->defense = 10;
        player->gold = 125;
        player->level = 1;
        printf("%sNew hero registered.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
    }

    pauseScreen();
}

/* ---------------------------- Text Interface Loop ------------------------- */
static void runTextInterface(MenuStack *stack,
                             Player *player,
                             Inventory *inventory,
                             QuestLog *quests,
                             Leaderboard *board,
                             WorldGraph *world,
                             Room **current_room,
                             EventLog *events,
                             MonsterInfo *bestiary_root)
{
    if (stack == NULL || player == NULL || inventory == NULL || quests == NULL || board == NULL || world == NULL || current_room == NULL || events == NULL)
    {
        return;
    }

    bool running = true;
    while (running)
    {
        MenuState state;
        if (!peekMenuState(stack, &state))
        {
            break;
        }

        switch (state)
        {
            case MAIN_MENU:
                running = handleMainMenu(stack, player, inventory, quests, board, world, current_room);
                break;
            case INVENTORY_MENU:
                handleInventoryMenu(stack, inventory);
                break;
            case QUEST_MENU:
                handleQuestMenu(stack, quests);
                break;
            case LEADERBOARD_MENU:
                handleLeaderboardMenu(stack, board);
                break;
            case WORLD_MENU:
                handleWorldMenu(stack, current_room);
                break;
            case COMBAT_MENU:
                handleCombatMenu(stack, player, events);
                break;
            case BESTIARY_MENU:
                handleBestiaryMenu(stack, bestiary_root);
                break;
            default:
                running = false;
                break;
        }
    }
}

/* ----------------------------------- Main ---------------------------------- */
int main(void)
{
    MenuStack stack;
    if (!initMenuStack(&stack))
    {
        fprintf(stderr, "Failed to initialize menu stack.\n");
        return EXIT_FAILURE;
    }

    if (!pushMenuState(&stack, MAIN_MENU))
    {
        fprintf(stderr, "Failed to seed menu stack.\n");
        freeMenuStack(&stack);
        return EXIT_FAILURE;
    }

    Player player;
    loginOrCreatePlayer(&player);

    Inventory inventory;
    initInventory(&inventory);

    QuestLog quests;
    initQuestLog(&quests);

    Leaderboard board;
    initLeaderboard(&board);

    EventLog events;
    if (!initEventLog(&events, EVENT_LOG_CAPACITY))
    {
        fprintf(stderr, "Failed to initialize event log.\n");
        freeMenuStack(&stack);
        return EXIT_FAILURE;
    }

    WorldGraph world;
    initWorldGraph(&world);
    if (!buildDefaultWorld(&world))
    {
        fprintf(stderr, "Failed to build world graph.\n");
        freeEventLog(&events);
        freeMenuStack(&stack);
        return EXIT_FAILURE;
    }

    Room *current_room = (world.count > 0U) ? world.rooms[0U] : NULL;
    MonsterInfo *bestiary_root = NULL;

    seedDemoData(&player, &inventory, &quests, &board, &bestiary_root);

    InitWindow(800, 600, "Arcadia RPG");
    SetTargetFPS(60);
    initTitleButtons(WINDOW_WIDTH, WINDOW_HEIGHT);

    GameScreen current_screen = SCREEN_LOGO;
    int logo_frames = 0;
    bool viewing_leaderboard_panel = false;
    bool launch_text_interface = false;
    bool exit_requested = false;

    while (!exit_requested && !WindowShouldClose())
    {
        switch (current_screen)
        {
            case SCREEN_LOGO:
                logo_frames++;
                if (logo_frames > 120)
                {
                    current_screen = SCREEN_TITLE_MENU;
                }
                break;
            case SCREEN_TITLE_MENU:
            {
                Vector2 mouse = GetMousePosition();
                for (size_t i = 0U; i < TITLE_BUTTON_COUNT; ++i)
                {
                    if (CheckCollisionPointRec(mouse, gTitleButtons[i].bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    {
                        if (i == 0U)
                        {
                            current_screen = SCREEN_GAMEPLAY;
                            viewing_leaderboard_panel = false;
                        }
                        else if (i == 1U)
                        {
                            current_screen = SCREEN_GAMEPLAY;
                            viewing_leaderboard_panel = true;
                        }
                        else
                        {
                            exit_requested = true;
                        }
                    }
                }
                break;
            }
            case SCREEN_GAMEPLAY:
                if (IsKeyPressed(KEY_ESCAPE))
                {
                    current_screen = SCREEN_TITLE_MENU;
                    viewing_leaderboard_panel = false;
                }
                if (IsKeyPressed(KEY_ENTER))
                {
                    launch_text_interface = true;
                    exit_requested = true;
                }
                break;
            default:
                break;
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        switch (current_screen)
        {
            case SCREEN_LOGO:
                DrawText("Arcadia Studios", 230, 260, 42, RAYWHITE);
                DrawText("Booting datapaths...", 250, 320, 20, SKYBLUE);
                break;
            case SCREEN_TITLE_MENU:
                DrawTitleScreen();
                break;
            case SCREEN_GAMEPLAY:
                DrawGameplayScreen(&player, viewing_leaderboard_panel, &board);
                break;
            default:
                break;
        }

        EndDrawing();
    }

    CloseWindow();

    if (launch_text_interface)
    {
        runTextInterface(&stack,
                         &player,
                         &inventory,
                         &quests,
                         &board,
                         &world,
                         &current_room,
                         &events,
                         bestiary_root);
    }

    savePlayer(&player);
    freeInventory(&inventory);
    freeQuestLog(&quests);
    freeEventLog(&events);
    freeWorldGraph(&world);
    freeBestiary(bestiary_root);
    freeMenuStack(&stack);
    return EXIT_SUCCESS;
}
