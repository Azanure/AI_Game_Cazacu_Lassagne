#pragma once

#include "common/GameRules.hpp"
#include "common/GameState.hpp"
#include "common/Move.hpp"
#include "common/SearchStats.hpp"
#include <vector>
#include <algorithm>
#include <chrono>

namespace AI_V3 {

    static std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    static double time_limit_ms;
    static bool time_out;
    static SearchStats stats;

    constexpr int INF = 10000;

    // --- HELPER : Structure pour trier les coups ---
    struct ScoredMove {
        Move move;
        int score; // Score estimé du coup (heuristique légère)

        // Pour trier du plus grand au plus petit score
        bool operator>(const ScoredMove& other) const {
            return score > other.score;
        }
    };

    // --- 1. GÉNÉRATEUR DE COUPS (Identique v1) ---
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

    // --- 2. EVALUATION (Identique v1 pour l'instant) ---
    inline int evaluate(const GameState& state, int maximizing_player_id) {
        int score_me = (maximizing_player_id == 1) ? state.score_p1 : state.score_p2;
        int score_opp = (maximizing_player_id == 1) ? state.score_p2 : state.score_p1;
        return (score_me - score_opp) * 100;
    }

    // --- 3. HEURISTIQUE DE TRI (NOUVEAU) ---
    // Attribue un score rapide à un coup pour savoir s'il est prometteur
    inline int score_move_for_ordering(const GameState& state, const Move& move, int player_id) {

        // 1. Récupérer le nombre de graines qu'on va semer
        // On doit regarder le bon type de graine selon le coup
        int seeds = 0;
        if (move.type == MoveType::RED) seeds = state.get_seeds(move.hole, RED);
        else if (move.type == MoveType::BLUE) seeds = state.get_seeds(move.hole, BLUE);
        else {
            // Cas transparent : on prend tout ce qui est transparent
            seeds = state.get_seeds(move.hole, TRANSPARENT);
        }

        if (seeds == 0) return -1000; // Sécurité

        // 2. Calculer l'indice d'arrivée théorique
        // Formule simple : départ + nombre de graines
        // (Note : cette formule ignore le saut du trou de départ si > 12 graines, 
        // mais c'est une approximation suffisante pour le tri)
        int final_hole = (move.hole + seeds) % NB_HOLES; // Assurez-vous que NB_HOLES est accessible (ex: 12)

        // 3. Score de base : nombre de graines jouées
        // Jouer beaucoup de graines est souvent dynamique
        int heuristic_score = seeds;

        // 4. Prédiction de capture (Le cœur de l'optimisation)
        // Est-ce que le trou d'arrivée est chez l'adversaire ?
        bool is_opponent_hole = !GameRules::is_current_player_hole(final_hole, player_id);

        if (is_opponent_hole) {
            // Combien y a-t-il de graines AVANT que la mienne n'arrive ?
            int existing_seeds = state.get_seeds(final_hole, RED)
                + state.get_seeds(final_hole, BLUE)
                + state.get_seeds(final_hole, TRANSPARENT);

            // Dans l'Awélé standard, on capture si le total devient 2 ou 3.
            // Donc si actuellement il y a 1 ou 2 graines, ma graine fera 2 ou 3.
            if (existing_seeds == 1 || existing_seeds == 2) {
                // BONUS ÉNORME : C'est probablement une capture !
                // On ajoute 100 points par graine capturée (estimation)
                heuristic_score += (existing_seeds + 1) * 100;
            }
        }

        return heuristic_score;
    }

    // --- 4. ALPHA-BETA AVEC MOVE ORDERING ---
    int alpha_beta(GameState state, int depth, int alpha, int beta, int player_id, int maximizing_player_id) {
        stats.nodes++;

        if ((stats.nodes & 2047) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double, std::milli>(now - start_time).count() >= time_limit_ms) {
                time_out = true; return 0;
            }
        }

        if (depth == 0 || state.score_p1 >= 49 || state.score_p2 >= 49 || state.moves_count >= 400) {
            return evaluate(state, maximizing_player_id);
        }

