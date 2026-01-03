// Fichier: common/SearchStats.hpp
#pragma once

struct SearchStats {
    long long nodes = 0;        // Total états visités
    long long cutoffs = 0;      // Nombre de branches coupées
    int max_depth = 0;          // Profondeur max atteinte
    double time_ms = 0.0;       // Temps pris
    long long nps = 0;          // Nœuds par seconde

    // Pour remettre à zéro entre les coups
    void reset() {
        nodes = 0; cutoffs = 0; max_depth = 0; time_ms = 0.0; nps = 0;
    }
};