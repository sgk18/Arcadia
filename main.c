#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define SHOP_QUEUE_CAPACITY 16

/* --------------------------------- Structs ---------------------------------
 */
typedef struct {
  char username[MAX_USERNAME_LENGTH];
  int hp;
  int max_hp;
  int attack;
  int defense;
  int gold;
  int level;
} Player;

typedef struct {
  char name[32];
  int value;
  int rarity;
} Item;

typedef struct InventoryNode {
  Item item;
  struct InventoryNode *prev;
  struct InventoryNode *next;
} InventoryNode;

typedef struct {
  InventoryNode *head;
  InventoryNode *tail;
  size_t count;
} Inventory;

typedef struct {
  char title[64];
  char description[128];
} Quest;

typedef struct QuestNode {
  Quest quest;
  struct QuestNode *next;
} QuestNode;

typedef struct {
  QuestNode *head;
  size_t count;
} QuestLog;

typedef struct {
  char username[MAX_USERNAME_LENGTH];
  int level;
} LeaderboardEntry;

typedef struct {
  LeaderboardEntry entries[LEADERBOARD_SIZE];
  size_t count;
} Leaderboard;

typedef enum {
  MENU_MAIN = 0,
  MENU_INVENTORY,
  MENU_QUEST,
  MENU_LEADERBOARD,
  MENU_WORLD,
  MENU_COMBAT,
  MENU_BESTIARY,
  MENU_SKILL, /* Circular Linked List: Skill Ring */
  MENU_SHOP,  /* Priority Queue: Item Shop      */
  MENU_TRANSITION
} MenuState;

typedef struct {
  MenuState *states;
  size_t count;
  size_t capacity;
} MenuStack;

typedef struct Edge Edge;

typedef struct Room {
  int id;
  char name[32];
  Edge *edges;
} Room;

struct Edge {
  Room *destination;
  char direction[8];
  Edge *next;
};

typedef struct {
  Room **rooms;
  size_t count;
} WorldGraph;

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

typedef struct {
  char **messages;
  size_t capacity;
  size_t count;
  size_t head;
} EventLog;

typedef struct MonsterInfo {
  int id;
  char name[32];
  char weakness[64];
  struct MonsterInfo *left;
  struct MonsterInfo *right;
} MonsterInfo;

typedef struct {
  unsigned long username_hash;
  Player player;
} PlayerRecord;

/* ---- Circular Linked List node (Skill Ring) ---- */
typedef struct SkillNode {
  char name[32];
  int power;
  struct SkillNode *next; /* always non-NULL when list is non-empty */
} SkillNode;

typedef struct {
  SkillNode *current; /* pointer to the currently active skill */
  size_t count;
} SkillRing;

/* ---- Priority Queue backing store (max-heap keyed on rarity) ---- */
typedef struct {
  Item items[SHOP_QUEUE_CAPACITY];
  size_t count;
} PriorityQueue;

/* --------------------------- Utility Declarations --------------------------
 */

static SkillRing gSkillRing;     /* Circular Linked List (Skill Ring)         */
static PriorityQueue gShopQueue; /* Priority Queue (Item Shop, max-heap)      */

static void readLine(char *buffer, size_t length);
static int readInt(void);
static void pauseScreen(void);
static void toUpperString(char *text);
static void clearScreen(void);
static void drawBox(int width, int height);
static void runTextInterface(MenuStack *stack, Player *player,
                             Inventory *inventory, QuestLog *quests,
                             Leaderboard *board, WorldGraph *world,
                             Room **current_room, EventLog *events,
                             MonsterInfo *bestiary_root, SkillRing *ring,
                             PriorityQueue *pq);

/* ----------------------------- Menu Stack Logic ----------------------------
 */
static bool initMenuStack(MenuStack *stack) {
  if (stack == NULL) {
    return false;
  }

  stack->count = 0U;
  stack->capacity = 8U;
  stack->states = (MenuState *)malloc(stack->capacity * sizeof(MenuState));

  if (stack->states == NULL) {
    stack->capacity = 0U;
    return false;
  }

  return true;
}

static void freeMenuStack(MenuStack *stack) {
  if (stack == NULL) {
    return;
  }

  free(stack->states);
  stack->states = NULL;
  stack->count = 0U;
  stack->capacity = 0U;
}

static bool pushMenuState(MenuStack *stack, MenuState state) {
  if (stack == NULL) {
    return false;
  }

  if (stack->count == stack->capacity) {
    size_t new_capacity = stack->capacity * 2U;
    MenuState *resized =
        (MenuState *)realloc(stack->states, new_capacity * sizeof(MenuState));

    if (resized == NULL) {
      return false;
    }

    stack->states = resized;
    stack->capacity = new_capacity;
  }

  stack->states[stack->count++] = state;
  return true;
}

static bool popMenuState(MenuStack *stack, MenuState *state) {
  if (stack == NULL || stack->count == 0U) {
    return false;
  }

  stack->count--;

  if (state != NULL) {
    *state = stack->states[stack->count];
  }

  return true;
}

static bool peekMenuState(const MenuStack *stack, MenuState *state) {
  if (stack == NULL || stack->count == 0U || state == NULL) {
    return false;
  }

  *state = stack->states[stack->count - 1U];
  return true;
}

/* --------------------------- Inventory Management --------------------------
 */
static void initInventory(Inventory *inventory) {
  if (inventory == NULL) {
    return;
  }

  inventory->head = NULL;
  inventory->tail = NULL;
  inventory->count = 0U;
}

static bool addItemToInventory(Inventory *inventory, const Item *item) {
  if (inventory == NULL || item == NULL) {
    return false;
  }

  InventoryNode *node = (InventoryNode *)malloc(sizeof(InventoryNode));
  if (node == NULL) {
    return false;
  }

  node->item = *item;
  node->prev = inventory->tail;
  node->next = NULL;

  if (inventory->tail != NULL) {
    inventory->tail->next = node;
  } else {
    inventory->head = node;
  }

  inventory->tail = node;
  inventory->count++;
  return true;
}

