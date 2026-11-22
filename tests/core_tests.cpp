#include "core/ansi.h"
#include "core/base64.h"
#include "core/i18n.h"
#include "core/json.h"
#include "core/updater.h"
#include "omp/protocol.h"
#include "ui/diff_view.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__,        \
                         __LINE__, #cond);                                       \
            std::exit(1);                                                        \
        }                                                                        \
    } while (0)

static void test_json_unicode_and_roundtrip() {
    ompe::Json j;
    std::string err;
    const bool ok = ompe::Json::parse(
        R"({"title":"OMP \u4e2d\u6587 \ud83d\ude80","num":42,"flt":3.14,"arr":[true,false,null]})",
        j, &err);
    CHECK(ok && err.empty());
    CHECK(j["num"].as_int() == 42);
    CHECK(j["title"].as_string() == "OMP 中文 🚀");
    CHECK(j["arr"].size() == 3);

    ompe::Json j2;
    CHECK(ompe::Json::parse(j.dump(0), j2));
    CHECK(j2["title"].as_string() == "OMP 中文 🚀");
}

static void test_rpc_v2_chunk_reassembly() {
    ompe::RpcFrameReassembler r;
    const std::string payload = R"({"type":"response","id":"req_9","command":"get_messages","success":true,"data":{"count":2}})";
    const size_t half = payload.size() / 2;
    const std::string c0 = ompe::base64_encode(std::string_view(payload).substr(0, half));
    const std::string c1 = ompe::base64_encode(std::string_view(payload).substr(half));

    ompe::Json frame0 = ompe::Json::object();
    frame0["type"] = "rpc_chunk";
    frame0["chunkId"] = "chk-1";
    frame0["index"] = 0;
    frame0["count"] = 2;
    frame0["byteLength"] = static_cast<int64_t>(payload.size());
    frame0["data"] = c0;

    ompe::Json frame1 = ompe::Json::object();
    frame1["type"] = "rpc_chunk";
    frame1["chunkId"] = "chk-1";
    frame1["index"] = 1;
    frame1["count"] = 2;
    frame1["byteLength"] = static_cast<int64_t>(payload.size());
    frame1["data"] = c1;

    ompe::Json out;
    std::string err;
    CHECK(r.feed_line(frame0.dump(0), out, err) == ompe::RpcFrameReassembler::Result::NeedMore);
    CHECK(r.feed_line(frame1.dump(0), out, err) == ompe::RpcFrameReassembler::Result::FrameReady);
    CHECK(out["id"].as_string() == "req_9");
    CHECK(out["data"]["count"].as_int() == 2);
}

static void test_ansi_terminal_buffer() {
    ompe::AnsiTerminalBuffer term(40, 8);
    term.feed("\x1b[32mPASS\x1b[0m 中文\nline2");
    CHECK(term.used_rows() == 2);
    CHECK(term.cells()[0].codepoint == 'P');
    CHECK(term.cells()[0].foreground.mode == etch::TerminalColorMode::Ansi16);
    CHECK(term.cells()[0].foreground.value == 2);
    CHECK(term.cells()[5].width == 2);
    CHECK(term.cells()[6].width == 0);
}

