#include "sync/firehose_watch.hpp"

#include <chrono>
#include <csignal>
#include <cstring>
#include <functional>
#include <string>

#include <wolfram/repo/car.h>
#include <wolfram/repo/cbor.h>
#include <wolfram/sync_subscribe.h>

namespace keepsake::sync {

std::string formatRemoteEvent(const RemoteEvent &remote) {
    const DecodedEvent &decoded = remote.event;
    std::string out =
        "[" + remote.did + "] " +
        (decoded.kind.empty() ? "(unknown event)" : decoded.kind) + " at " +
        (decoded.locationId.empty() ? "?" : decoded.locationId);
    if (!decoded.detail.empty()) out += ": " + decoded.detail;
    return out;
}

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

} // namespace

bool decodeEventRecord(const unsigned char *data, size_t len,
                       DecodedEvent &out) {
    wf_cbor_item *record = wf_cbor_parse(data, len);
    if (record == nullptr) return false;

    out.kind = cborString(cborMapGet(record, "kind"));
    out.locationId = cborString(cborMapGet(record, "locationId"));
    out.detail = cborString(cborMapGet(record, "detail"));

    wf_cbor_free(record);
    return !out.kind.empty() || !out.locationId.empty();
}

namespace {

using EventCallback = std::function<void(RemoteEvent)>;

struct EventContext {
    EventCallback callback;
};

// Decodes one click.croft.rpg.event record identified by `op` within
// `commit`'s CAR blocks and hands it to `callback`. Failures here (a
// malformed or foreign record at this path) are silently skipped, not
// fatal — this is reading data other clients wrote, which this process
// doesn't control.
void deliverEventRecord(const wf_subscribe_commit &commit,
                        const wf_subscribe_repo_op &op,
                        const EventCallback &callback) {
    if (!op.has_cid) return;

    wf_car car{};
    if (wf_car_parse(commit.blocks, commit.blocks_len, &car) != WF_OK) return;

    wf_car_block *block = wf_car_find_block(&car, &op.cid);
    if (block != nullptr) {
        DecodedEvent decoded;
        if (decodeEventRecord(block->data, block->data_len, decoded)) {
            callback(RemoteEvent{commit.did, std::move(decoded)});
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
        deliverEventRecord(commit, op, ctx->callback);
    }
}

} // namespace

void watchEvents(std::ostream &out) {
    EventContext ctx{[&out](RemoteEvent remote) {
        out << formatRemoteEvent(remote) << "\n" << std::flush;
    }};

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

EventBridge::~EventBridge() {
    stop();
}

void EventBridge::start() {
    stopRequested_.store(false, std::memory_order_relaxed);
    handle_.store(nullptr, std::memory_order_relaxed);

    thread_ = std::thread([this] {
        // start() and stop() can race (stop() called before this thread
        // even runs); if so, skip connecting entirely rather than opening
        // a subscription nothing will ever cancel via the normal path —
        // stop()'s bounded wait below covers the remaining, narrower race
        // where this check passes just before stopRequested_ flips.
        if (stopRequested_.load(std::memory_order_relaxed)) return;

        EventContext ctx{[this](RemoteEvent remote) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(remote));
        }};

        wf_subscribe_options opts{};
        opts.service = "wss://bsky.network";
        opts.on_event = onEvent;
        opts.on_error = nullptr;
        opts.userdata = &ctx;
        opts.ping_interval_ms = 0;

        // wf_subscribe_start wants a plain wf_subscribe_handle** to write
        // the connected handle through (see the g_handleSlot comment
        // above for why that write lands before the call starts
        // blocking). handle_ needs to be readable from stop() on another
        // thread though, unlike watchEvents()'s single-thread/signal-
        // handler case, so it's a std::atomic<wf_subscribe_handle*>
        // instead of a plain pointer — atomic pointer types are
        // guaranteed lock-free (and so share T*'s object representation)
        // on every platform this project targets (macOS/Linux,
        // x86_64/ARM64), which is what makes handing the library this
        // reinterpreted address safe in practice.
        wf_subscribe_start(&opts,
                           reinterpret_cast<wf_subscribe_handle **>(&handle_));
    });
}

void EventBridge::stop() {
    stopRequested_.store(true, std::memory_order_relaxed);

    // Covers the race where start()'s background thread hasn't published
    // its handle yet (or hasn't even started running) when stop() is
    // called — wait briefly for wf_subscribe_start to connect and publish
    // it. In the ordinary call pattern (start(), play a session, stop())
    // this loop never actually iterates: the handle is long since
    // published by the time a player quits.
    for (int i = 0;
         i < 50 && handle_.load(std::memory_order_acquire) == nullptr &&
         thread_.joinable();
         ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (wf_subscribe_handle *h = handle_.load(std::memory_order_acquire)) {
        wf_subscribe_stop(h);
    }
    if (thread_.joinable()) thread_.join();
}

std::vector<RemoteEvent> EventBridge::drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RemoteEvent> drained = std::move(queue_);
    queue_.clear();
    return drained;
}

} // namespace keepsake::sync
