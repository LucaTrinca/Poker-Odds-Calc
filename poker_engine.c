
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>



// =========================================================
// SECTION 1: CONSTANTS & DATA STRUCTURES
// =========================================================
#define HASH_SIZE   1048576 
#define HASH_MASK   (HASH_SIZE - 1)
#define MAX_PLAYERS 8

// Result Structure (Return this to your frontend)
typedef struct {
    double win_pct;
    double tie_pct;
    double loss_pct;
    double equity_pct;
} PlayerResult;

// Internal Engine Structures
typedef struct { unsigned int product; int score; } HashEntry;

// Engine Tiers
#define TIER_SF     0x9000000 
#define TIER_QUADS  0x8000000 
#define TIER_FH     0x7000000 
#define TIER_FLUSH  0x6000000 
#define TIER_STR    0x5000000 
#define TIER_TRIPS  0x4000000 
#define TIER_2PAIR  0x3000000 
#define TIER_PAIR   0x2000000 
#define TIER_HIGH   0x1000000 

// Globals (Lookup Tables - Read Only after Init)
int flush_lookup[8192] = {0};
HashEntry* prime_map = NULL; 
static const int PRIMES[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41};

// Thread-Safe Context for Recursion
typedef struct {
    long long wins[MAX_PLAYERS];
    long long ties[MAX_PLAYERS];
    long long losses[MAX_PLAYERS];
    double total_equity[MAX_PLAYERS];
    long long total_boards;
} SimStats;

// =========================================================
// SECTION 2: INITIALIZATION & LOOKUP LOGIC
// =========================================================

// Internal: Calculate score from scratch (used only for init)
static inline int calculate_score_init(int ranks[5]) {
    int counts[13] = {0}, unique = 0;
    for (int i=0; i<4; i++) for (int j=0; j<4-i; j++) 
        if (ranks[j] < ranks[j+1]) { int t=ranks[j]; ranks[j]=ranks[j+1]; ranks[j+1]=t; }
    for(int i=0; i<5; i++) { if (counts[ranks[i]]==0) unique++; counts[ranks[i]]++; }
    
    if (unique == 5) {
        if (ranks[0]-ranks[4] == 4) return TIER_STR | ranks[0]; 
        if (ranks[0]==12 && ranks[1]==3 && ranks[4]==0) return TIER_STR | 3; 
        return TIER_HIGH | (ranks[0]<<16) | (ranks[1]<<12) | (ranks[2]<<8) | (ranks[3]<<4) | ranks[4];
    }
    int quad=-1, trip=-1, p1=-1, p2=-1;
    for(int i=12; i>=0; i--) {
        if (counts[i]==4) quad=i; else if (counts[i]==3) trip=i;
        else if (counts[i]==2) { if (p1==-1) p1=i; else p2=i; }
    }
    if (quad != -1) return TIER_QUADS | (quad<<4) | ((ranks[0]==quad)?ranks[4]:ranks[0]);
    if (trip != -1 && p1 != -1) return TIER_FH | (trip<<4) | p1;
    if (trip != -1) {
        int k1=(ranks[0]==trip)?ranks[3]:ranks[0], k2=(ranks[4]==trip)?ranks[1]:ranks[4];
        return TIER_TRIPS | (trip<<8) | (k1<<4) | k2;
    }
    if (p2 != -1) {
        int k = (ranks[0]!=p1 && ranks[0]!=p2) ? ranks[0] : ((ranks[2]!=p1 && ranks[2]!=p2)?ranks[2]:ranks[4]);
        return TIER_2PAIR | (p1<<8) | (p2<<4) | k;
    }
    if (p1 != -1) {
        int k[3], x=0; for(int i=0; i<5; i++) if(ranks[i]!=p1) k[x++]=ranks[i];
        return TIER_PAIR | (p1<<12) | (k[0]<<8) | (k[1]<<4) | k[2];
    }
    return 0;
}

void insert_hash(unsigned int prod, int score) {
    unsigned int idx = (prod + (prod >> 11) + (prod * 17)) & HASH_MASK;
    while (prime_map[idx].product != 0) {
        if (prime_map[idx].product == prod) return;
        idx = (idx + 1) & HASH_MASK;
    }
    prime_map[idx].product = prod; prime_map[idx].score = score;
}

void generate_combinations(int depth, int start_idx, unsigned int current_prod, int current_ranks[]) {
    if (depth == 5) {
        int temp[5]; memcpy(temp, current_ranks, 5*sizeof(int));
        insert_hash(current_prod, calculate_score_init(temp));
        return;
    }
    for (int i = start_idx; i < 13; i++) {
        current_ranks[depth] = i;
        generate_combinations(depth + 1, i, current_prod * PRIMES[i], current_ranks);
    }
}

