#pragma once

#include "GameRules.hpp"
#include "GameState.hpp"
#include "Move.hpp"
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <random>
#include <array>

namespace AI
{

    // --- CONFIGURATION ---
    static std::chrono::time_point<std::chrono::high_resolution_clock> start_time; // Temps de début de la recherche
    static double time_limit_ms;                                                   // Limite de temps en millisecondes
    static bool time_out;                                                          // Indicateur de dépassement de temps

    static int nodes_count = 0; // Compteur de nœuds explorés

    constexpr int INF = 10000000; // Valeur infinie pour l'alpha-beta
    constexpr int MAX_DEPTH = 64; // Profondeur maximale de recherche

    // --- STRUCTURE DE DONNÉES LÉGÈRE POUR LES LISTES DE COUPS ---
    template <typename T, int Capacity>
    struct StaticVector
    {
        T data[Capacity]; // Stockage des éléments
        int count = 0;    // Nombre d'éléments actuellement stockés

        inline void clear() { count = 0; }                             // Réinitialise la liste
        inline void push_back(const T &item) { data[count++] = item; } // Ajoute un élément à la fin
        inline T *begin() { return data; }                             // Début de l'itération
        inline T *end() { return data + count; }                       // Fin de l'itération
        inline bool empty() const { return count == 0; }               // Vérifie si la liste est vide
        inline int size() const { return count; }                      // Retourne la taille actuelle
        inline T &operator[](int i) { return data[i]; }                // Accès par index
        inline const T &operator[](int i) const { return data[i]; }    // Accès par index (const)
    };

    // --- DONNÉES POUR L'IA ---
    struct BotDNA
    {
        int w_score = 160;                                                                      // Poids du score
        int w_seed_red = 16;                                                                    // Poids des graines rouges
        int w_seed_blue = 179;                                                                  // Poids des graines bleues
        int w_seed_trans = -5;                                                                  // Poids des graines transparentes
        int w_defense = -50;                                                                    // Poids de la défense
        int w_attack = 31;                                                                      // Poids de l'attaque
        int w_mobility = 78;                                                                    // Poids de la mobilité
        int w_hoard_penalty = 39;                                                               // Poids de la pénalité de thésaurisation
        int w_starvation = -5;                                                                  // Poids de la famine
        std::array<int, 16> w_holes = {5, 9, 25, 9, -13, 13, 1, -8, -1, 3, -5, 4, 7, 6, -3, 9}; // Poids positionnels des trous
    };

    static BotDNA ia_dna; // Instance des paramètres de l'IA

    // --- 1. ZOBRIST HASHING ---
    // Permet de générer un hash unique pour chaque état du jeu
    namespace Zobrist
    {
        static uint64_t table[NB_HOLES][3][64];
        static uint64_t turn_hash[2];
        static bool initialized = false;

        inline void init()
        {
            if (initialized)
                return;
            std::mt19937_64 rng(12345);
            for (int i = 0; i < NB_HOLES; ++i)
                for (int c = 0; c < 3; ++c)
                    for (int n = 0; n < 64; ++n)
                        table[i][c][n] = rng();
            turn_hash[0] = rng();
            turn_hash[1] = rng();
            initialized = true;
        }

        // Calcule le hash Zobrist pour un état de jeu donné
        inline uint64_t compute(const GameState &state, int player_id)
        {
            uint64_t h = 0;
            for (int i = 0; i < NB_HOLES; ++i)
            {
                int r = std::min((int)state.get_seeds(i, RED), 63);
                int b = std::min((int)state.get_seeds(i, BLUE), 63);
                int t = std::min((int)state.get_seeds(i, TRANSPARENT), 63);
                if (r > 0)
                    h ^= table[i][0][r];
                if (b > 0)
                    h ^= table[i][1][b];
                if (t > 0)
                    h ^= table[i][2][t];
            }
            h ^= turn_hash[player_id - 1];
            return h;
        }
    }

