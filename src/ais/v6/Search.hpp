#pragma once

#include "../../common/GameRules.hpp"
#include "../../common/GameState.hpp"
#include "../../common/Move.hpp"
#include "../../common/SearchStats.hpp"
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdint> // Pour uint64_t

namespace AI_V6 {

    // --- CONFIGURATION ---
    static std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    static double time_limit_ms;
    static bool time_out;
    static SearchStats stats;

    constexpr int INF = 10000;

    // --- 1. TABLE DE TRANSPOSITION (TT) ---

    // Types de résultats stockés
    enum class TTFlag : uint8_t {
        EXACT,      // Score exact
        LOWERBOUND, // Coupe Beta (le score est au moins égal à ça)
        UPPERBOUND  // Coupe Alpha (le score est au plus égal à ça)
    };

    struct TTEntry {
        uint64_t key;   // Hash unique du plateau (pour vérifier que c'est le bon)
        int score;      // Le score mémorisé
        int depth;      // La profondeur à laquelle on l'a calculé
        TTFlag flag;    // Le type de score
        Move best_move; // Le meilleur coup à jouer ici ! (C'est LUI qui sert au tri)
    };

    // Taille de la table : Doit être une puissance de 2. 
    // 1048576 (2^20) entrées * ~24 octets = ~25 Mo de RAM. Très léger.
    constexpr size_t TT_SIZE = 1048576;
    static std::vector<TTEntry> transposition_table(TT_SIZE);

    // Historique (Garde la V4 en fallback)
    static int history_table[NB_HOLES][4];

    // --- HASHING (FNV-1a Hash) ---
    // Fonction rapide pour générer un ID unique de 64 bits pour le plateau
    inline uint64_t compute_hash(const GameState& state) {
        uint64_t hash = 14695981039346656037ULL;
        // On hache le tableau de graines
        for (int i = 0; i < TOTAL_CELLS; ++i) {
            hash ^= state.board[i];
            hash *= 1099511628211ULL;
        }
        // On pourrait hacher les scores, mais ce n'est pas strictement nécessaire pour le choix du coup
        // On ajoute le joueur courant si besoin, mais ici l'AlphaBeta gère l'alternance.
        return hash;
    }

    // --- 2. EVALUATION "OPPORTUNISTE CALIBREE" (V6.3) ---
    // --- 2. EVALUATION "CHIRURGICALE" (V7) ---
    inline int evaluate(const GameState& state, int maximizing_player_id) {

        // 1. LE SCORE REEL (Le Roi)
        // On garde 5000. Une graine capturée vaut 5000 points.
        // Rien ne doit pouvoir compenser la perte d'une graine.
        int score_me = (maximizing_player_id == 1) ? state.score_p1 : state.score_p2;
        int score_opp = (maximizing_player_id == 1) ? state.score_p2 : state.score_p1;

        int evaluation = (score_me - score_opp) * 5000;

        // 2. ANALYSE DE POSITION (Tie-Breakers)
        // On utilise des valeurs TRÈS FAIBLES.
        // Le but est juste de dire : "À score égal, je préfère cette situation".

        for (int i = 0; i < NB_HOLES; ++i) {
            int seeds = state.count_total_seeds(i);
            if (seeds == 0) continue;

            bool is_my_hole = GameRules::is_current_player_hole(i, maximizing_player_id);

            if (is_my_hole) {
                // --- CHEZ MOI ---

                // A. KRU (Accumulation)
                // Bonus très léger (+5 par graine au-delà de 12).
                // Max bonus possible ~50 pts (1/100ème d'une capture).
                if (seeds > 12) {
                    evaluation += (seeds - 12) * 5;
                }

                // B. MUNITIONS (Bleus/Trans)
                // Avoir des munitions offensives est un léger plus.
                int blues = state.get_seeds(i, BLUE);
                int trans = state.get_seeds(i, TRANSPARENT);
                evaluation += (blues + trans) * 2;

            }
            else {
                // --- CHEZ L'ADVERSAIRE ---

                // C. CIBLES POTENTIELLES
                // Au lieu de +150, on met +10.
                // C'est juste une "indication", pas une promesse de capture.
                if (seeds == 1 || seeds == 2) {
                    evaluation += 10;
                }

                // D. GÊNE
                // Si l'adversaire a un très gros trou, petit malus
                if (seeds > 15) {
                    evaluation -= 20;
                }
            }
        }

        // PLUS DE MODE SURVIE ! 
        // L'évaluation doit rester linéaire et stable.

        return evaluation;
    }

    // --- 3. HEURISTIQUE DE TRI (V4 + TT) ---
    struct ScoredMove {
        Move move;
        int score;
        bool operator>(const ScoredMove& other) const { return score > other.score; }
    };

