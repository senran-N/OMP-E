#pragma once

#include "core/ansi.h"
#include "core/json.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ompe {

struct ReadyInfo {
    int protocol_version = 1;
    std::vector<int> supported_versions;
    size_t max_frame_bytes = 1048576;
    size_t max_reassembled_bytes = 67108864;
    bool supports_bash_exclude = false;
};

struct ModelInfo {
    std::string provider;
    std::string id;
    std::string name;
    std::string api;
    std::string base_url;
    bool reasoning = false;
    uint64_t context_window = 0;
    uint64_t max_tokens = 0;
    double cost_input = 0.0;
    double cost_output = 0.0;
    bool supports_image = false;

    std::string selector() const {
        return provider.empty() ? id : (provider + "/" + id);
    }
};

struct SlashCommandInfo {
    std::string name;
    std::string description;
    std::string source; // builtin, extension, prompt, skill, command
    std::string input_hint;
};

struct UsageLimitInfo {
    bool active = false;
    std::string stage; // wrap_up, low_priority
    int64_t resets_at_sec = 0;
    int allowance_left_percent = -1;
};

struct ContextUsageInfo {
    uint64_t tokens = 0;
    uint64_t context_window = 0;
    double percent = 0.0;
    bool valid = false;
};

struct SessionStats {
    uint32_t user_messages = 0;
    uint32_t assistant_messages = 0;
    uint32_t tool_calls = 0;
    uint64_t input_tokens = 0;
    uint64_t output_tokens = 0;
    uint64_t cache_read_tokens = 0;
    uint64_t cache_write_tokens = 0;
    uint64_t total_tokens = 0;
    double cost_usd = 0.0;
};

struct TodoItem {
    std::string id;
    std::string content;
    std::string status; // pending, in_progress, completed
};

struct TodoPhase {
    std::string name;
    std::vector<TodoItem> tasks;
};

struct SessionState {
    ModelInfo model;
    std::string thinking_level = "off";
    bool is_streaming = false;
    bool is_compacting = false;
    std::string steering_mode = "one-at-a-time";
    std::string follow_up_mode = "one-at-a-time";
    std::string interrupt_mode = "immediate";
    std::string session_file;
    std::string session_id;
    std::string session_name;
    bool auto_compaction = true;
    bool fast_mode_enabled = false;
    bool fast_mode_active = false;
    bool slow_mode_supported = false;
    bool slow_mode_enabled = false;
    double tokens_per_second = 0.0;
    uint32_t message_count = 0;
    bool has_pending_async_work = false;
    bool is_settled = true;
    std::vector<std::string> steering_queue;
    std::vector<std::string> follow_up_queue;
    uint32_t live_steered = 0;
    ContextUsageInfo context_usage;
    UsageLimitInfo usage_limit;
    std::string goal;
    std::vector<TodoPhase> todo_phases;
};

struct ImageAttachment {
    std::string mime_type = "image/png";
    std::string base64_data;
    std::string filename;
    uint32_t width = 0;
    uint32_t height = 0;
};

enum class ContentBlockKind : uint8_t {
    Text = 0,
    Thinking,
    RedactedThinking,
    ToolCall,
    Image,
    Notice,
};

struct ContentBlock {
    ContentBlockKind kind = ContentBlockKind::Text;
    std::string text;             // text or thinking body
    std::string tool_call_id;     // for ToolCall
    std::string tool_name;        // for ToolCall
    std::string tool_args_json;   // raw JSON args (streamed or final)
    std::string tool_intent;      // human-friendly intent summary
    std::string mime_type;        // for Image
    std::string image_b64;        // for Image
    bool complete = true;
    int64_t started_ms = 0;
    int64_t ended_ms = 0;
};

struct ToolExecution {
    std::string tool_call_id;
    std::string tool_name;
    std::string intent;
    std::string args_summary;
    std::string args_json;
    std::string output_text;
    std::string diff_text;        // Structured diff from result.details.diff / displayContent / args preview
    bool running = true;
    bool is_error = false;
    int64_t started_ms = 0;
    int64_t ended_ms = 0;
    std::shared_ptr<AnsiTerminalBuffer> terminal; // populated for bash/shell tools
};