static bool useItemFromInventory(Inventory *inventory, const char *name) {
  if (inventory == NULL || name == NULL) {
    return false;
  }

  InventoryNode *current = inventory->head;
  while (current != NULL) {
    if (strncmp(current->item.name, name, sizeof(current->item.name)) == 0) {
      if (current->prev != NULL) {
        current->prev->next = current->next;
      } else {
        inventory->head = current->next;
      }

      if (current->next != NULL) {
        current->next->prev = current->prev;
      } else {
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

static void printInventoryForward(const Inventory *inventory) {
  printf("%sInventory (forward traversal):%s\n", ANSI_COLOR_TITLE,
         ANSI_COLOR_RESET);
  if (inventory == NULL || inventory->head == NULL) {
    printf("  [Empty]\n");
    return;
  }

  const InventoryNode *current = inventory->head;
  while (current != NULL) {
    printf("  %-16s | Value %3d | Rarity %d\n", current->item.name,
           current->item.value, current->item.rarity);
    current = current->next;
  }
}

static void printInventoryBackward(const Inventory *inventory) {
  printf("%sInventory (reverse traversal):%s\n", ANSI_COLOR_TITLE,
         ANSI_COLOR_RESET);
  if (inventory == NULL || inventory->tail == NULL) {
    printf("  [Empty]\n");
    return;
  }

  const InventoryNode *current = inventory->tail;
  while (current != NULL) {
    printf("  %-16s | Value %3d | Rarity %d\n", current->item.name,
           current->item.value, current->item.rarity);
    current = current->prev;
  }
}

static void freeInventory(Inventory *inventory) {
  if (inventory == NULL) {
    return;
  }

  InventoryNode *current = inventory->head;
  while (current != NULL) {
    InventoryNode *next = current->next;
    free(current);
    current = next;
  }

  inventory->head = NULL;
  inventory->tail = NULL;
  inventory->count = 0U;
}

/* ================== Circular Linked List: Skill Ring ====================== */
/* Each SkillNode's next pointer forms a loop; tail->next == ring->current.  */

static void initSkillRing(SkillRing *ring) {
  if (ring == NULL) {
    return;
  }
  ring->current = NULL;
  ring->count = 0U;
}

/* Insertion: add a new skill after the current active skill.                */
static bool addSkill(SkillRing *ring, const char *name, int power) {
  if (ring == NULL || name == NULL) {
    return false;
  }
  SkillNode *node = (SkillNode *)malloc(sizeof(SkillNode));
  if (node == NULL) {
    return false;
  }
  strncpy(node->name, name, sizeof(node->name) - 1U);
  node->name[sizeof(node->name) - 1U] = '\0';
  node->power = power;

  if (ring->current == NULL) {
    node->next = node; /* single-element self-loop */
    ring->current = node;
  } else {
    /* Walk to the tail (the node whose next == current) */
    SkillNode *tail = ring->current;
    while (tail->next != ring->current) {
      tail = tail->next;
    }
    tail->next = node;
    node->next = ring->current;
  }
  ring->count++;
  return true;
}

/* Traversal: advance the active pointer one step forward around the ring.  */
static void rotateSkillForward(SkillRing *ring) {
  if (ring == NULL || ring->current == NULL) {
    return;
  }
  ring->current = ring->current->next;
}

/* Deletion: remove and free the currently active skill.                     */
static bool removeCurrentSkill(SkillRing *ring) {
  if (ring == NULL || ring->current == NULL) {
    return false;
  }
  if (ring->count == 1U) {
    free(ring->current);
    ring->current = NULL;
    ring->count = 0U;
    return true;
  }
  /* Find the node just before current */
  SkillNode *prev = ring->current;
  while (prev->next != ring->current) {
    prev = prev->next;
  }
  SkillNode *to_free = ring->current;
  prev->next = ring->current->next;
  ring->current = prev->next;
  free(to_free);
  ring->count--;
  return true;
}

/* Display: print all skills in ring order, marking the active one.         */
static void printSkillRing(const SkillRing *ring) {
  printf("%sSkill Ring (%zu skills):%s\n", ANSI_COLOR_TITLE,
         ring ? ring->count : 0U, ANSI_COLOR_RESET);
  if (ring == NULL || ring->current == NULL) {
    printf("  [Empty]\n");
    return;
  }
  const SkillNode *node = ring->current;
  size_t printed = 0U;
  do {
    if (printed == 0U) {
      printf("  >> %-22s | Power %3d  [ACTIVE]\n", node->name, node->power);
    } else {
      printf("     %-22s | Power %3d\n", node->name, node->power);
    }
    node = node->next;
    printed++;
  } while (node != ring->current && printed <= ring->count);
}

/* Free all nodes and reset ring to empty.                                   */
static void freeSkillRing(SkillRing *ring) {
  if (ring == NULL || ring->current == NULL) {
    return;
  }
  /* Break the circular link to allow linear traversal */
  SkillNode *tail = ring->current;
  while (tail->next != ring->current) {
    tail = tail->next;
  }
  tail->next = NULL;
  SkillNode *node = ring->current;
  while (node != NULL) {
    SkillNode *next = node->next;
    free(node);
    node = next;
  }
  ring->current = NULL;
  ring->count = 0U;
}

/* ----------------------------- Quest Log System -----------------------------
 */
static void initQuestLog(QuestLog *log) {
  if (log == NULL) {
    return;
  }

  log->head = NULL;
  log->count = 0U;
}

static bool addQuest(QuestLog *log, const char *title,
                     const char *description) {
  if (log == NULL || title == NULL || description == NULL) {
    return false;
  }

  QuestNode *node = (QuestNode *)malloc(sizeof(QuestNode));
  if (node == NULL) {
    return false;
  }

  strncpy(node->quest.title, title, sizeof(node->quest.title) - 1U);
  node->quest.title[sizeof(node->quest.title) - 1U] = '\0';
  strncpy(node->quest.description, description,
          sizeof(node->quest.description) - 1U);
  node->quest.description[sizeof(node->quest.description) - 1U] = '\0';

  node->next = log->head;
  log->head = node;
  log->count++;
  return true;
}

static bool completeQuest(QuestLog *log, const char *title) {
  if (log == NULL || title == NULL) {
    return false;
  }

  QuestNode *current = log->head;
  QuestNode *previous = NULL;

  while (current != NULL) {
    if (strncmp(current->quest.title, title, sizeof(current->quest.title)) ==
        0) {
      if (previous == NULL) {
        log->head = current->next;
      } else {
        previous->next = current->next;
      }

      free(current);
      if (log->count > 0U) {
        log->count--;
      }
      return true;
    }

    previous = current;
    current = current->next;
  }

  return false;
}

static void printQuestLog(const QuestLog *log) {
  printf("%sActive Quests:%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
  if (log == NULL || log->head == NULL) {
    printf("  No quests tracked.\n");
    return;
  }

  const QuestNode *current = log->head;
  while (current != NULL) {
    printf("  %-20s | %s\n", current->quest.title, current->quest.description);
    current = current->next;
  }
}

static void freeQuestLog(QuestLog *log) {
  if (log == NULL) {
    return;
  }

  QuestNode *current = log->head;
  while (current != NULL) {
    QuestNode *next = current->next;
    free(current);
    current = next;
  }

  log->head = NULL;
  log->count = 0U;
}

/* ---------------------------- Leaderboard Engine ---------------------------
 */
static void initLeaderboard(Leaderboard *board) {
  if (board == NULL) {
    return;
  }

  board->count = 0U;
}

static void sortLeaderboard(Leaderboard *board) {
  if (board == NULL) {
    return;
  }

  bool swapped = true;
  while (swapped) {
    swapped = false;
    for (size_t i = 0U; i + 1U < board->count; ++i) {
      if (board->entries[i].level < board->entries[i + 1U].level) {
        LeaderboardEntry temp = board->entries[i];
        board->entries[i] = board->entries[i + 1U];
        board->entries[i + 1U] = temp;
        swapped = true;
      }
    }
  }
}

static void addLeaderboardEntry(Leaderboard *board, const char *username,
                                int level) {
  if (board == NULL || username == NULL) {
    return;
  }

  if (board->count < LEADERBOARD_SIZE) {
    strncpy(board->entries[board->count].username, username,
            MAX_USERNAME_LENGTH - 1U);
    board->entries[board->count].username[MAX_USERNAME_LENGTH - 1U] = '\0';
    board->entries[board->count].level = level;
    board->count++;
  } else {
    size_t lowest_index = 0U;
    for (size_t i = 1U; i < board->count; ++i) {
      if (board->entries[i].level < board->entries[lowest_index].level) {
        lowest_index = i;
      }
    }

    if (level > board->entries[lowest_index].level) {
      strncpy(board->entries[lowest_index].username, username,
              MAX_USERNAME_LENGTH - 1U);
      board->entries[lowest_index].username[MAX_USERNAME_LENGTH - 1U] = '\0';
      board->entries[lowest_index].level = level;
    }
  }

  sortLeaderboard(board);
}

static void displayLeaderboard(const Leaderboard *board) {
  clearScreen();
  drawBox(60, 4);
  printf("%sArcadia Leaderboard%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);

  if (board == NULL || board->count == 0U) {
    printf("  No entries recorded.\n");
    return;
  }

  for (size_t i = 0U; i < board->count; ++i) {
    printf("  %zu) %-20s | Level %d\n", i + 1U, board->entries[i].username,
           board->entries[i].level);
  }
}

/* ============ Priority Queue: Item Shop (Max-Heap keyed on rarity) =========
 */
/* Binary max-heap: O(log n) insert and extract-max.                          */

static void initPriorityQueue(PriorityQueue *pq) {
  if (pq == NULL) {
    return;
  }
  pq->count = 0U;
}

static void pqHeapifyUp(PriorityQueue *pq, size_t idx) {
  while (idx > 0U) {
    size_t parent = (idx - 1U) / 2U;
    if (pq->items[parent].rarity < pq->items[idx].rarity) {
      Item tmp = pq->items[parent];
      pq->items[parent] = pq->items[idx];
      pq->items[idx] = tmp;
      idx = parent;
    } else {
      break;
    }
  }
}

static void pqHeapifyDown(PriorityQueue *pq, size_t idx) {
  size_t left, right, largest;
  while (true) {
    left = 2U * idx + 1U;
    right = 2U * idx + 2U;
    largest = idx;
    if (left < pq->count &&
        pq->items[left].rarity > pq->items[largest].rarity) {
      largest = left;
    }
    if (right < pq->count &&
        pq->items[right].rarity > pq->items[largest].rarity) {
      largest = right;
    }
    if (largest == idx) {
      break;
    }
    Item tmp = pq->items[idx];
    pq->items[idx] = pq->items[largest];
    pq->items[largest] = tmp;
    idx = largest;
  }
}

/* Insertion: add item to shop queue (O(log n)).                              */
static bool pqInsert(PriorityQueue *pq, const Item *item) {
  if (pq == NULL || item == NULL || pq->count >= SHOP_QUEUE_CAPACITY) {
    return false;
  }
  pq->items[pq->count] = *item;
  pqHeapifyUp(pq, pq->count);
  pq->count++;
  return true;
}

/* Deletion: remove and return the highest-rarity item (O(log n)).            */
static bool pqExtractMax(PriorityQueue *pq, Item *out) {
  if (pq == NULL || pq->count == 0U) {
    return false;
  }
  if (out != NULL) {
    *out = pq->items[0];
  }
  pq->count--;
  if (pq->count > 0U) {
    pq->items[0] = pq->items[pq->count];
    pqHeapifyDown(pq, 0U);
  }
  return true;
}

/* Searching: peek at the highest-priority item without removing it.          */
static bool pqPeek(const PriorityQueue *pq, Item *out) {
  if (pq == NULL || pq->count == 0U || out == NULL) {
    return false;
  }
  *out = pq->items[0];
  return true;
}

/* Display: print all shop items (heap storage order = rarity-descending).    */
static void pqDisplay(const PriorityQueue *pq) {
  printf("%sItem Shop (%zu items, sorted by rarity):%s\n", ANSI_COLOR_TITLE,
         pq ? pq->count : 0U, ANSI_COLOR_RESET);
  if (pq == NULL || pq->count == 0U) {
    printf("  [Empty]\n");
    return;
  }
  for (size_t i = 0U; i < pq->count; ++i) {
    printf("  %-20s | Value %3d | Rarity %d\n", pq->items[i].name,
           pq->items[i].value, pq->items[i].rarity);
  }
}

/* ===================== Searching and Sorting Utilities ======================
 */

/* LINEAR SEARCH: find the first inventory node whose name contains query.    */
static const InventoryNode *linearSearchInventory(const Inventory *inv,
                                                  const char *query) {
  if (inv == NULL || query == NULL) {
    return NULL;
  }
  const InventoryNode *node = inv->head;
  while (node != NULL) {
    if (strstr(node->item.name, query) != NULL) {
      return node;
    }
    node = node->next;
  }
  return NULL;
}

/* BINARY SEARCH: find a leaderboard entry by exact level (sorted descending).
 * Returns the array index or -1 if not found.                                */
static int binarySearchLeaderboard(const Leaderboard *board, int level) {
  if (board == NULL || board->count == 0U) {
    return -1;
  }
  int lo = 0;
  int hi = (int)board->count - 1;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    if (board->entries[mid].level == level) {
      return mid;
    } else if (board->entries[mid].level > level) {
      lo = mid + 1; /* descending: higher levels have lower indices */
    } else {
      hi = mid - 1;
    }
  }
  return -1;
}

/* INSERTION SORT: sort an array of Items descending by value.                */
static void insertionSortItems(Item *items, size_t count) {
  if (items == NULL || count < 2U) {
    return;
  }
  for (size_t i = 1U; i < count; ++i) {
    Item key = items[i];
    int j = (int)i - 1;
    while (j >= 0 && items[j].value < key.value) {
      items[j + 1] = items[j];
      j--;
    }
    items[j + 1] = key;
  }
}

/* Sort the doubly-linked inventory in-place by item value (descending).
 * Copies items into a temporary array, insertion-sorts it, writes back.      */
static void sortInventoryByValue(Inventory *inv) {
  if (inv == NULL || inv->head == NULL || inv->head->next == NULL) {
    return;
  }
  size_t n = inv->count;
  Item *arr = (Item *)malloc(n * sizeof(Item));
  if (arr == NULL) {
    return;
  }
  InventoryNode *node = inv->head;
  for (size_t i = 0U; i < n && node != NULL; ++i, node = node->next) {
    arr[i] = node->item;
  }
  insertionSortItems(arr, n);
  node = inv->head;
  for (size_t i = 0U; i < n && node != NULL; ++i, node = node->next) {
    node->item = arr[i];
  }
  free(arr);
}

/* --------------------------- Player File Handling --------------------------
 */
static unsigned long hashUsername(const char *username) {
  unsigned long hash = 5381UL;
  int c = 0;

  while (username != NULL && (c = *username++) != '\0') {
    hash = ((hash << 5) + hash) + (unsigned long)c;
  }

  return hash;
}

static bool savePlayer(const Player *player) {
  if (player == NULL) {
    return false;
  }

  FILE *file = fopen(PLAYER_DATA_FILE, "rb+");
  if (file == NULL) {
    file = fopen(PLAYER_DATA_FILE, "wb+");
    if (file == NULL) {
      return false;
    }
  }

  PlayerRecord record;
  unsigned long target_hash = hashUsername(player->username);

  while (fread(&record, sizeof(PlayerRecord), 1U, file) == 1U) {
    if (record.username_hash == target_hash &&
        strncmp(record.player.username, player->username,
                MAX_USERNAME_LENGTH) == 0) {
      long position = (long)(ftell(file) - (long)sizeof(PlayerRecord));
      if (position < 0) {
        fclose(file);
        return false;
      }

      record.player = *player;
      record.username_hash = target_hash;

      if (fseek(file, position, SEEK_SET) != 0) {
        fclose(file);
        return false;
      }

      if (fwrite(&record, sizeof(PlayerRecord), 1U, file) != 1U) {
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

  if (fwrite(&record, sizeof(PlayerRecord), 1U, file) != 1U) {
    fclose(file);
    return false;
  }

  fflush(file);
  fclose(file);
  return true;
}

static bool loadPlayer(const char *username, Player *out_player) {
  if (username == NULL || out_player == NULL) {
    return false;
  }

  FILE *file = fopen(PLAYER_DATA_FILE, "rb");
  if (file == NULL) {
    return false;
  }

  PlayerRecord record;
  unsigned long target_hash = hashUsername(username);

  while (fread(&record, sizeof(PlayerRecord), 1U, file) == 1U) {
    if (record.username_hash == target_hash &&
        strncmp(record.player.username, username, MAX_USERNAME_LENGTH) == 0) {
      *out_player = record.player;
      fclose(file);
      return true;
    }
  }

  fclose(file);
  return false;
}

/* ----------------------------- Event Log Queue -----------------------------
 */
static bool initEventLog(EventLog *log, size_t capacity) {
  if (log == NULL || capacity == 0U) {
    return false;
  }

  log->messages = (char **)malloc(capacity * sizeof(char *));
  if (log->messages == NULL) {
    return false;
  }

  for (size_t i = 0U; i < capacity; ++i) {
    log->messages[i] = (char *)malloc(EVENT_MESSAGE_LENGTH);
    if (log->messages[i] == NULL) {
      for (size_t j = 0U; j < i; ++j) {
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

static void resetEventLog(EventLog *log) {
  if (log == NULL || log->messages == NULL) {
    return;
  }

  for (size_t i = 0U; i < log->capacity; ++i) {
    log->messages[i][0] = '\0';
  }

  log->count = 0U;
  log->head = 0U;
}

static void enqueueEvent(EventLog *log, const char *message) {
  if (log == NULL || log->messages == NULL || message == NULL) {
    return;
  }

  size_t index = 0U;
  if (log->count < log->capacity) {
    index = (log->head + log->count) % log->capacity;
    log->count++;
  } else {
    index = log->head;
    log->head = (log->head + 1U) % log->capacity;
  }

  strncpy(log->messages[index], message, EVENT_MESSAGE_LENGTH - 1U);
  log->messages[index][EVENT_MESSAGE_LENGTH - 1U] = '\0';
}

static void printEventLog(const EventLog *log) {
  printf("%sRecent Combat Log:%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
  if (log == NULL || log->messages == NULL || log->count == 0U) {
    printf("  No events recorded.\n");
    return;
  }

  for (size_t i = 0U; i < log->count; ++i) {
    size_t index = (log->head + i) % log->capacity;
    printf("  %s\n", log->messages[index]);
  }
}

static void freeEventLog(EventLog *log) {
  if (log == NULL || log->messages == NULL) {
    return;
  }

  for (size_t i = 0U; i < log->capacity; ++i) {
    free(log->messages[i]);
  }

  free(log->messages);
  log->messages = NULL;
  log->capacity = 0U;
  log->count = 0U;
  log->head = 0U;
}

/* ------------------------------- World Graph -------------------------------
 */
static void initWorldGraph(WorldGraph *graph) {
  if (graph == NULL) {
    return;
  }

  graph->rooms = NULL;
  graph->count = 0U;
}

static void freeWorldGraph(WorldGraph *graph);

static Room *createRoom(int id, const char *name) {
  Room *room = (Room *)malloc(sizeof(Room));
  if (room == NULL) {
    return NULL;
  }

  room->id = id;
  strncpy(room->name, name, sizeof(room->name) - 1U);
  room->name[sizeof(room->name) - 1U] = '\0';
  room->edges = NULL;
  return room;
}

static bool appendRoom(WorldGraph *graph, Room *room) {
  if (graph == NULL || room == NULL) {
    return false;
  }

  Room **resized =
      (Room **)realloc(graph->rooms, (graph->count + 1U) * sizeof(Room *));
  if (resized == NULL) {
    return false;
  }

  graph->rooms = resized;
  graph->rooms[graph->count++] = room;
  return true;
}

static bool addEdge(Room *from, Room *to, const char *direction) {
  if (from == NULL || to == NULL || direction == NULL) {
    return false;
  }

  Edge *edge = (Edge *)malloc(sizeof(Edge));
  if (edge == NULL) {
    return false;
  }

  edge->destination = to;
  strncpy(edge->direction, direction, sizeof(edge->direction) - 1U);
  edge->direction[sizeof(edge->direction) - 1U] = '\0';
  edge->next = from->edges;
  from->edges = edge;
  return true;
}

static Room *moveToRoom(Room *current, const char *direction) {
  if (current == NULL || direction == NULL) {
    return NULL;
  }

  Edge *edge = current->edges;
  while (edge != NULL) {
    if (strncmp(edge->direction, direction, sizeof(edge->direction)) == 0) {
      return edge->destination;
    }
    edge = edge->next;
  }

  return NULL;
}

static void describeRoom(const Room *room) {
  if (room == NULL) {
    printf("Room data unavailable.\n");
    return;
  }

  printf("%sYou stand in %s.%s\n", ANSI_COLOR_TITLE, room->name,
         ANSI_COLOR_RESET);
  printf("  Exits: ");
  Edge *edge = room->edges;
  if (edge == NULL) {
    printf("None\n");
    return;
  }

  while (edge != NULL) {
    printf("%s ", edge->direction);
    edge = edge->next;
  }
  printf("\n");
}

static bool buildDefaultWorld(WorldGraph *graph) {
  if (graph == NULL) {
    return false;
  }

  Room *entrance = createRoom(0, "Sunlit Entrance");
  Room *armory = createRoom(1, "Forgemaster Armory");
  Room *library = createRoom(2, "Whispering Library");
  Room *sanctum = createRoom(3, "Crystal Sanctum");

  if (entrance == NULL || armory == NULL || library == NULL ||
      sanctum == NULL) {
    free(entrance);
    free(armory);
    free(library);
    free(sanctum);
    return false;
  }

  if (!appendRoom(graph, entrance) || !appendRoom(graph, armory) ||
      !appendRoom(graph, library) || !appendRoom(graph, sanctum)) {
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

static void freeWorldGraph(WorldGraph *graph) {
  if (graph == NULL || graph->rooms == NULL) {
    return;
  }

  for (size_t i = 0U; i < graph->count; ++i) {
    if (graph->rooms[i] == NULL) {
      continue;
    }

    Edge *edge = graph->rooms[i]->edges;
    while (edge != NULL) {
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

/* --------------------------- Combat Queue & Logic --------------------------
 */
static bool initCombatQueue(CombatQueue *queue, size_t capacity) {
  if (queue == NULL || capacity == 0U) {
    return false;
  }

  queue->slots = (Combatant **)malloc(capacity * sizeof(Combatant *));
  if (queue->slots == NULL) {
    return false;
  }

  queue->capacity = capacity;
  queue->head = 0U;
  queue->tail = 0U;
  queue->count = 0U;
  return true;
}

static void freeCombatQueue(CombatQueue *queue) {
  if (queue == NULL) {
    return;
  }

  free(queue->slots);
  queue->slots = NULL;
  queue->capacity = 0U;
  queue->head = 0U;
  queue->tail = 0U;
  queue->count = 0U;
}

static bool enqueueCombatant(CombatQueue *queue, Combatant *combatant) {
  if (queue == NULL || combatant == NULL || queue->count == queue->capacity) {
    return false;
  }

  queue->slots[queue->tail] = combatant;
  queue->tail = (queue->tail + 1U) % queue->capacity;
  queue->count++;
  return true;
}

static Combatant *dequeueCombatant(CombatQueue *queue) {
  if (queue == NULL || queue->count == 0U) {
    return NULL;
  }

  Combatant *combatant = queue->slots[queue->head];
  queue->head = (queue->head + 1U) % queue->capacity;
  queue->count--;
  return combatant;
}

static bool enemiesRemain(const Combatant *enemies, size_t count) {
  for (size_t i = 0U; i < count; ++i) {
    if (enemies[i].hp > 0) {
      return true;
    }
  }
  return false;
}

static size_t nextEnemyIndex(Combatant *enemies, size_t count) {
  for (size_t i = 0U; i < count; ++i) {
    if (enemies[i].hp > 0) {
      return i;
    }
  }
  return count;
}

static void startCombat(Player *player, EventLog *log) {
  if (player == NULL || log == NULL) {
    return;
  }

  CombatQueue queue;
  if (!initCombatQueue(&queue, 6U)) {
    enqueueEvent(log, "Combat queue initialization failed.");
    return;
  }

  Combatant player_unit;
  strncpy(player_unit.name, player->username, sizeof(player_unit.name) - 1U);
  player_unit.name[sizeof(player_unit.name) - 1U] = '\0';
  player_unit.hp = player->hp;
  player_unit.attack = player->attack;
  player_unit.is_player = true;

  Combatant enemies[3] = {{"Goblin Marauder", 35, 8, false},
                          {"Crystal Wisp", 28, 10, false},
                          {"Stone Bulwark", 45, 6, false}};

  enqueueCombatant(&queue, &player_unit);
  for (size_t i = 0U; i < 3U; ++i) {
    enqueueCombatant(&queue, &enemies[i]);
  }

  char message[EVENT_MESSAGE_LENGTH];

  while (player_unit.hp > 0 && enemiesRemain(enemies, 3U)) {
    Combatant *actor = dequeueCombatant(&queue);
    if (actor == NULL) {
      break;
    }

    if (actor->hp <= 0) {
      continue;
    }

    if (actor->is_player) {
      size_t index = nextEnemyIndex(enemies, 3U);
      if (index == 3U) {
        enqueueCombatant(&queue, actor);
        continue;
      }

      Combatant *target = &enemies[index];
      int damage = actor->attack;
      target->hp -= damage;
      if (target->hp < 0) {
        target->hp = 0;
      }

      snprintf(message, sizeof(message), "%s strikes %s for %d damage.",
               actor->name, target->name, damage);
      enqueueEvent(log, message);

      if (target->hp == 0) {
        snprintf(message, sizeof(message), "%s is defeated!", target->name);
        enqueueEvent(log, message);
      }
    } else {
      int damage = actor->attack - (player->defense / 4);
      if (damage < 1) {
        damage = 1;
      }

      player_unit.hp -= damage;
      if (player_unit.hp < 0) {
        player_unit.hp = 0;
      }

      snprintf(message, sizeof(message), "%s claws %s for %d damage.",
               actor->name, player_unit.name, damage);
      enqueueEvent(log, message);
    }

    if (actor->hp > 0) {
      enqueueCombatant(&queue, actor);
    }
  }

  player->hp = player_unit.hp;
  if (player->hp > player->max_hp) {
    player->hp = player->max_hp;
  }

  if (player->hp > 0) {
    player->gold += 25;
    if (player->level < 99) {
      player->level++;
    }
    snprintf(message, sizeof(message), "Victory! +25 gold. Level %d.",
             player->level);
    enqueueEvent(log, message);
  } else {
    enqueueEvent(log, "You have fallen in battle.");
    player->hp = player->max_hp / 2;
    if (player->hp == 0) {
      player->hp = 1;
    }
    snprintf(message, sizeof(message), "Revived at %d HP.", player->hp);
    enqueueEvent(log, message);
  }

  freeCombatQueue(&queue);
}

/* -------------------------------- Bestiary BST -----------------------------
 */
static MonsterInfo *insertMonster(MonsterInfo *root, int id, const char *name,
                                  const char *weakness) {
  if (name == NULL || weakness == NULL) {
    return root;
  }

  if (root == NULL) {
    MonsterInfo *node = (MonsterInfo *)malloc(sizeof(MonsterInfo));
    if (node == NULL) {
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

  if (id < root->id) {
    MonsterInfo *child = insertMonster(root->left, id, name, weakness);
    if (child != NULL) {
      root->left = child;
    }
  } else if (id > root->id) {
    MonsterInfo *child = insertMonster(root->right, id, name, weakness);
    if (child != NULL) {
      root->right = child;
    }
  } else {
    strncpy(root->name, name, sizeof(root->name) - 1U);
    root->name[sizeof(root->name) - 1U] = '\0';
    strncpy(root->weakness, weakness, sizeof(root->weakness) - 1U);
    root->weakness[sizeof(root->weakness) - 1U] = '\0';
  }

  return root;
}

static const MonsterInfo *searchMonster(const MonsterInfo *root, int id) {
  if (root == NULL) {
    return NULL;
  }

  if (id < root->id) {
    return searchMonster(root->left, id);
  }
  if (id > root->id) {
    return searchMonster(root->right, id);
  }
  return root;
}

static void printBestiaryInOrder(const MonsterInfo *root) {
  if (root == NULL) {
    return;
  }

  printBestiaryInOrder(root->left);
  printf("  ID %3d | %-16s | Weakness: %s\n", root->id, root->name,
         root->weakness);
  printBestiaryInOrder(root->right);
}

static void freeBestiary(MonsterInfo *root) {
  if (root == NULL) {
    return;
  }

  freeBestiary(root->left);
  freeBestiary(root->right);
  free(root);
}

/* ----------------------------- Input / UI Utils ----------------------------
 */
static void readLine(char *buffer, size_t length) {
  if (buffer == NULL || length == 0U) {
    return;
  }

  if (fgets(buffer, (int)length, stdin) != NULL) {
    size_t newline = strcspn(buffer, "\n");
    buffer[newline] = '\0';
  } else {
    buffer[0] = '\0';
  }
}

static int readInt(void) {
  char buffer[MAX_INPUT_LENGTH];
  readLine(buffer, sizeof(buffer));
  return (int)strtol(buffer, NULL, 10);
}

static void pauseScreen(void) {
  printf("\nPress Enter to continue...");
  fflush(stdout);
  char buffer[MAX_INPUT_LENGTH];
  readLine(buffer, sizeof(buffer));
}

static void toUpperString(char *text) {
  if (text == NULL) {
    return;
  }

  for (size_t i = 0U; text[i] != '\0'; ++i) {
    if (text[i] >= 'a' && text[i] <= 'z') {
      text[i] = (char)(text[i] - ('a' - 'A'));
    }
  }
}

static void clearScreen(void) {
  printf("%s", ANSI_CLEAR_SCREEN);
  fflush(stdout);
}

static void drawBox(int width, int height) {
  if (width < 2 || height < 2) {
    return;
  }

  printf("%s", ANSI_COLOR_BORDER);

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      bool is_top_or_bottom = (row == 0) || (row == height - 1);
      bool is_side = (col == 0) || (col == width - 1);

      if (is_top_or_bottom) {
        putchar((col == 0 || col == width - 1) ? '+' : '-');
      } else if (is_side) {
        putchar('|');
      } else {
        putchar(' ');
      }
    }
    putchar('\n');
  }

  printf("%s", ANSI_COLOR_RESET);
  fflush(stdout);
}

/* ------------------------------ Data Seeding -------------------------------
 */

static void seedDemoData(Player *player, Inventory *inventory, QuestLog *quests,
                         Leaderboard *board, MonsterInfo **bestiary_root) {
  if (inventory != NULL) {
    Item tonic = {"Solar Tonic", 40, 1};
    Item blade = {"Echo Blade", 125, 3};
    Item aegis = {"Prism Aegis", 90, 2};
    addItemToInventory(inventory, &tonic);
    addItemToInventory(inventory, &blade);
    addItemToInventory(inventory, &aegis);
  }

  if (quests != NULL) {
    addQuest(quests, "Beacon", "Restore the lighthouse flames");
    addQuest(quests, "Relic", "Retrieve the crystal heart");
  }

  if (board != NULL) {
    addLeaderboardEntry(board, "Azura", 12);
    addLeaderboardEntry(board, "Darius", 15);
    addLeaderboardEntry(board, "Nyx", 11);
    if (player != NULL) {
      addLeaderboardEntry(board, player->username, player->level);
    }
  }

  if (bestiary_root != NULL) {
    MonsterInfo *root = *bestiary_root;
    MonsterInfo *inserted = insertMonster(root, 101, "Goblin Scout", "Fire");
    if (inserted != NULL) {
      root = inserted;
    }
    inserted = insertMonster(root, 150, "Cave Bat", "Wind");
    if (inserted != NULL) {
      root = inserted;
    }
    inserted = insertMonster(root, 220, "Stone Golem", "Lightning");
    if (inserted != NULL) {
      root = inserted;
    }
    inserted = insertMonster(root, 310, "Crystal Hydra", "Frost");
    if (inserted != NULL) {
      root = inserted;
    }
    *bestiary_root = root;
  }
}

/* ------------------------------ Menu Handlers ------------------------------
 */
static bool handleMainMenu(MenuStack *stack, Player *player,
                           Inventory *inventory, QuestLog *quests,
                           Leaderboard *board, WorldGraph *world,
                           Room **current_room) {
  if (stack == NULL || player == NULL) {
    return false;
  }

  clearScreen();
  drawBox(80, 5);
  size_t inventory_count = (inventory != NULL) ? inventory->count : 0U;
  size_t quest_count = (quests != NULL) ? quests->count : 0U;
  size_t room_count = (world != NULL) ? world->count : 0U;
  const char *location = (current_room != NULL && *current_room != NULL)
                             ? (*current_room)->name
                             : "Unknown";

  printf("%sArcadia Command Nexus%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
  printf("Adventurer: %s | HP %d/%d | Gold %d | Level %d\n", player->username,
         player->hp, player->max_hp, player->gold, player->level);
  printf("Inventory %zu items | Quests %zu | Realms %zu | Location: %s\n",
         inventory_count, quest_count, room_count, location);

  printf("\n1) Inventory\n2) Quest Log\n3) Leaderboard\n4) Explore World\n5) "
         "Engage Combat\n6) Bestiary\n7) Skill Ring\n8) Item Shop\n9) Save & "
         "Exit\n> ");
  int choice = readInt();

  switch (choice) {
  case 1:
    pushMenuState(stack, MENU_INVENTORY);
    break;
  case 2:
    pushMenuState(stack, MENU_QUEST);
    break;
  case 3:
    pushMenuState(stack, MENU_LEADERBOARD);
    break;
  case 4:
    pushMenuState(stack, MENU_WORLD);
    break;
  case 5:
    pushMenuState(stack, MENU_COMBAT);
    break;
  case 6:
    pushMenuState(stack, MENU_BESTIARY);
    break;
  case 7:
    pushMenuState(stack, MENU_SKILL);
    break;
  case 8:
    pushMenuState(stack, MENU_SHOP);
    break;
  case 9:
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

static void handleInventoryMenu(MenuStack *stack, Inventory *inventory) {
  if (stack == NULL || inventory == NULL) {
    popMenuState(stack, NULL);
    return;
  }

  bool stay = true;
  while (stay) {
    clearScreen();
    drawBox(70, 4);
    printInventoryForward(inventory);
    printf("\n1) Add Item\n2) Use Item\n3) Reverse Traverse\n"
           "4) Search Item (Linear)\n5) Sort by Value (Insertion Sort)\n6) "
           "Back\n> ");
    int choice = readInt();

    switch (choice) {
    case 1: {
      Item item;
      printf("Item name: ");
      readLine(item.name, sizeof(item.name));
      printf("Value: ");
      item.value = readInt();
      printf("Rarity (1-5): ");
      item.rarity = readInt();
      if (addItemToInventory(inventory, &item)) {
        printf("%sItem stored.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      } else {
        printf("%sUnable to store item.%s\n", ANSI_COLOR_WARNING,
               ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 2: {
      char target[32];
      printf("Use which item? ");
      readLine(target, sizeof(target));
      if (useItemFromInventory(inventory, target)) {
        printf("%sItem used.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      } else {
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
    case 4: {
      /* LINEAR SEARCH demonstration */
      char query[32];
      printf("Search for item (partial name): ");
      readLine(query, sizeof(query));
      const InventoryNode *found = linearSearchInventory(inventory, query);
      if (found != NULL) {
        printf("%sFound: %-16s | Value %3d | Rarity %d%s\n", ANSI_COLOR_SUCCESS,
               found->item.name, found->item.value, found->item.rarity,
               ANSI_COLOR_RESET);
      } else {
        printf("%sNo item matching \"%s\" found.%s\n", ANSI_COLOR_WARNING,
               query, ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 5:
      /* INSERTION SORT demonstration */
      sortInventoryByValue(inventory);
      printf("%sInventory sorted by value (descending).%s\n",
             ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      pauseScreen();
      break;
    case 6:
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

static void handleQuestMenu(MenuStack *stack, QuestLog *quests) {
  if (stack == NULL || quests == NULL) {
    popMenuState(stack, NULL);
    return;
  }

  bool stay = true;
  while (stay) {
    clearScreen();
    drawBox(70, 4);
    printQuestLog(quests);
    printf("\n1) Add Quest\n2) Complete Quest\n3) Back\n> ");
    int choice = readInt();

    switch (choice) {
    case 1: {
      char title[64];
      char description[128];
      printf("Quest title: ");
      readLine(title, sizeof(title));
      printf("Description: ");
      readLine(description, sizeof(description));
      if (addQuest(quests, title, description)) {
        printf("%sQuest added.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      } else {
        printf("%sUnable to add quest.%s\n", ANSI_COLOR_WARNING,
               ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 2: {
      char title[64];
      printf("Quest to complete: ");
      readLine(title, sizeof(title));
      if (completeQuest(quests, title)) {
        printf("%sQuest completed.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      } else {
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

static void handleLeaderboardMenu(MenuStack *stack, Leaderboard *board) {
  if (stack == NULL || board == NULL) {
    popMenuState(stack, NULL);
    return;
  }

  bool stay = true;
  while (stay) {
    displayLeaderboard(board);
    printf("\n1) Add Entry\n2) Search by Level (Binary Search)\n3) Back\n> ");
    int choice = readInt();

    switch (choice) {
    case 1: {
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
    case 2: {
      /* BINARY SEARCH demonstration */
      printf("Search by exact level: ");
      int lvl = readInt();
      int idx = binarySearchLeaderboard(board, lvl);
      if (idx >= 0) {
        printf("%sFound: %s | Level %d%s\n", ANSI_COLOR_SUCCESS,
               board->entries[idx].username, board->entries[idx].level,
               ANSI_COLOR_RESET);
      } else {
        printf("%sNo entry with level %d found.%s\n", ANSI_COLOR_WARNING, lvl,
               ANSI_COLOR_RESET);
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

static void handleWorldMenu(MenuStack *stack, Room **current_room) {
  if (stack == NULL || current_room == NULL || *current_room == NULL) {
    printf("World map unavailable.\n");
    pauseScreen();
    popMenuState(stack, NULL);
    return;
  }

  bool stay = true;
  while (stay) {
    clearScreen();
    drawBox(70, 4);
    describeRoom(*current_room);
    printf("\nChoose direction (N/S/E/W) or B to return: ");
    char buffer[MAX_INPUT_LENGTH];
    readLine(buffer, sizeof(buffer));
    toUpperString(buffer);

    if (buffer[0] == '\0') {
      continue;
    }

    if (buffer[0] == 'B') {
      stay = false;
      popMenuState(stack, NULL);
      break;
    }

    char direction[2];
    direction[0] = buffer[0];
    direction[1] = '\0';
    Room *next = moveToRoom(*current_room, direction);
    if (next != NULL) {
      *current_room = next;
      printf("%sYou travel %c into %s.%s\n", ANSI_COLOR_SUCCESS, direction[0],
             (*current_room)->name, ANSI_COLOR_RESET);
    } else {
      printf("%sNo passage in that direction.%s\n", ANSI_COLOR_WARNING,
             ANSI_COLOR_RESET);
    }
    pauseScreen();
  }
}

static void handleCombatMenu(MenuStack *stack, Player *player,
                             EventLog *events) {
  if (stack == NULL || player == NULL || events == NULL) {
    popMenuState(stack, NULL);
    return;
  }

  clearScreen();
  drawBox(70, 4);
  printf("%sCombat Simulation Initiated%s\n", ANSI_COLOR_TITLE,
         ANSI_COLOR_RESET);
  resetEventLog(events);
  startCombat(player, events);
  printEventLog(events);
  pauseScreen();
  popMenuState(stack, NULL);
}

static void handleBestiaryMenu(MenuStack *stack, MonsterInfo *root) {
  if (stack == NULL) {
    return;
  }

  bool stay = true;
  while (stay) {
    clearScreen();
    drawBox(70, 4);
    printf("%sBestiary Archives%s\n", ANSI_COLOR_TITLE, ANSI_COLOR_RESET);
    printf("1) View All\n2) Search by ID\n3) Back\n> ");
    int choice = readInt();

    switch (choice) {
    case 1:
      if (root == NULL) {
        printf("No discoveries logged.\n");
      } else {
        printBestiaryInOrder(root);
      }
      pauseScreen();
      break;
    case 2: {
      printf("Enter monster ID: ");
      int id = readInt();
      const MonsterInfo *info = searchMonster(root, id);
      if (info != NULL) {
        printf("ID %d | %s | Weakness: %s\n", info->id, info->name,
               info->weakness);
      } else {
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

/* -------------------- Skill Ring Menu (Circular Linked List) -------------- */
static void handleSkillMenu(MenuStack *stack, SkillRing *ring) {
  if (stack == NULL || ring == NULL) {
    popMenuState(stack, NULL);
    return;
  }

  bool stay = true;
  while (stay) {
    clearScreen();
    drawBox(70, 5);
    printf("%sSkill Ring  [Circular Linked List]%s\n", ANSI_COLOR_TITLE,
           ANSI_COLOR_RESET);
    printSkillRing(ring);
    printf("\n1) Learn Skill\n2) Rotate (next skill)\n"
           "3) Forget Active Skill\n4) Back\n> ");
    int choice = readInt();

    switch (choice) {
    case 1: {
      char name[32];
      printf("Skill name: ");
      readLine(name, sizeof(name));
      printf("Power (1-100): ");
      int power = readInt();
      if (addSkill(ring, name, power)) {
        printf("%sSkill learned.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      } else {
        printf("%sFailed to learn skill.%s\n", ANSI_COLOR_WARNING,
               ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 2:
      rotateSkillForward(ring);
      printf("%sActive skill advanced.%s\n", ANSI_COLOR_SUCCESS,
             ANSI_COLOR_RESET);
      pauseScreen();
      break;
    case 3:
      if (removeCurrentSkill(ring)) {
        printf("%sSkill forgotten.%s\n", ANSI_COLOR_SUCCESS, ANSI_COLOR_RESET);
      } else {
        printf("%sNo active skill.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
      }
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

/* --------------------- Item Shop Menu (Priority Queue) -------------------- */
static void handleShopMenu(MenuStack *stack, PriorityQueue *pq,
                           Inventory *inventory) {
  if (stack == NULL || pq == NULL) {
    popMenuState(stack, NULL);
    return;
  }

  bool stay = true;
  while (stay) {
    clearScreen();
    drawBox(70, 5);
    printf("%sItem Shop  [Priority Queue / Max-Heap]%s\n", ANSI_COLOR_TITLE,
           ANSI_COLOR_RESET);
    pqDisplay(pq);
    printf("\n1) Add item to shop\n2) Buy best item (extract-max)\n"
           "3) Peek at best item\n4) Search leaderboard level (Binary Search)\n"
           "5) Back\n> ");
    int choice = readInt();

    switch (choice) {
    case 1: {
      Item item;
      printf("Item name: ");
      readLine(item.name, sizeof(item.name));
      printf("Value: ");
      item.value = readInt();
      printf("Rarity (1-5): ");
      item.rarity = readInt();
      if (pqInsert(pq, &item)) {
        printf("%sItem added to shop.%s\n", ANSI_COLOR_SUCCESS,
               ANSI_COLOR_RESET);
      } else {
        printf("%sShop is full.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 2: {
      Item bought;
      if (pqExtractMax(pq, &bought)) {
        printf("%sPurchased: %s (Value %d, Rarity %d).%s\n", ANSI_COLOR_SUCCESS,
               bought.name, bought.value, bought.rarity, ANSI_COLOR_RESET);
        if (inventory != NULL) {
          addItemToInventory(inventory, &bought);
        }
      } else {
        printf("%sShop is empty.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 3: {
      Item peeked;
      if (pqPeek(pq, &peeked)) {
        printf("Best item: %s | Value %d | Rarity %d\n", peeked.name,
               peeked.value, peeked.rarity);
      } else {
        printf("%sShop is empty.%s\n", ANSI_COLOR_WARNING, ANSI_COLOR_RESET);
      }
      pauseScreen();
      break;
    }
    case 4: {
      /* BINARY SEARCH: refer user to leaderboard menu */
      printf(
          "Binary search is available in the Leaderboard menu (option 2).\n");
      pauseScreen();
      break;
    }
    case 5:
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

/* ============================================================
 *  FLAT PERSISTABLE GAME STATE
 * ============================================================ */

#define GS_MAX_INVENTORY 32
#define GS_MAX_QUESTS    16
#define GS_MAX_SKILLS     8
#define GS_MAX_SHOP      16
#define GS_STATE_FILE    "game_state.dat"

typedef struct {
  char skill_name[32];
  int  skill_power;
} FlatSkill;

typedef struct {
  /* ---- player ---- */
  char   username[MAX_USERNAME_LENGTH];
  int    hp, max_hp, attack, defense, gold, level;
  /* ---- world ---- */
  int    room_id;
  /* ---- inventory ---- */
  Item   inventory[GS_MAX_INVENTORY];
  size_t inv_count;
  /* ---- quests ---- */
  Quest  quests[GS_MAX_QUESTS];
  size_t quest_count;
  /* ---- skill ring (flat + active index) ---- */
  FlatSkill skills[GS_MAX_SKILLS];
  size_t    skill_count;
  size_t    skill_active;
  /* ---- shop (flat max-heap on rarity) ---- */
  Item   shop[GS_MAX_SHOP];
  size_t shop_count;
  /* ---- event log (circular buffer) ---- */
  char   events[EVENT_LOG_CAPACITY][EVENT_MESSAGE_LENGTH];
  size_t event_count;
  size_t event_head;
  /* ---- metadata ---- */
  bool   initialized;
} GameState;

/* ---------- Static world adjacency ---------- */
#define ROOM_COUNT 4

static const char *kRoomNames[ROOM_COUNT] = {
  "Sunlit Entrance", "Forgemaster Armory",
  "Whispering Library", "Crystal Sanctum"
};

typedef struct { const char *dir; int dest; } RoomLink;
static const RoomLink kAdj[ROOM_COUNT][4] = {
  /* 0 Entrance */ {{"N",1},{"E",2},{NULL,-1},{NULL,-1}},
  /* 1 Armory   */ {{"S",0},{"E",2},{NULL,-1},{NULL,-1}},
  /* 2 Library  */ {{"W",1},{"N",3},{NULL,-1},{NULL,-1}},
  /* 3 Sanctum  */ {{"S",2},{NULL,-1},{NULL,-1},{NULL,-1}},
};

/* ---- Event log helper ---- */
static void gsEnqueueEvent(GameState *gs, const char *msg) {
  if (gs == NULL || msg == NULL) return;
  size_t idx;
  if (gs->event_count < EVENT_LOG_CAPACITY) {
    idx = (gs->event_head + gs->event_count) % EVENT_LOG_CAPACITY;
    gs->event_count++;
  } else {
    idx = gs->event_head;
    gs->event_head = (gs->event_head + 1U) % EVENT_LOG_CAPACITY;
  }
  strncpy(gs->events[idx], msg, EVENT_MESSAGE_LENGTH - 1U);
  gs->events[idx][EVENT_MESSAGE_LENGTH - 1U] = '\0';
}

/* ---- Shop heap helpers (max-heap on rarity) ---- */
static void gsShopHeapifyUp(GameState *gs, size_t idx) {
  while (idx > 0U) {
    size_t parent = (idx - 1U) / 2U;
    if (gs->shop[parent].rarity < gs->shop[idx].rarity) {
      Item tmp = gs->shop[parent];
      gs->shop[parent] = gs->shop[idx];
      gs->shop[idx] = tmp;
      idx = parent;
    } else {
      break;
    }
  }
}

static void gsShopHeapifyDown(GameState *gs, size_t idx) {
  size_t l, r, largest;
  while (true) {
    l = 2U * idx + 1U; r = 2U * idx + 2U; largest = idx;
    if (l < gs->shop_count && gs->shop[l].rarity > gs->shop[largest].rarity)
      largest = l;
    if (r < gs->shop_count && gs->shop[r].rarity > gs->shop[largest].rarity)
      largest = r;
    if (largest == idx) break;
    Item tmp = gs->shop[idx];
    gs->shop[idx] = gs->shop[largest];
    gs->shop[largest] = tmp;
    idx = largest;
  }
}

static bool gsShopInsert(GameState *gs, const Item *item) {
  if (gs == NULL || item == NULL || gs->shop_count >= GS_MAX_SHOP) return false;
  gs->shop[gs->shop_count] = *item;
  gsShopHeapifyUp(gs, gs->shop_count);
  gs->shop_count++;
  return true;
}

static bool gsShopExtractMax(GameState *gs, Item *out) {
  if (gs == NULL || gs->shop_count == 0U) return false;
  if (out != NULL) *out = gs->shop[0];
  gs->shop_count--;
  if (gs->shop_count > 0U) {
    gs->shop[0] = gs->shop[gs->shop_count];
    gsShopHeapifyDown(gs, 0U);
  }
  return true;
}

/* ---- Load / Save ---- */
static bool loadGameState(const char *username, GameState *gs) {
  if (username == NULL || gs == NULL) return false;
  FILE *f = fopen(GS_STATE_FILE, "rb");
  if (f == NULL) return false;
  GameState tmp;
  bool found = false;
  while (fread(&tmp, sizeof(GameState), 1U, f) == 1U) {
    if (strncmp(tmp.username, username, MAX_USERNAME_LENGTH) == 0) {
      *gs = tmp;
      found = true;
      break;
    }
  }
  fclose(f);
  return found;
}

static void saveGameState(const GameState *gs) {
  if (gs == NULL) return;
  FILE *f = fopen(GS_STATE_FILE, "rb+");
  if (f != NULL) {
    GameState tmp;
    while (fread(&tmp, sizeof(GameState), 1U, f) == 1U) {
      if (strncmp(tmp.username, gs->username, MAX_USERNAME_LENGTH) == 0) {
        long pos = (long)(ftell(f) - (long)sizeof(GameState));
        if (pos >= 0 && fseek(f, pos, SEEK_SET) == 0) {
          fwrite(gs, sizeof(GameState), 1U, f);
          fflush(f);
        }
        fclose(f);
        return;
      }
    }
    fwrite(gs, sizeof(GameState), 1U, f);
    fflush(f);
    fclose(f);
    return;
  }
  f = fopen(GS_STATE_FILE, "wb");
  if (f != NULL) {
    fwrite(gs, sizeof(GameState), 1U, f);
    fclose(f);
  }
}

/* ---- JSON helpers ---- */
static void jsonEscapeStr(const char *src, char *dst, size_t cap) {
  if (dst == NULL || cap == 0U) return;
  if (src == NULL) { dst[0] = '\0'; return; }
  size_t wi = 0U;
  for (size_t i = 0U; src[i] != '\0' && wi + 2U < cap; ++i) {
    char c = src[i];
    if      (c == '"')  { if (wi+3U<cap){ dst[wi++]='\\'; dst[wi++]='"';  } }
    else if (c == '\\') { if (wi+3U<cap){ dst[wi++]='\\'; dst[wi++]='\\'; } }
    else if (c == '\n') { if (wi+3U<cap){ dst[wi++]='\\'; dst[wi++]='n';  } }
    else                { dst[wi++] = c; }
  }
  dst[wi] = '\0';
}

static void printJsonOutput(const GameState *gs, const char *error_msg) {
  char esc[EVENT_MESSAGE_LENGTH * 2];
  printf("{");

  /* player */
  jsonEscapeStr(gs->username, esc, sizeof(esc));
  printf("\"player\":{"
         "\"username\":\"%s\","
         "\"hp\":%d,\"max_hp\":%d,"
         "\"attack\":%d,\"defense\":%d,"
         "\"gold\":%d,\"level\":%d"
         "},", esc, gs->hp, gs->max_hp, gs->attack, gs->defense, gs->gold, gs->level);

  /* location */
  const char *loc = (gs->room_id >= 0 && gs->room_id < ROOM_COUNT)
                      ? kRoomNames[gs->room_id] : "Unknown";
  jsonEscapeStr(loc, esc, sizeof(esc));
  printf("\"location\":\"%s\",", esc);

  /* exits */
  printf("\"exits\":[");
  bool first = true;
  for (int k = 0; k < 4; ++k) {
    if (kAdj[gs->room_id][k].dir == NULL) break;
    if (!first) printf(",");
    printf("\"%s\"", kAdj[gs->room_id][k].dir);
    first = false;
  }
  printf("],");

  /* inventory */
  printf("\"inventory\":[");
  for (size_t i = 0U; i < gs->inv_count; ++i) {
    if (i > 0U) printf(",");
    jsonEscapeStr(gs->inventory[i].name, esc, sizeof(esc));
    printf("{\"name\":\"%s\",\"value\":%d,\"rarity\":%d}",
           esc, gs->inventory[i].value, gs->inventory[i].rarity);
  }
  printf("],");

  /* quests */
  printf("\"quests\":[");
  for (size_t i = 0U; i < gs->quest_count; ++i) {
    if (i > 0U) printf(",");
    char esc2[256];
    jsonEscapeStr(gs->quests[i].title, esc, sizeof(esc));
    jsonEscapeStr(gs->quests[i].description, esc2, sizeof(esc2));
    printf("{\"title\":\"%s\",\"description\":\"%s\"}", esc, esc2);
  }
  printf("],");

  /* skills */
  printf("\"skills\":[");
  for (size_t i = 0U; i < gs->skill_count; ++i) {
    if (i > 0U) printf(",");
    jsonEscapeStr(gs->skills[i].skill_name, esc, sizeof(esc));
    printf("{\"name\":\"%s\",\"power\":%d,\"active\":%s}",
           esc, gs->skills[i].skill_power,
           (gs->skill_count > 0U && i == gs->skill_active) ? "true" : "false");
  }
  printf("],");

  /* shop */
  printf("\"shop\":[");
  for (size_t i = 0U; i < gs->shop_count; ++i) {
    if (i > 0U) printf(",");
    jsonEscapeStr(gs->shop[i].name, esc, sizeof(esc));
    printf("{\"name\":\"%s\",\"value\":%d,\"rarity\":%d}",
           esc, gs->shop[i].value, gs->shop[i].rarity);
  }
  printf("],");

  /* events */
  printf("\"events\":[");
  for (size_t i = 0U; i < gs->event_count; ++i) {
    if (i > 0U) printf(",");
    size_t idx = (gs->event_head + i) % EVENT_LOG_CAPACITY;
    jsonEscapeStr(gs->events[idx], esc, sizeof(esc));
    printf("\"%s\"", esc);
  }
  printf("],");

  /* error */
  if (error_msg != NULL) {
    jsonEscapeStr(error_msg, esc, sizeof(esc));
    printf("\"error\":\"%s\"", esc);
  } else {
    printf("\"error\":null");
  }
  printf("}\n");
  fflush(stdout);
}

/* ---- Seed a brand-new game ---- */
static void seedNewGame(GameState *gs) {
  /* Inventory */
  strncpy(gs->inventory[0].name,"Solar Tonic",31); gs->inventory[0].value=40;  gs->inventory[0].rarity=1;
  strncpy(gs->inventory[1].name,"Echo Blade",  31); gs->inventory[1].value=125; gs->inventory[1].rarity=3;
  strncpy(gs->inventory[2].name,"Prism Aegis", 31); gs->inventory[2].value=90;  gs->inventory[2].rarity=2;
  gs->inv_count = 3U;
  /* Quests */
  strncpy(gs->quests[0].title,"Beacon",63);
  strncpy(gs->quests[0].description,"Restore the lighthouse flames",127);
  strncpy(gs->quests[1].title,"Relic",63);
  strncpy(gs->quests[1].description,"Retrieve the crystal heart",127);
  gs->quest_count = 2U;
  /* Skills */
  strncpy(gs->skills[0].skill_name,"Arcane Bolt", 31); gs->skills[0].skill_power=40;
  strncpy(gs->skills[1].skill_name,"Shadow Dash", 31); gs->skills[1].skill_power=25;
  strncpy(gs->skills[2].skill_name,"Iron Bastion",31); gs->skills[2].skill_power=55;
  gs->skill_count  = 3U;
  gs->skill_active = 0U;
  /* Shop heap */
  Item shop_seed[] = {
    {"Void Crystal",200,5},{"Health Potion",30,1},
    {"Mana Shard",80,3},   {"Steel Ingot",50,2}
  };
  for (size_t i = 0U; i < 4U; ++i) gsShopInsert(gs, &shop_seed[i]);
}

/* ============================================================
 *  COMBAT (state-based, writes to event log only)
 * ============================================================ */
static void gsCombat(GameState *gs) {
  if (gs == NULL) return;
  char msg[EVENT_MESSAGE_LENGTH];

  Combatant player_unit;
  strncpy(player_unit.name, gs->username, sizeof(player_unit.name) - 1U);
  player_unit.name[sizeof(player_unit.name) - 1U] = '\0';
  player_unit.hp       = gs->hp;
  player_unit.attack   = gs->attack;
  player_unit.is_player = true;

  Combatant enemies[3] = {
    {"Goblin Marauder", 35, 8, false},
    {"Crystal Wisp",    28,10, false},
    {"Stone Bulwark",   45, 6, false}
  };

  CombatQueue queue;
  if (!initCombatQueue(&queue, 8U)) {
    gsEnqueueEvent(gs, "Combat init failed."); return;
  }

  enqueueCombatant(&queue, &player_unit);
  for (size_t i = 0U; i < 3U; ++i) enqueueCombatant(&queue, &enemies[i]);

  while (player_unit.hp > 0 && enemiesRemain(enemies, 3U)) {
    Combatant *actor = dequeueCombatant(&queue);
    if (actor == NULL) break;
    if (actor->hp <= 0) continue;

    if (actor->is_player) {
      size_t tidx = nextEnemyIndex(enemies, 3U);
      if (tidx == 3U) { enqueueCombatant(&queue, actor); continue; }
      Combatant *tgt = &enemies[tidx];
      int dmg = actor->attack;
      tgt->hp -= dmg;
      if (tgt->hp < 0) tgt->hp = 0;
      snprintf(msg, sizeof(msg), "%s strikes %s for %d damage.",
               actor->name, tgt->name, dmg);
      gsEnqueueEvent(gs, msg);
      if (tgt->hp == 0) {
        snprintf(msg, sizeof(msg), "%s is defeated!", tgt->name);
        gsEnqueueEvent(gs, msg);
      }
    } else {
      int dmg = actor->attack - (gs->defense / 4);
      if (dmg < 1) dmg = 1;
      player_unit.hp -= dmg;
      if (player_unit.hp < 0) player_unit.hp = 0;
      snprintf(msg, sizeof(msg), "%s claws %s for %d damage.",
               actor->name, player_unit.name, dmg);
      gsEnqueueEvent(gs, msg);
    }

    if (actor->hp > 0) enqueueCombatant(&queue, actor);
  }

  gs->hp = player_unit.hp;
  if (gs->hp > gs->max_hp) gs->hp = gs->max_hp;

  if (gs->hp > 0) {
    gs->gold += 25;
    if (gs->level < 99) gs->level++;
    snprintf(msg, sizeof(msg), "Victory! +25 gold. Level %d.", gs->level);
    gsEnqueueEvent(gs, msg);
  } else {
    gsEnqueueEvent(gs, "You have fallen in battle.");
    gs->hp = gs->max_hp / 2;
    if (gs->hp == 0) gs->hp = 1;
    snprintf(msg, sizeof(msg), "Revived at %d HP.", gs->hp);
    gsEnqueueEvent(gs, msg);
  }
  freeCombatQueue(&queue);
}

/* ============================================================
 *  COMMAND IMPLEMENTATIONS
 * ============================================================ */

/* init <username> */
static const char *cmdInit(GameState *gs, const char *username) {
  if (!username || username[0] == '\0') username = "Traveler";
  if (loadGameState(username, gs)) {
    return NULL; /* existing save loaded */
  }
  memset(gs, 0, sizeof(GameState));
  strncpy(gs->username, username, MAX_USERNAME_LENGTH - 1U);
  gs->username[MAX_USERNAME_LENGTH - 1U] = '\0';
  gs->hp = 100; gs->max_hp  = 100;
  gs->attack = 15; gs->defense = 10;
  gs->gold = 125;  gs->level   = 1;
  gs->room_id = 0;
  gs->initialized = true;
  seedNewGame(gs);
  gsEnqueueEvent(gs, "A new hero emerges from the mists of Arcadia.");
  return NULL;
}

/* combat */
static const char *cmdCombat(GameState *gs) {
  gsCombat(gs);
  return NULL;
}

/* move <direction> */
static const char *cmdMove(GameState *gs, const char *dir) {
  if (!dir || dir[0] == '\0') return "No direction provided.";
  char upper[8] = {0};
  for (size_t i = 0U; dir[i] != '\0' && i < 7U; ++i)
    upper[i] = (dir[i] >= 'a' && dir[i] <= 'z') ? (char)(dir[i] - 32) : dir[i];
  if (gs->room_id < 0 || gs->room_id >= ROOM_COUNT) return "Invalid room state.";
  for (int k = 0; k < 4; ++k) {
    if (kAdj[gs->room_id][k].dir == NULL) break;
    if (strncmp(kAdj[gs->room_id][k].dir, upper, 8) == 0) {
      int dest = kAdj[gs->room_id][k].dest;
      char msg[EVENT_MESSAGE_LENGTH];
      snprintf(msg, sizeof(msg), "Traveled %s to %s.", upper, kRoomNames[dest]);
      gsEnqueueEvent(gs, msg);
      gs->room_id = dest;
      return NULL;
    }
  }
  return "No passage in that direction.";
}

/* buy — extract highest-rarity item from shop into inventory */
static const char *cmdBuy(GameState *gs) {
  Item bought;
  if (!gsShopExtractMax(gs, &bought)) return "Shop is empty.";
  if (gs->inv_count >= GS_MAX_INVENTORY) {
    gsShopInsert(gs, &bought); /* restore */
    return "Inventory is full.";
  }
  gs->inventory[gs->inv_count++] = bought;
  char msg[EVENT_MESSAGE_LENGTH];
  snprintf(msg, sizeof(msg), "Purchased %s (Value %d, Rarity %d).",
           bought.name, bought.value, bought.rarity);
  gsEnqueueEvent(gs, msg);
  return NULL;
}

/* skill rotate */
static const char *cmdSkillRotate(GameState *gs) {
  if (gs->skill_count == 0U) return "No skills available.";
  gs->skill_active = (gs->skill_active + 1U) % gs->skill_count;
  char msg[EVENT_MESSAGE_LENGTH];
  snprintf(msg, sizeof(msg), "Active skill: %s.",
           gs->skills[gs->skill_active].skill_name);
  gsEnqueueEvent(gs, msg);
  return NULL;
}

/* skill learn <name> <power> */
static const char *cmdSkillLearn(GameState *gs, const char *name, int power) {
  if (!name || name[0] == '\0') return "Skill name required.";
  if (gs->skill_count >= GS_MAX_SKILLS) return "Skill ring is full.";
  strncpy(gs->skills[gs->skill_count].skill_name, name, 31U);
  gs->skills[gs->skill_count].skill_name[31U] = '\0';
  gs->skills[gs->skill_count].skill_power = power;
  gs->skill_count++;
  char msg[EVENT_MESSAGE_LENGTH];
  snprintf(msg, sizeof(msg), "Learned skill: %s (Power %d).", name, power);
  gsEnqueueEvent(gs, msg);
  return NULL;
}

/* ============================================================
 *  MAIN — CLI DISPATCHER
 *
 *  Commands:
 *    arcadia init   <username>
 *    arcadia combat <username>
 *    arcadia move   <username> <direction>
 *    arcadia buy    <username>
 *    arcadia skill  <username> rotate
 *    arcadia skill  <username> learn <name> <power>
 *
 *  Stdout: single-line JSON game state (no other output).
 * ============================================================ */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stdout, "{\"error\":\"Usage: arcadia <command> <username> [args]\"}\n");
    return EXIT_FAILURE;
  }

  const char *cmd = argv[1];
  GameState gs;
  memset(&gs, 0, sizeof(GameState));
  const char *err = NULL;

  if (strcmp(cmd, "init") == 0) {
    const char *uname = (argc >= 3) ? argv[2] : "Traveler";
    err = cmdInit(&gs, uname);

  } else {
    /* All stateful commands: argv[2] = username */
    const char *uname = (argc >= 3) ? argv[2] : NULL;
    if (uname == NULL || uname[0] == '\0') {
      fprintf(stdout,
              "{\"error\":\"Username required as second argument.\"}\n");
      return EXIT_FAILURE;
    }

    if (!loadGameState(uname, &gs)) {
      fprintf(stdout,
              "{\"error\":\"No save found for '%s'. Run init first.\"}\n",
              uname);
      return EXIT_FAILURE;
    }

    if (strcmp(cmd, "combat") == 0) {
      err = cmdCombat(&gs);

    } else if (strcmp(cmd, "move") == 0) {
      const char *dir = (argc >= 4) ? argv[3] : "";
      err = cmdMove(&gs, dir);

    } else if (strcmp(cmd, "buy") == 0) {
      err = cmdBuy(&gs);

    } else if (strcmp(cmd, "skill") == 0) {
      const char *action = (argc >= 4) ? argv[3] : "";
      if (strcmp(action, "rotate") == 0) {
        err = cmdSkillRotate(&gs);
      } else if (strcmp(action, "learn") == 0) {
        const char *sname  = (argc >= 5) ? argv[4] : "";
        int         spower = (argc >= 6) ? (int)strtol(argv[5], NULL, 10) : 10;
        err = cmdSkillLearn(&gs, sname, spower);
      } else {
        err = "Unknown skill action. Use: rotate | learn <name> <power>.";
      }

    } else {
      err = "Unknown command. Valid: init | combat | move | buy | skill.";
    }
  }

  saveGameState(&gs);
  printJsonOutput(&gs, err);
  return EXIT_SUCCESS;
}


