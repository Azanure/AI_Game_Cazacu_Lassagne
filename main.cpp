#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <sstream>

#include "src/GameState.hpp"
#include "src/GameRules.hpp"
#include "src/Move.hpp"
#include "src/Search.hpp"

using namespace std;

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
    string raw_suffix = s.substr(num_len);
    string suffix = "";

    // Nettoyage et normalisation (supprime les espaces/\r et met en majuscules)
    for (char c : raw_suffix) {
        if (!isspace(c)) {
            suffix += toupper(c);
        }
    }

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
    // Optimisation des flux d'entrée/sortie
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    GameState state;
    int my_player_id = 0;
    string input_line;

    // Boucle principale
    while (getline(cin, input_line))
    {
        // Nettoyage de l'entrée (suppression des retours chariot type Windows)
        if (!input_line.empty() && input_line.back() == '\r')
            input_line.pop_back();

        if (input_line.empty())
            continue;

        // 1. GESTION START
        if (input_line == "START")
        {
            my_player_id = 1;

            // Je joue le premier coup
            Move best_move = AI::find_best_move(state, my_player_id, 2);

            GameRules::apply_move(state, best_move, my_player_id);

            cout << move_to_string(best_move) << endl;
            continue;
        }

        // 2. GESTION END
        if (input_line == "END" || input_line.find("RESULT") != string::npos)
        {
            break;
        }

        // 3. GESTION COUP ADVERSE
        if (my_player_id == 0)
        {
            my_player_id = 2;
        }

        int opponent_id = (my_player_id == 1) ? 2 : 1;

        Move opp_move = string_to_move(input_line);

        if (opp_move.hole >= 0 && opp_move.hole < 16)
        {
            // Appliquer le coup adverse sur mon plateau local
            GameRules::apply_move(state, opp_move, opponent_id);
        }

        // 4. A MON TOUR DE JOUER
        Move best_move = AI::find_best_move(state, my_player_id, 2.0);

        if (best_move.hole == 255)
        {
            // Aucun coup trouvé : on n'envoie rien et on attend la réaction de l'arbitre (ou timeout)
             continue;
        }

        // Appliquer mon coup localement
        GameRules::apply_move(state, best_move, my_player_id);

        // Envoyer le coup (traduit en 1-16 automatiquement)
        cout << move_to_string(best_move) << endl;
    }

    return 0;
}