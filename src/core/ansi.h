#pragma once

#include <etch/etch.h>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ompe {

// Lightweight ANSI/VT100 terminal screen ring directly feeding etch::terminal_grid.
class AnsiTerminalBuffer {
public:
    AnsiTerminalBuffer(uint32_t cols = 96, uint32_t rows = 16);

    void reset(uint32_t cols, uint32_t rows);
    void clear();
    void feed(std::string_view utf8_chunk);

    uint32_t columns() const noexcept { return cols_; }
    uint32_t rows() const noexcept { return rows_; }
    uint32_t first_row() const noexcept { return first_row_; }
    uint32_t used_rows() const noexcept { return used_rows_; }
    int32_t cursor_col() const noexcept { return static_cast<int32_t>(cur_col_); }
    int32_t cursor_row() const noexcept { return static_cast<int32_t>(cur_row_); }
    const std::vector<etch::TerminalCell>& cells() const noexcept { return cells_; }

private:
    enum class State : uint8_t { Normal, Esc, Csi, Osc };

    void newline();
    void clear_row(uint32_t physical_row);
    void put_codepoint(uint32_t cp);
    void apply_csi(char final_ch);
    void apply_sgr();
    uint32_t physical_row(uint32_t logical_row) const noexcept;

    uint32_t cols_ = 96;
    uint32_t rows_ = 16;
    uint32_t first_row_ = 0;
    uint32_t used_rows_ = 1;
    uint32_t cur_col_ = 0;
    uint32_t cur_row_ = 0;

    etch::TerminalColor fg_{};
    etch::TerminalColor bg_{};
    bool bold_ = false;
    bool underline_ = false;
    bool inverse_ = false;

    State state_ = State::Normal;
    uint32_t csi_params_[16] = {};
    uint32_t csi_param_count_ = 0;
    bool csi_has_digit_ = false;
    uint8_t utf8_buf_[4] = {};
    uint8_t utf8_len_ = 0;
    uint8_t utf8_expected_ = 0;

    std::vector<etch::TerminalCell> cells_;
};

} // namespace ompe