static void test_omp_diff_extraction_and_parsing() {
    // 1. Real OMP toolResult wire format stores concise summary in content and canonical diff in details.diff
    const char* tr_json = R"json({
        "role": "toolResult",
        "toolCallId": "call_1",
        "toolName": "edit",
        "content": [{"type": "text", "text": "Edited src/a.cpp (1 hunk, +2 -1)"}],
        "details": {
            "path": "src/a.cpp",
            "diff": " 41|  int x = 1;\n-42|  return x + 1;\n+42|  return x + 2;\n+43|  // done"
        }
    })json";
    ompe::Json tr;
    CHECK(ompe::Json::parse(tr_json, tr));
    const ompe::ChatMessage msg = ompe::parse_agent_message(tr);
    CHECK(msg.tool_path == "src/a.cpp");
    CHECK(!msg.tool_diff.empty());
    const auto [a, d] = ompe::count_diff_stats(msg.tool_diff);
    CHECK(a == 2 && d == 1);

    const auto parsed = ompe::parse_structured_diff(msg.tool_diff);
    CHECK(parsed.size() == 4);
    CHECK(parsed[0].kind == ompe::DiffLineKind::Context && parsed[0].line_num == "41");
    CHECK(parsed[1].kind == ompe::DiffLineKind::Del && parsed[1].line_num == "42");
    CHECK(parsed[2].kind == ompe::DiffLineKind::Add && parsed[2].line_num == "42");
    // Intra-line emphasis should pinpoint '1' vs '2'
    CHECK(parsed[1].emph_end > parsed[1].emph_start);
    CHECK(parsed[2].emph_end > parsed[2].emph_start);
    CHECK(parsed[1].code.substr(parsed[1].emph_start, parsed[1].emph_end - parsed[1].emph_start) == "1");
    CHECK(parsed[2].code.substr(parsed[2].emph_start, parsed[2].emph_end - parsed[2].emph_start) == "2");

    // 2. ast_edit box-drawing vertical bar format + classic unified diff
    const std::string ast_diff =
        "# src/foo.ts\n"
        "-468\xE2\x94\x82" "const a = 1;\n"
        "+468\xE2\x94\x82" "const a = 42;\n"
        "\xE2\x80\xA6\n"
        "@@ -10,2 +10,2 @@\n"
        "-old()\n"
        "+new()";
    const auto ast_parsed = ompe::parse_structured_diff(ast_diff);
    CHECK(ast_parsed.size() == 7);
    CHECK(ast_parsed[0].kind == ompe::DiffLineKind::Header);
    CHECK(ast_parsed[1].kind == ompe::DiffLineKind::Del && ast_parsed[1].line_num == "468");
    CHECK(ast_parsed[2].kind == ompe::DiffLineKind::Add && ast_parsed[2].line_num == "468");
    CHECK(ast_parsed[3].kind == ompe::DiffLineKind::Hunk);
    CHECK(ast_parsed[4].kind == ompe::DiffLineKind::Hunk);
    CHECK(ast_parsed[5].kind == ompe::DiffLineKind::Del && ast_parsed[5].line_num == "10");
    CHECK(ast_parsed[6].kind == ompe::DiffLineKind::Add && ast_parsed[6].line_num == "10");
}

static void test_extensible_i18n_catalog() {
    namespace fs = std::filesystem;

    // 1. BCP-47 tag normalization
    CHECK(ompe::normalize_locale_tag("zh_CN.UTF-8") == "zh-CN");
    CHECK(ompe::normalize_locale_tag("zh") == "zh-CN");
    CHECK(ompe::normalize_locale_tag("en") == "en-US");
    CHECK(ompe::normalize_locale_tag("ja_jp@euro") == "ja-JP");
    CHECK(ompe::normalize_locale_tag("auto") == "auto");

    // 2. UTF-8 safe truncation never splits multi-byte CJK codepoints
    const std::string cjk = "你好世界测试字符串"; // 9 chars = 27 bytes
    const std::string trunc = ompe::utf8_truncate_ellipsis(cjk, 11);
    CHECK(trunc == "你好..."); // 6 bytes + 3 bytes = 9 <= 11, intact UTF-8 boundary

    // 3. Extensible directory discovery + 3rd locale (ja-JP) + fallback chain + user override + placeholder reordering
    const fs::path base = fs::temp_directory_path() / "ompe_i18n_test";
    const fs::path bundled = base / "bundled";
    const fs::path user_dir = base / "user";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(bundled, ec);
    fs::create_directories(user_dir, ec);

    {
        std::ofstream f(bundled / "en-US.json", std::ios::binary);
        f << R"({"_meta":{"code":"en-US","name":"English","englishName":"English"},"strings":{"greet":"Hello {name}","stats":"{nodes} nodes · {branches} branches","only_en":"Base Only"}})";
    }
    {
        std::ofstream f(bundled / "zh-CN.json", std::ios::binary);
        f << R"({"_meta":{"code":"zh-CN","name":"简体中文","englishName":"Chinese","fallback":"en-US","cjkSpacing":true},"strings":{"greet":"你好 {name}"}})";
    }
    {
        // Drop-in 3rd language in user locales folder with reordered placeholders & partial keys!
        std::ofstream f(user_dir / "ja-JP.json", std::ios::binary);
        f << R"({"_meta":{"code":"ja-JP","name":"日本語","englishName":"Japanese","fallback":"en-US","cjkSpacing":true},"strings":{"stats":"ブランチ {branches} · ノード {nodes}"}})";
    }
    {
        // User override patching a single key in en-US
        std::ofstream f(user_dir / "en-US.json", std::ios::binary);
        f << R"({"_meta":{"code":"en-US","name":"English"},"strings":{"greet":"Welcome, {name}!"}})";
    }

    ompe::I18nCatalog cat;
    cat.load_directories({bundled.string(), user_dir.string()}, 1);
    CHECK(cat.locales().size() == 3); // en-US, zh-CN, and dynamically discovered ja-JP!

    // User override on en-US takes precedence while retaining non-overridden keys
    cat.set_locale("en-US");
    CHECK(cat.format("greet", "", {{"name", "OMP"}}) == "Welcome, OMP!");
    CHECK(std::string(cat.get("only_en", "")) == "Base Only");
    CHECK(!cat.is_cjk_spacing());

    // Dynamically discovered 3rd locale (ja-JP): reordered placeholders + automatic fallback to en-US
    cat.set_locale("ja");
    CHECK(cat.active_code() == "ja-JP");
    CHECK(cat.is_cjk_spacing());
    CHECK(cat.format("stats", "", {{"nodes", "12"}, {"branches", "3"}}) == "ブランチ 3 · ノード 12");
    CHECK(std::string(cat.get("only_en", "")) == "Base Only"); // Inherited from fallback!

    // Region-prefix negotiation ("zh-SG" -> "zh-CN")
    cat.set_locale("zh-SG");
    CHECK(cat.active_code() == "zh-CN");
    CHECK(cat.format("greet", "", {{"name", "世界"}}) == "你好 世界");

    fs::remove_all(base, ec);
}

