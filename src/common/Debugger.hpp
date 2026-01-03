void debug_evaluate_state(const GameState& state, int player_id) {
    // --- PARAMETRES A SYNCHRONISER AVEC V7/Search.hpp ---
    const int FACTEUR_SCORE = 5000;  // Le Roi (Augmenté)
    const int FACTEUR_KRU = 5;     // Très faible (Divisé par 10)
    const int FACTEUR_AMMO = 2;     // Très faible (Divisé par 5)
    const int BONUS_CIBLE = 10;    // Juste une indication (Divisé par 15 !)
    const int MALUS_GENE = -20;   // Petit malus pour les gros trous adverses
    // ----------------------------------------------------

    int score_diff = (player_id == 1) ? (state.score_p1 - state.score_p2) : (state.score_p2 - state.score_p1);
    int mat_score = score_diff * FACTEUR_SCORE;

    int bonus_kru = 0;
    int bonus_ammo = 0;
    int bonus_target = 0;
    int malus_opp = 0;

    std::cout << "\n--- ANALYSE DE L'ETAT (Vue Joueur " << player_id << ") ---\n";

    // 1. ALERTE RETARD MATERIEL
    if (score_diff < 0) {
        std::cout << " !!! ALERTE CRITIQUE : RETARD DE " << (-score_diff) << " GRAINES !!!\n";
    }
    else if (score_diff > 0) {
        std::cout << " ... Avance confortable de " << score_diff << " graines.\n";
    }
    else {
        std::cout << " ... Egalite materielle parfaite.\n";
    }
    std::cout << "Score Reel : " << state.score_p1 << "-" << state.score_p2 << "\n";

    for (int i = 0; i < NB_HOLES; ++i) {
        int seeds = state.count_total_seeds(i);
        if (seeds == 0) continue;

        bool is_my_hole = GameRules::is_current_player_hole(i, player_id);

        if (is_my_hole) {
            // CHEZ MOI
            // KRU (Accumulation légère)
            if (seeds > 12) {
                int val = (seeds - 12) * FACTEUR_KRU;
                bonus_kru += val;
                // On n'affiche que si le bonus est significatif pour ne pas polluer
                if (val > 0) std::cout << "  + Petit Bonus KRU (Trou " << i << ") : " << val << "\n";
            }

            // MUNITIONS
            int blues = state.get_seeds(i, BLUE);
            int trans = state.get_seeds(i, TRANSPARENT);
            if (blues + trans > 0) {
                int val = (blues + trans) * FACTEUR_AMMO;
                bonus_ammo += val;
            }

        }
        else {
            // CHEZ L'ADVERSAIRE

            // CIBLES (Tie-Breaker)
            if (seeds == 1 || seeds == 2) {
                int val = BONUS_CIBLE;
                bonus_target += val;
                std::cout << "  + Cible Potentielle (Trou " << i << ") : " << val << "\n";
            }

            // GENE
            if (seeds > 15) {
                int val = MALUS_GENE;
                malus_opp += val;
                std::cout << "  - Gêne (Adversaire fort Trou " << i << ") : " << val << "\n";
            }
        }
    }

    // NOTE : Plus de "Mode Survie" ici, l'évaluation est linéaire.
    int eval_total = mat_score + bonus_kru + bonus_ammo + bonus_target + malus_opp;

    std::cout << "------------------------------------------\n";
    std::cout << "MATERIEL       : " << std::setw(8) << mat_score << "\n";
    std::cout << "BONUS STRAT    : " << std::setw(8) << (bonus_kru + bonus_ammo + bonus_target + malus_opp) << "\n";
    std::cout << "------------------------------------------\n";
    std::cout << "EVALUATION V7  : " << std::setw(8) << eval_total << "\n";

    if (eval_total < 0) {
        std::cout << ">>> CONCLUSION : SITUATION PERDANTE <<<\n";
    }
    else {
        std::cout << ">>> CONCLUSION : SITUATION GAGNANTE <<<\n";
    }
    std::cout << "------------------------------------------\n";
}