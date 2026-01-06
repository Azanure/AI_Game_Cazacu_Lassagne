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

namespace AI_V6 {

    // --- CONFIGURATION ---
    static std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    static double time_limit_ms;
    static bool time_out;
    static SearchStats stats;

    constexpr int INF = 1000000; // Augmenté car on a des scores * 1000
    constexpr int MAX_DEPTH = 64;

    // --- 1. ZOBRIST HASHING (Niveau 1 : Vitesse) ---
    // Table de nombres aléatoires pour calculer le hash instantanément
    namespace Zobrist {
        static uint64_t table[NB_HOLES][3][64]; // [Trou][Couleur][NbGraines]
        static uint64_t turn_hash[2];           // Pour différencier le tour J1/J2
        static bool initialized = false;

        inline void init() {
            if (initialized) return;
            std::mt19937_64 rng(12345); // Seed fixe pour être déterministe
            for (int i = 0; i < NB_HOLES; ++i) {
                for (int c = 0; c < 3; ++c) {
                    for (int n = 0; n < 64; ++n) {
                        table[i][c][n] = rng();
                    }
                }
            }
            turn_hash[0] = rng();
            turn_hash[1] = rng();
            initialized = true;
        }

        // Calcul rapide (O(N) mais XOR only, pas de multiplication lourde)
        // Idéalement, ceci devrait être incrémental dans apply_move, mais ici on le recalcul vite.
        inline uint64_t compute(const GameState& state, int player_id) {
            uint64_t h = 0;
            for (int i = 0; i < NB_HOLES; ++i) {
                // On cap à 63 graines pour éviter overflow tableau
                // On force la conversion en (int) pour satisfaire std::min
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
        uint64_t key;
        int score;
        int depth;
        TTFlag flag;
        Move best_move;
    };

    constexpr size_t TT_SIZE = 2097152; // 2M entrées (~48 Mo RAM)
    static std::vector<TTEntry> transposition_table(TT_SIZE);

    // --- 3. KILLER MOVES (Niveau 2 : Heuristique) ---
    // Stocke 2 coups qui ont provoqué un cutoff à cette profondeur
    static Move killer_moves[MAX_DEPTH][2];

    // Historique (Butterfly Heuristic)
    static int history_table[NB_HOLES][4];

    // --- 4. NOUVELLE EVALUATION (Intelligente) ---
    // --- 4. EVALUATION STRATÉGIQUE ---
    inline int evaluate(const GameState& state, int maximizing_player_id) {
        int score_p1 = state.score_p1;
        int score_p2 = state.score_p2;

        // --- A. VICTOIRE / DÉFAITE IMMÉDIATE ---
        // Si on a gagné, on renvoie l'infini. C'est la priorité absolue.
        if (score_p1 >= 49) return (maximizing_player_id == 1) ? INF : -INF;
        if (score_p2 >= 49) return (maximizing_player_id == 2) ? INF : -INF;

        // --- B. SCORE MATÉRIEL (Le "cash") ---
        // C'est la base. 1000 points par graine d'avance.
        int my_real_score = (maximizing_player_id == 1) ? score_p1 : score_p2;
        int opp_real_score = (maximizing_player_id == 1) ? score_p2 : score_p1;

        int eval = (my_real_score - opp_real_score) * 1000;

        // --- C. ANALYSE TACTIQUE (Le "potentiel") ---
        // On va scanner le plateau pour voir les opportunités et les dangers

        int my_threats = 0;       // Occasions d'attaquer l'adversaire
        int my_vulnerabilities = 0; // Mes trous en danger
        int seeds_in_my_camp = 0;   // Pour évaluer le "Starving"

        for (int i = 0; i < NB_HOLES; ++i) {
            int seeds = state.count_total_seeds(i);
            bool is_my_hole = GameRules::is_current_player_hole(i, maximizing_player_id);

            if (is_my_hole) {
                seeds_in_my_camp += seeds;

                // DANGER DÉFENSIF
                // Si j'ai un trou avec 1 ou 2 graines, l'adversaire peut tomber dessus et me voler !
                // C'est très grave. On pénalise.
                if (seeds == 1 || seeds == 2) {
                    // Malus plus fort si c'est 2 graines (capture immédiate possible)
                    my_vulnerabilities += (seeds == 2) ? 80 : 40;
                }
            }
            else { // Trou de l'adversaire
                // OPPORTUNITÉ OFFENSIVE
                // Si l'adversaire a 1 ou 2 graines, je peux potentiellement capturer.
                // C'est très bon pour moi.
                if (seeds == 1 || seeds == 2) {
                    // Bonus plus fort si 2 graines (car une seule graine suffit pour capturer)
                    my_threats += (seeds == 2) ? 100 : 50;
                }
            }
        }

        // --- D. TOTAL ET AJUSTEMENTS ---

        // 1. On intègre les menaces
        eval += my_threats;
        eval -= my_vulnerabilities;

        // 2. Gestion du "Starving" (Affamement)
        // Si je n'ai plus de graines, je perds tout le reste. C'est la catastrophe.
        // Si l'adversaire n'a plus de graines, je gagne tout. C'est le jackpot.
        // (Note: La règle dit que celui qui n'a plus de graines perd les siennes restantes, 
        // ou alors le jeu s'arrête. On simplifie ici : avoir des graines = survie).
        if (seeds_in_my_camp == 0) {
            eval -= 5000; // Quasi-défaite
        }

        return eval;
    }

    // --- 5. TRI DES COUPS (Move Ordering Complet) ---
    struct ScoredMove {
        Move move;
        int score;
        bool operator>(const ScoredMove& other) const { return score > other.score; }
    };

    inline int score_move(const GameState& state, const Move& move, int depth, const Move& tt_move) {
        // 1. TT Move (Le ROI)
        if (move.hole == tt_move.hole && move.type == tt_move.type) return 2000000;

        // 2. Captures (Simple simulation locale) - On favorise l'agressivité
        // Note: Ici on pourrait remettre la logique "si trou adversaire a 1 ou 2 graines" 
        // pour booster les captures, mais c'est lourd. On fait confiance à l'eval pour ça.

        // 3. Killer Moves
        if (move.hole == killer_moves[depth][0].hole && move.type == killer_moves[depth][0].type) return 1000000;
        if (move.hole == killer_moves[depth][1].hole && move.type == killer_moves[depth][1].type) return 900000;

        // 4. History Heuristic / Quantité de graines
        int seeds = 0;
        if (move.type == MoveType::RED) seeds = state.get_seeds(move.hole, RED);
        else if (move.type == MoveType::BLUE) seeds = state.get_seeds(move.hole, BLUE);
        else seeds = state.get_seeds(move.hole, TRANSPARENT);

        // On combine le nombre de graines (immédiat) et l'histoire (long terme)
        return seeds + history_table[move.hole][(int)move.type];
    }

    inline std::vector<Move> generate_moves(const GameState& state, int player_id) {
        std::vector<Move> moves;
        moves.reserve(16);
        for (int i = 0; i < NB_HOLES; ++i) {
            if (!GameRules::is_current_player_hole(i, player_id)) continue;
            int r = state.get_seeds(i, RED); int b = state.get_seeds(i, BLUE); int t = state.get_seeds(i, TRANSPARENT);
            if (r == 0 && b == 0 && t == 0) continue;
            if (r > 0) moves.emplace_back(i, MoveType::RED);
            if (b > 0) moves.emplace_back(i, MoveType::BLUE);
            if (t > 0) { moves.emplace_back(i, MoveType::TRANS_AS_RED); moves.emplace_back(i, MoveType::TRANS_AS_BLUE); }
        }
        return moves;
    }

    // --- 6. PVS (PRINCIPAL VARIATION SEARCH) - Niveau 3 ---
    int alpha_beta_pvs(GameState state, int depth, int alpha, int beta, int player_id, int maximizing_player_id) {
        stats.nodes++;

        // Check Time (tous les 2048 noeuds)
        if ((stats.nodes & 2047) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double, std::milli>(now - start_time).count() >= time_limit_ms) {
                time_out = true; return 0;
            }
        }

        int alpha_orig = alpha;
        uint64_t hash = Zobrist::compute(state, player_id);
        int tt_index = hash % TT_SIZE;
        TTEntry& entry = transposition_table[tt_index];
        Move tt_move;

        // TT Probe
        if (entry.key == hash) {
            tt_move = entry.best_move;
            if (entry.depth >= depth) {
                if (entry.flag == TTFlag::EXACT) return entry.score;
                if (entry.flag == TTFlag::LOWERBOUND) alpha = std::max(alpha, entry.score);
                else if (entry.flag == TTFlag::UPPERBOUND) beta = std::min(beta, entry.score);
                if (alpha >= beta) { stats.cutoffs++; return entry.score; }
            }
        }

        if (depth == 0 || state.score_p1 >= 49 || state.score_p2 >= 49 || state.moves_count >= 400) {
            return evaluate(state, maximizing_player_id);
        }

        std::vector<Move> moves = generate_moves(state, player_id);
        if (moves.empty()) return evaluate(state, maximizing_player_id);

        // Sorting
        std::vector<ScoredMove> scored_moves;
        scored_moves.reserve(moves.size());
        for (const auto& m : moves) {
            scored_moves.push_back({ m, score_move(state, m, depth, tt_move) });
        }
        std::sort(scored_moves.begin(), scored_moves.end(), std::greater<ScoredMove>());

        Move best_move_this_node;
        int best_val = (player_id == maximizing_player_id) ? -INF : INF;
        int next_player = (player_id == 1) ? 2 : 1;

        // --- BOUCLE PVS ---
        for (size_t i = 0; i < scored_moves.size(); ++i) {
            Move move = scored_moves[i].move;
            GameState next_state = state;
            GameRules::apply_move(next_state, move, player_id);

            int val;

            if (i == 0) {
                // 1. Premier coup (PV) : Recherche fenêtre complète
                val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
            }
            else {
                // 2. Autres coups : Recherche fenêtre nulle (Null Window Search)
                // On suppose qu'ils ne vont pas améliorer le score (val <= alpha pour Max, val >= beta pour Min)
                if (player_id == maximizing_player_id) {
                    val = alpha_beta_pvs(next_state, depth - 1, alpha, alpha + 1, next_player, maximizing_player_id);
                    if (val > alpha && val < beta) { // Fail-High : On s'est trompé, il faut re-chercher à fond
                        val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                    }
                }
                else {
                    val = alpha_beta_pvs(next_state, depth - 1, beta - 1, beta, next_player, maximizing_player_id);
                    if (val < beta && val > alpha) { // Fail-Low
                        val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                    }
                }
            }

            if (time_out) return 0;

            if (player_id == maximizing_player_id) {
                if (val > best_val) { best_val = val; best_move_this_node = move; }
                alpha = std::max(alpha, best_val);
                if (beta <= alpha) {
                    stats.cutoffs++;
                    // Update Killer & History
                    if (move.hole != killer_moves[depth][0].hole || move.type != killer_moves[depth][0].type) {
                        killer_moves[depth][1] = killer_moves[depth][0];
                        killer_moves[depth][0] = move;
                    }
                    history_table[move.hole][(int)move.type] += depth * depth;
                    break;
                }
            }
            else {
                if (val < best_val) { best_val = val; best_move_this_node = move; }
                beta = std::min(beta, best_val);
                if (beta <= alpha) {
                    stats.cutoffs++;
                    // Update Killer & History
                    if (move.hole != killer_moves[depth][0].hole || move.type != killer_moves[depth][0].type) {
                        killer_moves[depth][1] = killer_moves[depth][0];
                        killer_moves[depth][0] = move;
                    }
                    history_table[move.hole][(int)move.type] += depth * depth;
                    break;
                }
            }
        }

        if (!time_out) {
            TTEntry new_entry;
            new_entry.key = hash;
            new_entry.score = best_val;
            new_entry.depth = depth;
            new_entry.best_move = best_move_this_node;

            if (best_val <= alpha_orig) new_entry.flag = TTFlag::UPPERBOUND;
            else if (best_val >= beta) new_entry.flag = TTFlag::LOWERBOUND;
            else new_entry.flag = TTFlag::EXACT;

            // Remplacement : Profondeur plus grande OU collision d'une vieille entrée
            transposition_table[tt_index] = new_entry;
        }

        return best_val;
    }

