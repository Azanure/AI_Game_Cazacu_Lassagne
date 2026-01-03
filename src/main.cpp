#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <iomanip> // Pour std::setw
#include <limits> 

#include "common/GameState.hpp"
#include "common/GameRules.hpp"
#include "common/SearchStats.hpp" 
#include "common/Debugger.hpp"

// Importation des IAs
#include "ais/v1/Search.hpp"
#include "ais/v2/Search.hpp"
#include "ais/v3/Search.hpp"
#include "ais/v4/Search.hpp"
#include "ais/v5/Search.hpp"
#include "ais/v6/Search.hpp"

// --- FONCTION D'AFFICHAGE DU PLATEAU (NOUVEAU) ---
void display_board(const GameState& s) {
    std::cout << "\n   +------+----+-----+-----+-----+-----+\n";
    std::cout << "   | TROU | P  |  R  |  B  |  T  | TOT |\n";
    std::cout << "   +------+----+-----+-----+-----+-----+\n";

    for (int i = 0; i < 16; ++i) {
        int r = s.get_seeds(i, RED);
        int b = s.get_seeds(i, BLUE);
        int t = s.get_seeds(i, TRANSPARENT);
        int tot = r + b + t;

        // P1 a les indices pairs (0, 2...) qui correspondent aux trous impairs (1, 3...)
        std::string owner = (i % 2 == 0) ? "P1" : "P2";

        // Coloration simple pour lisibilité si le terminal le supporte, sinon affichage brut
        std::cout << "   | " << std::setw(4) << (i + 1) << " | " << owner << " | "
            << std::setw(3) << r << " | "
            << std::setw(3) << b << " | "
            << std::setw(3) << t << " | "
            << std::setw(3) << tot << " |\n";
    }
    std::cout << "   +------+----+-----+-----+-----+-----+\n";
    std::cout << "   >> SCORES ACTUELS : P1 = " << s.score_p1 << "  |  P2 = " << s.score_p2 << " <<\n\n";
}

// --- STRUCTURES POUR LE BENCHMARK ---

struct MatchMetrics {
    std::string name;
    long long total_nodes = 0;
    double total_time_ms = 0;
    long long total_depth = 0;
    long long total_cutoffs = 0;
    int move_count = 0;
    int max_depth_reached = 0;

    void add(const SearchStats& s) {
        total_nodes += s.nodes;
        total_time_ms += s.time_ms;
        total_depth += s.max_depth;
        total_cutoffs += s.cutoffs;
        move_count++;
        if (s.max_depth > max_depth_reached) max_depth_reached = s.max_depth;
    }

    void normalize(int n) {
        if (n <= 1) return;
        total_nodes /= n;
        total_time_ms /= n;
        total_depth /= n;
        move_count /= n;
    }
};

struct PlayerAdapter {
    std::string name;
    std::function<Move(const GameState&, int)> play_func;
    std::function<SearchStats()> get_stats_func;
};

// --- SELECTION DES JOUEURS ---

PlayerAdapter get_player_config(int mode, int player_num) {
    PlayerAdapter player;

    if (mode == 1 && player_num == 1) {
        player.name = "Humain";
        player.play_func = [](const GameState& s, int p) {
            int hole;
            std::cout << ">> [Humain] Trou (0-15) : ";
            while (!(std::cin >> hole) || hole < 0 || hole > 15) {
                std::cin.clear(); std::cin.ignore(10000, '\n');
            }
            return Move(hole, MoveType::RED); // TODO: Gérer choix couleur pour l'humain si besoin
            };
        player.get_stats_func = []() { return SearchStats(); };
        return player;
    }

    int v;
    std::cout << "Version IA pour Joueur " << player_num << " (ex: 6) : ";
    std::cin >> v;
    player.name = "IA_v" + std::to_string(v);

    double time_limit = 0.1; // 0.2s par défaut

    switch (v) {
    case 1:
        player.play_func = [time_limit](const GameState& s, int p) { return AI_V1::find_best_move(s, p, time_limit); };
        player.get_stats_func = []() { return AI_V1::stats; };
        break;
    case 2:
        player.play_func = [time_limit](const GameState& s, int p) { return AI_V2::find_best_move(s, p, time_limit); };
        player.get_stats_func = []() { return AI_V2::stats; };
        break;
    case 3:
        player.play_func = [time_limit](const GameState& s, int p) { return AI_V3::find_best_move(s, p, time_limit); };
        player.get_stats_func = []() { return AI_V3::stats; };
        break;
    case 4:
        player.play_func = [time_limit](const GameState& s, int p) { return AI_V4::find_best_move(s, p, time_limit); };
        player.get_stats_func = []() { return AI_V4::stats; };
        break;
    case 5:
        player.play_func = [time_limit](const GameState& s, int p) { return AI_V5::find_best_move(s, p, time_limit); };
        player.get_stats_func = []() { return AI_V5::stats; };
        break;
    case 6:
        player.play_func = [time_limit](const GameState& s, int p) { return AI_V6::find_best_move(s, p, time_limit); };
        player.get_stats_func = []() { return AI_V6::stats; };
        break;
    default:
        std::cerr << "Version inconnue. Exit." << std::endl;
        exit(1);
    }
    return player;
}