    // --- 2. TABLE DE TRANSPOSITION ---
    // Sert à mémoriser les positions déjà évaluées
    enum class TTFlag : uint8_t
    {
        EXACT,
        LOWERBOUND,
        UPPERBOUND
    };
    struct TTEntry
    {
        uint64_t key = 0;
        int score = 0;
        int depth = 0;
        TTFlag flag = TTFlag::EXACT;
        Move best_move;
    };

    constexpr size_t TT_SIZE = 1048576;                       // Valeur de la table de transposition (2^20)
    constexpr size_t TT_MASK = TT_SIZE - 1;                   // Masque pour l'indexation
    static std::vector<TTEntry> transposition_table(TT_SIZE); // Table de transposition

    // --- 3. HEURISTIQUES DE TRI ---
    static Move killer_moves[MAX_DEPTH][2]; // Deux coups tueurs par profondeur
    static int history_table[NB_HOLES][4];  // Table d'historique des coups

    // --- 4. EVALUATION ---
    inline int evaluate(const GameState &state, int maximizing_player_id)
    {
        if (state.score_p1 >= 49)
            return (maximizing_player_id == 1) ? INF : -INF; // Victoire joueur 1
        if (state.score_p2 >= 49)
            return (maximizing_player_id == 2) ? INF : -INF; // Victoire joueur 2

        int score_diff = (maximizing_player_id == 1) ? (state.score_p1 - state.score_p2) : (state.score_p2 - state.score_p1); // Différence de score
        int eval = score_diff * ia_dna.w_score;                                                                               // Poids du score

        int my_mobility = 0;        // Mobilité du joueur courant
        int opp_mobility = 0;       // Mobilité de l'adversaire
        int opp_seeds_on_board = 0; // Graines adverses restantes sur le plateau

        // Analyse du plateau
        for (int i = 0; i < NB_HOLES; ++i)
        {
            int r = state.get_seeds(i, RED);
            int b = state.get_seeds(i, BLUE);
            int t = state.get_seeds(i, TRANSPARENT);
            int total = r + b + t;

            bool is_mine = GameRules::is_current_player_hole(i, maximizing_player_id); // Vérifie si le trou appartient au joueur courant

            int material = (r * ia_dna.w_seed_red) + (b * ia_dna.w_seed_blue) + (t * ia_dna.w_seed_trans); // Valeur matérielle des graines
            int positional = total * ia_dna.w_holes[i];                                                    // Valeur positionnelle des graines

            // Ajustement de l'évaluation en fonction de la possession du trou
            if (is_mine)
            {
                eval += material + positional;
                if (total > 0)
                    my_mobility++;
                if (total == 1 || total == 2)
                    eval -= ia_dna.w_defense;
                if (total > 12)
                    eval -= ia_dna.w_hoard_penalty * (total - 12);
            }
            else
            {
                eval -= material;
                opp_seeds_on_board += total;
                if (total > 0)
                    opp_mobility++;
                if (total == 1 || total == 2)
                    eval += ia_dna.w_attack;
            }
        }

        // Ajustement final basé sur la mobilité et la famine
        eval += (my_mobility - opp_mobility) * ia_dna.w_mobility;
        if (opp_seeds_on_board < 10)
            eval += ia_dna.w_starvation * (10 - opp_seeds_on_board);

        return eval;
    }

    // --- 5. LOGIQUE DE TRI ---
    inline int score_move(const GameState &state, const Move &move, int depth, const Move &tt_move)
    {
        if (move.hole == tt_move.hole && move.type == tt_move.type)
            return 2000000; // Meilleur
        if (move.hole == killer_moves[depth][0].hole && move.type == killer_moves[depth][0].type)
            return 1000000; // Premier coup tueur
        if (move.hole == killer_moves[depth][1].hole && move.type == killer_moves[depth][1].type)
            return 900000; // Second coup tueur

        return history_table[move.hole][(int)move.type];
    }