// PUBLIC API: Call this once at server startup
void init_poker_engine() {
    if (prime_map) return; // Idempotent check
    prime_map = (HashEntry*)calloc(HASH_SIZE, sizeof(HashEntry));
    if (!prime_map) exit(1);

    for (int mask = 0; mask < 8192; mask++) {
        int bits=0, ranks[5], r=0;
        for (int i=0; i<13; i++) if ((mask>>i)&1) { ranks[r++]=i; bits++; }
        if (bits == 5) {
            int s = calculate_score_init(ranks);
            if ((s & 0xF000000) == TIER_STR) flush_lookup[mask] = TIER_SF | (s & 0xFFFFFF);
            else flush_lookup[mask] = TIER_FLUSH | ((ranks[4]<<16)|(ranks[3]<<12)|(ranks[2]<<8)|(ranks[1]<<4)|ranks[0]);
        }
    }
    int dummy[5]; generate_combinations(0, 0, 1, dummy);
}

// =========================================================
// SECTION 3: FAST EVALUATION
// =========================================================
static inline int evaluate_5(int c1, int c2, int c3, int c4, int c5) {
    if ((c1 & c2 & c3 & c4 & c5) & 0xF00000) return flush_lookup[(c1|c2|c3|c4|c5) & 0x1FFF];
    unsigned int p = ((c1>>13)&0x3F) * ((c2>>13)&0x3F) * ((c3>>13)&0x3F) * ((c4>>13)&0x3F) * ((c5>>13)&0x3F);
    unsigned int idx = (p + (p>>11) + (p*17)) & HASH_MASK;
    if (prime_map[idx].product == p) return prime_map[idx].score;
    idx = (idx + 1) & HASH_MASK;
    if (prime_map[idx].product == p) return prime_map[idx].score;
    while (prime_map[idx].product != p) idx = (idx + 1) & HASH_MASK;
    return prime_map[idx].score;
}

static const int COMBS[21][5] = { {0,1,2,3,4}, {0,1,2,3,5}, {0,1,2,3,6}, {0,1,2,4,5}, {0,1,2,4,6}, {0,1,2,5,6}, {0,1,3,4,5}, {0,1,3,4,6}, {0,1,3,5,6}, {0,1,4,5,6}, {0,2,3,4,5}, {0,2,3,4,6}, {0,2,3,5,6}, {0,2,4,5,6}, {0,3,4,5,6}, {1,2,3,4,5}, {1,2,3,4,6}, {1,2,3,5,6}, {1,2,4,5,6}, {1,3,4,5,6}, {2,3,4,5,6} };

static inline int evaluate_7(int cards[7]) {
    int max = 0;
    for (int i=0; i<21; i++) {
        int s = evaluate_5(cards[COMBS[i][0]], cards[COMBS[i][1]], cards[COMBS[i][2]], cards[COMBS[i][3]], cards[COMBS[i][4]]);
        if (s > max) max = s;
    }
    return max;
}

// =========================================================
// SECTION 4: PARSING
// =========================================================
static inline int get_rank_idx(char r) {
    if (isdigit(r)) return r-'2';
    switch (toupper(r)) { case 'T':return 8; case 'J':return 9; case 'Q':return 10; case 'K':return 11; case 'A':return 12; } return -1;
}

static inline int get_suit_val(char s) {
    switch (toupper(s)) { case 'S':return 0x800000; case 'H':return 0x400000; case 'D':return 0x200000; case 'C':return 0x100000; } return 0;
}

// Helper to convert string "As Kh" to int array
void parse_hand_string(const char* str, int* cards, int num_cards) {
    if (!str || strlen(str)==0) return;
    char temp[256]; strncpy(temp, str, 255); temp[255] = '\0';
    char* token = strtok(temp, " ");
    int count = 0;
    while (token && count < num_cards) {
        if (strlen(token) >= 2) {
            int r = get_rank_idx(token[0]);
            int s = get_suit_val(token[1]);
            int p = PRIMES[r];
            cards[count++] = s | (p<<13) | (1<<r); 
        }
        token = strtok(NULL, " ");
    }
}

// =========================================================
// SECTION 5: EXACT SOLVER (RECURSIVE)
// =========================================================

