#include "core/ansi.h"

#include <algorithm>

namespace ompe {
namespace {

bool is_wide_codepoint(uint32_t cp) noexcept {
    return (cp >= 0x1100u && cp <= 0x115Fu) ||
           (cp >= 0x2E80u && cp <= 0xA4CFu) ||
           (cp >= 0xAC00u && cp <= 0xD7A3u) ||
           (cp >= 0xF900u && cp <= 0xFAFFu) ||
           (cp >= 0xFE10u && cp <= 0xFE19u) ||
           (cp >= 0xFE30u && cp <= 0xFE6Fu) ||
           (cp >= 0xFF00u && cp <= 0xFF60u) ||
           (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
           (cp >= 0x1F300u && cp <= 0x1FAFFu) ||
           (cp >= 0x20000u && cp <= 0x3FFFDu);
}

} // namespace

AnsiTerminalBuffer::AnsiTerminalBuffer(uint32_t cols, uint32_t rows) {
    reset(cols, rows);
}

void AnsiTerminalBuffer::reset(uint32_t cols, uint32_t rows) {
    cols_ = std::max(16u, cols);
    rows_ = std::max(4u, rows);
    cells_.assign(static_cast<size_t>(cols_) * rows_, etch::TerminalCell{});
    clear();
}

void AnsiTerminalBuffer::clear() {
    for (auto& c : cells_) c = etch::TerminalCell{};
    first_row_ = 0;
    used_rows_ = 1;
    cur_col_ = 0;
    cur_row_ = 0;
    fg_ = {};
    bg_ = {};
    bold_ = false;
    underline_ = false;
    inverse_ = false;
    state_ = State::Normal;
    csi_param_count_ = 0;
    csi_has_digit_ = false;
    utf8_len_ = 0;
    utf8_expected_ = 0;
}

uint32_t AnsiTerminalBuffer::physical_row(uint32_t logical_row) const noexcept {
    return (first_row_ + logical_row) % rows_;
}

void AnsiTerminalBuffer::clear_row(uint32_t phys_row) {
    const size_t base = static_cast<size_t>(phys_row) * cols_;
    for (uint32_t c = 0; c < cols_; ++c) {
        cells_[base + c] = etch::TerminalCell{};
    }
}

void AnsiTerminalBuffer::newline() {
    cur_col_ = 0;
    if (cur_row_ + 1 < rows_) {
        ++cur_row_;
        if (cur_row_ + 1 > used_rows_) used_rows_ = cur_row_ + 1;
    } else {
        first_row_ = (first_row_ + 1) % rows_;
        clear_row(physical_row(rows_ - 1));
        used_rows_ = rows_;
    }
}

void AnsiTerminalBuffer::put_codepoint(uint32_t cp) {
    const uint8_t w = is_wide_codepoint(cp) ? 2u : 1u;
    if (cur_col_ + w > cols_) {
        newline();
    }
    const size_t base = static_cast<size_t>(physical_row(cur_row_)) * cols_;
    etch::TerminalCell cell{};
    cell.codepoint = cp;
    cell.foreground = fg_;
    cell.background = bg_;
    cell.width = w;
    cell.bold = bold_;
    cell.underline = underline_;
    cell.inverse = inverse_;
    cells_[base + cur_col_] = cell;
    if (w == 2 && cur_col_ + 1 < cols_) {
        etch::TerminalCell cont{};
        cont.codepoint = ' ';
        cont.foreground = fg_;
        cont.background = bg_;
        cont.width = 0;
        cells_[base + cur_col_ + 1] = cont;
    }
    cur_col_ += w;
}

void AnsiTerminalBuffer::apply_sgr() {
    if (csi_param_count_ == 0) {
        csi_params_[0] = 0;
        csi_param_count_ = 1;
    }
    for (uint32_t i = 0; i < csi_param_count_; ++i) {
        const uint32_t p = csi_params_[i];
        if (p == 0) {
            fg_ = {};
            bg_ = {};
            bold_ = false;
            underline_ = false;
            inverse_ = false;
        } else if (p == 1) {
            bold_ = true;
        } else if (p == 4) {
            underline_ = true;
        } else if (p == 7) {
            inverse_ = true;
        } else if (p == 22) {
            bold_ = false;
        } else if (p == 24) {
            underline_ = false;
        } else if (p == 27) {
            inverse_ = false;
        } else if (p >= 30 && p <= 37) {
            fg_ = {etch::TerminalColorMode::Ansi16, p - 30};
        } else if (p == 39) {
            fg_ = {};
        } else if (p >= 40 && p <= 47) {
            bg_ = {etch::TerminalColorMode::Ansi16, p - 40};
        } else if (p == 49) {
            bg_ = {};
        } else if (p >= 90 && p <= 97) {
            fg_ = {etch::TerminalColorMode::Ansi16, (p - 90) + 8};
        } else if (p >= 100 && p <= 107) {
            bg_ = {etch::TerminalColorMode::Ansi16, (p - 100) + 8};
        } else if ((p == 38 || p == 48) && i + 1 < csi_param_count_) {
            etch::TerminalColor& target = (p == 38) ? fg_ : bg_;
            if (csi_params_[i + 1] == 5 && i + 2 < csi_param_count_) {
                target = {etch::TerminalColorMode::Ansi256, csi_params_[i + 2] & 0xFFu};
                i += 2;
            } else if (csi_params_[i + 1] == 2 && i + 4 < csi_param_count_) {
                const uint32_t r = csi_params_[i + 2] & 0xFFu;
                const uint32_t g = csi_params_[i + 3] & 0xFFu;
                const uint32_t b = csi_params_[i + 4] & 0xFFu;
                target = {etch::TerminalColorMode::TrueColor, (r << 16) | (g << 8) | b};
                i += 4;
            }
        }
    }
}

void AnsiTerminalBuffer::apply_csi(char final_ch) {
    if (csi_has_digit_ && csi_param_count_ < 16) {
        ++csi_param_count_;
    }
    switch (final_ch) {
        case 'm':
            apply_sgr();
            break;
        case 'K': {
            const uint32_t mode = csi_param_count_ > 0 ? csi_params_[0] : 0;
            const size_t base = static_cast<size_t>(physical_row(cur_row_)) * cols_;
            if (mode == 0) {
                for (uint32_t c = cur_col_; c < cols_; ++c) cells_[base + c] = etch::TerminalCell{};
            } else if (mode == 2) {
                clear_row(physical_row(cur_row_));
            }
            break;
        }
        case 'J': {
            const uint32_t mode = csi_param_count_ > 0 ? csi_params_[0] : 0;
            if (mode == 2 || mode == 3) {
                for (uint32_t r = 0; r < rows_; ++r) clear_row(r);
                cur_col_ = 0;
                cur_row_ = 0;
                used_rows_ = 1;
            }
            break;
        }
        case 'G': {
            const uint32_t col = csi_param_count_ > 0 && csi_params_[0] > 0 ? csi_params_[0] - 1 : 0;
            cur_col_ = std::min(col, cols_ - 1);
            break;
        }
        case 'H':
        case 'f': {
            const uint32_t r = (csi_param_count_ > 0 && csi_params_[0] > 0) ? csi_params_[0] - 1 : 0;
            const uint32_t c = (csi_param_count_ > 1 && csi_params_[1] > 0) ? csi_params_[1] - 1 : 0;
            cur_row_ = std::min(r, rows_ - 1);
            cur_col_ = std::min(c, cols_ - 1);
            break;
        }
        default:
            break;
    }
}

void AnsiTerminalBuffer::feed(std::string_view utf8_chunk) {
    for (unsigned char ch : utf8_chunk) {
        if (state_ == State::Esc) {
            if (ch == '[') {
                state_ = State::Csi;
                csi_param_count_ = 0;
                csi_has_digit_ = false;
                for (auto& p : csi_params_) p = 0;
            } else if (ch == ']') {
                state_ = State::Osc;
            } else {
                state_ = State::Normal;
            }
            continue;
        }
        if (state_ == State::Csi) {
            if (ch >= '0' && ch <= '9') {
                if (csi_param_count_ < 16) {
                    csi_params_[csi_param_count_] = csi_params_[csi_param_count_] * 10u + static_cast<uint32_t>(ch - '0');
                    csi_has_digit_ = true;
                }
            } else if (ch == ';') {
                if (csi_param_count_ < 15) {
                    ++csi_param_count_;
                    csi_params_[csi_param_count_] = 0;
                }
                csi_has_digit_ = false;
            } else if (ch == '?' || ch == '>' || ch == '<' || ch == ' ') {
                // Ignore private mode prefixes
            } else if (ch >= 0x40u && ch <= 0x7Eu) {
                apply_csi(static_cast<char>(ch));
                state_ = State::Normal;
            } else {
                state_ = State::Normal;
            }
            continue;
        }
        if (state_ == State::Osc) {
            if (ch == 0x07u) {
                state_ = State::Normal;
            } else if (ch == 0x1Bu) {
                state_ = State::Esc;
            }
            continue;
        }

        // Normal state
        if (ch == 0x1Bu) {
            utf8_len_ = 0;
            utf8_expected_ = 0;
            state_ = State::Esc;
            continue;
        }
        if (ch == '\r') {
            cur_col_ = 0;
            continue;
        }
        if (ch == '\n') {
            newline();
            continue;
        }
        if (ch == '\t') {
            const uint32_t next = (cur_col_ + 4u) & ~3u;
            while (cur_col_ < next && cur_col_ < cols_) put_codepoint(' ');
            continue;
        }
        if (ch == '\b') {
            if (cur_col_ > 0) --cur_col_;
            continue;
        }
        if (ch < 0x20u) continue;

        if (ch < 0x80u) {
            utf8_len_ = 0;
            utf8_expected_ = 0;
            put_codepoint(ch);
        } else if ((ch & 0xC0u) != 0x80u) {
            utf8_buf_[0] = ch;
            utf8_len_ = 1;
            if ((ch & 0xE0u) == 0xC0u) utf8_expected_ = 2;
            else if ((ch & 0xF0u) == 0xE0u) utf8_expected_ = 3;
            else if ((ch & 0xF8u) == 0xF0u) utf8_expected_ = 4;
            else utf8_expected_ = 0;
        } else if (utf8_expected_ > 0 && utf8_len_ < utf8_expected_) {
            utf8_buf_[utf8_len_++] = ch;
            if (utf8_len_ == utf8_expected_) {
                uint32_t cp = 0xFFFDu;
                if (utf8_expected_ == 2) {
                    cp = ((utf8_buf_[0] & 0x1Fu) << 6) | (utf8_buf_[1] & 0x3Fu);
                } else if (utf8_expected_ == 3) {
                    cp = ((utf8_buf_[0] & 0x0Fu) << 12) | ((utf8_buf_[1] & 0x3Fu) << 6) | (utf8_buf_[2] & 0x3Fu);
                } else if (utf8_expected_ == 4) {
                    cp = ((utf8_buf_[0] & 0x07u) << 18) | ((utf8_buf_[1] & 0x3Fu) << 12) |
                         ((utf8_buf_[2] & 0x3Fu) << 6) | (utf8_buf_[3] & 0x3Fu);
                }
                put_codepoint(cp);
                utf8_len_ = 0;
                utf8_expected_ = 0;
            }
        }
    }
}

} // namespace ompe