struct ChatMessage {
    std::string id;
    std::string role; // "user", "assistant", "toolResult", "bashExecution", "notice", "custom", "compactionSummary", "branchSummary"
    std::string custom_type;
    std::vector<ContentBlock> blocks;
    std::string tool_call_id;
    std::string tool_name;
    std::string tool_diff;
    std::string tool_path;
    bool is_error = false;
    bool hidden = false; // true when OMP marks display=false or internal context-only notice
    std::string model_id;
    std::string stop_reason;
    std::string error_message;
    int64_t timestamp_ms = 0;
    double cost_usd = 0.0;
    uint64_t output_tokens = 0;
    bool streaming = false;
    uint64_t render_revision = 1; // bumped on text delta to sync InlayDoc incrementally
};

struct AskOption {
    std::string label;
    std::string description;
    std::string preview;
};

struct AskQuestion {
    std::string id;
    std::string header;
    std::string question;
    std::vector<AskOption> options;
    bool multi = false;
    int recommended = -1;
};

struct ExtensionUiRequest {
    std::string id;
    std::string method; // ask, select, confirm, input, editor, open_url, notify, setStatus, setWidget, cancel
    std::string title;
    std::string message;
    std::string placeholder;
    std::string prefill;
    std::string url;
    std::string launch_url;
    std::string instructions;
    std::string notify_type;
    std::string status_key;
    std::string status_text;
    std::string widget_key;
    std::vector<std::string> widget_lines;
    std::string target_id;
    std::vector<std::string> options;
    std::vector<std::string> option_descriptions;
    std::vector<int> checked_indices;
    std::string selection_marker; // radio, checkbox
    int initial_index = 0;
    bool secret = false;
    bool prompt_style = false;
    int timeout_ms = 0;
    std::vector<AskQuestion> questions;
    int64_t received_ms = 0;
};

struct SubagentInfo {
    std::string id;
    std::string agent;
    std::string agent_source;
    std::string description;
    std::string task;
    std::string status; // pending, running, active, completed, failed, aborted
    std::string session_file;
    std::string parent_tool_call_id;
    std::string resolved_model;
    std::string current_tool;
    std::string current_tool_args;
    std::string last_intent;
    uint32_t tool_count = 0;
    uint32_t requests = 0;
    uint64_t tokens = 0;
    uint64_t context_tokens = 0;
    uint64_t context_window = 0;
    double cost_usd = 0.0;
    int64_t duration_ms = 0;
    int64_t last_update_ms = 0;
    bool can_cancel = false;
    bool can_steer = false;
    std::vector<std::string> recent_output;
};

struct SessionTreeNode {
    std::string id;
    std::string parent_id;
    std::string type;
    std::string role;
    std::string preview;
    std::string label;
    std::string timestamp;
    std::vector<SessionTreeNode> children;

    bool is_conversation_turn() const noexcept {
        if (type == "compaction" || type == "branch_summary") return true;
        if (type == "message") return role == "user" || role == "assistant";
        return false;
    }
};

struct BtwTurn {
    std::string question;
    std::string answer;
    std::string status; // running, complete, cancelled, error, interrupted
    std::string error;
    int64_t created_at = 0;
};

struct BtwRecord {
    std::string id;
    std::string leaf_id;
    std::string title;
    std::string question;
    std::string answer;
    std::string status;
    std::string error;
    int64_t created_at = 0;
    std::vector<BtwTurn> follow_ups;
};

struct LoginProviderInfo {
    std::string id;
    std::string name;
    bool available = true;
    bool authenticated = false;
};

struct LogoutAccountInfo {
    int64_t credential_id = 0;
    std::string provider;
    std::string label;
    std::string detail;
    std::string type; // api_key, oauth
    bool active = false;
};

struct RpcLogEntry {
    int64_t timestamp_ms = 0;
    bool outbound = false;
    std::string frame_type;
    std::string summary;
    std::string raw_preview;
};

// Strict Protocol v2 chunk reassembler matching packages/coding-agent/src/modes/rpc/rpc-frame.ts
class RpcFrameReassembler {
public:
    enum class Result : uint8_t {
        FrameReady,  // out_json contains a complete JSON frame
        NeedMore,    // buffered an rpc_chunk, waiting for remaining chunks
        Error,       // malformed JSON or violated chunk sequence
    };

    void reset();
    void set_max_reassembled_bytes(size_t max_bytes) noexcept { max_bytes_ = max_bytes; }

    Result feed_line(std::string_view line, Json& out_json, std::string& out_error);

private:
    bool active_ = false;
    std::string chunk_id_;
    uint32_t expected_index_ = 0;
    uint32_t expected_count_ = 0;
    size_t expected_bytes_ = 0;
    size_t max_bytes_ = 67108864;
    std::string buffer_;
};

