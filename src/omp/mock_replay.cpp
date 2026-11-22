#include "omp/mock_replay.h"
#include "core/platform.h"

namespace ompe {

void populate_showcase_fixture(OmpClient& client) {
    const int64_t now = unix_time_ms();
    client.status_ = ConnectionStatus::Connected;
    client.negotiated_version_ = 2;
    client.delta_mode_active_ = true;

    client.state_.session_id = "0195a3b2-7c4d-7000-8a1e-9f4e2a100420";
    client.state_.session_name = "GPU Particle Dissolve & RPC v2 Engine";
    client.state_.model = {
        .provider = "anthropic",
        .id = "claude-sonnet-4-5",
        .name = "Claude Sonnet 4.5",
        .api = "anthropic-messages",
        .reasoning = true,
        .context_window = 200000,
        .max_tokens = 16384,
        .cost_input = 3.0,
        .cost_output = 15.0,
        .supports_image = true,
    };
    client.state_.thinking_level = "high";
    client.state_.is_streaming = false;
    client.state_.is_settled = true;
    client.state_.tokens_per_second = 74.2;
    client.state_.context_usage = {48200, 200000, 24.1, true};
    client.state_.steering_queue = {"Keep idle frame draw calls <= 3 when settled"};
    client.state_.follow_up_queue = {"Run headless visual checks for both Dark and Light palettes"};

    client.stats_.user_messages = 3;
    client.stats_.assistant_messages = 3;
    client.stats_.tool_calls = 4;
    client.stats_.input_tokens = 41800;
    client.stats_.output_tokens = 6400;
    client.stats_.total_tokens = 48200;
    client.stats_.cost_usd = 0.214;

    client.available_models_ = {
        client.state_.model,
        {"anthropic", "claude-opus-4-5", "Claude Opus 4.5", "anthropic-messages", "", true, 200000, 32000, 15.0, 75.0, true},
        {"openai", "gpt-5.2", "GPT-5.2", "openai-responses", "", true, 256000, 32768, 5.0, 20.0, true},
        {"google", "gemini-2.5-pro", "Gemini 2.5 Pro", "google-generative-ai", "", true, 1000000, 65536, 1.25, 10.0, true},
        {"deepseek", "deepseek-reasoner", "DeepSeek Reasoner", "openai-completions", "https://api.deepseek.com/v1", true, 128000, 16384, 0.55, 2.19, false},
        {"ollama", "qwen2.5-coder:32b", "Qwen 2.5 Coder 32B (Local)", "openai-completions", "http://127.0.0.1:11434/v1", false, 32768, 8192, 0.0, 0.0, false},
    };
    client.thinking_levels_ = {"off", "low", "medium", "high", "xhigh"};
    client.available_commands_ = {
        {"compact", "Compact conversation context with optional focus instructions", "builtin", "[focus]"},
        {"fork", "Fork current session branch into a new file", "builtin", ""},
        {"tree", "Inspect and navigate session history tree", "builtin", ""},
        {"btw", "Ask a quick side question without polluting main context", "builtin", "<question>"},
        {"model", "Switch active provider and model", "builtin", ""},
        {"login", "Authenticate with a provider via OAuth", "builtin", "<provider>"},
    };
    client.login_providers_ = {
        {"anthropic", "Anthropic (Claude)", true, true},
        {"openai-codex", "OpenAI Codex", true, true},
        {"github-copilot", "GitHub Copilot", true, false},
        {"google-gemini-cli", "Google Gemini CLI", true, false},
    };

    client.messages_.clear();
    client.tool_execs_.clear();

    // Turn 1: User
    {
        ChatMessage u;
        u.id = "msg_u1";
        u.role = "user";
        u.timestamp_ms = now - 180000;
        u.blocks.push_back({ContentBlockKind::Text, "帮我把 OMP RPC v2 的流式增量解析和 Etch 粒子溶解动效打通，保持界面极简、零冗余。"});
        client.messages_.push_back(std::move(u));
    }

    // Turn 1: Assistant with Thinking + 2 compact Tool Executions (edit + bash) + rich Markdown/Math/Mermaid
    {
        ChatMessage a;
        a.id = "msg_a1";
        a.role = "assistant";
        a.model_id = "claude-sonnet-4-5";
        a.timestamp_ms = now - 165000;
        a.output_tokens = 1420;
        a.cost_usd = 0.048;

        ContentBlock th;
        th.kind = ContentBlockKind::Thinking;
        th.text = "Inspecting OMP RPC v2 framing contract: negotiate_protocol(2) unlocks uninterrupted rpc_chunk reassembly up to 64 MiB. "
                  "Enabling messageUpdates: \"delta\" keeps frame payload O(delta) so InlayDoc incremental appends run in sub-millisecond time without layout reflow.\n"
                  "For tactile particle dissolve, emit GPU-batched micro-rects in Etch's overlay pass with additive ember blending on dark surfaces and immediate idle sleep when particles settle.";
        th.started_ms = now - 178000;
        th.ended_ms = now - 174600;
        a.blocks.push_back(std::move(th));

        // Consecutive read-only exploration steps -> coalesce cleanly into ONE borderless trace line!
        auto add_explore = [&](const char* cid, const char* tname, const char* arg, const char* out, int64_t dur) {
            ContentBlock b;
            b.kind = ContentBlockKind::ToolCall;
            b.tool_call_id = cid;
            b.tool_name = tname;
            b.tool_intent = arg;
            a.blocks.push_back(std::move(b));

            ToolExecution te;
            te.tool_call_id = cid;
            te.tool_name = tname;
            te.args_summary = arg;
            te.output_text = out;
            te.running = false;
            te.started_ms = now - 174000;
            te.ended_ms = now - 174000 + dur;
            client.tool_execs_[te.tool_call_id] = std::move(te);
        };
        add_explore("call_read_1", "read", "src/omp/protocol.cpp",
                    "1: #include \"omp/protocol.h\"\n2: #include \"core/base64.h\"\n...\n42: RpcFrameReassembler::Result RpcFrameReassembler::feed_line(std::string_view line, JsonValue& out_json) {", 14);
        add_explore("call_read_2", "read", "src/theme/particles.h",
                    "enum class DissolveStyle : uint8_t { Dust, Ember, Sand, Vortex };\nvoid spawn_dissolve_rect(etch::Rect r, etch::Color base, etch::Color accent, DissolveStyle style, uint32_t count);", 9);
        add_explore("call_grep_1", "grep", "rpc_chunk in src/omp",
                    "src/omp/protocol.cpp:44: if (type != \"rpc_chunk\")\nsrc/omp/client.cpp:318: reassembler_.feed_line(line, msg)", 22);

        // Structured Execution Plan Card
        {
            ContentBlock pl;
            pl.kind = ContentBlockKind::ToolCall;
            pl.tool_call_id = "call_plan_1";
            pl.tool_name = "todo_write";
            pl.tool_args_json =
                "{\"todos\":["
                "{\"content\":\"Wire RPC v2 chunk reassembly and delta stream projection\",\"status\":\"completed\"},"
                "{\"content\":\"Upgrade Etch duotone SVG icons & Inlay auto-fit Mermaid layout\",\"status\":\"completed\"},"
                "{\"content\":\"Run unit tests and headless D3D12 visual verification\",\"status\":\"completed\"}"
                "]}";
            a.blocks.push_back(std::move(pl));
        }

        ContentBlock tc1;
        tc1.kind = ContentBlockKind::ToolCall;
        tc1.tool_call_id = "call_edit_1";
        tc1.tool_name = "edit";
        tc1.tool_intent = "Wire RPC v2 chunk reassembly and delta projection";
        tc1.tool_args_json = "{\"path\": \"src/omp/protocol.cpp\"}";
        a.blocks.push_back(std::move(tc1));

        ToolExecution te1;
        te1.tool_call_id = "call_edit_1";
        te1.tool_name = "edit";
        te1.intent = "Wire RPC v2 chunk reassembly and delta projection";
        te1.args_summary = "src/omp/protocol.cpp";
        te1.running = false;
        te1.started_ms = now - 173000;
        te1.ended_ms = now - 172620;
        // Mirrors real OMP wire contract: output_text is concise summary; diff_text holds OMP canonical line-numbered diff
        te1.output_text = "Edited src/omp/protocol.cpp (2 hunks, +5 -2)";
        te1.diff_text =
            " 41|  if (type != \"rpc_chunk\") {\n"
            "-42|    out_json = parsed;\n"
            "+42|    if (active_) reset();\n"
            "+43|    out_json = std::move(parsed);\n"
            "+44|    return Result::FrameReady;\n"
            " 45|  }\n"
            "\xE2\x80\xA6\n"
            "-78|  return Json::parse(buffer_, out_json) ? Result::FrameReady : Result::Error;\n"
            "+80|  // Enforce strict UTF-8 boundary before JSON parse\n"
            "+81|  return is_valid_utf8(buffer_) && Json::parse(buffer_, out_json) ? Result::FrameReady : Result::Error;";
        client.tool_execs_[te1.tool_call_id] = std::move(te1);

        ContentBlock tc2;
        tc2.kind = ContentBlockKind::ToolCall;
        tc2.tool_call_id = "call_bash_1";
        tc2.tool_name = "bash";
        tc2.tool_intent = "Run unit tests and headless GPU verification";
        tc2.tool_args_json = "{\"command\": \"ctest --test-dir build --output-on-failure\"}";
        a.blocks.push_back(std::move(tc2));

        ToolExecution te2;
        te2.tool_call_id = "call_bash_1";
        te2.tool_name = "bash";
        te2.intent = "Run unit tests and headless GPU verification";
        te2.args_summary = "ctest --test-dir build --output-on-failure";
        te2.running = false;
        te2.started_ms = now - 171500;
        te2.ended_ms = now - 170140;
        te2.output_text =
            "\x1b[32m1/2 Test #1: ompe_core_unit ..................   Passed    0.04 sec\x1b[0m\n"
            "\x1b[32m2/2 Test #2: ompe_headless_visual ............   Passed    0.31 sec\x1b[0m\n"
            "\x1b[1m100% tests passed, 0 tests failed out of 2\x1b[0m";
        te2.terminal = std::make_shared<AnsiTerminalBuffer>(88, 6);
        te2.terminal->feed("\x1b[33m$ ctest --test-dir build --output-on-failure\x1b[0m\n" + te2.output_text);
        client.tool_execs_[te2.tool_call_id] = std::move(te2);

        ContentBlock body;
        body.kind = ContentBlockKind::Text;
        body.text =
            "已完成 **OMP RPC v2 通信闭环** 与 **GPU 粒子微交互引擎** 的对接，整条链路保持纯原生渲染与零冗余界面：\n\n"
            "1. **无损分片与纯增量流**：握手后自动升级至 `protocolVersion: 2`，开启 `messageUpdates: \"delta\"`，流式文本直接增量送入 `InlayDoc`（冻结块零重排）。\n"
            "2. **克制粒子物理**：在发送消息、撤回排队芯片、关闭通知及中止生成时触发 `DissolveStyle::Dust / Ember` 溶解粒子，粒子衰减公式满足 $\\alpha(t) = \\min(1, 8t)(1 - t)^2$，归零瞬间立即恢复 GPU `0%` 空闲休眠。\n\n"
            "```mermaid\n"
            "flowchart LR\n"
            "  OMP[omp --mode rpc-ui] -->|JSONL v2 delta| Pipe[Async Pipe Thread]\n"
            "  Pipe -->|wake_up| UI[Etch Pure-GPU UI]\n"
            "  UI -->|InlayDoc| Stream[Markdown + Math + Mermaid]\n"
            "```\n\n"
            "```diff\n"
            "@@ -18,3 +18,4 @@ Composer::trigger_send\n"
            "   if (msg.empty()) return;\n"
            "-  client.send_prompt(msg);\n"
            "+  particles.spawn_send_burst(composer_top, pal.accent, pal.secondary);\n"
            "+  client.send_prompt(msg, pending_images);\n"
            "```";
        a.blocks.push_back(std::move(body));
        client.messages_.push_back(std::move(a));
    }

    // Subagents for Agent Hub inspector
    client.subagents_ = {
        {
            .id = "ResearchWorker-1",
            .agent = "research",
            .agent_source = "bundled",
            .description = "Audit session lease lock contract across Win/Linux/macOS",
            .task = "Verify NtQueryDirectoryObject & OFD fcntl lease probes",
            .status = "running",
            .resolved_model = "anthropic/claude-haiku-4-5",
            .current_tool = "grep",
            .current_tool_args = "file_lock in crates/natives/src",
            .tool_count = 7,
            .requests = 4,
            .tokens = 12450,
            .context_tokens = 12450,
            .context_window = 200000,
            .cost_usd = 0.019,
            .duration_ms = 14200,
            .last_update_ms = now - 1200,
            .can_cancel = true,
            .can_steer = true,
            .recent_output = {"Scanned crates/natives/src/file_lock/windows.rs", "Confirmed non-intrusive probe via NtQueryDirectoryObject"},
        },
        {
            .id = "ShaderVerifier-2",
            .agent = "builder",
            .agent_source = "project",
            .description = "Cross-compile omp_ambient.hlsl to DXIL / SPIR-V / MSL",
            .task = "Verify zero warnings under DXC -WX -Ges",
            .status = "completed",
            .resolved_model = "anthropic/claude-sonnet-4-5",
            .current_tool = "",
            .tool_count = 3,
            .requests = 2,
            .tokens = 5820,
            .context_tokens = 5820,
            .context_window = 200000,
            .cost_usd = 0.024,
            .duration_ms = 6400,
            .last_update_ms = now - 45000,
            .can_cancel = false,
            .can_steer = false,
            .recent_output = {"DXIL + SPIR-V + MSL headers generated cleanly"},
        },
    };

    // Session Tree for Canvas DAG inspector
    SessionTreeNode root{.id = "e001", .type = "message", .role = "user", .preview = "Design OMP native desktop on Etch + Inlay"};
    SessionTreeNode n2{.id = "e002", .parent_id = "e001", .type = "message", .role = "assistant", .preview = "Architecture: Pure C++20 + RPC v2 + OKLCH"};
    SessionTreeNode n3a{.id = "e003", .parent_id = "e002", .type = "message", .role = "user", .preview = "Add GPU dissolve particles & strip clutter", .label = "main"};
    SessionTreeNode n4a{.id = "e004", .parent_id = "e003", .type = "message", .role = "assistant", .preview = "Implemented ParticleSystem & 4 OKLCH palettes"};
    SessionTreeNode n3b{.id = "e005", .parent_id = "e002", .type = "message", .role = "user", .preview = "Explore Rust FFI bridge alternative", .label = "abandoned"};
    n3a.children.push_back(std::move(n4a));
    n2.children.push_back(std::move(n3a));
    n2.children.push_back(std::move(n3b));
    root.children.push_back(std::move(n2));
    client.tree_roots_ = {std::move(root)};
    client.tree_leaf_id_ = "e004";

    // BTW sidecar thread
    client.btw_records_ = {
        {
            .id = "btw_01",
            .leaf_id = "e004",
            .title = "models.yml hot reload?",
            .question = "改完 models.yml 后 RPC 会自动热重载吗？",
            .answer = "不会。OMP 的 RPC 模式仅热监听 `config.yml`，`models.yml` 改动需要重启 RPC 子进程才能刷新模型目录——桌面端在保存提供商时会自动平滑重连。",
            .status = "complete",
            .created_at = now - 90000,
        },
    };
}

void populate_showcase_sessions(std::vector<SessionListItem>& out_sessions) {
    const int64_t now = unix_time_ms();
    out_sessions = {
        {"0195a3b2-7c4d-7000-8a1e-9f4e2a100420", "", "/workspace/omp-desktop", "GPU Particle Dissolve & RPC v2 Engine", "", "complete", now - 3600000, now - 60000, 18420, true, false},
        {"0195a3b0-1122-7000-8a1e-9f4e2a100111", "", "/workspace/omp-desktop", "OKLCH Obsidian Brass & Alabaster Ink", "", "complete", now - 7200000, now - 2400000, 9240, true, false},
        {"0195a3af-9988-7000-8a1e-9f4e2a100222", "", "/workspace/omp-desktop", "Cross-platform Job Object stdio pipes", "", "interrupted", now - 18000000, now - 9600000, 14100, false, true},
        {"0195a3ae-5566-7000-8a1e-9f4e2a100333", "", "/workspace/etch", "Slug Bezier small-text hinting check", "", "complete", now - 86400000, now - 82000000, 6100, false, false},
    };
}

} // namespace ompe
