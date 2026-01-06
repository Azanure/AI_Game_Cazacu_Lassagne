#pragma once

#include "../../common/GameRules.hpp"
#include "../../common/GameState.hpp"
#include "../../common/Move.hpp"
#include "../../common/SearchStats.hpp"
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <random>
#include <array>

namespace AI_V8 {

    // --- CONFIGURATION ---
    static std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    static double time_limit_ms;
    static bool time_out;
    static SearchStats stats;

    constexpr int INF = 10000000;
    constexpr int MAX_DEPTH = 64;

    // --- ADN DU CHAMPION (Issu de la Génération 154) ---
    struct BotDNA {
        int w_score = 160;
        int w_seed_red = 16;
        int w_seed_blue = 179;
        int w_seed_trans = -5;
        int w_defense = -50;       // Stratégie de baiting
        int w_attack = 31;        // Agressivité calibrée
        int w_mobility = 78;      // Contrôle de terrain massif
        int w_hoard_penalty = 39; // Propreté du plateau
        int w_starvation = -5;   // Gestion de fin de partie tactique

        // Poids positionnels optimisés
        std::array<int, 16> w_holes = { 5, 9, 25, 9, -13, 13, 1, -8, -1, 3, -5, 4, 7, 6, -3, 9 };
    };

    static BotDNA champion_dna;

    // --- 1. ZOBRIST HASHING ---
    namespace Zobrist {
        static uint64_t table[NB_HOLES][3][64];
        static uint64_t turn_hash[2];
        static bool initialized = false;

        inline void init() {
            if (initialized) return;
            std::mt19937_64 rng(12345);
            for (int i = 0; i < NB_HOLES; ++i)
                for (int c = 0; c < 3; ++c)
                    for (int n = 0; n < 64; ++n)
                        table[i][c][n] = rng();
            turn_hash[0] = rng();
            turn_hash[1] = rng();
            initialized = true;
        }

        inline uint64_t compute(const GameState& state, int player_id) {
            uint64_t h = 0;
            for (int i = 0; i < NB_HOLES; ++i) {
                int r = std::min((int)state.get_seeds(i, RED), 63);
                int b = std::min((int)state.get_seeds(i, BLUE), 63);
                int t = std::min((int)state.get_seeds(i, TRANSPARENT), 63);
                if (r > 0) h ^= table[i][0][r];
                if (b > 0) h ^= table[i][1][b];
                if (t > 0) h ^= table[i][2][t];
            }
            h ^= turn_hash[player_id - 1];
            return h;
        }
    }

    // --- 2. TABLE DE TRANSPOSITION ---
    enum class TTFlag : uint8_t { EXACT, LOWERBOUND, UPPERBOUND };
    struct TTEntry {
        uint64_t key = 0;
        int score = 0;
        int depth = 0;
        TTFlag flag = TTFlag::EXACT;
        Move best_move;
    };

    constexpr size_t TT_SIZE = 1048576;
    static std::vector<TTEntry> transposition_table(TT_SIZE);

    // --- 3. HEURISTIQUES DE TRI ---
    static Move killer_moves[MAX_DEPTH][2];
    static int history_table[NB_HOLES][4];

    // --- 4. ÉVALUATION V8 AVEC PARAMÈTRES CHAMPIONS ---
    inline int evaluate(const GameState& state, int maximizing_player_id) {
        if (state.score_p1 >= 49) return (maximizing_player_id == 1) ? INF : -INF;
        if (state.score_p2 >= 49) return (maximizing_player_id == 2) ? INF : -INF;

        int score_diff = (maximizing_player_id == 1) ? (state.score_p1 - state.score_p2) : (state.score_p2 - state.score_p1);
        int eval = score_diff * champion_dna.w_score;

        int my_mobility = 0;
        int opp_mobility = 0;
        int my_seeds_on_board = 0;
        int opp_seeds_on_board = 0;

        for (int i = 0; i < NB_HOLES; ++i) {
            int r = state.get_seeds(i, RED);
            int b = state.get_seeds(i, BLUE);
            int t = state.get_seeds(i, TRANSPARENT);
            int total = r + b + t;

            bool is_mine = GameRules::is_current_player_hole(i, maximizing_player_id);

            int material = (r * champion_dna.w_seed_red) + (b * champion_dna.w_seed_blue) + (t * champion_dna.w_seed_trans);
            int positional = total * champion_dna.w_holes[i];

            if (is_mine) {
                eval += material + positional;
                my_seeds_on_board += total;
                if (total > 0) my_mobility++;
                if (total == 1 || total == 2) eval -= champion_dna.w_defense; // Note: Df est négatif dans l'ADN
                if (total > 12) eval -= champion_dna.w_hoard_penalty * (total - 12);
            }
            else {
                eval -= material;
                opp_seeds_on_board += total;
                if (total > 0) opp_mobility++;
                if (total == 1 || total == 2) eval += champion_dna.w_attack;
            }
        }

        eval += (my_mobility - opp_mobility) * champion_dna.w_mobility;
        if (opp_seeds_on_board < 10) eval += champion_dna.w_starvation * (10 - opp_seeds_on_board);

        return eval;
    }

    // --- 5. LOGIQUE DE TRI ---
    inline int score_move(const GameState& state, const Move& move, int depth, const Move& tt_move) {
        if (move.hole == tt_move.hole && move.type == tt_move.type) return 2000000;
        if (move.hole == killer_moves[depth][0].hole && move.type == killer_moves[depth][0].type) return 1000000;
        if (move.hole == killer_moves[depth][1].hole && move.type == killer_moves[depth][1].type) return 900000;

        int history = history_table[move.hole][(int)move.type];
        return history;
    }

