#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>

#include "GameState.hpp"
#include "GameRules.hpp"
#include "Move.hpp"
#include "Search.hpp"

using namespace std;

// --- LOGGING POUR DEBUG ---
ofstream debug_file;

void init_debug()
{
    // Ouvre le fichier en mode "append" pour ne pas ecraser si 2 joueurs
    debug_file.open("player_debug.txt", ios::app);
    debug_file << "=== NOUVEAU LANCEMENT ===" << endl;
}

void log_msg(const string &msg)
{
    if (debug_file.is_open())
    {
        auto now = chrono::system_clock::now();
        auto in_time_t = chrono::system_clock::to_time_t(now);
        debug_file << std::put_time(std::localtime(&in_time_t), "%X") << " - " << msg << endl;
        debug_file.flush(); // Force l'ecriture immediate
    }
}
// --------------------------

// Convertit un coup interne (0-15) en string pour l'Arbitre (1-16)
string move_to_string(const Move &m)
{
    // On ajoute 1 pour passer de l'index 0-15 à 1-16
    string s = to_string(m.hole + 1);

    if (m.type == MoveType::RED)
        s += "R";
    else if (m.type == MoveType::BLUE)
        s += "B";
    else if (m.type == MoveType::TRANS_AS_RED)
        s += "TR";
    else if (m.type == MoveType::TRANS_AS_BLUE)
        s += "TB";

    return s;
}

// Convertit le string de l'Arbitre (1-16) en coup interne (0-15)
Move string_to_move(const string &s)
{
    size_t num_len = 0;
    while (num_len < s.size() && isdigit(s[num_len]))
        num_len++;

    int hole = 0;
    try
    {
        int parsed_hole = stoi(s.substr(0, num_len));
        // Conversion en index 0-15
        hole = parsed_hole - 1;
    }
    catch (...)
    {
        return {255, MoveType::RED}; // Coup invalide
    }

    // Extraction du type de coup
    string suffix = s.substr(num_len);

    // Normaliser en majuscules pour la robustesse
    for (auto &c : suffix)
        c = toupper(c);

    MoveType type = MoveType::RED;

    if (suffix == "R")
        type = MoveType::RED;
    else if (suffix == "B")
        type = MoveType::BLUE;
    else if (suffix == "TR")
        type = MoveType::TRANS_AS_RED;
    else if (suffix == "TB")
        type = MoveType::TRANS_AS_BLUE;

    return {hole, type};
}

int main()
{
    init_debug();
    log_msg("Main started. Waiting for input.");

    // Optimisation des flux d'entrée/sortie
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    GameState state;
    int my_player_id = 0;
    string input_line;

    // Boucle principale
    while (getline(cin, input_line))
    {

        if (input_line.empty())
            continue;

        log_msg("Received input: " + input_line);

        // 1. GESTION START
        if (input_line == "START")
        {
            my_player_id = 1;
            log_msg("I am Player 1. Thinking...");

            // Je joue le premier coup
            Move best_move = AI::find_best_move(state, my_player_id, 2.5);
            log_msg("Move found: " + move_to_string(best_move));

            GameRules::apply_move(state, best_move, my_player_id);

            cout << move_to_string(best_move) << endl;
            continue;
        }

        // 2. GESTION END
        if (input_line == "END" || input_line.find("RESULT") != string::npos)
        {
            log_msg("Game Over signal received.");
            break;
        }

        // 3. GESTION COUP ADVERSE
        if (my_player_id == 0)
        {
            my_player_id = 2;
            log_msg("I am Player 2.");
        }

        int opponent_id = (my_player_id == 1) ? 2 : 1;

        Move opp_move = string_to_move(input_line);

        if (opp_move.hole >= 0 && opp_move.hole < 16)
        {
            // Appliquer le coup adverse sur mon plateau local
            GameRules::apply_move(state, opp_move, opponent_id);
            log_msg("Applied opponent move: " + input_line);
        }

        // 4. A MON TOUR DE JOUER
        log_msg("My turn. Thinking...");
        Move best_move = AI::find_best_move(state, my_player_id, 2.5);
        log_msg("Move found: " + move_to_string(best_move));

        // Appliquer mon coup localement
        GameRules::apply_move(state, best_move, my_player_id);

        // Envoyer le coup (traduit en 1-16 automatiquement)
        cout << move_to_string(best_move) << endl;
    }

    return 0;
}