// --- AFFICHAGE DU TABLEAU FINAL ---

void print_comparison_table(const MatchMetrics& p1, const MatchMetrics& p2, double score1, double score2, int games_played) {
    auto avg_time1 = p1.move_count > 0 ? p1.total_time_ms / p1.move_count : 0;
    auto avg_time2 = p2.move_count > 0 ? p2.total_time_ms / p2.move_count : 0;
    auto avg_depth1 = p1.move_count > 0 ? (double)p1.total_depth / p1.move_count : 0;
    auto avg_depth2 = p2.move_count > 0 ? (double)p2.total_depth / p2.move_count : 0;
    auto nps1 = p1.total_time_ms > 0 ? (long long)(p1.total_nodes * 1000.0 / p1.total_time_ms) : 0;
    auto nps2 = p2.total_time_ms > 0 ? (long long)(p2.total_nodes * 1000.0 / p2.total_time_ms) : 0;

    std::cout << "\n=======================================================\n";
    if (games_played > 1) std::cout << "        MOYENNES SUR " << games_played << " PARTIES         \n";
    else std::cout << "                RAPPORT DE FIN DE MATCH                 \n";
    std::cout << "=======================================================\n";

    std::cout << std::left << std::setw(20) << "METRIQUE"
        << std::setw(18) << ("P1 (" + p1.name + ")")
        << std::setw(18) << ("P2 (" + p2.name + ")") << "\n";
    std::cout << "-------------------------------------------------------\n";

    std::cout << std::left << std::setw(20) << ((games_played > 1) ? "SCORE MOYEN" : "SCORE FINAL")
        << std::setw(18) << std::fixed << std::setprecision(1) << score1
        << std::setw(18) << score2 << "\n";
    std::cout << "-------------------------------------------------------\n";

    std::cout << std::left << std::setw(20) << "Temps Moyen/Coup"
        << std::setw(15) << std::fixed << std::setprecision(1) << avg_time1 << " ms"
        << "   " << std::setw(15) << avg_time2 << " ms" << "\n";

    std::cout << std::left << std::setw(20) << "Profondeur Moyenne"
        << std::setw(18) << std::setprecision(2) << avg_depth1
        << std::setw(18) << avg_depth2 << "\n";

    std::cout << std::left << std::setw(20) << "Profondeur Max"
        << std::setw(18) << std::setprecision(0) << p1.max_depth_reached
        << std::setw(18) << p2.max_depth_reached << "\n";

    std::cout << std::left << std::setw(20) << ((games_played > 1) ? "Noeuds/Partie" : "Noeuds Totaux")
        << std::setw(18) << p1.total_nodes
        << std::setw(18) << p2.total_nodes << "\n";

    std::cout << std::left << std::setw(20) << "NPS (Vitesse)"
        << std::setw(18) << nps1
        << std::setw(18) << nps2 << "\n";

    std::cout << std::left << std::setw(20) << ((games_played > 1) ? "Cutoffs/Partie" : "Cutoffs Totaux")
        << std::setw(18) << p1.total_cutoffs
        << std::setw(18) << p2.total_cutoffs << "\n";
}

// --- MAIN ---