    inline int score_move_for_ordering(const GameState& state, const Move& move, int player_id, const Move& tt_move) {
        // 1. PRIORITÉ ABSOLUE : Si c'est le coup de la Table de Transposition
        if (move.hole == tt_move.hole && move.type == tt_move.type) {
            return 2000000; // Score énorme pour être trié en premier à 100%
        }

        // 2. Sinon, logique V4 classique
        int seeds = 0;
        if (move.type == MoveType::RED) seeds = state.get_seeds(move.hole, RED);
        else if (move.type == MoveType::BLUE) seeds = state.get_seeds(move.hole, BLUE);
        else seeds = state.get_seeds(move.hole, TRANSPARENT);

        if (seeds == 0) return -1000;
        int heuristic_score = seeds;

        // Simulation Prises Multiples (V4)
        int final_hole = (move.hole + seeds) % NB_HOLES;
        int current_check_hole = final_hole;
        bool capturing = true;
        int check_limit = 0;
        while (capturing && check_limit < 4) {
            if (GameRules::is_current_player_hole(current_check_hole, player_id)) break;
            int total = state.count_total_seeds(current_check_hole);
            if (current_check_hole == final_hole) total++;
            if (total == 2 || total == 3) {
                heuristic_score += total * 100;
                current_check_hole = (current_check_hole - 1 + NB_HOLES) % NB_HOLES;
                check_limit++;
            }
            else capturing = false;
        }

        // Bonus Historique
        heuristic_score += history_table[move.hole][(int)move.type];
        return heuristic_score;
    }

    inline std::vector<Move> generate_moves(const GameState& state, int player_id) {
        // (Copie de ton code V4 standard)
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

    // --- 4. ALPHA-BETA AVEC TT ---
    int alpha_beta(GameState state, int depth, int alpha, int beta, int player_id, int maximizing_player_id) {
        stats.nodes++;

        // A. Check Time
        if ((stats.nodes & 2047) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double, std::milli>(now - start_time).count() >= time_limit_ms) {
                time_out = true; return 0;
            }
        }

        int alpha_orig = alpha; // On garde l'alpha original pour savoir quel Flag mettre dans la TT

        // B. TT PROBE (Lecture)
        // On calcule le hash de la position actuelle
        uint64_t hash = compute_hash(state);
        // On trouve l'index dans le tableau (modulo taille)
        int tt_index = hash % TT_SIZE;
        TTEntry& entry = transposition_table[tt_index];

        Move tt_move; // Le meilleur coup mémorisé (si existe)

        // Si on a une entrée valide pour cette position (Hash correspond)
        if (entry.key == hash) {
            tt_move = entry.best_move; // On le note pour le tri plus tard

            // Si la profondeur stockée est suffisante (>= profondeur actuelle), on peut utiliser le score !
            if (entry.depth >= depth) {
                if (entry.flag == TTFlag::EXACT) {
                    return entry.score; // Victoire ! On évite tout le calcul
                }
                if (entry.flag == TTFlag::LOWERBOUND) {
                    alpha = std::max(alpha, entry.score);
                }
                else if (entry.flag == TTFlag::UPPERBOUND) {
                    beta = std::min(beta, entry.score);
                }

                if (alpha >= beta) {
                    stats.cutoffs++; // Coupure grâce à la TT
                    return entry.score;
                }
            }
        }

        // C. Conditions de fin
        if (depth == 0 || state.score_p1 >= 49 || state.score_p2 >= 49 || state.moves_count >= 400) {
            return evaluate(state, maximizing_player_id);
        }

        std::vector<Move> moves = generate_moves(state, player_id);
        if (moves.empty()) return evaluate(state, maximizing_player_id);

        // D. TRI DES COUPS (Avec le TT Move en priorité)
        if (depth > 0) { // Toujours trier sauf aux feuilles
            std::vector<ScoredMove> scored_moves;
            scored_moves.reserve(moves.size());
            for (const auto& m : moves) {
                // On passe 'tt_move' à la fonction de tri
                int s = score_move_for_ordering(state, m, player_id, tt_move);
                scored_moves.push_back({ m, s });
            }
            std::sort(scored_moves.begin(), scored_moves.end(), std::greater<ScoredMove>());
            for (size_t i = 0; i < moves.size(); ++i) moves[i] = scored_moves[i].move;
        }

        // E. RECHERCHE
        Move best_move_this_node; // Pour sauvegarder dans la TT
        int best_val = -INF;
        int next_player = (player_id == 1) ? 2 : 1;

        if (player_id == maximizing_player_id) {
            best_val = -INF;
            for (const auto& move : moves) {
                GameState next_state = state;
                GameRules::apply_move(next_state, move, player_id);

                int val = alpha_beta(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);

                if (time_out) return 0;

                if (val > best_val) {
                    best_val = val;
                    best_move_this_node = move; // On a trouvé un nouveau meilleur coup
                }

                alpha = std::max(alpha, best_val);
                if (beta <= alpha) {
                    stats.cutoffs++;
                    history_table[move.hole][(int)move.type] += depth * depth;
                    break;
                }
            }
        }
        else {
            // Logique Min (Adversaire)
            // Attention : Ici best_val doit être vu du point de vue de Max pour la remontée, 
            // mais l'élagage se fait sur Min. Pour simplifier, on garde la structure standard Min/Max.
            best_val = INF;
            for (const auto& move : moves) {
                GameState next_state = state;
                GameRules::apply_move(next_state, move, player_id);

                int val = alpha_beta(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);

                if (time_out) return 0;

                if (val < best_val) {
                    best_val = val;
                    best_move_this_node = move; // Meilleur coup pour Min (le pire pour nous)
                }

                beta = std::min(beta, best_val);
                if (beta <= alpha) {
                    stats.cutoffs++;
                    history_table[move.hole][(int)move.type] += depth * depth;
                    break;
                }
            }
        }

        // F. TT STORE (Sauvegarde)
        // On ne sauvegarde pas si on a time out, car le résultat est partiel/faux
        if (!time_out) {
            TTEntry new_entry;
            new_entry.key = hash;
            new_entry.score = best_val;
            new_entry.depth = depth;
            new_entry.best_move = best_move_this_node; // On sauve le coup qui a généré ce score

            if (best_val <= alpha_orig) {
                new_entry.flag = TTFlag::UPPERBOUND; // On n'a pas tout exploré, mais c'est <= à ce qu'on voulait
            }
            else if (best_val >= beta) {
                new_entry.flag = TTFlag::LOWERBOUND; // Coupure Beta
            }
            else {
                new_entry.flag = TTFlag::EXACT; // Score exact
            }

            // Stratégie de remplacement : On remplace toujours (ou si profondeur plus grande)
            // Ici simple : on remplace toujours (plus récent = souvent plus pertinent dans l'arbre)
            transposition_table[tt_index] = new_entry;
        }

        return best_val;
    }