    // Génère tous les coups légaux pour le joueur courant
    inline StaticVector<Move, 40> generate_moves(const GameState &state, int player_id)
    {
        StaticVector<Move, 40> moves;
        for (int i = 0; i < NB_HOLES; ++i)
        {
            if (!GameRules::is_current_player_hole(i, player_id))
                continue;
            int r = state.get_seeds(i, RED);
            int b = state.get_seeds(i, BLUE);
            int t = state.get_seeds(i, TRANSPARENT);

            if (r == 0 && b == 0 && t == 0)
                continue; // Pas de graines à jouer

            // Ajouter les coups possibles en fonction des graines disponibless
            if (r > 0)
                moves.push_back(Move(i, MoveType::RED));
            if (b > 0)
                moves.push_back(Move(i, MoveType::BLUE));
            if (t > 0)
            {
                moves.push_back(Move(i, MoveType::TRANS_AS_RED));
                moves.push_back(Move(i, MoveType::TRANS_AS_BLUE));
            }
        }
        return moves;
    }

    // --- 6. ALPHA-BETA PVS ---
    int alpha_beta_pvs(GameState state, int depth, int alpha, int beta, int player_id, int maximizing_player_id)
    {
        nodes_count++;

        // Vérification du temps écoulé toutes les 1024 itérations
        if ((nodes_count & 1023) == 0)
        {
            auto now = std::chrono::high_resolution_clock::now();
            // Marge de sécurité augmentée à 50ms
            if (std::chrono::duration<double, std::milli>(now - start_time).count() >= time_limit_ms)
            {
                time_out = true;
                return 0;
            }
        }

        int alpha_orig = alpha;                               // Sauvegarde de la valeur originale d'alpha
        uint64_t hash = Zobrist::compute(state, player_id);   // Calcul du hash Zobrist
        TTEntry &entry = transposition_table[hash & TT_MASK]; // Accès à l'entrée de la table de transposition
        Move tt_move;                                         // Meilleur coup stocké dans la table de transposition

        // Vérification de l'entrée de la table de transposition
        if (entry.key == hash)
        {
            tt_move = entry.best_move;
            if (entry.depth >= depth)
            {
                if (entry.flag == TTFlag::EXACT)
                    return entry.score;
                if (entry.flag == TTFlag::LOWERBOUND)
                    alpha = std::max(alpha, entry.score);
                else if (entry.flag == TTFlag::UPPERBOUND)
                    beta = std::min(beta, entry.score);
                if (alpha >= beta)
                {
                    return entry.score;
                }
            }
        }

        // Condition de coup terminal ou profondeur maximale atteinte
        if (depth == 0 || GameRules::is_game_over(state))
        {
            return evaluate(state, maximizing_player_id);
        }

        // Génération et tri des coups
        StaticVector<Move, 40> moves = generate_moves(state, player_id);
        if (moves.empty())
            return evaluate(state, maximizing_player_id);

        // Structure pour le tri des coups
        struct ScoredMove
        {
            Move m;
            int s;
            bool operator>(const ScoredMove &o) const { return s > o.s; }
        };

        // Tri des coups selon leur score
        StaticVector<ScoredMove, 40> scored_moves;
        for (int i = 0; i < moves.size(); ++i)
        {
            scored_moves.push_back({moves[i], score_move(state, moves[i], depth, tt_move)});
        }
        std::sort(scored_moves.begin(), scored_moves.end(), std::greater<ScoredMove>());

        // Initialisation du meilleur coup pour ce nœud
        Move best_move_this_node;
        if (!scored_moves.empty())
            best_move_this_node = scored_moves[0].m;

        // Recherche principale avec PVS
        int best_val = (player_id == maximizing_player_id) ? -INF : INF;
        int next_player = (player_id == 1) ? 2 : 1;

        // Parcours des coups triés
        for (int i = 0; i < scored_moves.size(); ++i)
        {
            GameState next_state = state;
            GameRules::apply_move(next_state, scored_moves[i].m, player_id);

            // Recherche récursive
            int val;
            if (i == 0)
            {
                val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
            }
            else
            {
                if (player_id == maximizing_player_id)
                {
                    val = alpha_beta_pvs(next_state, depth - 1, alpha, alpha + 1, next_player, maximizing_player_id);
                    if (val > alpha && val < beta)
                        val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                }
                else
                {
                    val = alpha_beta_pvs(next_state, depth - 1, beta - 1, beta, next_player, maximizing_player_id);
                    if (val < beta && val > alpha)
                        val = alpha_beta_pvs(next_state, depth - 1, alpha, beta, next_player, maximizing_player_id);
                }
            }

            if (time_out)
                return 0; // Arrêt si dépassement de temps

            // Mise à jour des bornes alpha/beta et du meilleur coup
            if (player_id == maximizing_player_id)
            {
                if (val > best_val)
                {
                    best_val = val;
                    best_move_this_node = scored_moves[i].m;
                }
                alpha = std::max(alpha, best_val);
            }
            else
            {
                if (val < best_val)
                {
                    best_val = val;
                    best_move_this_node = scored_moves[i].m;
                }
                beta = std::min(beta, best_val);
            }

            // Coup tueur et historique
            if (alpha >= beta)
            {
                if (scored_moves[i].m.hole != killer_moves[depth][0].hole)
                {
                    killer_moves[depth][1] = killer_moves[depth][0];
                    killer_moves[depth][0] = scored_moves[i].m;
                }
                history_table[scored_moves[i].m.hole][(int)scored_moves[i].m.type] += depth * depth;
                break;
            }
        }

        // Stockage dans la table de transposition
        if (!time_out)
        {
            entry.key = hash;
            entry.score = best_val;
            entry.depth = depth;
            if (best_move_this_node.hole < NB_HOLES)
            {
                entry.best_move = best_move_this_node;
            }
            if (best_val <= alpha_orig)
                entry.flag = TTFlag::UPPERBOUND;
            else if (best_val >= beta)
                entry.flag = TTFlag::LOWERBOUND;
            else
                entry.flag = TTFlag::EXACT;
        }

        return best_val;
    }