    inline std::vector<Move> generate_moves(const GameState& state, int player_id) {
        std::vector<Move> moves;
        moves.reserve(16);
        for (int i = 0; i < NB_HOLES; ++i) {
            if (!GameRules::is_current_player_hole(i, player_id)) continue;
            int r = state.get_seeds(i, RED);
            int b = state.get_seeds(i, BLUE);
            int t = state.get_seeds(i, TRANSPARENT);
            if (r == 0 && b == 0 && t == 0) continue;
            if (r > 0) moves.emplace_back(i, MoveType::RED);
            if (b > 0) moves.emplace_back(i, MoveType::BLUE);
            if (t > 0) {
                moves.emplace_back(i, MoveType::TRANS_AS_RED);
                moves.emplace_back(i, MoveType::TRANS_AS_BLUE);
            }
        }
        return moves;
    }

    // --- 6. PVS SEARCH ---
    int alpha_beta_pvs(GameState state, int depth, int alpha, int beta, int player_id, int maximizing_player_id) {
        stats.nodes++;

        if ((stats.nodes & 2047) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double, std::milli>(now - start_time).count() >= time_limit_ms) {
                time_out = true; return 0;
            }
        }

        int alpha_orig = alpha;
        uint64_t hash = Zobrist::compute(state, player_id);
        TTEntry& entry = transposition_table[hash % TT_SIZE];
        Move tt_move;

        if (entry.key == hash) {
            tt_move = entry.best_move;
            if (entry.depth >= depth) {
                if (entry.flag == TTFlag::EXACT) return entry.score;
                if (entry.flag == TTFlag::LOWERBOUND) alpha = std::max(alpha, entry.score);
                else if (entry.flag == TTFlag::UPPERBOUND) beta = std::min(beta, entry.score);
                if (alpha >= beta) return entry.score;
            }
        }

        if (depth == 0 || state.score_p1 >= 49 || state.score_p2 >= 49) {
            return evaluate(state, maximizing_player_id);
        }

        std::vector<Move> moves = generate_moves(state, player_id);
        if (moves.empty()) return evaluate(state, maximizing_player_id);

        struct ScoredMove { Move m; int s; bool operator>(const ScoredMove& o) const { return s > o.s; } };
        std::vector<ScoredMove> scored_moves;
        for (const auto& m : moves) scored_moves.push_back({ m, score_move(state, m, depth, tt_move) });
        std::sort(scored_moves.begin(), scored_moves.end(), std::greater<ScoredMove>());

        Move best_move_this_node;
        int best_val = (player_id == maximizing_player_id) ? -INF : INF;
        int next_player = (player_id == 1) ? 2 : 1;

        for (size_t i = 0; i < scored_moves.size(); ++i) {
            GameState next_state = state;
            GameRules::apply_move(next_state, scored_moves[i].m, player_id);

            int val;
            if (i == 0) {
                val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
            }
            else {
                if (player_id == maximizing_player_id) {
                    val = alpha_beta_pvs(next_state, depth - 1, alpha, alpha + 1, next_player, maximizing_player_id);
                    if (val > alpha && val < beta)
                        val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                }
                else {
                    val = alpha_beta_pvs(next_state, depth - 1, beta - 1, beta, next_player, maximizing_player_id);
                    if (val < beta && val > alpha)
                        val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                }
            }

            if (time_out) return 0;

            if (player_id == maximizing_player_id) {
                if (val > best_val) { best_val = val; best_move_this_node = scored_moves[i].m; }
                alpha = std::max(alpha, best_val);
            }
            else {
                if (val < best_val) { best_val = val; best_move_this_node = scored_moves[i].m; }
                beta = std::min(beta, best_val);
            }

            if (alpha >= beta) {
                if (scored_moves[i].m.hole != killer_moves[depth][0].hole) {
                    killer_moves[depth][1] = killer_moves[depth][0];
                    killer_moves[depth][0] = scored_moves[i].m;
                }
                history_table[scored_moves[i].m.hole][(int)scored_moves[i].m.type] += depth * depth;
                break;
            }
        }

        if (!time_out) {
            entry.key = hash;
            entry.score = best_val;
            entry.depth = depth;
            entry.best_move = best_move_this_node;
            if (best_val <= alpha_orig) entry.flag = TTFlag::UPPERBOUND;
            else if (best_val >= beta) entry.flag = TTFlag::LOWERBOUND;
            else entry.flag = TTFlag::EXACT;
        }

        return best_val;
    }

    // --- 7. INTERFACE ---
    inline Move find_best_move(const GameState& root_state, int player_id, double time_limit_sec) {
        Zobrist::init();
        stats.reset();
        std::memset(history_table, 0, sizeof(history_table));
        std::memset(killer_moves, 0, sizeof(killer_moves));

        start_time = std::chrono::high_resolution_clock::now();
        time_limit_ms = (time_limit_sec * 1000.0) - 40.0;
        time_out = false;

        std::vector<Move> moves = generate_moves(root_state, player_id);
        if (moves.empty()) return Move();

        Move best_move_found = moves[0];

        for (int depth = 1; depth <= MAX_DEPTH; ++depth) {
            alpha_beta_pvs(root_state, depth, -INF, INF, player_id, player_id);
            if (time_out) break;

            uint64_t root_hash = Zobrist::compute(root_state, player_id);
            TTEntry& entry = transposition_table[root_hash % TT_SIZE];
            if (entry.key == root_hash) {
                best_move_found = entry.best_move;
            }
            stats.max_depth = depth;
        }

        return best_move_found;
    }
};