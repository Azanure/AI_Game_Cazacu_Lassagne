#pragma once

#include "common/GameRules.hpp"
#include "common/GameState.hpp"
#include "common/Move.hpp"
#include "common/SearchStats.hpp" // <--- NOUVEAU : On inclut la structure commune

#include <vector>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip> // Pour l'affichage formaté

namespace AI_V1 {

    // --- VARIABLES GLOBALES INTERNES ---
    // Ces variables sont accessibles par le main via les lambdas, mais isolées dans ce namespace
    static std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    static double time_limit_ms;
    static bool time_out;

    // Instance des stats pour cette IA. 
    // Le main viendra lire cette variable après chaque coup via l'adaptateur.
    static SearchStats stats;

    constexpr int INF = 10000;

    // --- 1. GÉNÉRATEUR DE COUPS ---
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

    // --- 2. EVALUATION ---
    inline int evaluate(const GameState& state, int maximizing_player_id) {
        int score_me = (maximizing_player_id == 1) ? state.score_p1 : state.score_p2;
        int score_opp = (maximizing_player_id == 1) ? state.score_p2 : state.score_p1;

        // Simple différence de score
        return (score_me - score_opp) * 100;
    }

    // --- 3. ALPHA-BETA INSTRUMENTÉ ---
    int alpha_beta(GameState state, int depth, int alpha, int beta, int player_id, int maximizing_player_id) {

        // [METRIQUE] On compte chaque nœud visité
        stats.nodes++;

        // A. Vérification du temps 
        // On utilise un masque binaire (2047 = 0x7FF) pour vérifier tous les 2048 nœuds
        // C'est beaucoup plus rapide que le modulo (%)
        if ((stats.nodes & 2047) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = now - start_time;
            if (elapsed.count() >= time_limit_ms) {
                time_out = true;
                return 0; // Valeur bidon, on remontera via le drapeau time_out
            }
        }

        // B. Conditions d'arrêt (Feuille ou Fin de jeu)
        if (depth == 0 || state.score_p1 >= 49 || state.score_p2 >= 49 || state.moves_count >= 400) {
            return evaluate(state, maximizing_player_id);
        }

        std::vector<Move> moves = generate_moves(state, player_id);

        // Si aucun coup possible
        if (moves.empty()) {
            return evaluate(state, maximizing_player_id);
        }

        int next_player = (player_id == 1) ? 2 : 1;

        if (player_id == maximizing_player_id) {
            // Nœud MAX
            int max_eval = -INF;
            for (const auto& move : moves) {
                GameState next_state = state;
                GameRules::apply_move(next_state, move, player_id);

                int eval = alpha_beta(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);

                if (time_out) return 0; // Remontée d'urgence

                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);

                // [METRIQUE] Coupure Beta
                if (beta <= alpha) {
                    stats.cutoffs++;
                    break;
                }
            }
            return max_eval;
        }
        else {
            // Nœud MIN
            int min_eval = INF;
            for (const auto& move : moves) {
                GameState next_state = state;
                GameRules::apply_move(next_state, move, player_id);

                int eval = alpha_beta(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);

                if (time_out) return 0;

                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);

                // [METRIQUE] Coupure Alpha
                if (beta <= alpha) {
                    stats.cutoffs++;
                    break;
                }
            }
            return min_eval;
        }
    }

    // --- 4. INTERFACE PRINCIPALE ---
    inline Move find_best_move(const GameState& root_state, int player_id, double time_limit_sec) {

        // 1. Initialisation des métriques pour ce tour
        stats.reset();
        start_time = std::chrono::high_resolution_clock::now();
        time_limit_ms = (time_limit_sec * 1000.0) - 50.0; // Marge de sécurité de 50ms
        time_out = false;

        std::vector<Move> moves = generate_moves(root_state, player_id);
        if (moves.empty()) return Move(); // Should not happen if checked before call

        Move best_move_found = moves[0];

        // 2. ITERATIVE DEEPENING (Approfondissement progressif)
        for (int depth = 1; depth <= 64; ++depth) {

            int best_score_this_depth = -INF;
            Move best_move_this_depth = moves[0];
            bool this_depth_completed = true;

            int alpha = -INF;
            int beta = INF;
            int next_player = (player_id == 1) ? 2 : 1;

            for (const auto& move : moves) {
                GameState next_state = root_state;
                GameRules::apply_move(next_state, move, player_id);

                int score = alpha_beta(next_state, depth - 1, alpha, beta, next_player, player_id);

                if (time_out) {
                    this_depth_completed = false;
                    break;
                }

                if (score > best_score_this_depth) {
                    best_score_this_depth = score;
                    best_move_this_depth = move;
                }
                alpha = std::max(alpha, score);
            }

            if (this_depth_completed) {
                best_move_found = best_move_this_depth;
                stats.max_depth = depth; // On met à jour la profondeur atteinte validée
            }
            else {
                break; // Temps écoulé, on garde le résultat de la profondeur précédente
            }
        }

        // 3. Finalisation des calculs de stats
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> total_elapsed = end_time - start_time;
        stats.time_ms = total_elapsed.count();

        if (stats.time_ms > 0) {
            stats.nps = (long long)(stats.nodes * 1000.0 / stats.time_ms);
        }

        // 4. Log console (Optionnel : utile pour voir l'IA réfléchir en direct)
        std::cout << "[AI v1] D:" << stats.max_depth
            << " Nodes:" << stats.nodes
            << " Cutoffs:" << stats.cutoffs
            << " NPS:" << stats.nps
            << " Time:" << std::fixed << std::setprecision(0) << stats.time_ms << "ms"
            << std::endl;

        return best_move_found;
    }
};