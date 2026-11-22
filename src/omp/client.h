#pragma once

#include "core/process.h"
#include "omp/protocol.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ompe {

enum class ConnectionStatus : uint8_t {
    Disconnected = 0,
    Starting,
    Handshaking,
    Connected,
    Error,
};

struct ToastMessage {
    uint64_t id = 0;
    std::string text;
    std::string level; // info, success, warning, error
    int64_t created_ms = 0;
    int64_t duration_ms = 3600;
};

struct ClientLaunchConfig {
    std::string omp_executable;
    std::string working_dir;
    std::string profile;
    std::string approval_mode;
    std::string resume_session_path;
};

// Full stateful OMP RPC v2 desktop client (thread-safe pipe I/O -> UI-thread poll).
class OmpClient {
public:
    OmpClient();
    ~OmpClient();

    void set_wake_callback(std::function<void()> fn) { wake_cb_ = std::move(fn); }

    bool connect(const ClientLaunchConfig& cfg);
    void disconnect();
    bool reconnect();

    // Drain arrived stdout/stderr frames on the UI thread. Returns true if state changed.
    bool poll();

    // High-level OMP actions
    std::string send_prompt(const std::string& message,
                            const std::vector<ImageAttachment>& images = {},
                            const std::string& streaming_behavior = "");
    std::string send_steer(const std::string& message, const std::vector<ImageAttachment>& images = {});
    std::string send_follow_up(const std::string& message, const std::vector<ImageAttachment>& images = {});
    std::string remove_queued(const std::string& message, const std::string& queue);
    std::string promote_queued(const std::string& message);
    std::string abort_turn();
    std::string abort_and_restore_queue();
    std::string new_session();
    std::string switch_session(const std::string& session_path);
    std::string fork_session(const std::string& entry_id = "");
    std::string navigate_tree(const std::string& entry_id, bool summarize = false);
    std::string rename_session(const std::string& name);
    std::string set_model(const std::string& provider, const std::string& model_id);
    std::string set_thinking_level(const std::string& level);
    std::string set_fast_mode(bool enabled);
    std::string set_slow_mode(bool enabled);
    std::string set_auto_compaction(bool enabled);
    std::string compact_now(const std::string& instructions = "");
    std::string refresh_state();
    std::string refresh_models();
    std::string refresh_tree();
    std::string refresh_subagents();
    std::string refresh_login_providers();
    std::string request_session_leases(const std::vector<std::string>& session_ids);
    std::string start_oauth_login(const std::string& provider_id);
    std::string query_logout_accounts(const std::string& provider_id);
    std::string logout_account(const std::string& provider_id, int64_t credential_id);
    std::string ask_btw(const std::string& question, const std::string& record_id = "");
    std::string cancel_btw(const std::string& record_id = "");
    std::string steer_subagent(const std::string& subagent_id, const std::string& message);
    std::string cancel_subagent(const std::string& subagent_id);
    std::string fetch_subagent_messages(const std::string& subagent_id, uint64_t from_byte = 0);
    std::string request_word_prediction(const std::string& text, uint32_t utf16_cursor);
    void send_word_prediction_feedback(const std::string& text, uint32_t utf16_cursor,
                                       const std::string& suggestion, bool accepted);
    std::string start_live_voice();
    std::string stop_live_voice();
    std::string toggle_live_mute(bool muted);

    // Extension UI responses (immediate side-channel)
    void reply_ui_value(const std::string& req_id, const std::string& value);
    void reply_ui_confirm(const std::string& req_id, bool confirmed);
    void reply_ui_cancel(const std::string& req_id, bool timed_out = false);
    void reply_ui_ask(const std::string& req_id,
                      const std::vector<std::tuple<std::string, std::vector<std::string>, std::string>>& answers);

    // Accessors for UI
    ConnectionStatus status() const noexcept { return status_; }
    const std::string& status_error() const noexcept { return status_error_; }
    const ClientLaunchConfig& launch_config() const noexcept { return launch_cfg_; }
    const ReadyInfo& ready_info() const noexcept { return ready_; }
    int negotiated_version() const noexcept { return negotiated_version_; }
    uint32_t process_pid() const noexcept { return proc_.pid(); }

    SessionState& state() noexcept { return state_; }
    const SessionState& state() const noexcept { return state_; }
    SessionStats& stats() noexcept { return stats_; }
    const SessionStats& stats() const noexcept { return stats_; }

    std::vector<ChatMessage>& messages() noexcept { return messages_; }
    const std::vector<ChatMessage>& messages() const noexcept { return messages_; }
    std::unordered_map<std::string, ToolExecution>& tool_executions() noexcept { return tool_execs_; }
    const std::unordered_map<std::string, ToolExecution>& tool_executions() const noexcept { return tool_execs_; }