void test_hidden_custom_message_and_system_xml_stripping() {
    // Real OMP eval-prelude-guide custom_message with display=false must be marked hidden
    const char* prelude_raw =
        R"json({"role":"custom","customType":"eval-prelude-guide","content":"<system-notice id=\"prelude-guide\">\n`browser` guide\n</system-notice>","display":false,"attribution":"agent"})json";
    ompe::Json prelude_msg;
    CHECK(ompe::Json::parse(prelude_raw, prelude_msg));
    const ompe::ChatMessage m1 = ompe::parse_agent_message(prelude_msg, "m1");
    CHECK(m1.hidden);
    CHECK(m1.custom_type == "eval-prelude-guide");

    // User message with injected <system-reminder> keeps only genuine user prompt
    const char* user_raw =
        R"json({"role":"user","content":[{"type":"text","text":"<system-reminder>\nToday is 2026-11-22\n</system-reminder>\n\nFix the maximize button"}]})json";
    ompe::Json user_msg;
    CHECK(ompe::Json::parse(user_raw, user_msg));
    const ompe::ChatMessage m2 = ompe::parse_agent_message(user_msg, "m2");
    CHECK(!m2.hidden);
    CHECK(m2.blocks.size() == 1);
    CHECK(m2.blocks[0].text == "Fix the maximize button");

    // Streaming assistant message_start with empty content array must NOT be hidden!
    const char* empty_asst_raw = R"json({"role":"assistant","content":[]})json";
    ompe::Json empty_asst;
    CHECK(ompe::Json::parse(empty_asst_raw, empty_asst));
    const ompe::ChatMessage m3 = ompe::parse_agent_message(empty_asst, "m3");
    CHECK(!m3.hidden);

    // Explicit display=true custom message stays visible after stripping any embedded system notice
    const char* visible_custom_raw =
        R"json({"role":"custom","customType":"plugin-banner","content":"Connected to workspace","display":true})json";
    ompe::Json visible_custom;
    CHECK(ompe::Json::parse(visible_custom_raw, visible_custom));
    const ompe::ChatMessage m4 = ompe::parse_agent_message(visible_custom, "m4");
    CHECK(!m4.hidden);
    CHECK(m4.blocks.size() == 1 && m4.blocks[0].text == "Connected to workspace");

    // Tree node for custom_message (display=false) is categorized as non-turn meta without raw XML leak
    const char* tree_node_raw =
        R"json({"entry":{"id":"n1","type":"custom_message","customType":"eval-prelude-guide","display":false,"content":"<system-notice id=\"prelude-guide\">secret</system-notice>"},"children":[]})json";
    ompe::Json tree_node;
    CHECK(ompe::Json::parse(tree_node_raw, tree_node));
    const ompe::SessionTreeNode tn = ompe::parse_tree_node(tree_node);
    CHECK(tn.role == "meta");
    CHECK(!tn.is_conversation_turn());
    CHECK(tn.preview == "eval-prelude-guide");
}