// Wire deserialization helpers
ReadyInfo parse_ready_frame(const Json& j);
ModelInfo parse_model_info(const Json& j);
SessionState parse_session_state(const Json& j);
SessionStats parse_session_stats(const Json& j);
ChatMessage parse_agent_message(const Json& j, std::string_view fallback_id = "");
ExtensionUiRequest parse_extension_ui_request(const Json& j);
SubagentInfo parse_subagent_snapshot(const Json& j);
SessionTreeNode parse_tree_node(const Json& j);
BtwRecord parse_btw_record(const Json& j);
std::string summarize_tool_args(std::string_view tool_name, const Json& args);
std::string strip_system_xml_blocks(std::string_view text);

// Exact outbound command builders (matching rpc-types.ts)
Json cmd_negotiate_protocol(const std::string& id, int version = 2);
Json cmd_set_ask_dialog(const std::string& id, bool enabled);
Json cmd_set_event_filter(const std::string& id, bool delta_mode);
Json cmd_set_subagent_subscription(const std::string& id, const std::string& level);
Json cmd_simple(const std::string& id, const std::string& type);
Json cmd_prompt(const std::string& id, const std::string& message,
                const std::vector<ImageAttachment>& images,
                const std::string& streaming_behavior = "");
Json cmd_steer(const std::string& id, const std::string& message, const std::vector<ImageAttachment>& images);
Json cmd_follow_up(const std::string& id, const std::string& message, const std::vector<ImageAttachment>& images);
Json cmd_remove_queued(const std::string& id, const std::string& message, const std::string& queue);
Json cmd_promote_queued(const std::string& id, const std::string& message);
Json cmd_set_model(const std::string& id, const std::string& provider, const std::string& model_id);
Json cmd_set_thinking(const std::string& id, const std::string& level);
Json cmd_set_fast_mode(const std::string& id, bool enabled);
Json cmd_set_slow_mode(const std::string& id, bool enabled);
Json cmd_set_auto_compaction(const std::string& id, bool enabled);
Json cmd_compact(const std::string& id, const std::string& instructions = "");
Json cmd_switch_session(const std::string& id, const std::string& session_path);
Json cmd_open_session(const std::string& id, const std::string& session_dir);
Json cmd_new_session(const std::string& id, const std::string& parent_session = "");
Json cmd_fork(const std::string& id, const std::string& entry_id = "");
Json cmd_navigate_tree(const std::string& id, const std::string& entry_id, bool summarize = false);
Json cmd_set_session_name(const std::string& id, const std::string& name);
Json cmd_get_messages_page(const std::string& id, const std::string& cursor = "", int limit = 100);
Json cmd_get_session_lease(const std::string& id, const std::vector<std::string>& session_ids);
Json cmd_login(const std::string& id, const std::string& provider_id);
Json cmd_get_logout_accounts(const std::string& id, const std::string& provider_id);
Json cmd_logout(const std::string& id, const std::string& provider_id, int64_t credential_id);
Json cmd_btw(const std::string& id, const std::string& question, const std::string& record_id = "");
Json cmd_btw_cancel(const std::string& id, const std::string& record_id = "");
Json cmd_predict_word(const std::string& id, const std::string& text, uint32_t utf16_cursor);
Json cmd_predict_word_feedback(const std::string& id, const std::string& text, uint32_t utf16_cursor,
                               const std::string& suggestion, bool accepted);
Json cmd_cancel_subagent(const std::string& id, const std::string& subagent_id);
Json cmd_steer_subagent(const std::string& id, const std::string& subagent_id, const std::string& message);
Json cmd_get_subagent_messages(const std::string& id, const std::string& subagent_id, uint64_t from_byte = 0);
Json cmd_bash(const std::string& id, const std::string& command, bool exclude_from_context = false);
Json cmd_live_start(const std::string& id);
Json cmd_live_stop(const std::string& id);
Json cmd_live_mute(const std::string& id, bool muted);

// Side-channel extension UI responses
Json ui_response_value(const std::string& id, const std::string& value);
Json ui_response_confirm(const std::string& id, bool confirmed);
Json ui_response_cancel(const std::string& id, bool timed_out = false);
Json ui_response_ask(const std::string& id,
                     const std::vector<std::tuple<std::string, std::vector<std::string>, std::string>>& answers);

} // namespace ompe