    const std::vector<ModelInfo>& available_models() const noexcept { return available_models_; }
    const std::vector<std::string>& available_thinking_levels() const noexcept { return thinking_levels_; }
    const std::vector<SlashCommandInfo>& available_commands() const noexcept { return available_commands_; }
    const std::vector<LoginProviderInfo>& login_providers() const noexcept { return login_providers_; }
    const std::vector<LogoutAccountInfo>& logout_accounts() const noexcept { return logout_accounts_; }
    const std::vector<SubagentInfo>& subagents() const noexcept { return subagents_; }
    const std::vector<SessionTreeNode>& session_tree() const noexcept { return tree_roots_; }
    const std::string& tree_leaf_id() const noexcept { return tree_leaf_id_; }
    const std::vector<BtwRecord>& btw_records() const noexcept { return btw_records_; }
    const std::unordered_set<std::string>& held_leases() const noexcept { return held_leases_; }
    const std::deque<ExtensionUiRequest>& pending_ui_requests() const noexcept { return ui_requests_; }
    const std::unordered_map<std::string, std::string>& extension_statuses() const noexcept { return ext_statuses_; }
    const std::unordered_map<std::string, std::vector<std::string>>& extension_widgets() const noexcept { return ext_widgets_; }
    const std::deque<RpcLogEntry>& rpc_log() const noexcept { return rpc_log_; }
    const std::deque<ToastMessage>& toasts() const noexcept { return toasts_; }
    const std::string& ghost_prediction() const noexcept { return ghost_prediction_; }
    void clear_ghost_prediction() { ghost_prediction_.clear(); }

    // Live voice state
    bool live_voice_active() const noexcept { return live_voice_active_; }
    const std::string& live_voice_phase() const noexcept { return live_voice_phase_; }
    float live_input_level() const noexcept { return live_input_level_; }
    float live_output_level() const noexcept { return live_output_level_; }
    const std::string& live_transcript() const noexcept { return live_transcript_; }

    // Restored draft after `abort_and_restore_queue` or `set_editor_text`
    bool take_pending_editor_text(std::string& out_text);
    bool take_just_settled_flag() noexcept;
    bool take_session_list_dirty_flag() noexcept;
    bool take_oauth_url_event(ExtensionUiRequest& out_req);

    // Selected subagent transcript cache
    const std::vector<ChatMessage>& subagent_transcript(const std::string& subagent_id) const;

    void push_toast(const std::string& text, const std::string& level = "info", int64_t duration_ms = 3600);
    void dismiss_toast(uint64_t id);
    void prune_toasts();

    // Allow mock fixture loader to hydrate state directly for offline showcase / visual tests
    friend void populate_showcase_fixture(OmpClient& client);

private:
    struct IncomingItem {
        enum class Kind : uint8_t { StdoutLine, StderrLine, Exited };
        Kind kind = Kind::StdoutLine;
        std::string text;
        int exit_code = 0;
    };

    std::string next_req_id(const char* prefix = "req");
    bool send_raw(const Json& payload, std::string_view summary = "");
    void record_log(bool outbound, std::string_view type, std::string_view summary, std::string_view raw);
    void handle_frame(const Json& frame);
    void handle_response(const Json& frame);
    void handle_event(const Json& frame);
    void handle_message_update(const Json& frame);
    void handle_extension_ui(const Json& frame);
    void handle_subagent_frame(const Json& frame);
    void handle_btw_frame(const Json& frame);
    void handle_live_frame(const Json& frame);
    void start_initial_sync();
    void load_messages_page(const std::string& cursor);
    void rebuild_tool_execs_from_messages();

    Subprocess proc_;
    ClientLaunchConfig launch_cfg_;
    ConnectionStatus status_ = ConnectionStatus::Disconnected;
    std::string status_error_;
    ReadyInfo ready_;
    int negotiated_version_ = 1;
    bool delta_mode_active_ = false;

    std::mutex inbox_mu_;
    std::deque<IncomingItem> inbox_;
    std::function<void()> wake_cb_;
    RpcFrameReassembler reassembler_;

    uint64_t req_seq_ = 0;
    uint64_t toast_seq_ = 0;
    std::unordered_map<std::string, std::string> pending_cmd_meta_;

    // Pagination accumulation buffer
    std::vector<ChatMessage> paging_accum_;
    bool paging_active_ = false;

    SessionState state_;
    SessionStats stats_;
    std::vector<ChatMessage> messages_;
    std::unordered_map<std::string, size_t> msg_index_by_id_;
    std::unordered_map<std::string, ToolExecution> tool_execs_;

    std::vector<ModelInfo> available_models_;
    std::vector<std::string> thinking_levels_ = {"off", "low", "medium", "high"};
    std::vector<SlashCommandInfo> available_commands_;
    std::vector<LoginProviderInfo> login_providers_;
    std::vector<LogoutAccountInfo> logout_accounts_;
    std::vector<SubagentInfo> subagents_;
    std::unordered_map<std::string, std::vector<ChatMessage>> subagent_messages_;
    std::vector<SessionTreeNode> tree_roots_;
    std::string tree_leaf_id_;
    std::vector<BtwRecord> btw_records_;
    std::unordered_set<std::string> held_leases_;
    std::deque<ExtensionUiRequest> ui_requests_;
    std::unordered_map<std::string, std::string> ext_statuses_;
    std::unordered_map<std::string, std::vector<std::string>> ext_widgets_;
    std::deque<RpcLogEntry> rpc_log_;
    std::deque<ToastMessage> toasts_;

    std::string ghost_prediction_;
    std::string last_predict_req_id_;

    bool live_voice_active_ = false;
    std::string live_voice_phase_;
    float live_input_level_ = 0.0f;
    float live_output_level_ = 0.0f;
    std::string live_transcript_;

    bool has_pending_editor_text_ = false;
    std::string pending_editor_text_;
    bool just_settled_ = false;
    bool session_list_dirty_ = false;
    bool has_oauth_url_event_ = false;
    ExtensionUiRequest oauth_url_event_;
};

} // namespace ompe
