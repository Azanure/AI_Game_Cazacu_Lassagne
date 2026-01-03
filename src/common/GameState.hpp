#pragma once // Remplace les #ifndef... #define... du C. Empêche d'inclure 2 fois le fichier.

#include <array>
#include <cstdint> // Pour avoir uint8_t (entier 8 bits)

// --- CONSTANTES ---
// En C++, on préfère 'constexpr' aux '#define'. 
// Le compilateur connaît le type, c'est plus sûr.
constexpr int NB_HOLES = 16;
constexpr int NB_COLORS = 3; // Rouge, Bleu, Transparent
constexpr int TOTAL_CELLS = NB_HOLES * NB_COLORS; // 16 * 3 = 48 cases mémoires

// Indices pour les couleurs (plus lisible que 0, 1, 2)
constexpr int RED = 0;
constexpr int BLUE = 1;
constexpr int TRANSPARENT = 2;

struct GameState {
    // --- DONNÉES ---
    
    // Le plateau est un tableau plat de 48 octets.
    // Index 0 = Trou 1 Rouge, Index 1 = Trou 1 Bleu, Index 2 = Trou 1 Transp.
    // Index 3 = Trou 2 Rouge, etc.
    std::array<uint8_t, TOTAL_CELLS> board;

    uint16_t score_p1;    // Score joueur 1
    uint16_t score_p2;    // Score joueur 2
    uint16_t moves_count; // Compteur pour la règle des 400 coups

    // --- CONSTRUCTEUR ---
    // C'est une fonction spéciale appelée automatiquement à la création de l'objet.
    // Elle remplace ta fonction "init_game()" en C.
    GameState() {
        score_p1 = 0;
        score_p2 = 0;
        moves_count = 0;

        // Remplissage initial : 2 graines de chaque couleur par trou
        for (int i = 0; i < TOTAL_CELLS; ++i) {
            board[i] = 2;
        }
    }

    // --- MÉTHODES (Fonctions) ---
    // 'const' à la fin veut dire : "Cette fonction ne modifie PAS le plateau" (lecture seule)
    
    // Récupérer le nombre de graines d'une couleur dans un trou
    // hole (0-15), color (0-2)
    inline uint8_t get_seeds(int hole_idx, int color) const {
        return board[hole_idx * 3 + color];
    }

    // Modifier le nombre de graines
    inline void set_seeds(int hole_idx, int color, int count) {
        board[hole_idx * 3 + color] = static_cast<uint8_t>(count);
    }

    // Ajouter des graines
    inline void add_seeds(int hole_idx, int color, int amount) {
        board[hole_idx * 3 + color] += static_cast<uint8_t>(amount);
    }
    
    // Vider un trou (utile quand on prend les graines pour semer)
    inline void clear_seeds(int hole_idx, int color) {
        board[hole_idx * 3 + color] = 0;
    }

    // Compter le total de graines dans un trou (toutes couleurs confondues)
    // Utile pour vérifier la condition de prise (2 ou 3 graines)
    inline int count_total_seeds(int hole_idx) const {
        int base_idx = hole_idx * 3;
        return board[base_idx + RED] + board[base_idx + BLUE] + board[base_idx + TRANSPARENT];
    }
};