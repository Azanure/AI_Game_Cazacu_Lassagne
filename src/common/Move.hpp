#pragma once

#include <cstdint>
#include <string> // Juste pour l'affichage (debug)

// On définit les 4 types de coups possibles.
// On utilise 'enum class' (C++11) : c'est plus propre que des #define ou des enum simples du C.
// Cela empêche de mélanger accidentellement une couleur de graine avec un type de coup.
enum class MoveType : uint8_t {
    RED = 0,            // Jouer les graines Rouges
    BLUE = 1,           // Jouer les graines Bleues
    TRANS_AS_RED = 2,   // Jouer Transparentes -> comportement Rouge
    TRANS_AS_BLUE = 3   // Jouer Transparentes -> comportement Bleu
};

struct Move {
    uint8_t hole;       // Le trou choisi (0 à 15)
    MoveType type;      // Le type de graines et leur comportement

    // Constructeur par défaut : On met une valeur IMPOSSIBLE (255)
	// Pour réserver de la place si on veut créer un tableau de Move sans l'initialiser.
    Move() : hole(255), type(MoveType::RED) {}

    // Constructeur pratique
    Move(int h, MoveType t) : hole(static_cast<uint8_t>(h)), type(t) {}

    // Une petite méthode pour voir le coup en texte (utile pour tes tests)
    // Exemple : "14-TR"
    std::string to_string() const {
        std::string s = std::to_string(hole + 1) + "-"; // +1 car l'affichage est 1-16
        switch (type) {
            case MoveType::RED: s += "R"; break;
            case MoveType::BLUE: s += "B"; break;
            case MoveType::TRANS_AS_RED: s += "TR"; break;
            case MoveType::TRANS_AS_BLUE: s += "TB"; break;
        }
        return s;
    }
    
    // Surcharge de l'opérateur == pour comparer deux coups facilement
    bool operator==(const Move& other) const {
        return hole == other.hole && type == other.type;
    }
};