    // --- 7. INTERFACE ---
    inline Move find_best_move(const GameState& root_state, int player_id, double time_limit_sec) {
        Zobrist::init(); // S'assurer que c'est init
        stats.reset();
        std::memset(history_table, 0, sizeof(history_table));
        std::memset(killer_moves, 0, sizeof(killer_moves)); // Reset killers

        start_time = std::chrono::high_resolution_clock::now();
        time_limit_ms = (time_limit_sec * 1000.0) - 50.0;
        time_out = false;

        std::vector<Move> moves = generate_moves(root_state, player_id);
        if (moves.empty()) return Move();

        Move best_move_found = moves[0];

        // Iterative Deepening
        for (int depth = 1; depth <= MAX_DEPTH; ++depth) {
            int alpha = -INF;
            int beta = INF;
            int next_player = (player_id == 1) ? 2 : 1;

            // Pour la racine, on appelle directement le PVS ou une version simplifiée
            // Ici on appelle notre PVS
            int score = alpha_beta_pvs(root_state, depth, alpha, beta, player_id, player_id);

            if (time_out) break;

            // Extraction du meilleur coup depuis la TT (plus fiable que le retour de fonction)
            uint64_t root_hash = Zobrist::compute(root_state, player_id);
            TTEntry& entry = transposition_table[root_hash % TT_SIZE];
            if (entry.key == root_hash && entry.flag == TTFlag::EXACT) {
                best_move_found = entry.best_move;
            }
            else {
                // Fallback si la TT n'a pas save (rare) ou collision
                // On pourrait garder le best_move_found de la profondeur précédente
            }

            stats.max_depth = depth;
        }

        // Log final
        auto end_time = std::chrono::high_resolution_clock::now();
        stats.time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        if (stats.time_ms > 0) stats.nps = (long long)(stats.nodes * 1000.0 / stats.time_ms);

        std::cout << "[AI V6 TURBO] D:" << stats.max_depth
            << " NPS:" << stats.nps
            << " Cutoffs:" << stats.cutoffs << std::endl;

        return best_move_found;
    }
};