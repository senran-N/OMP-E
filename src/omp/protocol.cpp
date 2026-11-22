#include "omp/protocol.h"
#include "core/i18n.h"
#include "core/base64.h"
#include "core/platform.h"

#include <algorithm>
#include <tuple>

namespace ompe {

void RpcFrameReassembler::reset() {
    active_ = false;
    chunk_id_.clear();
    expected_index_ = 0;
    expected_count_ = 0;
    expected_bytes_ = 0;
    buffer_.clear();
}

RpcFrameReassembler::Result RpcFrameReassembler::feed_line(
    std::string_view line, Json& out_json, std::string& out_error) {
    out_error.clear();
    if (line.empty()) return Result::NeedMore;

    Json parsed;
    if (!Json::parse(line, parsed, &out_error)) {
        reset();
        return Result::Error;
    }
    if (!parsed.is_object()) {
        out_error = "RPC frame is not a JSON object";
        reset();
        return Result::Error;
    }

    const std::string& type = parsed["type"].as_string();
    if (type != "rpc_chunk") {
        if (active_) {
            // Interleaved frame interrupts incomplete chunk sequence; drop stale chunks cleanly
            reset();
        }
        out_json = std::move(parsed);
        return Result::FrameReady;
    }

    const std::string& cid = parsed["chunkId"].as_string();
    const int64_t idx = parsed["index"].as_int(-1);
    const int64_t cnt = parsed["count"].as_int(-1);
    const int64_t byte_len = parsed["byteLength"].as_int(-1);
    const std::string& b64 = parsed["data"].as_string();

    if (cid.empty() || idx < 0 || cnt < 2 || idx >= cnt || byte_len <= 0 ||
        static_cast<size_t>(byte_len) > max_bytes_) {
        out_error = "Invalid rpc_chunk header metadata";
        reset();
        return Result::Error;
    }

    if (!active_) {
        if (idx != 0) {
            out_error = "rpc_chunk sequence did not start at index 0";
            return Result::Error;
        }
        active_ = true;
        chunk_id_ = cid;
        expected_index_ = 0;
        expected_count_ = static_cast<uint32_t>(cnt);
        expected_bytes_ = static_cast<size_t>(byte_len);
        buffer_.clear();
        buffer_.reserve(expected_bytes_);
    } else {
        if (cid != chunk_id_ || static_cast<uint32_t>(idx) != expected_index_ ||
            static_cast<uint32_t>(cnt) != expected_count_ ||
            static_cast<size_t>(byte_len) != expected_bytes_) {
            out_error = "Interrupted or mismatched rpc_chunk sequence";
            reset();
            return Result::Error;
        }
    }

    std::string decoded;
    if (!base64_decode_to_string(b64, decoded)) {
        out_error = "Invalid base64 payload in rpc_chunk";
        reset();
        return Result::Error;
    }
    if (buffer_.size() + decoded.size() > expected_bytes_) {
        out_error = "rpc_chunk reassembled size exceeds declared byteLength";
        reset();
        return Result::Error;
    }
    buffer_.append(decoded);
    ++expected_index_;

    if (expected_index_ < expected_count_) {
        return Result::NeedMore;
    }

    if (buffer_.size() != expected_bytes_) {
        out_error = "rpc_chunk final byteLength mismatch";
        reset();
        return Result::Error;
    }
    if (!is_valid_utf8(buffer_)) {
        out_error = "rpc_chunk reassembled payload is not valid UTF-8";
        reset();
        return Result::Error;
    }

    Json full;
    if (!Json::parse(buffer_, full, &out_error) || !full.is_object()) {
        if (out_error.empty()) out_error = "Reassembled rpc_chunk is not a JSON object";
        reset();
        return Result::Error;
    }
    reset();
    out_json = std::move(full);
    return Result::FrameReady;
}

ReadyInfo parse_ready_frame(const Json& j) {
    ReadyInfo r;
    r.protocol_version = static_cast<int>(j["protocolVersion"].as_int(1));
    for (const auto& v : j["supportedProtocolVersions"].as_array()) {
        r.supported_versions.push_back(static_cast<int>(v.as_int(1)));
    }
    r.max_frame_bytes = static_cast<size_t>(j["maxFrameBytes"].as_int(1048576));
    r.max_reassembled_bytes = static_cast<size_t>(j["maxReassembledFrameBytes"].as_int(67108864));
    r.supports_bash_exclude = j["supportsBashExcludeFromContext"].as_bool(false);
    return r;
}

ModelInfo parse_model_info(const Json& j) {
    ModelInfo m;
    if (!j.is_object()) return m;
    m.provider = j["provider"].as_string_copy();
    m.id = j["id"].as_string_copy();
    m.name = j["name"].as_string_copy(m.id);
    m.api = j["api"].as_string_copy();
    m.base_url = j["baseUrl"].as_string_copy();
    m.reasoning = j["reasoning"].as_bool(false);
    m.context_window = static_cast<uint64_t>(j["contextWindow"].as_int(0));
    m.max_tokens = static_cast<uint64_t>(j["maxTokens"].as_int(0));
    if (j["cost"].is_object()) {
        m.cost_input = j["cost"]["input"].as_double(0.0);
        m.cost_output = j["cost"]["output"].as_double(0.0);
    }
    for (const auto& inp : j["input"].as_array()) {
        if (inp.as_string() == "image") m.supports_image = true;
    }
    return m;
}

SessionState parse_session_state(const Json& j) {
    SessionState s;
    if (!j.is_object()) return s;
    if (j["model"].is_object()) s.model = parse_model_info(j["model"]);
    s.thinking_level = j["thinkingLevel"].as_string_copy("off");
    s.is_streaming = j["isStreaming"].as_bool(false);
    s.is_compacting = j["isCompacting"].as_bool(false);
    s.steering_mode = j["steeringMode"].as_string_copy("one-at-a-time");
    s.follow_up_mode = j["followUpMode"].as_string_copy("one-at-a-time");
    s.interrupt_mode = j["interruptMode"].as_string_copy("immediate");
    s.session_file = j["sessionFile"].as_string_copy();
    s.session_id = j["sessionId"].as_string_copy();
    s.session_name = j["sessionName"].as_string_copy();
    s.auto_compaction = j["autoCompactionEnabled"].as_bool(true);
    s.fast_mode_enabled = j["fastModeEnabled"].as_bool(false);
    s.fast_mode_active = j["fastModeActive"].as_bool(false);
    s.slow_mode_supported = j["slowModeSupported"].as_bool(false);
    s.slow_mode_enabled = j["slowModeEnabled"].as_bool(false);
    s.tokens_per_second = j["tokensPerSecond"].as_double(0.0);
    s.message_count = static_cast<uint32_t>(j["messageCount"].as_int(0));
    s.has_pending_async_work = j["hasPendingAsyncWork"].as_bool(false);
    s.is_settled = j["isSettled"].as_bool(!s.is_streaming && !s.has_pending_async_work);

    if (j["queuedMessages"].is_object()) {
        const auto& qm = j["queuedMessages"];
        for (const auto& v : qm["steering"].as_array()) s.steering_queue.push_back(v.as_string_copy());
        for (const auto& v : qm["followUp"].as_array()) s.follow_up_queue.push_back(v.as_string_copy());
        s.live_steered = static_cast<uint32_t>(qm["liveSteered"].as_int(0));
    }

    if (j["contextUsage"].is_object()) {
        const auto& cu = j["contextUsage"];
        s.context_usage.tokens = static_cast<uint64_t>(cu["tokens"].as_int(0));
        s.context_usage.context_window = static_cast<uint64_t>(cu["contextWindow"].as_int(0));
        s.context_usage.percent = cu["percent"].as_double(0.0);
        s.context_usage.valid = s.context_usage.context_window > 0;
    }

    if (j["usageLimit"].is_object()) {
        const auto& ul = j["usageLimit"];
        s.usage_limit.active = true;
        s.usage_limit.stage = ul["stage"].as_string_copy();
        s.usage_limit.resets_at_sec = ul["resetsAtSec"].as_int(0);
        s.usage_limit.allowance_left_percent = static_cast<int>(ul["allowanceLeftPercent"].as_int(-1));
    }

    if (j["goal"].is_string()) {
        s.goal = j["goal"].as_string_copy();
    } else if (j["goal"].is_object()) {
        s.goal = j["goal"]["objective"].as_string_copy();
    }

    for (const auto& ph : j["todoPhases"].as_array()) {
        TodoPhase tp;
        tp.name = ph["name"].as_string_copy();
        for (const auto& t : ph["tasks"].as_array()) {
            TodoItem ti;
            ti.id = t["id"].as_string_copy();
            ti.content = t["content"].as_string_copy();
            ti.status = t["status"].as_string_copy("pending");
            tp.tasks.push_back(std::move(ti));
        }
        s.todo_phases.push_back(std::move(tp));
    }
    return s;
}

SessionStats parse_session_stats(const Json& j) {
    SessionStats st;
    if (!j.is_object()) return st;
    st.user_messages = static_cast<uint32_t>(j["userMessages"].as_int(0));
    st.assistant_messages = static_cast<uint32_t>(j["assistantMessages"].as_int(0));
    st.tool_calls = static_cast<uint32_t>(j["toolCalls"].as_int(0));
    if (j["tokens"].is_object()) {
        const auto& t = j["tokens"];
        st.input_tokens = static_cast<uint64_t>(t["input"].as_int(0));
        st.output_tokens = static_cast<uint64_t>(t["output"].as_int(0));
        st.cache_read_tokens = static_cast<uint64_t>(t["cacheRead"].as_int(0));
        st.cache_write_tokens = static_cast<uint64_t>(t["cacheWrite"].as_int(0));
        st.total_tokens = static_cast<uint64_t>(t["total"].as_int(0));
    }
    st.cost_usd = j["cost"].as_double(0.0);
    return st;
}

std::string summarize_tool_args(std::string_view tool_name, const Json& args) {
    if (!args.is_object()) return {};
    const auto pick = [&](std::initializer_list<const char*> keys) -> std::string {
        for (const char* k : keys) {
            if (args.contains(k) && args[k].is_string() && !args[k].as_string().empty()) {
                return args[k].as_string();
            }
        }
        return {};
    };
    if (tool_name == "bash" || tool_name == "shell") {
        return pick({"command", "cmd"});
    }
    if (tool_name == "read" || tool_name == "edit" || tool_name == "write" ||
        tool_name == "apply_patch" || tool_name == "patch" || tool_name == "ast_edit") {
        std::string p = pick({"path", "file_path", "filePath", "target"});
        if (p.empty() && args.contains("paths") && args["paths"].is_array() && !args["paths"].as_array().empty()) {
            p = args["paths"].as_array().front().as_string_copy();
        }
        return p;
    }
    if (tool_name == "grep" || tool_name == "find" || tool_name == "glob") {
        std::string p = pick({"pattern", "query", "glob"});
        std::string dir = pick({"path", "dir"});
        if (!dir.empty() && !p.empty()) return p + " in " + dir;
        return !p.empty() ? p : dir;
    }
    if (tool_name == "task") {
        return pick({"description", "prompt", "agent"});
    }
    std::string fallback = pick({"path", "command", "query", "url", "description", "question"});
    if (!fallback.empty()) return fallback;
    std::string raw = args.dump(0);
    if (raw.size() > 100) raw = raw.substr(0, 97) + "...";
    return raw;
}

std::string strip_system_xml_blocks(std::string_view text) {
    if (text.empty()) return {};
    static constexpr std::string_view kTags[] = {
        "system-notice",
        "system-reminder",
        "system-instruction",
    };

    std::string cur(text);
    bool removed_any = false;

    for (std::string_view tag : kTags) {
        const std::string open_prefix = "<" + std::string(tag);
        const std::string close_tag = "</" + std::string(tag) + ">";
        size_t pos = 0;
        while (pos < cur.size()) {
            const size_t open_pos = cur.find(open_prefix, pos);
            if (open_pos == std::string::npos) break;
            const size_t after = open_pos + open_prefix.size();
            if (after >= cur.size()) break;
            const char delim = cur[after];
            if (delim != '>' && delim != ' ' && delim != '\t' && delim != '\r' && delim != '\n') {
                pos = after;
                continue;
            }
            const size_t open_end = cur.find('>', after);
            if (open_end == std::string::npos) {
                pos = after;
                continue;
            }
            const size_t close_pos = cur.find(close_tag, open_end + 1);
            if (close_pos != std::string::npos) {
                cur.erase(open_pos, (close_pos + close_tag.size()) - open_pos);
                removed_any = true;
                pos = open_pos;
            } else {
                // Only strip unclosed block if it starts at the beginning (e.g., truncated preview snippet)
                size_t lead_ws = 0;
                while (lead_ws < open_pos &&
                       (cur[lead_ws] == ' ' || cur[lead_ws] == '\t' || cur[lead_ws] == '\r' || cur[lead_ws] == '\n')) {
                    ++lead_ws;
                }
                if (lead_ws == open_pos) {
                    cur.erase(open_pos);
                    removed_any = true;
                    break;
                }
                pos = open_end + 1;
            }
        }
    }

    if (!removed_any) return cur;

    // Trim surrounding whitespace left behind after stripping injected system XML blocks
    size_t b = 0;
    while (b < cur.size() && (cur[b] == ' ' || cur[b] == '\t' || cur[b] == '\r' || cur[b] == '\n')) ++b;
    size_t e = cur.size();
    while (e > b && (cur[e - 1] == ' ' || cur[e - 1] == '\t' || cur[e - 1] == '\r' || cur[e - 1] == '\n')) --e;
    return cur.substr(b, e - b);
}

ChatMessage parse_agent_message(const Json& j, std::string_view fallback_id) {
    ChatMessage msg;
    msg.id = fallback_id.empty() ? j["id"].as_string_copy() : std::string(fallback_id);
    msg.role = j["role"].as_string_copy("assistant");
    msg.custom_type = j["customType"].as_string_copy();
    msg.tool_call_id = j["toolCallId"].as_string_copy();
    msg.tool_name = j["toolName"].as_string_copy();
    msg.is_error = j["isError"].as_bool(false);

    // Respect OMP display=false metadata (e.g., eval-prelude-guide <system-notice> injected for LLM context only)
    const Json* disp = j.find("display");
    if (disp && disp->is_bool() && !disp->as_bool()) {
        msg.hidden = true;
    }
    if (msg.role == "system" || msg.role == "developer") {
        msg.hidden = true;
    } else if (msg.role == "custom" && !(disp && disp->is_bool() && disp->as_bool())) {
        msg.hidden = true;
    }
    if (msg.custom_type == "eval-prelude-guide" || msg.custom_type == "date-cwd-reminder") {
        msg.hidden = true;
    }
    if (j["details"].is_object()) {
        const Json& det = j["details"];
        if (det["path"].is_string()) msg.tool_path = det["path"].as_string();
        if (det["diff"].is_string() && !det["diff"].as_string().empty()) {
            msg.tool_diff = det["diff"].as_string();
        } else if (det["displayContent"].is_string() && !det["displayContent"].as_string().empty()) {
            msg.tool_diff = det["displayContent"].as_string();
        } else if (det["perFileResults"].is_array()) {
            std::string merged;
            for (const auto& pf : det["perFileResults"].as_array()) {
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
            msg.tool_diff = std::move(merged);
        } else if (det["op"].as_string() == "delete" && det["oldText"].is_string() && !det["oldText"].as_string().empty()) {
            std::string del_diff;
            const std::string& ot = det["oldText"].as_string();
            size_t pos = 0;
            while (pos <= ot.size()) {
                const size_t nl = ot.find('\n', pos);
                const std::string_view ln = (nl == std::string::npos)
                    ? std::string_view(ot).substr(pos)
                    : std::string_view(ot).substr(pos, nl - pos);
                if (!del_diff.empty()) del_diff.push_back('\n');
                del_diff.push_back('-');
                del_diff.append(ln);
                if (nl == std::string::npos) break;
                pos = nl + 1;
            }
            msg.tool_diff = std::move(del_diff);
        }
    }
    msg.model_id = j["model"].as_string_copy();
    msg.stop_reason = j["stopReason"].as_string_copy();
    msg.error_message = j["errorMessage"].as_string_copy();
    msg.timestamp_ms = j["timestamp"].as_int(unix_time_ms());

    if (j["usage"].is_object()) {
        msg.output_tokens = static_cast<uint64_t>(j["usage"]["output"].as_int(0));
        if (j["usage"]["cost"].is_object()) {
            msg.cost_usd = j["usage"]["cost"]["total"].as_double(0.0);
        }
    }

    const bool strip_xml = (msg.role != "toolResult" && msg.role != "bashExecution");
    bool stripped_any_text = false;
    auto push_text_block = [&](std::string raw_text) {
        if (strip_xml) {
            std::string cleaned = strip_system_xml_blocks(raw_text);
            if (cleaned.size() != raw_text.size()) {
                stripped_any_text = true;
            }
            if (cleaned.empty() && stripped_any_text) {
                return;
            }
            raw_text = std::move(cleaned);
        }
        ContentBlock b;
        b.kind = ContentBlockKind::Text;
        b.text = std::move(raw_text);
        msg.blocks.push_back(std::move(b));
    };

    const Json& content = j["content"];
    if (content.is_string()) {
        push_text_block(content.as_string());
    } else if (content.is_array()) {
        for (const auto& item : content.as_array()) {
            if (!item.is_object()) continue;
            const std::string& bt = item["type"].as_string();
            ContentBlock b;
            if (bt == "text") {
                push_text_block(item["text"].as_string_copy());
            } else if (bt == "thinking") {
                b.kind = ContentBlockKind::Thinking;
                b.text = item["thinking"].as_string_copy();
                msg.blocks.push_back(std::move(b));
            } else if (bt == "redactedThinking") {
                b.kind = ContentBlockKind::RedactedThinking;
                b.text = "[Redacted reasoning]";
                msg.blocks.push_back(std::move(b));
            } else if (bt == "toolCall") {
                b.kind = ContentBlockKind::ToolCall;
                b.tool_call_id = item["id"].as_string_copy();
                b.tool_name = item["name"].as_string_copy();
                b.tool_intent = item["intent"].as_string_copy();
                b.tool_args_json = item["arguments"].dump(2);
                msg.blocks.push_back(std::move(b));
            } else if (bt == "image") {
                b.kind = ContentBlockKind::Image;
                b.mime_type = item["mimeType"].as_string_copy("image/png");
                b.image_b64 = item["data"].as_string_copy();
                msg.blocks.push_back(std::move(b));
            }
        }
    } else if (msg.role == "bashExecution") {
        ContentBlock b;
        b.kind = ContentBlockKind::Text;
        std::string cmd = j["command"].as_string_copy();
        std::string out = j["output"].as_string_copy();
        b.text = "$ " + cmd + (out.empty() ? "" : ("\n" + out));
        msg.blocks.push_back(std::move(b));
    } else if (msg.role == "compactionSummary" || msg.role == "branchSummary") {
        ContentBlock b;
        b.kind = ContentBlockKind::Notice;
        b.text = j["summary"].as_string_copy();
        msg.blocks.push_back(std::move(b));
    }

    if ((stripped_any_text || msg.role == "custom") && msg.blocks.empty() && msg.error_message.empty()) {
        msg.hidden = true;
    }

    return msg;
}

ExtensionUiRequest parse_extension_ui_request(const Json& j) {
    ExtensionUiRequest r;
    r.id = j["id"].as_string_copy();
    r.method = j["method"].as_string_copy();
    r.title = j["title"].as_string_copy();
    r.message = j["message"].as_string_copy();
    r.placeholder = j["placeholder"].as_string_copy();
    r.prefill = j["prefill"].as_string_copy();
    r.url = j["url"].as_string_copy();
    r.launch_url = j["launchUrl"].as_string_copy();
    r.instructions = j["instructions"].as_string_copy();
    r.notify_type = j["notifyType"].as_string_copy("info");
    r.status_key = j["statusKey"].as_string_copy();
    r.status_text = j["statusText"].as_string_copy();
    r.widget_key = j["widgetKey"].as_string_copy();
    r.target_id = j["targetId"].as_string_copy();
    r.selection_marker = j["selectionMarker"].as_string_copy();
    r.initial_index = static_cast<int>(j["initialIndex"].as_int(0));
    r.secret = j["secret"].as_bool(false);
    r.prompt_style = j["promptStyle"].as_bool(false);
    r.timeout_ms = static_cast<int>(j["timeout"].as_int(0));
    r.received_ms = unix_time_ms();

    for (const auto& l : j["widgetLines"].as_array()) r.widget_lines.push_back(l.as_string_copy());
    for (const auto& o : j["options"].as_array()) r.options.push_back(o.as_string_copy());
    for (const auto& d : j["optionDetails"].as_array()) {
        r.option_descriptions.push_back(d["description"].as_string_copy());
    }
    for (const auto& idx : j["checkedIndices"].as_array()) {
        r.checked_indices.push_back(static_cast<int>(idx.as_int(-1)));
    }
    for (const auto& qj : j["questions"].as_array()) {
        AskQuestion q;
        q.id = qj["id"].as_string_copy();
        q.header = qj["header"].as_string_copy();
        q.question = qj["question"].as_string_copy();
        q.multi = qj["multi"].as_bool(false);
        q.recommended = static_cast<int>(qj["recommended"].as_int(-1));
        for (const auto& oj : qj["options"].as_array()) {
            AskOption opt;
            opt.label = oj["label"].as_string_copy();
            opt.description = oj["description"].as_string_copy();
            opt.preview = oj["preview"].as_string_copy();
            q.options.push_back(std::move(opt));
        }
        r.questions.push_back(std::move(q));
    }
    return r;
}

static std::string normalize_subagent_status(std::string s) {
    if (s == "started" || s == "in_progress") return "running";
    if (s == "done" || s == "success") return "completed";
    if (s == "cancelled" || s == "canceled") return "aborted";
    return s;
}

SubagentInfo parse_subagent_snapshot(const Json& raw) {
    const Json& j = raw["payload"].is_object() ? raw["payload"] : raw;
    SubagentInfo s;
    s.id = j["id"].as_string_copy();
    if (s.id.empty()) s.id = j["subagentId"].as_string_copy();
    s.agent = j["agent"].as_string_copy();
    s.agent_source = j["agentSource"].as_string_copy();
    s.description = j["description"].as_string_copy();
    if (j["status"].is_string()) {
        s.status = normalize_subagent_status(j["status"].as_string_copy());
    }
    s.session_file = j["sessionFile"].as_string_copy();
    s.parent_tool_call_id = j["parentToolCallId"].as_string_copy();
    s.last_update_ms = j["lastUpdate"].as_int(unix_time_ms());
    if (j["task"].is_string()) s.task = j["task"].as_string_copy();
    if (s.task.empty() && j["assignment"].is_string()) s.task = j["assignment"].as_string_copy();

    const Json& p = j["progress"];
    if (p.is_object()) {
        if (s.id.empty()) s.id = p["id"].as_string_copy();
        if (s.agent.empty()) s.agent = p["agent"].as_string_copy();
        if (p["status"].is_string()) s.status = normalize_subagent_status(p["status"].as_string_copy());
        if (p["task"].is_string() && !p["task"].as_string().empty()) s.task = p["task"].as_string_copy();
        if (p["description"].is_string() && !p["description"].as_string().empty()) s.description = p["description"].as_string_copy();
        s.last_intent = p["lastIntent"].as_string_copy();
        s.current_tool = p["currentTool"].as_string_copy();
        s.current_tool_args = p["currentToolArgs"].as_string_copy();
        s.resolved_model = p["resolvedModel"].as_string_copy();
        s.tool_count = static_cast<uint32_t>(p["toolCount"].as_int(0));
        s.requests = static_cast<uint32_t>(p["requests"].as_int(0));
        s.tokens = static_cast<uint64_t>(p["tokens"].as_int(0));
        s.context_tokens = static_cast<uint64_t>(p["contextTokens"].as_int(0));
        s.context_window = static_cast<uint64_t>(p["contextWindow"].as_int(0));
        s.cost_usd = p["cost"].as_double(0.0);
        s.duration_ms = p["durationMs"].as_int(0);
        for (const auto& line : p["recentOutput"].as_array()) {
            s.recent_output.push_back(line.as_string_copy());
        }
    }
    if (s.status.empty()) s.status = "running";
    const bool active = (s.status == "running" || s.status == "active" || s.status == "pending");
    s.can_cancel = active && j["canCancel"].as_bool(true);
    s.can_steer = active && j["canSteer"].as_bool(true);
    return s;
}

SessionTreeNode parse_tree_node(const Json& j) {
    SessionTreeNode n;
    const Json& e = j["entry"];
    if (e.is_object()) {
        n.id = e["id"].as_string_copy();
        n.parent_id = e["parentId"].as_string_copy();
        n.type = e["type"].as_string_copy();
        n.timestamp = e["timestamp"].as_string_copy();
        if (n.type == "message" && e["message"].is_object()) {
            const Json& m = e["message"];
            n.role = m["role"].as_string_copy();
            const Json* disp = m.find("display");
            if ((disp && disp->is_bool() && !disp->as_bool()) || n.role == "custom" || n.role == "system") {
                n.role = "meta";
                std::string ct = m["customType"].as_string_copy();
                n.preview = ct.empty() ? "System context" : ct;
            } else if (n.role == "toolResult") {
                const std::string tname = m["toolName"].as_string_copy();
                n.preview = tname.empty() ? "Tool result" : ("Tool: " + tname);
            } else if (m["content"].is_string()) {
                n.preview = strip_system_xml_blocks(m["content"].as_string());
            } else if (m["content"].is_array()) {
                std::string tool_fallback;
                for (const auto& blk : m["content"].as_array()) {
                    if (blk["type"].as_string() == "text" && !blk["text"].as_string().empty()) {
                        std::string cleaned = strip_system_xml_blocks(blk["text"].as_string());
                        if (!cleaned.empty()) {
                            n.preview = std::move(cleaned);
                            break;
                        }
                    }
                    if (tool_fallback.empty() && blk["type"].as_string() == "toolCall") {
                        tool_fallback = "Tool: " + blk["name"].as_string_copy();
                    }
                }
                if (n.preview.empty() && !tool_fallback.empty()) {
                    n.preview = std::move(tool_fallback);
                }
            }
        } else if (n.type == "custom_message" || n.type == "custom") {
            n.role = "meta";
            const Json* disp = e.find("display");
            if (disp && disp->is_bool() && !disp->as_bool()) {
                const std::string ct = e["customType"].as_string_copy();
                n.preview = ct.empty() ? "System context" : ct;
            } else if (e["content"].is_string()) {
                n.preview = strip_system_xml_blocks(e["content"].as_string());
                if (n.preview.empty()) {
                    const std::string ct = e["customType"].as_string_copy();
                    n.preview = ct.empty() ? n.type : ct;
                }
            } else {
                const std::string ct = e["customType"].as_string_copy();
                n.preview = ct.empty() ? n.type : ct;
            }
        } else if (n.type == "compaction") {
            n.role = "compaction";
            n.preview = e["shortSummary"].as_string_copy(e["summary"].as_string_copy("Compacted context"));
        } else if (n.type == "branch_summary") {
            n.role = "branch";
            n.preview = e["summary"].as_string_copy("Branch summary");
        } else if (n.type == "model_change") {
            n.role = "model";
            std::string mdl = e["model"].as_string_copy();
            if (mdl.empty()) mdl = e["modelId"].as_string_copy();
            n.preview = mdl.empty() ? "Model switched" : mdl;
        } else if (n.type == "thinking_level_change") {
            n.role = "thinking";
            const std::string lvl = e["thinkingLevel"].as_string_copy();
            n.preview = lvl.empty() ? "Thinking effort changed" : ("Effort: " + lvl);
        } else if (n.type == "session_info" || n.type == "title" || n.type == "title_change") {
            n.role = "meta";
            n.preview = e["name"].as_string_copy(e["title"].as_string_copy("Session info"));
        } else {
            n.role = "meta";
            n.preview = e["summary"].as_string_copy(e["name"].as_string_copy(n.type));
        }
    }
    n.label = j["label"].as_string_copy();
    for (char& ch : n.preview) {
        if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
    }
    // Collapse leading whitespace
    size_t lead = 0;
    while (lead < n.preview.size() && n.preview[lead] == ' ') ++lead;
    if (lead > 0) n.preview.erase(0, lead);
    n.preview = utf8_truncate_ellipsis(n.preview, 220);
    for (const auto& ch : j["children"].as_array()) {
        n.children.push_back(parse_tree_node(ch));
    }
    return n;
}

BtwRecord parse_btw_record(const Json& j) {
    BtwRecord r;
    r.id = j["id"].as_string_copy();
    r.leaf_id = j["leafId"].as_string_copy();
    r.title = j["title"].as_string_copy();
    r.question = j["question"].as_string_copy();
    r.answer = j["answer"].as_string_copy();
    r.status = j["status"].as_string_copy("complete");
    r.error = j["error"].as_string_copy();
    r.created_at = j["createdAt"].as_int(0);
    for (const auto& f : j["followUps"].as_array()) {
        BtwTurn t;
        t.question = f["question"].as_string_copy();
        t.answer = f["answer"].as_string_copy();
        t.status = f["status"].as_string_copy("complete");
        t.error = f["error"].as_string_copy();
        t.created_at = f["createdAt"].as_int(0);
        r.follow_ups.push_back(std::move(t));
    }
    return r;
}

// ============================================================================
// Command Builders
// ============================================================================

static Json make_cmd(const std::string& id, const std::string& type) {
    Json j = Json::object();
    if (!id.empty()) j["id"] = id;
    j["type"] = type;
    return j;
}

static Json encode_images(const std::vector<ImageAttachment>& images) {
    Json arr = Json::array();
    for (const auto& img : images) {
        Json o = Json::object();
        o["type"] = "image";
        o["mimeType"] = img.mime_type;
        o["data"] = img.base64_data;
        arr.push_back(std::move(o));
    }
    return arr;
}

Json cmd_negotiate_protocol(const std::string& id, int version) {
    Json j = make_cmd(id, "negotiate_protocol");
    j["protocolVersion"] = version;
    return j;
}

Json cmd_set_ask_dialog(const std::string& id, bool enabled) {
    Json j = make_cmd(id, "set_ask_dialog");
    j["enabled"] = enabled;
    return j;
}

Json cmd_set_event_filter(const std::string& id, bool delta_mode) {
    Json j = make_cmd(id, "set_event_filter");
    j["events"] = Json(nullptr);
    j["messageUpdates"] = delta_mode ? "delta" : "full";
    return j;
}

Json cmd_set_subagent_subscription(const std::string& id, const std::string& level) {
    Json j = make_cmd(id, "set_subagent_subscription");
    j["level"] = level;
    return j;
}

Json cmd_simple(const std::string& id, const std::string& type) {
    return make_cmd(id, type);
}

Json cmd_prompt(const std::string& id, const std::string& message,
                const std::vector<ImageAttachment>& images,
                const std::string& streaming_behavior) {
    Json j = make_cmd(id, "prompt");
    j["message"] = message;
    if (!images.empty()) j["images"] = encode_images(images);
    if (!streaming_behavior.empty()) j["streamingBehavior"] = streaming_behavior;
    return j;
}

Json cmd_steer(const std::string& id, const std::string& message, const std::vector<ImageAttachment>& images) {
    Json j = make_cmd(id, "steer");
    j["message"] = message;
    if (!images.empty()) j["images"] = encode_images(images);
    return j;
}

Json cmd_follow_up(const std::string& id, const std::string& message, const std::vector<ImageAttachment>& images) {
    Json j = make_cmd(id, "follow_up");
    j["message"] = message;
    if (!images.empty()) j["images"] = encode_images(images);
    return j;
}

Json cmd_remove_queued(const std::string& id, const std::string& message, const std::string& queue) {
    Json j = make_cmd(id, "remove_queued_message");
    j["message"] = message;
    j["queue"] = queue;
    return j;
}

Json cmd_promote_queued(const std::string& id, const std::string& message) {
    Json j = make_cmd(id, "promote_queued_message");
    j["message"] = message;
    return j;
}

Json cmd_set_model(const std::string& id, const std::string& provider, const std::string& model_id) {
    Json j = make_cmd(id, "set_model");
    j["provider"] = provider;
    j["modelId"] = model_id;
    return j;
}

Json cmd_set_thinking(const std::string& id, const std::string& level) {
    Json j = make_cmd(id, "set_thinking_level");
    j["level"] = level;
    return j;
}

Json cmd_set_fast_mode(const std::string& id, bool enabled) {
    Json j = make_cmd(id, "set_fast_mode");
    j["enabled"] = enabled;
    return j;
}

Json cmd_set_slow_mode(const std::string& id, bool enabled) {
    Json j = make_cmd(id, "set_slow_mode");
    j["enabled"] = enabled;
    return j;
}

Json cmd_set_auto_compaction(const std::string& id, bool enabled) {
    Json j = make_cmd(id, "set_auto_compaction");
    j["enabled"] = enabled;
    return j;
}

Json cmd_compact(const std::string& id, const std::string& instructions) {
    Json j = make_cmd(id, "compact");
    if (!instructions.empty()) j["customInstructions"] = instructions;
    return j;
}

Json cmd_switch_session(const std::string& id, const std::string& session_path) {
    Json j = make_cmd(id, "switch_session");
    j["sessionPath"] = session_path;
    return j;
}

Json cmd_open_session(const std::string& id, const std::string& session_dir) {
    Json j = make_cmd(id, "open_session");
    j["sessionDir"] = session_dir;
    return j;
}

Json cmd_new_session(const std::string& id, const std::string& parent_session) {
    Json j = make_cmd(id, "new_session");
    if (!parent_session.empty()) j["parentSession"] = parent_session;
    return j;
}

Json cmd_fork(const std::string& id, const std::string& entry_id) {
    Json j = make_cmd(id, "fork");
    if (!entry_id.empty()) j["entryId"] = entry_id;
    return j;
}

Json cmd_navigate_tree(const std::string& id, const std::string& entry_id, bool summarize) {
    Json j = make_cmd(id, "navigate_tree");
    j["entryId"] = entry_id;
    j["summarize"] = summarize;
    return j;
}

Json cmd_set_session_name(const std::string& id, const std::string& name) {
    Json j = make_cmd(id, "set_session_name");
    j["name"] = name;
    return j;
}

Json cmd_get_messages_page(const std::string& id, const std::string& cursor, int limit) {
    Json j = make_cmd(id, "get_messages_page");
    if (!cursor.empty()) j["cursor"] = cursor;
    j["limit"] = limit;
    return j;
}

Json cmd_get_session_lease(const std::string& id, const std::vector<std::string>& session_ids) {
    Json j = make_cmd(id, "get_session_lease");
    Json arr = Json::array();
    for (const auto& s : session_ids) arr.push_back(s);
    j["sessionIds"] = std::move(arr);
    return j;
}

Json cmd_login(const std::string& id, const std::string& provider_id) {
    Json j = make_cmd(id, "login");
    j["providerId"] = provider_id;
    return j;
}

Json cmd_get_logout_accounts(const std::string& id, const std::string& provider_id) {
    Json j = make_cmd(id, "get_logout_accounts");
    j["providerId"] = provider_id;
    return j;
}

Json cmd_logout(const std::string& id, const std::string& provider_id, int64_t credential_id) {
    Json j = make_cmd(id, "logout");
    j["providerId"] = provider_id;
    j["credentialId"] = credential_id;
    return j;
}

Json cmd_btw(const std::string& id, const std::string& question, const std::string& record_id) {
    Json j = make_cmd(id, "btw");
    j["question"] = question;
    if (!record_id.empty()) j["recordId"] = record_id;
    return j;
}

Json cmd_btw_cancel(const std::string& id, const std::string& record_id) {
    Json j = make_cmd(id, "btw_cancel");
    if (!record_id.empty()) j["recordId"] = record_id;
    return j;
}

Json cmd_predict_word(const std::string& id, const std::string& text, uint32_t utf16_cursor) {
    Json j = make_cmd(id, "predict_word");
    j["text"] = text;
    j["cursor"] = utf16_cursor;
    return j;
}

Json cmd_predict_word_feedback(const std::string& id, const std::string& text, uint32_t utf16_cursor,
                               const std::string& suggestion, bool accepted) {
    Json j = make_cmd(id, "predict_word_feedback");
    j["text"] = text;
    j["cursor"] = utf16_cursor;
    j["suggestion"] = suggestion;
    j["accepted"] = accepted;
    return j;
}

Json cmd_cancel_subagent(const std::string& id, const std::string& subagent_id) {
    Json j = make_cmd(id, "cancel_subagent");
    j["subagentId"] = subagent_id;
    return j;
}

Json cmd_steer_subagent(const std::string& id, const std::string& subagent_id, const std::string& message) {
    Json j = make_cmd(id, "steer_subagent");
    j["subagentId"] = subagent_id;
    j["message"] = message;
    return j;
}

Json cmd_get_subagent_messages(const std::string& id, const std::string& subagent_id, uint64_t from_byte) {
    Json j = make_cmd(id, "get_subagent_messages");
    j["subagentId"] = subagent_id;
    j["fromByte"] = from_byte;
    return j;
}

Json cmd_bash(const std::string& id, const std::string& command, bool exclude_from_context) {
    Json j = make_cmd(id, "bash");
    j["command"] = command;
    if (exclude_from_context) j["excludeFromContext"] = true;
    return j;
}

Json cmd_live_start(const std::string& id) { return make_cmd(id, "live_start"); }
Json cmd_live_stop(const std::string& id) { return make_cmd(id, "live_stop"); }
Json cmd_live_mute(const std::string& id, bool muted) {
    Json j = make_cmd(id, "live_mute");
    j["muted"] = muted;
    return j;
}

Json ui_response_value(const std::string& id, const std::string& value) {
    Json j = Json::object();
    j["type"] = "extension_ui_response";
    j["id"] = id;
    j["value"] = value;
    return j;
}

Json ui_response_confirm(const std::string& id, bool confirmed) {
    Json j = Json::object();
    j["type"] = "extension_ui_response";
    j["id"] = id;
    j["confirmed"] = confirmed;
    return j;
}

Json ui_response_cancel(const std::string& id, bool timed_out) {
    Json j = Json::object();
    j["type"] = "extension_ui_response";
    j["id"] = id;
    j["cancelled"] = true;
    if (timed_out) j["timedOut"] = true;
    return j;
}

Json ui_response_ask(const std::string& id,
                     const std::vector<std::tuple<std::string, std::vector<std::string>, std::string>>& answers) {
    Json j = Json::object();
    j["type"] = "extension_ui_response";
    j["id"] = id;
    Json arr = Json::array();
    for (const auto& [qid, selected, custom] : answers) {
        Json a = Json::object();
        a["id"] = qid;
        Json opts = Json::array();
        for (const auto& s : selected) opts.push_back(s);
        a["selectedOptions"] = std::move(opts);
        if (!custom.empty()) a["customInput"] = custom;
        arr.push_back(std::move(a));
    }
    j["answers"] = std::move(arr);
    return j;
}

} // namespace ompe
