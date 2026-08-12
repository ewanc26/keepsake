#include "sync/firehose_watch.hpp"

#include <csignal>
#include <cstring>
#include <string>

#include <wolfram/repo/car.h>
#include <wolfram/repo/cbor.h>
#include <wolfram/sync_subscribe.h>

namespace keepsake::sync {

namespace {

// wf_subscribe_start writes *out before it starts blocking (see
// sync_subscribe.c: `*out = handle;` precedes the blocking subscribe_loop
// call), so by the time SIGINT can fire, `*g_handleSlot` already holds the
// real handle — this local's address is captured before the call, not its
// (still-null) value. wf_subscribe_stop only sets a plain flag internally,
// so calling it from a signal handler is safe in practice even though it
// isn't on the strict POSIX async-signal-safe list.
wf_subscribe_handle **g_handleSlot = nullptr;

void handleSigint(int) {
    if (g_handleSlot && *g_handleSlot) wf_subscribe_stop(*g_handleSlot);
}

const wf_cbor_item *cborMapGet(const wf_cbor_item *item, const char *key) {
    if (item == nullptr || item->type != WF_CBOR_MAP) return nullptr;
    for (size_t i = 0; i < item->map.count; ++i) {
        const wf_cbor_item *k = item->map.pairs[i].key;
        if (k != nullptr && k->type == WF_CBOR_STRING &&
            k->string.str != nullptr && std::strcmp(k->string.str, key) == 0) {
            return item->map.pairs[i].value;
        }
    }
    return nullptr;
}

std::string cborString(const wf_cbor_item *item) {
    if (item == nullptr || item->type != WF_CBOR_STRING ||
        item->string.str == nullptr) {
        return "";
    }
    return std::string(item->string.str, item->string.len);
}

struct EventContext {
    std::ostream *out;
};

// Decodes and prints one click.croft.rpg.event record identified by `op`
// within `commit`'s CAR blocks. Failures here (a malformed or foreign
// record at this path) are silently skipped, not fatal — this is reading
// data other clients wrote, which this process doesn't control.
void printEventRecord(const wf_subscribe_commit &commit,
                      const wf_subscribe_repo_op &op, std::ostream &out) {
    if (!op.has_cid) return;

    wf_car car{};
    if (wf_car_parse(commit.blocks, commit.blocks_len, &car) != WF_OK) return;

    wf_car_block *block = wf_car_find_block(&car, &op.cid);
    if (block != nullptr) {
        wf_cbor_item *record = wf_cbor_parse(block->data, block->data_len);
        if (record != nullptr) {
            std::string kind = cborString(cborMapGet(record, "kind"));
            std::string locationId =
                cborString(cborMapGet(record, "locationId"));
            std::string detail = cborString(cborMapGet(record, "detail"));

            out << "[" << commit.did << "] "
                << (kind.empty() ? "(unknown event)" : kind) << " at "
                << (locationId.empty() ? "?" : locationId);
            if (!detail.empty()) out << ": " << detail;
            out << "\n" << std::flush;

            wf_cbor_free(record);
        }
    }
    wf_car_free(&car);
}

void onEvent(const wf_subscribe_event *event, void *userdata) {
    if (event == nullptr || event->type != WF_SUBSCRIBE_EVENT_COMMIT) return;
    auto *ctx = static_cast<EventContext *>(userdata);
    const wf_subscribe_commit &commit = event->data.commit;

    for (size_t i = 0; i < commit.ops_count; ++i) {
        const wf_subscribe_repo_op &op = commit.ops[i];
        if (op.path == nullptr) continue;
        if (std::strcmp(op.action, "create") != 0) continue;
        std::string path = op.path;
        if (path.rfind("click.croft.rpg.event/", 0) != 0) continue;
        printEventRecord(commit, op, *ctx->out);
    }
}

} // namespace

void watchEvents(std::ostream &out) {
    EventContext ctx{&out};

    wf_subscribe_options opts{};
    opts.service = "wss://bsky.network";
    opts.on_event = onEvent;
    opts.on_error = nullptr;
    opts.userdata = &ctx;
    opts.ping_interval_ms = 0;

    wf_subscribe_handle *handle = nullptr;
    g_handleSlot = &handle;
    struct sigaction action{};
    action.sa_handler = handleSigint;
    sigemptyset(&action.sa_mask);
    struct sigaction previous{};
    sigaction(SIGINT, &action, &previous);

    out << "Watching the firehose for click.croft.rpg.event records — "
           "Ctrl+C to stop.\n"
        << std::flush;
    wf_subscribe_start(&opts, &handle);

    sigaction(SIGINT, &previous, nullptr);
    g_handleSlot = nullptr;
    out << "Stopped.\n";
}

} // namespace keepsake::sync