int main() {
    std::cout << "=== ARENE DE JEU ===" << std::endl;
    std::cout << "1. Humain vs IA" << std::endl;
    std::cout << "2. IA vs IA (1 Match)" << std::endl;
    std::cout << "3. IA vs IA (Benchmark 10 Matchs)" << std::endl;
    std::cout << "Mode : ";
    int mode;
    std::cin >> mode;

    int num_games = (mode == 3) ? 10 : 1;

    PlayerAdapter p1 = get_player_config((mode == 3 ? 2 : mode), 1);
    PlayerAdapter p2 = get_player_config((mode == 3 ? 2 : mode), 2);

    MatchMetrics m1_stats; m1_stats.name = p1.name;
    MatchMetrics m2_stats; m2_stats.name = p2.name;

    int p1_wins = 0, p2_wins = 0, draws = 0;
    long long total_score_p1 = 0, total_score_p2 = 0;

    std::cout << "\nLancement : " << p1.name << " vs " << p2.name << " (" << num_games << " parties)\n\n";

    for (int g = 0; g < num_games; ++g) {

        GameState state;

        // --- MEMOIRE DE LA PARTIE (NOUVEAU) ---
        // Pour garder l'état du plateau à chaque coup
        std::vector<GameState> history;
        history.push_back(state); // Etat initial

        int current_player = 1;

        while (!GameRules::is_game_over(state)) {

            // Detection Famine
            if (!GameRules::has_moves(state, current_player)) {
                int other_player = (current_player == 1) ? 2 : 1;
                for (int i = 0; i < 16; ++i) {
                    int total = state.count_total_seeds(i);
                    if (total > 0) {
                        if (other_player == 1) state.score_p1 += total;
                        else state.score_p2 += total;
                        state.clear_seeds(i, 0); state.clear_seeds(i, 1); state.clear_seeds(i, 2);
                    }
                }
                break;
            }

            Move move;
            if (current_player == 1) {
                move = p1.play_func(state, 1);
                m1_stats.add(p1.get_stats_func());
            }
            else {
                move = p2.play_func(state, 2);
                m2_stats.add(p2.get_stats_func());
            }

            if (move.hole >= 16) {
                std::cerr << "ERREUR CRITIQUE : Coup invalide. Fin." << std::endl;
                break;
            }

            // Application du coup
            GameRules::apply_move(state, move, current_player);

            // --- SAUVEGARDE DANS L'HISTORIQUE ---
            history.push_back(state);

            // --- AFFICHAGE DETAILLE (Uniquement en mode 1 match) ---
            if (num_games == 1) {
                std::string pName = (current_player == 1) ? p1.name : p2.name;
                std::string colorStr;
                switch (move.type) {
                case MoveType::RED: colorStr = "ROUGE"; break;
                case MoveType::BLUE: colorStr = "BLEU"; break;
                case MoveType::TRANS_AS_RED: colorStr = "TRANS -> ROUGE"; break;
                case MoveType::TRANS_AS_BLUE: colorStr = "TRANS -> BLEU"; break;
                }

                std::cout << "========================================================\n";
                std::cout << "COUP " << state.moves_count << " : Joueur " << current_player << " (" << pName << ")\n";
                std::cout << "ACTION : Trou " << (int)move.hole + 1 << " (" << (int)move.hole << "), Couleur " << colorStr << "\n";
                debug_evaluate_state(state, current_player);

                // Affichage du plateau résultant
                display_board(state);
            }

            current_player = (current_player == 1) ? 2 : 1;
        }

        total_score_p1 += state.score_p1;
        total_score_p2 += state.score_p2;

        if (state.score_p1 > state.score_p2) p1_wins++;
        else if (state.score_p2 > state.score_p1) p2_wins++;
        else draws++;

        if (num_games > 1) {
            std::cout << "Partie " << (g + 1) << " finie."
                << " (Score: " << state.score_p1 << "-" << state.score_p2 << ")" << std::endl;
        }
        else {
            if (state.score_p1 > state.score_p2) std::cout << ">> VAINQUEUR : " << p1.name << " (P1) <<\n";
            else if (state.score_p2 > state.score_p1) std::cout << ">> VAINQUEUR : " << p2.name << " (P2) <<\n";
            else std::cout << ">> MATCH NUL <<\n";
        }
    }

    if (num_games > 1) {
        std::cout << "\n#######################################################\n";
        std::cout << "                   RESULTATS DU TOURNOI                \n";
        std::cout << "#######################################################\n";
        std::cout << "P1 (" << p1.name << ") Victoires : " << p1_wins << "\n";
        std::cout << "P2 (" << p2.name << ") Victoires : " << p2_wins << "\n";
        std::cout << "Matchs Nuls      : " << draws << "\n";
    }

    m1_stats.normalize(num_games);
    m2_stats.normalize(num_games);

    print_comparison_table(m1_stats, m2_stats,
        (double)total_score_p1 / num_games,
        (double)total_score_p2 / num_games,
        num_games);

    return 0;
}