    // --- 5. INTERFACE ---
    inline Move find_best_move(const GameState& root_state, int player_id, double time_limit_sec) {
        stats.reset();
        std::memset(history_table, 0, sizeof(history_table));

        // On ne reset PAS la transposition_table ici ! 
        // On veut garder les infos du tour précédent (c'est ça la force du système)
        // Sauf si tu veux faire des tests unitaires isolés. Pour un match, garde-la.
        // Si tu veux reset : std::fill(transposition_table.begin(), transposition_table.end(), TTEntry{});

        start_time = std::chrono::high_resolution_clock::now();
        time_limit_ms = (time_limit_sec * 1000.0) - 50.0;
        time_out = false;

        std::vector<Move> moves = generate_moves(root_state, player_id);
        if (moves.empty()) return Move();

        Move best_move_found = moves[0];

        // Pour la racine, on utilise aussi la TT pour le tri initial
        // Mais comme on appelle alpha_beta itérativement, la TT se remplit et s'améliore.

        for (int depth = 1; depth <= 64; ++depth) {
            int best_score_this_depth = -INF;
            Move best_move_this_depth = moves[0];
            bool this_depth_completed = true;

            int alpha = -INF;
            int beta = INF;
            int next_player = (player_id == 1) ? 2 : 1;

            // On trie les coups à la racine avant de lancer l'itération
            // On utilise la TT (probe) sur la racine pour savoir quel coup mettre en premier
            uint64_t root_hash = compute_hash(root_state);
            int root_index = root_hash % TT_SIZE;
            Move root_tt_move = (transposition_table[root_index].key == root_hash) ? transposition_table[root_index].best_move : Move();

            std::vector<ScoredMove> root_scored_moves;
            for (const auto& m : moves) {
                root_scored_moves.push_back({ m, score_move_for_ordering(root_state, m, player_id, root_tt_move) });
            }
            std::sort(root_scored_moves.begin(), root_scored_moves.end(), std::greater<ScoredMove>());
            for (size_t i = 0; i < moves.size(); ++i) moves[i] = root_scored_moves[i].move;


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
            }
            else {
                break;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        stats.time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        if (stats.time_ms > 0) stats.nps = (long long)(stats.nodes * 1000.0 / stats.time_ms);

        std::cout << "[AI v6] D:" << stats.max_depth
            << " Nodes:" << stats.nodes
            << " Cutoffs:" << stats.cutoffs
            << " NPS:" << stats.nps
            << " Time:" << std::fixed << std::setprecision(0) << stats.time_ms << "ms"
            << std::endl;

        return best_move_found;
    }
};