    // --- 7. INTERFACE ---
    inline Move find_best_move(const GameState &root_state, int player_id, double time_limit_sec)
    {
        Zobrist::init(); // Initialisation du Zobrist hashing
        // Réinitialisation des structures de données de l'IA si c'est une nouvelle partie
        if (root_state.moves_count < 2)
        {
            std::fill(transposition_table.begin(), transposition_table.end(), TTEntry());
        }
        // Réinitialisation des compteurs et tables heuristiques
        nodes_count = 0;
        std::memset(history_table, 0, sizeof(history_table));
        std::memset(killer_moves, 0, sizeof(killer_moves));

        // Configuration du temps de recherche
        start_time = std::chrono::high_resolution_clock::now();
        time_limit_ms = (time_limit_sec * 1000.0) - 50.0;
        time_out = false;

        // Génération des coups initiaux
        StaticVector<Move, 40> moves = generate_moves(root_state, player_id);
        if (moves.empty())
            return Move();

        // Recherche itérative avec augmentation progressive de la profondeur
        Move best_move_found = moves[0];

        // Boucle de recherche itérative
        for (int depth = 1; depth <= MAX_DEPTH; ++depth)
        {
            alpha_beta_pvs(root_state, depth, -INF, INF, player_id, player_id);
            if (time_out)
                break;

            uint64_t root_hash = Zobrist::compute(root_state, player_id);
            // Optimisation bitwise
            TTEntry &entry = transposition_table[root_hash & TT_MASK];

            if (entry.key == root_hash && entry.best_move.hole < NB_HOLES)
            {
                best_move_found = entry.best_move;
            }
        }

        return best_move_found;
    }
};