void solve_recursive(int deck_idx, int cards_picked, int cards_needed, 
                     int current_board[], int fixed_cnt, 
                     int hands[MAX_PLAYERS][2], int num_players, 
                     int* deck, int deck_size, SimStats* stats) {
    
    if (cards_picked == cards_needed) {
        int scores[MAX_PLAYERS];
        int max_score = -1;
        int temp_hand[7];
        
        // Build 7-card hand for board
        for(int i=0; i<fixed_cnt + cards_needed; i++) temp_hand[2+i] = current_board[i];

        // Evaluate all players
        for (int p=0; p<num_players; p++) {
            temp_hand[0] = hands[p][0];
            temp_hand[1] = hands[p][1];
            scores[p] = evaluate_7(temp_hand);
            if (scores[p] > max_score) max_score = scores[p];
        }

        // Determine Winners
        int winners_count = 0;
        int winners_indices[MAX_PLAYERS];
        for (int p=0; p<num_players; p++) {
            if (scores[p] == max_score) winners_indices[winners_count++] = p;
        }

        // Update Stats
        double share = 1.0 / (double)winners_count;
        
        // Handle Wins/Ties
        if (winners_count == 1) {
            stats->wins[winners_indices[0]]++;
        } else {
            for(int w=0; w<winners_count; w++) stats->ties[winners_indices[w]]++;
        }
        
        // Handle Losses (Everyone who didn't win)
        for(int p=0; p<num_players; p++) {
            bool is_winner = false;
            for(int w=0; w<winners_count; w++) if (winners_indices[w] == p) is_winner = true;
            
            if (is_winner) stats->total_equity[p] += share;
            else stats->losses[p]++;
        }

        stats->total_boards++;
        return;
    }

    // Recursion
    for (int i = deck_idx; i < deck_size; i++) {
        current_board[fixed_cnt + cards_picked] = deck[i];
        solve_recursive(i+1, cards_picked+1, cards_needed, current_board, fixed_cnt, hands, num_players, deck, deck_size, stats);
    }
}

// =========================================================
// SECTION 6: PUBLIC API FUNCTION
// =========================================================

/*
 * calculate_equity
 * ----------------
 * Input:
 * num_players: Number of players (2-8)
 * player_hands: Array of strings (e.g., {"As Ks", "Td Tc"})
 * board_cards: String (e.g., "2h 5h 9s" or "")
 * Output:
 * results: Array of PlayerResult structs to be filled
 */



void calculate_equity(int num_players, const char* player_hands[], const char* board_cards, PlayerResult* results) {
    if (!prime_map) init_poker_engine(); // Auto-init if forgotten

    // 1. Parsing
    int hands_int[MAX_PLAYERS][2];
    int known_cards[52];
    int known_count = 0;

    for(int p=0; p<num_players; p++) {
        int temp[2] = {0};
        parse_hand_string(player_hands[p], temp, 2);
        hands_int[p][0] = temp[0]; hands_int[p][1] = temp[1];
        known_cards[known_count++] = temp[0];
        known_cards[known_count++] = temp[1];
    }

    int board_int[5] = {0};
    int board_count = 0;
    if (board_cards && strlen(board_cards) > 0) {
        int temp[5] = {0};
        parse_hand_string(board_cards, temp, 5);
        for(int i=0; i<5; i++) {
            if (temp[i] != 0) {
                board_int[board_count++] = temp[i];
                known_cards[known_count++] = temp[i];
            }
        }
    }

    // 2. Build Deck
    int deck[52], deck_size = 0;
    for (int r=0; r<13; r++) {
        for (int s_idx=0; s_idx<4; s_idx++) {
            int s = 0x100000 << s_idx; int p = PRIMES[r];
            int card = s | (p<<13) | (1<<r);
            
            bool used = false;
            for(int k=0; k<known_count; k++) if (known_cards[k] == card) used = true;
            if (!used) deck[deck_size++] = card;
        }
    }

    // 3. Solve
    SimStats stats;
    memset(&stats, 0, sizeof(SimStats));
    int cards_needed = 5 - board_count;
    
    // Copy board to working buffer
    int working_board[5];
    for(int i=0; i<board_count; i++) working_board[i] = board_int[i];

    // Run Recursive Solver
    solve_recursive(0, 0, cards_needed, working_board, board_count, hands_int, num_players, deck, deck_size, &stats);

    // 4. Format Results
    for (int p=0; p<num_players; p++) {
        if (stats.total_boards > 0) {
            results[p].win_pct = ((double)stats.wins[p] / stats.total_boards) * 100.0;
            results[p].tie_pct = ((double)stats.ties[p] / stats.total_boards) * 100.0;
            results[p].loss_pct = ((double)stats.losses[p] / stats.total_boards) * 100.0;
            results[p].equity_pct = (stats.total_equity[p] / stats.total_boards) * 100.0;
        } else {
            results[p].win_pct = 0.0;
            results[p].tie_pct = 0.0;
            results[p].loss_pct = 0.0;
            results[p].equity_pct = 0.0;
        }
    }
}

// Minimal main for demonstration/testing (can be removed for library use)
int main() {
    const char* players[] = { "As Ks", "Td Tc" };
    const char* noard =  "Qs Js 9h";
    PlayerResult results[2];
    
    printf("Running Exact Calculation (AsKs vs TT)...\n");
    calculate_equity(2, players, noard, results);
    
    for(int i=0; i<2; i++) {
        printf("P%d: Win %.2f%% | Tie %.2f%% | Loss %.2f%% | Equity %.2f%%\n", 
               i+1, results[i].win_pct, results[i].tie_pct, results[i].loss_pct, results[i].equity_pct);
    }
    return 0;
}