        std::vector<Move> moves = generate_moves(state, player_id);
        if (moves.empty()) return evaluate(state, maximizing_player_id);

        // --- OPTIMISATION : MOVE ORDERING ---
        // On ne trie pas à la profondeur 0 (les feuilles), inutile.
        // On trie surtout près de la racine (depth élevé).
        if (depth > 1) {
            std::vector<ScoredMove> scored_moves;
            scored_moves.reserve(moves.size());

            for (const auto& m : moves) {
                int s = score_move_for_ordering(state, m, player_id);
                scored_moves.push_back({ m, s });
            }

            // Tri décroissant : les meilleurs coups d'abord
            std::sort(scored_moves.begin(), scored_moves.end(), std::greater<ScoredMove>());

            // On remet dans le vecteur original
            for (size_t i = 0; i < moves.size(); ++i) {
                moves[i] = scored_moves[i].move;
            }
        }
        // ------------------------------------

        int next_player = (player_id == 1) ? 2 : 1;

        if (player_id == maximizing_player_id) {
            int max_eval = -INF;
            for (const auto& move : moves) {
                GameState next_state = state;
                GameRules::apply_move(next_state, move, player_id);
                int eval = alpha_beta(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                if (time_out) return 0;

                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);
                if (beta <= alpha) { stats.cutoffs++; break; }
            }
            return max_eval;
        }
        else {
            int min_eval = INF;
            for (const auto& move : moves) {
                GameState next_state = state;
                GameRules::apply_move(next_state, move, player_id);
                int eval = alpha_beta(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                if (time_out) return 0;

                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
                if (beta <= alpha) { stats.cutoffs++; break; }
            }
            return min_eval;
        }
    }

    // --- 5. INTERFACE (Similaire v1) ---
    inline Move find_best_move(const GameState& root_state, int player_id, double time_limit_sec) {
        stats.reset();
        start_time = std::chrono::high_resolution_clock::now();
        time_limit_ms = (time_limit_sec * 1000.0) - 50.0;
        time_out = false;

        std::vector<Move> moves = generate_moves(root_state, player_id);
        if (moves.empty()) return Move();

        Move best_move_found = moves[0];

        // --- MOVE ORDERING A LA RACINE ---
        // Très important : on trie aussi la racine pour commencer par le meilleur coup supposé
        std::vector<ScoredMove> root_scored_moves;
        for (const auto& m : moves) {
            root_scored_moves.push_back({ m, score_move_for_ordering(root_state, m, player_id) });
        }
        std::sort(root_scored_moves.begin(), root_scored_moves.end(), std::greater<ScoredMove>());
        for (size_t i = 0; i < moves.size(); ++i) moves[i] = root_scored_moves[i].move;
        // --------------------------------

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

                if (time_out) { this_depth_completed = false; break; }

                if (score > best_score_this_depth) {
                    best_score_this_depth = score;
                    best_move_this_depth = move;
                }
                alpha = std::max(alpha, score);
            }

            if (this_depth_completed) {
                best_move_found = best_move_this_depth;
                stats.max_depth = depth;
                // Petite astuce : Pour la profondeur suivante, on met le meilleur coup trouvé 
                // tout au début de la liste 'moves'. Comme ça il sera testé en premier.
                // Cela s'appelle "History Heuristic" basique.
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (moves[i].hole == best_move_found.hole && moves[i].type == best_move_found.type) {
                        std::swap(moves[0], moves[i]);
                        break;
                    }
                }
            }
            else {
                break;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        stats.time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        if (stats.time_ms > 0) stats.nps = (long long)(stats.nodes * 1000.0 / stats.time_ms);

        std::cout << "[AI v3] D:" << stats.max_depth
            << " Nodes:" << stats.nodes
            << " Cutoffs:" << stats.cutoffs
            << " NPS:" << stats.nps
            << " Time:" << std::fixed << std::setprecision(0) << stats.time_ms << "ms"
            << std::endl;

        return best_move_found;
    }
};