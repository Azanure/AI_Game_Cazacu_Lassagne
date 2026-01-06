#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <sstream>

// Inclusions de ta structure V8 et du Jeu
#include "common/GameState.hpp"
#include "common/GameRules.hpp"
#include "common/Move.hpp"       // Move est défini ici (globalement)
#include "ais/v6/Search.hpp" 

using namespace std;

// --- FONCTIONS AUXILIAIRES DE PROTOCOLE ---

// Convertit un coup (Move) en string pour l'Arbitre (ex: "5r", "12tb")
// CORRECTION : On utilise 'const Move&' et pas 'const AI_V8::Move&'
string move_to_string(const Move& m) {
    string s = to_string(m.hole);

    // CORRECTION : On utilise 'MoveType::...' directement
    if (m.type == MoveType::RED) s += "r";
    else if (m.type == MoveType::BLUE) s += "b";
    else if (m.type == MoveType::TRANS_AS_RED) s += "tr";
    else if (m.type == MoveType::TRANS_AS_BLUE) s += "tb";

    return s;
}

// Convertit le string de l'Arbitre en coup (Move)
Move string_to_move(const string& s) {
    // Extraction de l'index (partie numérique)
    size_t num_len = 0;
    while (num_len < s.size() && isdigit(s[num_len])) num_len++;

    int hole = 0;
    try {
        hole = stoi(s.substr(0, num_len));
    }
    catch (...) {
        // En cas d'erreur de parsing, on renvoie un coup invalide
        return { 255, MoveType::RED };
    }

    // Extraction du type (partie texte)
    string suffix = s.substr(num_len);
    MoveType type = MoveType::RED; // Par défaut

    if (suffix == "r") type = MoveType::RED;
    else if (suffix == "b") type = MoveType::BLUE;
    else if (suffix == "tr") type = MoveType::TRANS_AS_RED;
    else if (suffix == "tb") type = MoveType::TRANS_AS_BLUE;

    return { hole, type };
}

// Fonction de log pour le débogage (écrit sur CERR pour ne pas perturber l'Arbitre)
void log(const string& msg) {
    cerr << "[BOT V8] " << msg << endl;
}

// --- MAIN ---

int main() {
    // Optimisation I/O pour la vitesse
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    GameState state;
    int my_player_id = 0; // 0 = Inconnu au début
    string input_line;

    // Boucle de lecture infinie (l'Arbitre tuera le processus à la fin)
    while (getline(cin, input_line)) {

        // Nettoyage des espaces éventuels
        if (input_line.empty()) continue;

        // 1. GESTION START
        if (input_line == "START") {
            my_player_id = 1;
            log("Je suis P1 (START reçu). Je commence.");

            // Je joue le premier coup
            // CORRECTION : find_best_move renvoie un 'Move' global
            Move best_move = AI_V6::find_best_move(state, my_player_id, 2.5); // 2.5s max
            GameRules::apply_move(state, best_move, my_player_id);

            cout << move_to_string(best_move) << endl; // ENVOI A L'ARBITRE
            continue;
        }

        // 2. GESTION END
        if (input_line == "END" || input_line.find("RESULT") != string::npos) {
            log("Fin de partie.");
            break;
        }

        // 3. GESTION COUP ADVERSE
        // Si on reçoit un coup (ex: "4r"), c'est que l'adversaire a joué
        if (my_player_id == 0) {
            my_player_id = 2; // Si je reçois un coup sans avoir eu START, je suis P2
            log("Je suis P2 (Coup adverse reçu).");
        }

        int opponent_id = (my_player_id == 1) ? 2 : 1;

        Move opp_move = string_to_move(input_line);

        if (opp_move.hole >= 16) {
            log("ERREUR: Coup adverse invalide (" + input_line + ")");
            // On continue quand même, l'arbitre gérera l'erreur s'il faut
        }
        else {
            // Appliquer le coup adverse sur mon plateau local
            GameRules::apply_move(state, opp_move, opponent_id);
        }

        // 4. A MON TOUR DE JOUER
        Move best_move = AI_V6::find_best_move(state, my_player_id, 2.5);

        // Appliquer mon coup localement pour rester synchronisé
        GameRules::apply_move(state, best_move, my_player_id);

        // Envoyer le coup
        string str_move = move_to_string(best_move);
        log("Je joue : " + str_move);
        cout << str_move << endl; // Important : endl vide le buffer
    }

    return 0;
}