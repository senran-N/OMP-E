#include "omp/client.h"
#include "core/platform.h"

#include <algorithm>

namespace ompe {
namespace {

std::string extract_content_text(const Json& content) {
    if (content.is_string()) return content.as_string();
    if (!content.is_array()) return {};
    std::string out;
    for (const auto& item : content.as_array()) {
        if (item["type"].as_string() == "text") {
            if (!out.empty()) out.push_back('\n');
            out += item["text"].as_string();
        }
    }
    return out;
}

std::string prefix_lines(const std::string& text, char sign, int max_lines = 240) {
    if (text.empty()) return {};
    std::string out;
    size_t pos = 0;
    int count = 0;
    while (pos <= text.size() && count < max_lines) {
        const size_t nl = text.find('\n', pos);
        std::string_view ln = (nl == std::string::npos)
            ? std::string_view(text).substr(pos)
            : std::string_view(text).substr(pos, nl - pos);
        if (!ln.empty() && ln.back() == '\r') ln.remove_suffix(1);
        if (!out.empty()) out.push_back('\n');
        out.push_back(sign);
        out.append(ln);
        ++count;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return out;
}

std::string extract_diff_from_details(const Json& details) {
    if (!details.is_object()) return {};
    if (details["diff"].is_string() && !details["diff"].as_string().empty()) {
        return details["diff"].as_string();
    }
    if (details["displayContent"].is_string() && !details["displayContent"].as_string().empty()) {
        return details["displayContent"].as_string();
    }
    if (details["perFileResults"].is_array()) {
        std::string merged;
        for (const auto& pf : details["perFileResults"].as_array()) {
            if (!pf.is_object()) continue;
            const std::string& d = pf["diff"].as_string();
            if (!d.empty()) {
                if (!merged.empty()) merged.push_back('\n');
                if (pf["path"].is_string() && !pf["path"].as_string().empty()) {
                    merged += "+++ " + pf["path"].as_string() + "\n";
                }
                merged += d;
            }
        }
        if (!merged.empty()) return merged;
    }
    if (details["op"].as_string() == "delete" && details["oldText"].is_string() && !details["oldText"].as_string().empty()) {
        return prefix_lines(details["oldText"].as_string(), '-');
    }
    return {};
}

std::string extract_diff_from_args(std::string_view tool_name, const Json& args) {
    if (!args.is_object()) return {};
    if (args["previewDiff"].is_string() && !args["previewDiff"].as_string().empty()) {
        return args["previewDiff"].as_string();
    }
    if (args["diff"].is_string() && !args["diff"].as_string().empty()) {
        return args["diff"].as_string();
    }
    if (args["patch"].is_string() && !args["patch"].as_string().empty()) {
        return args["patch"].as_string();
    }
    if (args["edits"].is_array()) {
        std::string merged;
        for (const auto& ed : args["edits"].as_array()) {
            if (!ed.is_object()) continue;
            if (ed["diff"].is_string() && !ed["diff"].as_string().empty()) {
                if (!merged.empty()) merged.push_back('\n');
                merged += ed["diff"].as_string();
            } else {
                const std::string old_t = ed["old_text"].is_string() ? ed["old_text"].as_string() : ed["oldText"].as_string_copy();
                const std::string new_t = ed["new_text"].is_string() ? ed["new_text"].as_string() : ed["newText"].as_string_copy();
                if (!old_t.empty() || !new_t.empty()) {
                    if (!merged.empty()) merged += "\n@@\n";
                    if (!old_t.empty()) merged += prefix_lines(old_t, '-');
                    if (!old_t.empty() && !new_t.empty()) merged.push_back('\n');
                    if (!new_t.empty()) merged += prefix_lines(new_t, '+');
                }
            }
        }
        if (!merged.empty()) return merged;
    }
    const std::string old_t = args["old_text"].is_string() ? args["old_text"].as_string() :
                              (args["oldText"].is_string() ? args["oldText"].as_string() : args["old_str"].as_string_copy());
    const std::string new_t = args["new_text"].is_string() ? args["new_text"].as_string() :
                              (args["newText"].is_string() ? args["newText"].as_string() : args["new_str"].as_string_copy());
    if (!old_t.empty() || !new_t.empty()) {
        std::string out;
        if (!old_t.empty()) out += prefix_lines(old_t, '-');
        if (!old_t.empty() && !new_t.empty()) out.push_back('\n');
        if (!new_t.empty()) out += prefix_lines(new_t, '+');
        return out;
    }
    if ((tool_name == "write" || tool_name == "write_file") && args["content"].is_string() && !args["content"].as_string().empty()) {
        return prefix_lines(args["content"].as_string(), '+');
    }
    return {};
}

} // namespace

OmpClient::OmpClient() = default;
OmpClient::~OmpClient() { disconnect(); }

std::string OmpClient::next_req_id(const char* prefix) {
    return std::string(prefix) + "_" + std::to_string(++req_seq_);
}

void OmpClient::record_log(bool outbound, std::string_view type, std::string_view summary, std::string_view raw) {
    RpcLogEntry e;
    e.timestamp_ms = unix_time_ms();
    e.outbound = outbound;
    e.frame_type = std::string(type);
    e.summary = std::string(summary);
    e.raw_preview = raw.size() > 360 ? (std::string(raw.substr(0, 357)) + "...") : std::string(raw);
    rpc_log_.push_back(std::move(e));
    while (rpc_log_.size() > 240) rpc_log_.pop_front();
}

void OmpClient::push_toast(const std::string& text, const std::string& level, int64_t duration_ms) {
    ToastMessage t;
    t.id = ++toast_seq_;
    t.text = text;
    t.level = level;
    t.created_ms = unix_time_ms();
    t.duration_ms = duration_ms;
    toasts_.push_back(std::move(t));
    while (toasts_.size() > 5) toasts_.pop_front();
}

void OmpClient::dismiss_toast(uint64_t id) {
    toasts_.erase(
        std::remove_if(toasts_.begin(), toasts_.end(), [id](const ToastMessage& t) { return t.id == id; }),
        toasts_.end());
}

void OmpClient::prune_toasts() {
    const int64_t now = unix_time_ms();
    toasts_.erase(
        std::remove_if(toasts_.begin(), toasts_.end(), [now](const ToastMessage& t) {
            return t.duration_ms > 0 && (now - t.created_ms) >= t.duration_ms;
        }),
        toasts_.end());
}

bool OmpClient::connect(const ClientLaunchConfig& cfg) {
    disconnect();
    launch_cfg_ = cfg;
    status_ = ConnectionStatus::Starting;
    status_error_.clear();
    reassembler_.reset();
    negotiated_version_ = 1;
    delta_mode_active_ = false;
    pending_cmd_meta_.clear();
    ui_requests_.clear();
    ext_statuses_.clear();
    ext_widgets_.clear();
    live_voice_active_ = false;

    ProcessOptions po;
    po.executable = cfg.omp_executable.empty() ? find_omp_executable() : cfg.omp_executable;
    po.working_dir = cfg.working_dir;
    po.args = {"--mode", "rpc-ui"};
    if (!cfg.profile.empty()) {
        po.args.push_back("--profile");
        po.args.push_back(cfg.profile);
    }
    if (!cfg.approval_mode.empty()) {
        po.args.push_back("--approval-mode");
        po.args.push_back(cfg.approval_mode);
    }
    if (!cfg.resume_session_path.empty()) {
        po.args.push_back("--resume");
        po.args.push_back(cfg.resume_session_path);
    }
    po.env_overrides.emplace_back("PI_RPC_EMIT_TITLE", "1");

    std::string err;
    const bool started = proc_.start(
        po,
        [this](std::string line) {
            {
                std::lock_guard<std::mutex> lk(inbox_mu_);
                inbox_.push_back({IncomingItem::Kind::StdoutLine, std::move(line), 0});
            }
            if (wake_cb_) wake_cb_();
        },
        [this](std::string line) {
            {
                std::lock_guard<std::mutex> lk(inbox_mu_);
                inbox_.push_back({IncomingItem::Kind::StderrLine, std::move(line), 0});
            }
            if (wake_cb_) wake_cb_();
        },
        [this](int code) {
            {
                std::lock_guard<std::mutex> lk(inbox_mu_);
                inbox_.push_back({IncomingItem::Kind::Exited, {}, code});
            }
            if (wake_cb_) wake_cb_();
        },
        &err);

    if (!started) {
        status_ = ConnectionStatus::Error;
        status_error_ = err.empty() ? "Unable to start omp binary" : err;
        record_log(false, "process_error", status_error_, status_error_);
        return false;
    }

    status_ = ConnectionStatus::Handshaking;
    record_log(true, "spawn", "omp --mode rpc-ui", po.executable);
    return true;
}

void OmpClient::disconnect() {
    proc_.stop(600);
    {
        std::lock_guard<std::mutex> lk(inbox_mu_);
        inbox_.clear();
    }
    status_ = ConnectionStatus::Disconnected;
    state_.is_streaming = false;
    state_.is_settled = true;
}

bool OmpClient::reconnect() {
    return connect(launch_cfg_);
}

bool OmpClient::send_raw(const Json& payload, std::string_view summary) {
    if (!proc_.is_running()) return false;
    const std::string line = payload.dump(0);
    const std::string& type = payload["type"].as_string();
    record_log(true, type, summary.empty() ? std::string_view(type) : summary, line);
    return proc_.write_line(line);
}

bool OmpClient::poll() {
    std::deque<IncomingItem> batch;
    {
        std::lock_guard<std::mutex> lk(inbox_mu_);
        if (inbox_.empty()) return false;
        batch.swap(inbox_);
    }

    bool changed = false;
    for (auto& item : batch) {
        changed = true;
        if (item.kind == IncomingItem::Kind::StderrLine) {
            if (!item.text.empty()) {
                record_log(false, "stderr", item.text, item.text);
            }
            continue;
        }
        if (item.kind == IncomingItem::Kind::Exited) {
            state_.is_streaming = false;
            state_.is_settled = true;
            if (status_ != ConnectionStatus::Disconnected) {
                status_ = (item.exit_code == 0) ? ConnectionStatus::Disconnected : ConnectionStatus::Error;
                if (item.exit_code != 0) {
                    status_error_ = "omp process exited with code " + std::to_string(item.exit_code);
                    push_toast(status_error_, "error", 5000);
                }
            }
            record_log(false, "process_exit", "exit code " + std::to_string(item.exit_code), "");
            continue;
        }

        Json frame;
        std::string err;
        const auto res = reassembler_.feed_line(item.text, frame, err);
        if (res == RpcFrameReassembler::Result::NeedMore) {
            continue;
        }
        if (res == RpcFrameReassembler::Result::Error) {
            record_log(false, "parse_error", err, item.text);
            continue;
        }
        handle_frame(frame);
    }
    return changed;
}

void OmpClient::start_initial_sync() {
    send_raw(cmd_set_ask_dialog(next_req_id("init"), true), "enable ask dialog");
    send_raw(cmd_set_event_filter(next_req_id("init"), true), "delta message updates");
    send_raw(cmd_set_subagent_subscription(next_req_id("init"), "progress"), "subagent progress");
    refresh_state();
    send_raw(cmd_simple(next_req_id("init"), "get_available_thinking_levels"), "thinking levels");
    send_raw(cmd_simple(next_req_id("init"), "get_available_commands"), "slash commands");
    send_raw(cmd_simple(next_req_id("init"), "get_session_stats"), "session stats");
    refresh_login_providers();
    refresh_tree();
    refresh_subagents();
    send_raw(cmd_simple(next_req_id("init"), "get_btw_history"), "btw history");
    load_messages_page("");
    // Send get_available_models LAST so background online model discovery never blocks fast local sync!
    refresh_models();
}

void OmpClient::load_messages_page(const std::string& cursor) {
    if (cursor.empty()) {
        paging_accum_.clear();
        paging_active_ = true;
    }
    const std::string id = next_req_id("page");
    pending_cmd_meta_[id] = cursor;
    send_raw(cmd_get_messages_page(id, cursor, 128), cursor.empty() ? "load messages" : "next page");
}

void OmpClient::rebuild_tool_execs_from_messages() {
    msg_index_by_id_.clear();
    tool_execs_.clear();
    for (size_t i = 0; i < messages_.size(); ++i) {
        if (!messages_[i].id.empty()) msg_index_by_id_[messages_[i].id] = i;
        // Reconstruct completed tool cards & structured diffs from transcript history
        for (const auto& b : messages_[i].blocks) {
            if (b.kind == ContentBlockKind::ToolCall && !b.tool_call_id.empty()) {
                auto& te = tool_execs_[b.tool_call_id];
                te.tool_call_id = b.tool_call_id;
                te.tool_name = b.tool_name;
                te.intent = b.tool_intent;
                te.args_json = b.tool_args_json;
                Json parsed_args;
                if (Json::parse(b.tool_args_json, parsed_args)) {
                    te.args_summary = summarize_tool_args(b.tool_name, parsed_args);
                    if (te.diff_text.empty()) {
                        te.diff_text = extract_diff_from_args(b.tool_name, parsed_args);
                    }
                }
                te.running = false;
            }
        }
        if (messages_[i].role == "toolResult" && !messages_[i].tool_call_id.empty()) {
            auto& te = tool_execs_[messages_[i].tool_call_id];
            te.tool_call_id = messages_[i].tool_call_id;
            if (te.tool_name.empty()) te.tool_name = messages_[i].tool_name;
            if (te.args_summary.empty() && !messages_[i].tool_path.empty()) {
                te.args_summary = messages_[i].tool_path;
            }
            if (!messages_[i].tool_diff.empty()) {
                te.diff_text = messages_[i].tool_diff;
            }
            te.is_error = messages_[i].is_error;
            te.running = false;
            for (const auto& b : messages_[i].blocks) {
                if (b.kind == ContentBlockKind::Text) {
                    if (!te.output_text.empty()) te.output_text.push_back('\n');
                    te.output_text += b.text;
                }
            }
            if ((te.tool_name == "bash" || te.tool_name == "shell") && !te.output_text.empty()) {
                te.terminal = std::make_shared<AnsiTerminalBuffer>(92, 16);
                te.terminal->feed(te.output_text);
            }
        }
    }
}

void OmpClient::handle_frame(const Json& frame) {
    const std::string& type = frame["type"].as_string();
    if (type != "message_update" && type != "live_levels") {
        record_log(false, type, frame["command"].as_string_copy(type), frame.dump(0));
    }

    if (type == "ready") {
        ready_ = parse_ready_frame(frame);
        reassembler_.set_max_reassembled_bytes(ready_.max_reassembled_bytes);
        bool has_v2 = false;
        for (int v : ready_.supported_versions) {
            if (v == 2) has_v2 = true;
        }
        status_ = ConnectionStatus::Connected;
        if (has_v2) {
            send_raw(cmd_negotiate_protocol(next_req_id("proto"), 2), "negotiate v2");
        } else {
            start_initial_sync();
        }
        return;
    }
    if (type == "response") {
        handle_response(frame);
        return;
    }
    if (type == "extension_ui_request") {
        handle_extension_ui(frame);
        return;
    }
    if (type == "available_commands_update") {
        available_commands_.clear();
        for (const auto& c : frame["commands"].as_array()) {
            SlashCommandInfo info;
            info.name = c["name"].as_string_copy();
            info.description = c["description"].as_string_copy();
            info.source = c["source"].as_string_copy("builtin");
            if (c["input"].is_object()) info.input_hint = c["input"]["hint"].as_string_copy();
            available_commands_.push_back(std::move(info));
        }
        return;
    }
    if (type == "prompt_result") {
        const std::string& st = frame["status"].as_string();
        if (st == "error" && frame["error"].is_object()) {
            push_toast(frame["error"]["message"].as_string_copy("Model request failed"), "error", 5200);
        }
        if (frame["sessionSettled"].as_bool(false)) {
            state_.is_streaming = false;
            state_.is_settled = true;
            just_settled_ = true;
        }
        refresh_state();
        send_raw(cmd_simple(next_req_id("st"), "get_session_stats"));
        refresh_tree();
        session_list_dirty_ = true;
        return;
    }
    if (type == "session_settled") {
        state_.is_streaming = false;
        state_.has_pending_async_work = false;
        state_.is_settled = true;
        just_settled_ = true;
        refresh_state();
        send_raw(cmd_simple(next_req_id("st"), "get_session_stats"));
        session_list_dirty_ = true;
        return;
    }
    if (type == "subagent_lifecycle" || type == "subagent_progress" || type == "subagent_event") {
        handle_subagent_frame(frame);
        return;
    }
    if (type == "btw_delta" || type == "btw_record") {
        handle_btw_frame(frame);
        return;
    }
    if (type == "live_phase" || type == "live_levels" || type == "live_transcript" || type == "live_end") {
        handle_live_frame(frame);
        return;
    }
    if (type == "command_output") {
        const std::string text = frame["text"].as_string_copy();
        if (!text.empty()) {
            ChatMessage m;
            m.id = "cmd_" + std::to_string(unix_time_ms());
            m.role = "notice";
            m.is_error = frame["isError"].as_bool(false);
            m.timestamp_ms = unix_time_ms();
            ContentBlock b;
            b.kind = ContentBlockKind::Text;
            b.text = text;
            m.blocks.push_back(std::move(b));
            messages_.push_back(std::move(m));
        }
        return;
    }
    if (type == "session_info_update") {
        if (frame["sessionId"].is_string()) state_.session_id = frame["sessionId"].as_string();
        if (frame["sessionFile"].is_string()) state_.session_file = frame["sessionFile"].as_string();
        if (frame["sessionName"].is_string()) state_.session_name = frame["sessionName"].as_string();
        session_list_dirty_ = true;
        return;
    }
    if (type == "config_update") {
        refresh_state();
        return;
    }
    if (type == "notice") {
        push_toast(frame["message"].as_string_copy(), frame["level"].as_string_copy("info"), 4500);
        return;
    }
    if (type == "extension_error") {
        push_toast("Extension error: " + frame["error"].as_string_copy(), "warning", 4800);
        return;
    }

    handle_event(frame);
}

void OmpClient::handle_response(const Json& frame) {
    const std::string& id = frame["id"].as_string();
    const std::string& cmd = frame["command"].as_string();
    const bool ok = frame["success"].as_bool(false);
    const Json& data = frame["data"];

    std::string meta;
    if (auto it = pending_cmd_meta_.find(id); it != pending_cmd_meta_.end()) {
        meta = std::move(it->second);
        pending_cmd_meta_.erase(it);
    }

    if (!ok) {
        if (cmd == "predict_word") {
            return; // Silently suppress transient prediction daemon backoff
        }
        if (cmd == "get_messages_page") {
            paging_active_ = false;
            const std::string& code = frame["code"].as_string();
            if (code == "session_busy" || code == "stale_cursor") {
                send_raw(cmd_simple(next_req_id("msgs"), "get_messages"), "fallback get_messages");
                return;
            }
        }
        const std::string err = frame["error"].as_string_copy("Command failed: " + cmd);
        push_toast(err, "error", 4800);
        return;
    }

    if (cmd == "negotiate_protocol") {
        negotiated_version_ = static_cast<int>(data["protocolVersion"].as_int(2));
        start_initial_sync();
    } else if (cmd == "set_event_filter") {
        delta_mode_active_ = (data["messageUpdates"].as_string() == "delta");
    } else if (cmd == "get_state") {
        state_ = parse_session_state(data);
    } else if (cmd == "get_session_stats") {
        stats_ = parse_session_stats(data);
    } else if (cmd == "get_available_models") {
        available_models_.clear();
        for (const auto& m : data["models"].as_array()) {
            available_models_.push_back(parse_model_info(m));
        }
    } else if (cmd == "get_available_thinking_levels") {
        thinking_levels_.clear();
        for (const auto& l : data["levels"].as_array()) {
            thinking_levels_.push_back(l.as_string_copy());
        }
        if (thinking_levels_.empty()) thinking_levels_ = {"off", "low", "medium", "high"};
    } else if (cmd == "get_available_commands") {
        available_commands_.clear();
        for (const auto& c : data["commands"].as_array()) {
            SlashCommandInfo info;
            info.name = c["name"].as_string_copy();
            info.description = c["description"].as_string_copy();
            info.source = c["source"].as_string_copy("builtin");
            if (c["input"].is_object()) info.input_hint = c["input"]["hint"].as_string_copy();
            available_commands_.push_back(std::move(info));
        }
    } else if (cmd == "get_login_providers") {
        login_providers_.clear();
        for (const auto& p : data["providers"].as_array()) {
            LoginProviderInfo lp;
            lp.id = p["id"].as_string_copy();
            lp.name = p["name"].as_string_copy(lp.id);
            lp.available = p["available"].as_bool(true);
            lp.authenticated = p["authenticated"].as_bool(false);
            login_providers_.push_back(std::move(lp));
        }
    } else if (cmd == "login") {
        push_toast("Signed in to " + data["providerName"].as_string_copy(data["providerId"].as_string_copy()), "success", 3600);
        refresh_login_providers();
        refresh_models();
    } else if (cmd == "get_logout_accounts") {
        logout_accounts_.clear();
        for (const auto& a : data["accounts"].as_array()) {
            LogoutAccountInfo la;
            la.credential_id = a["credentialId"].as_int(0);
            la.provider = a["provider"].as_string_copy();
            la.label = a["label"].as_string_copy();
            la.detail = a["detail"].as_string_copy();
            la.type = a["type"].as_string_copy();
            la.active = a["active"].as_bool(false);
            logout_accounts_.push_back(std::move(la));
        }
    } else if (cmd == "logout") {
        push_toast("Signed out account", "info", 3000);
        refresh_login_providers();
        refresh_models();
    } else if (cmd == "get_messages_page") {
        for (const auto& mj : data["messages"].as_array()) {
            const std::string fallback = "hist_" + std::to_string(paging_accum_.size());
            paging_accum_.push_back(parse_agent_message(mj, fallback));
        }
        const std::string next_cursor = data["nextCursor"].as_string_copy();
        if (!next_cursor.empty()) {
            load_messages_page(next_cursor);
        } else {
            paging_active_ = false;
            messages_ = std::move(paging_accum_);
            rebuild_tool_execs_from_messages();
        }
    } else if (cmd == "get_messages") {
        messages_.clear();
        for (const auto& mj : data["messages"].as_array()) {
            const std::string fallback = "hist_" + std::to_string(messages_.size());
            messages_.push_back(parse_agent_message(mj, fallback));
        }
        rebuild_tool_execs_from_messages();
    } else if (cmd == "get_tree") {
        tree_roots_.clear();
        tree_leaf_id_ = data["leafId"].as_string_copy();
        for (const auto& n : data["tree"].as_array()) {
            tree_roots_.push_back(parse_tree_node(n));
        }
    } else if (cmd == "get_subagents") {
        // Merge active subagents returned by OMP without erasing terminal subagents from this session
        std::unordered_set<std::string> live_ids;
        for (const auto& sa_json : data["subagents"].as_array()) {
            SubagentInfo incoming = parse_subagent_snapshot(sa_json);
            if (incoming.id.empty()) continue;
            live_ids.insert(incoming.id);
            auto it = std::find_if(subagents_.begin(), subagents_.end(),
                                   [&](const SubagentInfo& s) { return s.id == incoming.id; });
            if (it == subagents_.end()) {
                subagents_.push_back(std::move(incoming));
            } else {
                *it = std::move(incoming);
            }
        }
        for (auto& s : subagents_) {
            const bool was_active = (s.status == "running" || s.status == "active" || s.status == "pending");
            if (was_active && !live_ids.count(s.id)) {
                s.status = "completed";
                s.current_tool.clear();
                s.current_tool_args.clear();
                s.can_cancel = false;
                s.can_steer = false;
            }
        }
    } else if (cmd == "get_subagent_messages") {
        if (!meta.empty()) {
            auto& vec = subagent_messages_[meta];
            if (data["reset"].as_bool(false)) vec.clear();
            for (const auto& mj : data["messages"].as_array()) {
                vec.push_back(parse_agent_message(mj));
            }
        }
    } else if (cmd == "get_btw_history") {
        btw_records_.clear();
        for (const auto& r : data["records"].as_array()) {
            btw_records_.push_back(parse_btw_record(r));
        }
    } else if (cmd == "get_session_lease") {
        held_leases_.clear();
        for (const auto& h : data["held"].as_array()) {
            held_leases_.insert(h.as_string_copy());
        }
    } else if (cmd == "set_model" || cmd == "cycle_model") {
        if (data.is_object() && data.contains("id")) state_.model = parse_model_info(data);
        refresh_state();
        send_raw(cmd_simple(next_req_id("thk"), "get_available_thinking_levels"));
    } else if (cmd == "set_thinking_level" || cmd == "cycle_thinking_level" ||
               cmd == "set_fast_mode" || cmd == "set_slow_mode" || cmd == "set_auto_compaction") {
        refresh_state();
    } else if (cmd == "new_session" || cmd == "switch_session" || cmd == "open_session" || cmd == "fork") {
        if (!data["cancelled"].as_bool(false)) {
            messages_.clear();
            msg_index_by_id_.clear();
            tool_execs_.clear();
            subagents_.clear();
            btw_records_.clear();
            refresh_state();
            send_raw(cmd_simple(next_req_id("thk"), "get_available_thinking_levels"));
            load_messages_page("");
            refresh_tree();
            refresh_subagents();
            send_raw(cmd_simple(next_req_id("st"), "get_session_stats"));
            send_raw(cmd_simple(next_req_id("btw"), "get_btw_history"));
            session_list_dirty_ = true;
            if (cmd == "fork") {
                push_toast("Forked new branch thread", "success", 2400);
            }
        }
    } else if (cmd == "navigate_tree") {
        if (!data["cancelled"].as_bool(false)) {
            if (data["editorText"].is_string() && !data["editorText"].as_string().empty()) {
                pending_editor_text_ = data["editorText"].as_string();
                has_pending_editor_text_ = true;
            }
            load_messages_page("");
            refresh_tree();
            refresh_state();
            push_toast("Switched active branch", "success", 2200);
        }
    } else if (cmd == "abort_and_restore_queue") {
        std::string restored;
        for (const auto& m : data["steering"].as_array()) {
            if (!restored.empty()) restored += "\n";
            restored += m["text"].as_string_copy();
        }
        for (const auto& m : data["followUp"].as_array()) {
            if (!restored.empty()) restored += "\n";
            restored += m["text"].as_string_copy();
        }
        if (!restored.empty()) {
            pending_editor_text_ = std::move(restored);
            has_pending_editor_text_ = true;
        }
    } else if (cmd == "predict_word") {
        if (id == last_predict_req_id_) {
            ghost_prediction_ = data["suffix"].is_string() ? data["suffix"].as_string() : std::string();
        }
    } else if (cmd == "set_session_name") {
        session_list_dirty_ = true;
        refresh_state();
    } else if (cmd == "compact") {
        push_toast("Context compacted", "success", 3000);
        refresh_state();
        load_messages_page("");
    }
}

void OmpClient::handle_event(const Json& frame) {
    const std::string& type = frame["type"].as_string();

    if (type == "agent_start") {
        state_.is_streaming = true;
        state_.is_settled = false;
    } else if (type == "agent_end") {
        if (frame["yielded"].as_bool(true)) {
            state_.is_streaming = false;
        }
        if (frame["isTerminal"].as_bool(true)) {
            state_.is_streaming = false;
        }
        refresh_tree();
        refresh_subagents();
    } else if (type == "message_start") {
        std::string mid = frame["messageId"].as_string_copy();
        if (mid.empty()) mid = "msg_" + std::to_string(messages_.size());
        ChatMessage m = parse_agent_message(frame["message"], mid);
        m.streaming = (!m.hidden && m.role == "assistant");
        if (auto it = msg_index_by_id_.find(mid); it != msg_index_by_id_.end()) {
            messages_[it->second] = std::move(m);
        } else {
            msg_index_by_id_[mid] = messages_.size();
            messages_.push_back(std::move(m));
        }
    } else if (type == "message_update") {
        handle_message_update(frame);
    } else if (type == "message_end") {
        const std::string mid = frame["messageId"].as_string_copy();
        const std::string resolved_id = mid.empty() ? ("msg_" + std::to_string(messages_.size())) : mid;
        ChatMessage m = parse_agent_message(frame["message"], resolved_id);
        m.streaming = false;
        if (!mid.empty()) {
            if (auto it = msg_index_by_id_.find(mid); it != msg_index_by_id_.end()) {
                m.render_revision = messages_[it->second].render_revision + 1;
                messages_[it->second] = std::move(m);
            } else {
                msg_index_by_id_[mid] = messages_.size();
                messages_.push_back(std::move(m));
            }
        } else {
            msg_index_by_id_[resolved_id] = messages_.size();
            messages_.push_back(std::move(m));
        }
    } else if (type == "tool_execution_start") {
        const std::string tid = frame["toolCallId"].as_string_copy();
        auto& te = tool_execs_[tid];
        te.tool_call_id = tid;
        te.tool_name = frame["toolName"].as_string_copy();
        te.intent = frame["intent"].as_string_copy();
        te.args_json = frame["args"].dump(2);
        te.args_summary = summarize_tool_args(te.tool_name, frame["args"]);
        const std::string preview_diff = extract_diff_from_args(te.tool_name, frame["args"]);
        if (!preview_diff.empty()) te.diff_text = preview_diff;
        te.running = true;
        te.is_error = false;
        te.started_ms = unix_time_ms();
        if (te.tool_name == "bash" || te.tool_name == "shell") {
            te.terminal = std::make_shared<AnsiTerminalBuffer>(92, 16);
            if (!te.args_summary.empty()) {
                te.terminal->feed("\x1b[33m$ " + te.args_summary + "\x1b[0m\n");
            }
        }
    } else if (type == "tool_execution_update") {
        const std::string tid = frame["toolCallId"].as_string_copy();
        auto& te = tool_execs_[tid];
        const std::string chunk = extract_content_text(frame["partialResult"]["content"]);
        const std::string p_diff = extract_diff_from_details(frame["partialResult"]["details"]);
        if (!p_diff.empty()) te.diff_text = p_diff;
        // Also ingest live subagent progress snapshots embedded inside synchronous `task` tool updates
        const Json& det_prog = frame["partialResult"]["details"]["progress"];
        if (det_prog.is_array()) {
            for (const auto& pj : det_prog.as_array()) {
                if (!pj.is_object()) continue;
                Json wrapper = Json::object();
                wrapper["type"] = "subagent_progress";
                Json payload = Json::object();
                payload["parentToolCallId"] = tid;
                payload["progress"] = pj;
                wrapper["payload"] = std::move(payload);
                handle_subagent_frame(wrapper);
            }
        }
        if (!chunk.empty()) {
            te.output_text = chunk;
            if (te.terminal) {
                te.terminal->clear();
                if (!te.args_summary.empty()) te.terminal->feed("\x1b[33m$ " + te.args_summary + "\x1b[0m\n");
                te.terminal->feed(chunk);
            }
        }
    } else if (type == "tool_execution_end") {
        const std::string tid = frame["toolCallId"].as_string_copy();
        auto& te = tool_execs_[tid];
        te.running = false;
        te.is_error = frame["isError"].as_bool(false);
        te.ended_ms = unix_time_ms();
        const Json& res = frame["result"];
        const std::string final_text = extract_content_text(res["content"]);
        const std::string det_diff = extract_diff_from_details(res["details"]);
        if (!det_diff.empty()) {
            te.diff_text = det_diff;
        }
        const Json& end_prog = res["details"]["progress"];
        if (end_prog.is_array()) {
            for (const auto& pj : end_prog.as_array()) {
                if (!pj.is_object()) continue;
                Json wrapper = Json::object();
                wrapper["type"] = "subagent_progress";
                Json payload = Json::object();
                payload["parentToolCallId"] = tid;
                payload["progress"] = pj;
                wrapper["payload"] = std::move(payload);
                handle_subagent_frame(wrapper);
            }
        }
        // If this was a task/subagent tool call and finished, reconcile any child subagent still marked active
        if (te.tool_name == "task" || te.tool_name == "subagent" || te.tool_name == "dispatch_agent") {
            for (auto& sa : subagents_) {
                if (sa.parent_tool_call_id == tid &&
                    (sa.status == "running" || sa.status == "active" || sa.status == "pending")) {
                    sa.status = te.is_error ? "failed" : "completed";
                    sa.current_tool.clear();
                    sa.current_tool_args.clear();
                    sa.can_cancel = false;
                    sa.can_steer = false;
                }
            }
        }
        if (te.args_summary.empty() && res["details"].is_object() && res["details"]["path"].is_string()) {
            te.args_summary = res["details"]["path"].as_string();
        }
        if (!final_text.empty()) {
            te.output_text = final_text;
            if (te.terminal) {
                te.terminal->clear();
                if (!te.args_summary.empty()) te.terminal->feed("\x1b[33m$ " + te.args_summary + "\x1b[0m\n");
                te.terminal->feed(final_text);
            }
        }
    } else if (type == "queue_update") {
        state_.steering_queue.clear();
        state_.follow_up_queue.clear();
        for (const auto& s : frame["steering"].as_array()) state_.steering_queue.push_back(s.as_string_copy());
        for (const auto& s : frame["followUp"].as_array()) state_.follow_up_queue.push_back(s.as_string_copy());
        state_.live_steered = static_cast<uint32_t>(frame["liveSteered"].as_int(0));
    } else if (type == "model_changed" || type == "thinking_level_changed") {
        refresh_state();
    } else if (type == "auto_compaction_start" || type == "manual_compaction_start") {
        state_.is_compacting = true;
        push_toast("Compacting conversation context...", "info", 2800);
    } else if (type == "auto_compaction_end" || type == "manual_compaction_end") {
        state_.is_compacting = false;
        refresh_state();
    } else if (type == "retry_fallback_applied") {
        push_toast("Fallback: " + frame["from"].as_string_copy() + " -> " + frame["to"].as_string_copy(), "warning", 4200);
    } else if (type == "ttsr_triggered") {
        push_toast("Rulebook rule injected into turn", "info", 2800);
    } else if (type == "goal_updated") {
        state_.goal = frame["goal"].is_string() ? frame["goal"].as_string_copy() : std::string();
    }
}

void OmpClient::handle_message_update(const Json& frame) {
    const std::string mid = frame["messageId"].as_string_copy();
    size_t msg_idx = 0;
    if (auto it = msg_index_by_id_.find(mid); it != msg_index_by_id_.end()) {
        msg_idx = it->second;
    } else {
        ChatMessage m;
        m.id = mid.empty() ? ("msg_" + std::to_string(messages_.size())) : mid;
        m.role = frame["message"]["role"].as_string_copy("assistant");
        m.streaming = true;
        m.timestamp_ms = unix_time_ms();
        msg_idx = messages_.size();
        msg_index_by_id_[m.id] = msg_idx;
        messages_.push_back(std::move(m));
    }

    ChatMessage& msg = messages_[msg_idx];
    const Json& ev = frame["assistantMessageEvent"];
    if (!ev.is_object()) return;

    const std::string& et = ev["type"].as_string();
    const size_t ci = static_cast<size_t>(std::max<int64_t>(0, ev["contentIndex"].as_int(0)));

    auto ensure_block = [&](size_t idx, ContentBlockKind kind) -> ContentBlock& {
        if (msg.blocks.size() <= idx) msg.blocks.resize(idx + 1);
        auto& b = msg.blocks[idx];
        b.kind = kind;
        return b;
    };

    if (et == "start" && ev["message"].is_object()) {
        msg = parse_agent_message(ev["message"], msg.id);
        msg.streaming = true;
    } else if (et == "text_start") {
        auto& b = ensure_block(ci, ContentBlockKind::Text);
        b.text.clear();
        b.complete = false;
        b.started_ms = unix_time_ms();
    } else if (et == "text_delta") {
        auto& b = ensure_block(ci, ContentBlockKind::Text);
        b.text += ev["delta"].as_string();
        b.complete = false;
        ++msg.render_revision;
    } else if (et == "text_end") {
        auto& b = ensure_block(ci, ContentBlockKind::Text);
        if (ev["content"].is_string()) b.text = ev["content"].as_string();
        b.complete = true;
        b.ended_ms = unix_time_ms();
        ++msg.render_revision;
    } else if (et == "thinking_start") {
        auto& b = ensure_block(ci, ContentBlockKind::Thinking);
        b.text.clear();
        b.complete = false;
        b.started_ms = unix_time_ms();
    } else if (et == "thinking_delta") {
        auto& b = ensure_block(ci, ContentBlockKind::Thinking);
        b.text += ev["delta"].as_string();
        b.complete = false;
        ++msg.render_revision;
    } else if (et == "thinking_end") {
        auto& b = ensure_block(ci, ContentBlockKind::Thinking);
        if (ev["content"].is_string()) b.text = ev["content"].as_string();
        b.complete = true;
        b.ended_ms = unix_time_ms();
        ++msg.render_revision;
    } else if (et == "toolcall_start") {
        auto& b = ensure_block(ci, ContentBlockKind::ToolCall);
        b.complete = false;
        b.started_ms = unix_time_ms();
        if (ev["block"].is_object()) {
            b.tool_call_id = ev["block"]["id"].as_string_copy();
            b.tool_name = ev["block"]["name"].as_string_copy();
            b.tool_intent = ev["block"]["intent"].as_string_copy();
        }
        b.tool_args_json.clear();
    } else if (et == "toolcall_delta") {
        auto& b = ensure_block(ci, ContentBlockKind::ToolCall);
        b.tool_args_json += ev["delta"].as_string();
    } else if (et == "toolcall_end") {
        auto& b = ensure_block(ci, ContentBlockKind::ToolCall);
        if (ev["toolCall"].is_object()) {
            const Json& tc = ev["toolCall"];
            b.tool_call_id = tc["id"].as_string_copy(b.tool_call_id);
            b.tool_name = tc["name"].as_string_copy(b.tool_name);
            b.tool_intent = tc["intent"].as_string_copy(b.tool_intent);
            b.tool_args_json = tc["arguments"].dump(2);
        }
        b.complete = true;
        b.ended_ms = unix_time_ms();
    } else if (et == "image_end" && ev["content"].is_object()) {
        auto& b = ensure_block(ci, ContentBlockKind::Image);
        b.mime_type = ev["content"]["mimeType"].as_string_copy("image/png");
        b.image_b64 = ev["content"]["data"].as_string_copy();
        b.complete = true;
    }
}

void OmpClient::handle_extension_ui(const Json& frame) {
    ExtensionUiRequest req = parse_extension_ui_request(frame);
    if (req.method == "cancel") {
        ui_requests_.erase(
            std::remove_if(ui_requests_.begin(), ui_requests_.end(),
                           [&](const ExtensionUiRequest& r) { return r.id == req.target_id; }),
            ui_requests_.end());
        return;
    }
    if (req.method == "notify") {
        push_toast(req.message, req.notify_type.empty() ? "info" : req.notify_type, 3800);
        return;
    }
    if (req.method == "setStatus") {
        if (req.status_text.empty()) ext_statuses_.erase(req.status_key);
        else ext_statuses_[req.status_key] = req.status_text;
        return;
    }
    if (req.method == "setWidget") {
        if (req.widget_lines.empty()) ext_widgets_.erase(req.widget_key);
        else ext_widgets_[req.widget_key] = req.widget_lines;
        return;
    }
    if (req.method == "set_editor_text") {
        pending_editor_text_ = req.message.empty() ? frame["text"].as_string_copy() : req.message;
        has_pending_editor_text_ = true;
        return;
    }
    if (req.method == "setTitle") {
        if (!req.title.empty()) state_.session_name = req.title;
        return;
    }
    if (req.method == "open_url") {
        const std::string target = !req.launch_url.empty() ? req.launch_url : req.url;
        if (!target.empty()) open_external_url(target);
        oauth_url_event_ = std::move(req);
        has_oauth_url_event_ = true;
        return;
    }
    // Interactive request (ask, select, confirm, input, editor)
    ui_requests_.push_back(std::move(req));
}

void OmpClient::handle_subagent_frame(const Json& frame) {
    const std::string& type = frame["type"].as_string();
    const Json& payload = frame["payload"].is_object() ? frame["payload"] : frame;
    SubagentInfo incoming = parse_subagent_snapshot(payload);
    if (incoming.id.empty()) return;

    const bool has_progress = payload["progress"].is_object();
    const bool is_event = (type == "subagent_event");

    auto it = std::find_if(subagents_.begin(), subagents_.end(),
                           [&](const SubagentInfo& s) { return s.id == incoming.id; });
    if (it == subagents_.end()) {
        subagents_.push_back(std::move(incoming));
        it = std::prev(subagents_.end());
    } else {
        if (!incoming.agent.empty()) it->agent = incoming.agent;
        if (!incoming.agent_source.empty()) it->agent_source = incoming.agent_source;
        if (!incoming.session_file.empty()) it->session_file = incoming.session_file;
        if (!incoming.parent_tool_call_id.empty()) it->parent_tool_call_id = incoming.parent_tool_call_id;
        if (!is_event && !incoming.status.empty()) it->status = incoming.status;
        if (!incoming.task.empty()) it->task = incoming.task;
        if (!incoming.description.empty()) it->description = incoming.description;
        if (has_progress) {
            // In subagent_progress frames, an absent currentTool means no tool is currently running
            it->current_tool = incoming.current_tool;
            it->current_tool_args = incoming.current_tool_args;
        } else if (!incoming.current_tool.empty()) {
            it->current_tool = incoming.current_tool;
            it->current_tool_args = incoming.current_tool_args;
        }
        if (!incoming.last_intent.empty()) it->last_intent = incoming.last_intent;
        if (!incoming.resolved_model.empty()) it->resolved_model = incoming.resolved_model;
        if (incoming.tool_count > 0) it->tool_count = incoming.tool_count;
        if (incoming.requests > 0) it->requests = incoming.requests;
        if (incoming.tokens > 0) it->tokens = incoming.tokens;
        if (incoming.context_tokens > 0) it->context_tokens = incoming.context_tokens;
        if (incoming.context_window > 0) it->context_window = incoming.context_window;
        if (incoming.cost_usd > 0.0) it->cost_usd = incoming.cost_usd;
        if (incoming.duration_ms > 0) it->duration_ms = incoming.duration_ms;
        if (!incoming.recent_output.empty()) it->recent_output = incoming.recent_output;
        it->last_update_ms = unix_time_ms();
    }

    // Handle raw subagent_event inner session events when subscription is "events"
    if (is_event && payload["event"].is_object()) {
        const Json& ev = payload["event"];
        const std::string& et = ev["type"].as_string();
        if (et == "tool_execution_start") {
            it->status = "running";
            it->current_tool = ev["toolName"].as_string_copy();
            it->current_tool_args = summarize_tool_args(it->current_tool, ev["args"]);
            ++it->tool_count;
        } else if (et == "tool_execution_end") {
            it->current_tool.clear();
            it->current_tool_args.clear();
        } else if (et == "agent_start") {
            it->status = "running";
        }
    }

    const bool active = (it->status == "running" || it->status == "active" || it->status == "pending");
    it->can_cancel = active;
    it->can_steer = active;
    if (!active) {
        it->current_tool.clear();
        it->current_tool_args.clear();
    }
}

void OmpClient::handle_btw_frame(const Json& frame) {
    const std::string& type = frame["type"].as_string();
    if (type == "btw_delta") {
        const std::string& rid = frame["recordId"].as_string();
        const std::string& delta = frame["delta"].as_string();
        for (auto& r : btw_records_) {
            if (r.id == rid) {
                if (!r.follow_ups.empty() && r.follow_ups.back().status == "running") {
                    r.follow_ups.back().answer += delta;
                } else {
                    r.answer += delta;
                }
                return;
            }
        }
    } else if (type == "btw_record" && frame["record"].is_object()) {
        BtwRecord rec = parse_btw_record(frame["record"]);
        auto it = std::find_if(btw_records_.begin(), btw_records_.end(),
                               [&](const BtwRecord& b) { return b.id == rec.id; });
        if (it != btw_records_.end()) *it = std::move(rec);
        else btw_records_.insert(btw_records_.begin(), std::move(rec));
    }
}

void OmpClient::handle_live_frame(const Json& frame) {
    const std::string& type = frame["type"].as_string();
    if (type == "live_phase") {
        live_voice_phase_ = frame["phase"].as_string_copy();
        live_voice_active_ = (live_voice_phase_ != "error");
    } else if (type == "live_levels") {
        live_input_level_ = static_cast<float>(frame["input"].as_double(0.0));
        live_output_level_ = static_cast<float>(frame["output"].as_double(0.0));
    } else if (type == "live_transcript") {
        live_transcript_ = frame["text"].as_string_copy();
    } else if (type == "live_end") {
        live_voice_active_ = false;
        live_voice_phase_.clear();
        live_input_level_ = 0.0f;
        live_output_level_ = 0.0f;
    }
}

// ============================================================================
// High-Level Commands
// ============================================================================

std::string OmpClient::send_prompt(const std::string& message,
                                   const std::vector<ImageAttachment>& images,
                                   const std::string& streaming_behavior) {
    const std::string id = next_req_id("prompt");
    std::string behavior = streaming_behavior;
    if (behavior.empty() && state_.is_streaming) {
        behavior = "followUp";
    }
    send_raw(cmd_prompt(id, message, images, behavior), "prompt");
    return id;
}

std::string OmpClient::send_steer(const std::string& message, const std::vector<ImageAttachment>& images) {
    const std::string id = next_req_id("steer");
    send_raw(cmd_steer(id, message, images), "steer");
    return id;
}

std::string OmpClient::send_follow_up(const std::string& message, const std::vector<ImageAttachment>& images) {
    const std::string id = next_req_id("fup");
    send_raw(cmd_follow_up(id, message, images), "follow_up");
    return id;
}

std::string OmpClient::remove_queued(const std::string& message, const std::string& queue) {
    const std::string id = next_req_id("rmq");
    send_raw(cmd_remove_queued(id, message, queue), "remove queued");
    return id;
}

std::string OmpClient::promote_queued(const std::string& message) {
    const std::string id = next_req_id("pro");
    send_raw(cmd_promote_queued(id, message), "promote queued");
    return id;
}

std::string OmpClient::abort_turn() {
    const std::string id = next_req_id("abt");
    send_raw(cmd_simple(id, "abort"), "abort");
    return id;
}

std::string OmpClient::abort_and_restore_queue() {
    const std::string id = next_req_id("abtr");
    send_raw(cmd_simple(id, "abort_and_restore_queue"), "abort & restore queue");
    return id;
}

std::string OmpClient::new_session() {
    const std::string id = next_req_id("new");
    send_raw(cmd_new_session(id), "new_session");
    return id;
}

std::string OmpClient::switch_session(const std::string& session_path) {
    const std::string id = next_req_id("sw");
    send_raw(cmd_switch_session(id, session_path), "switch_session");
    return id;
}

std::string OmpClient::fork_session(const std::string& entry_id) {
    const std::string id = next_req_id("fk");
    send_raw(cmd_fork(id, entry_id), "fork");
    return id;
}

std::string OmpClient::navigate_tree(const std::string& entry_id, bool summarize) {
    const std::string id = next_req_id("nav");
    send_raw(cmd_navigate_tree(id, entry_id, summarize), "navigate_tree");
    return id;
}

std::string OmpClient::rename_session(const std::string& name) {
    const std::string id = next_req_id("ren");
    send_raw(cmd_set_session_name(id, name), "set_session_name");
    return id;
}

std::string OmpClient::set_model(const std::string& provider, const std::string& model_id) {
    const std::string id = next_req_id("mdl");
    send_raw(cmd_set_model(id, provider, model_id), provider + "/" + model_id);
    return id;
}

std::string OmpClient::set_thinking_level(const std::string& level) {
    const std::string id = next_req_id("thk");
    send_raw(cmd_set_thinking(id, level), "thinking=" + level);
    return id;
}

std::string OmpClient::set_fast_mode(bool enabled) {
    const std::string id = next_req_id("fast");
    send_raw(cmd_set_fast_mode(id, enabled));
    return id;
}

std::string OmpClient::set_slow_mode(bool enabled) {
    const std::string id = next_req_id("slow");
    send_raw(cmd_set_slow_mode(id, enabled));
    return id;
}

std::string OmpClient::set_auto_compaction(bool enabled) {
    const std::string id = next_req_id("acmp");
    send_raw(cmd_set_auto_compaction(id, enabled));
    return id;
}

std::string OmpClient::compact_now(const std::string& instructions) {
    const std::string id = next_req_id("cmp");
    send_raw(cmd_compact(id, instructions), "compact");
    return id;
}

std::string OmpClient::refresh_state() {
    const std::string id = next_req_id("st");
    send_raw(cmd_simple(id, "get_state"));
    return id;
}

std::string OmpClient::refresh_models() {
    const std::string id = next_req_id("mdls");
    send_raw(cmd_simple(id, "get_available_models"));
    return id;
}

std::string OmpClient::refresh_tree() {
    const std::string id = next_req_id("tree");
    send_raw(cmd_simple(id, "get_tree"));
    return id;
}

std::string OmpClient::refresh_subagents() {
    const std::string id = next_req_id("sub");
    send_raw(cmd_simple(id, "get_subagents"));
    return id;
}

std::string OmpClient::refresh_login_providers() {
    const std::string id = next_req_id("auth");
    send_raw(cmd_simple(id, "get_login_providers"));
    return id;
}

std::string OmpClient::request_session_leases(const std::vector<std::string>& session_ids) {
    if (session_ids.empty()) return {};
    const std::string id = next_req_id("lease");
    send_raw(cmd_get_session_lease(id, session_ids), "probe leases");
    return id;
}

std::string OmpClient::start_oauth_login(const std::string& provider_id) {
    const std::string id = next_req_id("login");
    send_raw(cmd_login(id, provider_id), "OAuth " + provider_id);
    return id;
}

std::string OmpClient::query_logout_accounts(const std::string& provider_id) {
    const std::string id = next_req_id("accts");
    send_raw(cmd_get_logout_accounts(id, provider_id));
    return id;
}

std::string OmpClient::logout_account(const std::string& provider_id, int64_t credential_id) {
    const std::string id = next_req_id("logout");
    send_raw(cmd_logout(id, provider_id, credential_id), "logout " + provider_id);
    return id;
}

std::string OmpClient::ask_btw(const std::string& question, const std::string& record_id) {
    const std::string id = next_req_id("btw");
    send_raw(cmd_btw(id, question, record_id), "btw");
    return id;
}

std::string OmpClient::cancel_btw(const std::string& record_id) {
    const std::string id = next_req_id("btwc");
    send_raw(cmd_btw_cancel(id, record_id), "btw_cancel");
    return id;
}

std::string OmpClient::steer_subagent(const std::string& subagent_id, const std::string& message) {
    const std::string id = next_req_id("sstr");
    send_raw(cmd_steer_subagent(id, subagent_id, message), "steer " + subagent_id);
    return id;
}

std::string OmpClient::cancel_subagent(const std::string& subagent_id) {
    const std::string id = next_req_id("scan");
    send_raw(cmd_cancel_subagent(id, subagent_id), "cancel " + subagent_id);
    return id;
}

std::string OmpClient::fetch_subagent_messages(const std::string& subagent_id, uint64_t from_byte) {
    const std::string id = next_req_id("smsg");
    pending_cmd_meta_[id] = subagent_id;
    send_raw(cmd_get_subagent_messages(id, subagent_id, from_byte));
    return id;
}

std::string OmpClient::request_word_prediction(const std::string& text, uint32_t utf16_cursor) {
    const std::string id = next_req_id("pred");
    last_predict_req_id_ = id;
    send_raw(cmd_predict_word(id, text, utf16_cursor));
    return id;
}

void OmpClient::send_word_prediction_feedback(const std::string& text, uint32_t utf16_cursor,
                                              const std::string& suggestion, bool accepted) {
    send_raw(cmd_predict_word_feedback(next_req_id("pfb"), text, utf16_cursor, suggestion, accepted));
}

std::string OmpClient::start_live_voice() {
    const std::string id = next_req_id("live");
    live_voice_active_ = true;
    live_voice_phase_ = "connecting";
    send_raw(cmd_live_start(id), "live_start");
    return id;
}

std::string OmpClient::stop_live_voice() {
    const std::string id = next_req_id("live");
    live_voice_active_ = false;
    live_voice_phase_.clear();
    send_raw(cmd_live_stop(id), "live_stop");
    return id;
}

std::string OmpClient::toggle_live_mute(bool muted) {
    const std::string id = next_req_id("mute");
    send_raw(cmd_live_mute(id, muted), muted ? "mute" : "unmute");
    return id;
}

void OmpClient::reply_ui_value(const std::string& req_id, const std::string& value) {
    send_raw(ui_response_value(req_id, value), "ui value");
    ui_requests_.erase(std::remove_if(ui_requests_.begin(), ui_requests_.end(),
                                      [&](const ExtensionUiRequest& r) { return r.id == req_id; }),
                       ui_requests_.end());
}

void OmpClient::reply_ui_confirm(const std::string& req_id, bool confirmed) {
    send_raw(ui_response_confirm(req_id, confirmed), confirmed ? "confirm yes" : "confirm no");
    ui_requests_.erase(std::remove_if(ui_requests_.begin(), ui_requests_.end(),
                                      [&](const ExtensionUiRequest& r) { return r.id == req_id; }),
                       ui_requests_.end());
}

void OmpClient::reply_ui_cancel(const std::string& req_id, bool timed_out) {
    send_raw(ui_response_cancel(req_id, timed_out), "ui cancel");
    ui_requests_.erase(std::remove_if(ui_requests_.begin(), ui_requests_.end(),
                                      [&](const ExtensionUiRequest& r) { return r.id == req_id; }),
                       ui_requests_.end());
}

void OmpClient::reply_ui_ask(
    const std::string& req_id,
    const std::vector<std::tuple<std::string, std::vector<std::string>, std::string>>& answers) {
    send_raw(ui_response_ask(req_id, answers), "ui ask answers");
    ui_requests_.erase(std::remove_if(ui_requests_.begin(), ui_requests_.end(),
                                      [&](const ExtensionUiRequest& r) { return r.id == req_id; }),
                       ui_requests_.end());
}

bool OmpClient::take_pending_editor_text(std::string& out_text) {
    if (!has_pending_editor_text_) return false;
    has_pending_editor_text_ = false;
    out_text = std::move(pending_editor_text_);
    pending_editor_text_.clear();
    return true;
}

bool OmpClient::take_just_settled_flag() noexcept {
    const bool v = just_settled_;
    just_settled_ = false;
    return v;
}

bool OmpClient::take_session_list_dirty_flag() noexcept {
    const bool v = session_list_dirty_;
    session_list_dirty_ = false;
    return v;
}

bool OmpClient::take_oauth_url_event(ExtensionUiRequest& out_req) {
    if (!has_oauth_url_event_) return false;
    has_oauth_url_event_ = false;
    out_req = std::move(oauth_url_event_);
    return true;
}

const std::vector<ChatMessage>& OmpClient::subagent_transcript(const std::string& subagent_id) const {
    static const std::vector<ChatMessage> kEmpty;
    auto it = subagent_messages_.find(subagent_id);
    return (it != subagent_messages_.end()) ? it->second : kEmpty;
}

} // namespace ompe
