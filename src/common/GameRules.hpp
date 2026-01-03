#pragma once

#include "GameState.hpp"
#include "Move.hpp"
#include <vector>
#include <cmath>

namespace GameRules {

    // --- 1. FONCTIONS UTILITAIRES (Helpers) ---
    // "inline" permet au compilateur de copier le code directement pour aller vite.

    inline static bool is_game_over(const GameState& state) {
        // Condition de fin : un joueur a la majorité absolue (49 graines ou plus sur 96 ?)
        return (state.score_p1 >= 49 || state.score_p2 >= 49 || state.moves_count >= 400);
    }

    // Retourne vrai si le trou appartient au joueur 1 (Trous 1, 3, 5... -> Indices 0, 2, 4...)
    inline bool is_p1_hole(int hole_idx) {
        return (hole_idx % 2) == 0; 
    }

    // Retourne vrai si le trou appartient au joueur courant
    inline bool is_current_player_hole(int hole_idx, int player_id) {
        if (player_id == 1) return is_p1_hole(hole_idx);
        return !is_p1_hole(hole_idx); // Si c'est pas P1, c'est P2
    }

    // Vérifie si un joueur peut jouer (a-t-il des graines dans au moins un de ses trous ?)
    inline bool has_moves(const GameState& state, int player_id) {
        for (int i = 0; i < NB_HOLES; ++i) {
            // Si ce n'est pas mon trou, je zappe
            if (!is_current_player_hole(i, player_id)) continue;

            // Si j'ai au moins une graine (peu importe la couleur), je peux jouer
            // Note : Vérifie si ta règle autorise de jouer un trou avec seulement des transparentes
            // Si oui :
            if (state.count_total_seeds(i) > 0) return true;
        }
        return false;
    }

    // Calcul du trou suivant (modulo 16)
    inline int next(int hole_idx) {
        return (hole_idx + 1) % NB_HOLES;
    }

    // Calcul du trou précédent (pour la capture en arrière)
    inline int prev(int hole_idx) {
        return (hole_idx - 1 + NB_HOLES) % NB_HOLES;
    }

    // --- 2. LOGIQUE PRINCIPALE ---

    // Applique un coup sur un état du jeu.
    // Cette fonction MODIFIE l'état 'state' passé en paramètre.
    inline void apply_move(GameState& state, const Move& move, int player_id) {
        if (move.hole >= NB_HOLES) {
            return; // On ignore le coup fantôme
        }

        // A. PRÉLÈVEMENT DES GRAINES (Harvest)
        // -------------------------------------
        int seeds_trans = 0;
        int seeds_color = 0; // Rouges ou Bleues selon le coup
        int color_played = RED; // Par défaut
        
        // On regarde quel type de graines on joue
        if (move.type == MoveType::RED) {
            seeds_color = state.get_seeds(move.hole, RED);
            state.clear_seeds(move.hole, RED);
            color_played = RED;
        } 
        else if (move.type == MoveType::BLUE) {
            seeds_color = state.get_seeds(move.hole, BLUE);
            state.clear_seeds(move.hole, BLUE);
            color_played = BLUE;
        }
        else if (move.type == MoveType::TRANS_AS_RED) {
            seeds_trans = state.get_seeds(move.hole, TRANSPARENT);
            seeds_color = state.get_seeds(move.hole, RED);
            state.clear_seeds(move.hole, TRANSPARENT);
            state.clear_seeds(move.hole, RED);
            color_played = RED; // On imite le Rouge
        }
        else if (move.type == MoveType::TRANS_AS_BLUE) {
            seeds_trans = state.get_seeds(move.hole, TRANSPARENT);
            seeds_color = state.get_seeds(move.hole, BLUE);
            state.clear_seeds(move.hole, TRANSPARENT);
            state.clear_seeds(move.hole, BLUE);
            color_played = BLUE; // On imite le Bleu
        }

        // B. SEMAILLE (Sowing)
        // --------------------
        int current_hole = move.hole;
        
        // On doit semer d'abord TOUTES les transparentes, puis TOUTES les colorées.
        // On fait une boucle qui traite les deux phases.
        
        // Phase 1 : Transparentes (si y en a) -> Phase 2 : Colorées
        for (int phase = 0; phase < 2; ++phase) {
            int seeds_to_sow = (phase == 0) ? seeds_trans : seeds_color;
            int type_to_sow = (phase == 0) ? TRANSPARENT : color_played;

            while (seeds_to_sow > 0) {
                // Avancer d'un trou
                current_hole = next(current_hole);

                // Règle 1 : On ne sème jamais dans le trou de départ
                if (current_hole == move.hole) {
                    continue;
                }

                // Règle 2 (Filtre Bleu) : Si on joue BLEU (ou Transp as Bleu), 
                // on ne sème PAS chez soi.
                bool is_blue_mode = (color_played == BLUE);
                if (is_blue_mode && is_current_player_hole(current_hole, player_id)) {
                    continue; // On saute notre propre trou
                }

                // Si on arrive ici, on pose une graine
                state.add_seeds(current_hole, type_to_sow, 1);
                seeds_to_sow--;
            }
        }

        // C. CAPTURE (Prise)
        // ------------------
        // On part du dernier trou visité (current_hole) et on recule.
        int capture_hole = current_hole;
        bool keep_capturing = true;

        while (keep_capturing) {
            // Combien de graines au TOTAL dans ce trou ?
            int total = state.count_total_seeds(capture_hole);

            // Condition : 2 ou 3 graines
            if (total == 2 || total == 3) {
                // On capture tout !
                int captured = total;
                
                // On vide le trou (R, B et T)
                state.clear_seeds(capture_hole, RED);
                state.clear_seeds(capture_hole, BLUE);
                state.clear_seeds(capture_hole, TRANSPARENT);

                // On ajoute au score du joueur
                if (player_id == 1) state.score_p1 += captured;
                else state.score_p2 += captured;

                // On recule pour vérifier le trou d'avant (Rafle)
                capture_hole = prev(capture_hole);
            } else {
                // La chaîne est brisée
                keep_capturing = false;
            }
            
            // Sécurité anti-boucle infinie (si on a fait le tour complet et tout mangé)
            // Rare mais possible techniquement en théorie.
            if (capture_hole == current_hole && keep_capturing) { 
                 break; 
            }
        }
        
        // Mise à jour du compteur de tours
        state.moves_count++;
    }
};