static void test_subagent_payload_and_lifecycle() {
    // Real OMP subagent_lifecycle frame wraps fields in "payload" and sends status="started"
    const char* lifecycle_raw = R"json({
        "type": "subagent_lifecycle",
        "payload": {
            "id": "sub_42",
            "parentToolCallId": "call_task_9",
            "agent": "explore",
            "description": "Audit protocol framing",
            "status": "started",
            "model": "claude-sonnet-4-5"
        }
    })json";
    ompe::Json lf;
    CHECK(ompe::Json::parse(lifecycle_raw, lf));
    const ompe::SubagentInfo s1 = ompe::parse_subagent_snapshot(lf);
    CHECK(s1.id == "sub_42");
    CHECK(s1.parent_tool_call_id == "call_task_9");
    CHECK(s1.agent == "explore");
    CHECK(s1.status == "running");
    CHECK(s1.can_cancel && s1.can_steer);

    // Real OMP subagent_progress frame nests progress object inside payload.progress
    const char* progress_raw = R"json({
        "type": "subagent_progress",
        "payload": {
            "progress": {
                "id": "sub_42",
                "agent": "explore",
                "status": "completed",
                "currentTool": "grep",
                "toolCount": 6,
                "tokens": 4200,
                "costUsd": 0.014,
                "durationMs": 3100
            }
        }
    })json";
    ompe::Json pr;
    CHECK(ompe::Json::parse(progress_raw, pr));
    const ompe::SubagentInfo s2 = ompe::parse_subagent_snapshot(pr);
    CHECK(s2.id == "sub_42");
    CHECK(s2.status == "completed");
    CHECK(s2.tool_count == 6);
    CHECK(s2.tokens == 4200);
    CHECK(!s2.can_cancel && !s2.can_steer);
}

static void test_updater_semver_and_release_parsing() {
    const auto v010 = ompe::SemVer::parse("v0.1.0");
    const auto v011 = ompe::SemVer::parse("0.1.1");
    const auto v020_rc = ompe::SemVer::parse("v0.2.0-rc.1");
    const auto v020 = ompe::SemVer::parse("v0.2.0");
    CHECK(v010.valid && v011.valid && v020_rc.valid && v020.valid);
    CHECK(v011.is_newer_than(v010));
    CHECK(!v010.is_newer_than(v011));
    CHECK(!v010.is_newer_than(v010));
    CHECK(v020.is_newer_than(v020_rc));
    CHECK(!v020_rc.is_newer_than(v020));

    const char* rel_json = R"json({
        "tag_name": "v0.2.0",
        "name": "OMP Desktop v0.2.0",
        "body": "## Highlights\n- Faster startup",
        "html_url": "https://github.com/senran-N/OMP-E/releases/tag/v0.2.0",
        "draft": false,
        "prerelease": false,
        "assets": [
            {
                "name": "OMP-Desktop-0.2.0-windows-x64-setup.exe",
                "browser_download_url": "https://github.com/senran-N/OMP-E/releases/download/v0.2.0/OMP-Desktop-0.2.0-windows-x64-setup.exe",
                "size": 12345678
            },
            {
                "name": "OMP-Desktop-0.2.0-macos-arm64.dmg",
                "browser_download_url": "https://github.com/senran-N/OMP-E/releases/download/v0.2.0/OMP-Desktop-0.2.0-macos-arm64.dmg",
                "size": 11000000
            },
            {
                "name": "SHA256SUMS.txt",
                "browser_download_url": "https://github.com/senran-N/OMP-E/releases/download/v0.2.0/SHA256SUMS.txt",
                "size": 256
            }
        ]
    })json";

    ompe::UpdateSnapshot snap;
    CHECK(ompe::parse_github_release_json(rel_json, "0.1.0", snap));
    CHECK(snap.state == ompe::UpdateState::Available);
    CHECK(snap.latest_version == "0.2.0");
#ifdef _WIN32
    CHECK(snap.asset_name == "OMP-Desktop-0.2.0-windows-x64-setup.exe");
    CHECK(snap.asset_size_bytes == 12345678u);
#endif
    CHECK(!snap.sha256_url.empty());

    // Already up-to-date when on v0.2.0
    ompe::UpdateSnapshot snap_same;
    CHECK(ompe::parse_github_release_json(rel_json, "0.2.0", snap_same));
    CHECK(snap_same.state == ompe::UpdateState::UpToDate);

    // SHA256SUMS parser handles standard lowercase/uppercase and '*' binary mode prefix
    const std::string sums =
        "AABBCCDDEEFF00112233445566778899AABBCCDDEEFF00112233445566778899  *OMP-Desktop-0.2.0-windows-x64-setup.exe\n"
        "1111111111111111111111111111111111111111111111111111111111111111  other.dmg\n";
    CHECK(ompe::find_sha256_for_asset(sums, "OMP-Desktop-0.2.0-windows-x64-setup.exe") ==
          "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899");
}

int main() {
    test_json_unicode_and_roundtrip();
    test_rpc_v2_chunk_reassembly();
    test_ansi_terminal_buffer();
    test_omp_diff_extraction_and_parsing();
    test_extensible_i18n_catalog();
    test_hidden_custom_message_and_system_xml_stripping();
    test_subagent_payload_and_lifecycle();
    test_updater_semver_and_release_parsing();
    std::puts("ompe_core_tests: PASS");
    return 0;
}
