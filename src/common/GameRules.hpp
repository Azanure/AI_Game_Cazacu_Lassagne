#pragma once
#include "GameState.hpp"
#include "Move.hpp"

namespace GameRules {

    inline bool is_p1_hole(int hole_idx) { return (hole_idx % 2) == 0; }

    inline bool is_current_player_hole(int hole_idx, int player_id) {
        if (player_id == 1) return is_p1_hole(hole_idx);
        return !is_p1_hole(hole_idx);
    }

    inline bool has_moves(const GameState& state, int player_id) {
        for (int i = 0; i < NB_HOLES; ++i) {
            if (!is_current_player_hole(i, player_id)) continue;
            if (state.count_total_seeds(i) > 0) return true;
        }
        return false;
    }

    inline bool is_game_over(const GameState& state) {
        return state.IsGameOver();
    }

    inline int next(int hole_idx) { return (hole_idx + 1) % NB_HOLES; }
    inline int prev(int hole_idx) { return (hole_idx - 1 + NB_HOLES) % NB_HOLES; }

    inline void apply_move(GameState& state, const Move& move, int player_id) {
        if (move.hole >= NB_HOLES) return;

        int seeds_trans = 0;
        int seeds_color = 0;
        int color_played = RED;

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
            color_played = RED;
        }
        else if (move.type == MoveType::TRANS_AS_BLUE) {
            seeds_trans = state.get_seeds(move.hole, TRANSPARENT);
            seeds_color = state.get_seeds(move.hole, BLUE);
            state.clear_seeds(move.hole, TRANSPARENT);
            state.clear_seeds(move.hole, BLUE);
            color_played = BLUE;
        }

        int current_hole = move.hole;

        // Semaille en 2 phases
        for (int phase = 0; phase < 2; ++phase) {
            int seeds_to_sow = (phase == 0) ? seeds_trans : seeds_color;
            int type_to_sow = (phase == 0) ? TRANSPARENT : color_played;

            while (seeds_to_sow > 0) {
                current_hole = next(current_hole);
                if (current_hole == move.hole) continue; // Sauter trou départ

                // Sauter trou adversaire si on joue Bleu
                bool is_blue_mode = (color_played == BLUE);
                if (is_blue_mode && is_current_player_hole(current_hole, player_id)) continue;

                state.add_seeds(current_hole, type_to_sow, 1);
                seeds_to_sow--;
            }
        }

        // Capture
        int capture_hole = current_hole;
        bool keep_capturing = true;
        int loops = 0;

        while (keep_capturing && loops < NB_HOLES) {
            int total = state.count_total_seeds(capture_hole);
            if (total == 2 || total == 3) {
                int captured = total;
                state.clear_seeds(capture_hole, RED);
                state.clear_seeds(capture_hole, BLUE);
                state.clear_seeds(capture_hole, TRANSPARENT);

                if (player_id == 1) state.score_p1 += captured;
                else state.score_p2 += captured;

                capture_hole = prev(capture_hole);
            }
            else {
                keep_capturing = false;
            }
            loops++;
        }
        state.moves_count++;
    }
}