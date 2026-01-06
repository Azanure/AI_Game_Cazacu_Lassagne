#pragma once
#include <array>
#include <cstdint>

constexpr int NB_HOLES = 16;
constexpr int NB_COLORS = 3;
constexpr int TOTAL_CELLS = NB_HOLES * NB_COLORS;

constexpr int RED = 0;
constexpr int BLUE = 1;
constexpr int TRANSPARENT = 2;

struct GameState {
    std::array<uint8_t, TOTAL_CELLS> board;
    uint16_t score_p1;
    uint16_t score_p2;
    uint16_t moves_count;

    GameState() {
        score_p1 = 0; score_p2 = 0; moves_count = 0;
        for (int i = 0; i < TOTAL_CELLS; ++i) board[i] = 2;
    }

    // Accesseurs
    inline uint8_t get_seeds(int hole_idx, int color) const {
        return board[hole_idx * 3 + color];
    }

    inline void set_seeds(int hole_idx, int color, int count) {
        board[hole_idx * 3 + color] = static_cast<uint8_t>(count);
    }

    inline void add_seeds(int hole_idx, int color, int amount) {
        board[hole_idx * 3 + color] += static_cast<uint8_t>(amount);
    }

    // Le voilà, indispensable pour GameRules
    inline void clear_seeds(int hole_idx, int color) {
        board[hole_idx * 3 + color] = 0;
    }

    inline int count_total_seeds(int hole_idx) const {
        int base_idx = hole_idx * 3;
        return board[base_idx + RED] + board[base_idx + BLUE] + board[base_idx + TRANSPARENT];
    }

    // Pour l'IA (compte tout le plateau)
    inline int count_all_seeds() const {
        int total = 0;
        for (int i = 0; i < TOTAL_CELLS; ++i) total += board[i];
        return total;
    }

    inline bool IsGameOver() const {
        return score_p1 >= 49 || score_p2 >= 49 || moves_count >= 400 || count_all_seeds() < 10;
    }
};