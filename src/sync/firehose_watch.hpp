#ifndef KEEPSAKE_SYNC_FIREHOSE_WATCH_HPP
#define KEEPSAKE_SYNC_FIREHOSE_WATCH_HPP

#include <atomic>
#include <cstddef>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

struct wf_subscribe_handle;

namespace keepsake::sync {

struct DecodedEvent {
    std::string kind;
    std::string locationId;
    std::string detail; // may be empty — the field is optional
};

// A decoded click.croft.rpg.event record plus the DID of the repo it came
// from — everything printEventRecord()/EventBridge need to describe one
// firehose event.
struct RemoteEvent {
    std::string did;
    DecodedEvent event;
};

// "[did] kind at locationId: detail" — the same line shape `keepsake
// events` has always printed, factored out so EventBridge's consumer (the
// live game loop) can reuse it instead of re-deriving its own format.
std::string formatRemoteEvent(const RemoteEvent &event);

// Decodes one DAG-CBOR-encoded click.croft.rpg.event record — the bytes
// of a single CAR block, as wf_car_find_block returns them. Returns false
// if the bytes don't parse as CBOR or the record has neither a `kind` nor
// a `locationId` field (a foreign or malformed record at this path).
// Exposed (not file-local) so test/firehose_decode_test.cpp can exercise
// it with synthetic CBOR — the one part of the firehose pipeline that
// could not be verified against live data (see AGENTS.md) can still be
// verified this way, independent of network access.
bool decodeEventRecord(const unsigned char *data, size_t len,
                       DecodedEvent &out);

// Connects to the public relay firehose (no session needed — this is an
// unauthenticated read, the "shared world" half of the design that reading
// achievements/events never requires signing in for) and prints every
// click.croft.rpg.event record any player's client writes, as it arrives.
// Blocks until interrupted with Ctrl+C. This is the standalone `keepsake
// events` command; EventBridge below is the variant that runs alongside
// the interactive game loop instead of taking over the terminal.
void watchEvents(std::ostream &out);

// Bridges the firehose's background thread to the interactive game loop
// without either one touching the other's state directly: EventBridge
// owns the subscription thread and a mutex-protected queue; ui::run calls
// drain() once per turn, from the main thread, to collect whatever arrived
// since the last call and print it as ambient text. Nothing in World or
// Progress is ever touched by the background thread — ui::run only ever
// reads the queued RemoteEvents to decide what to print, never to decide
// what to do, which is what keeps this safe without a bigger
// synchronization redesign (see AGENTS.md's former "not wired into the
// live game loop" entry for why that mattered).
class EventBridge {
  public:
    EventBridge() = default;
    ~EventBridge();
    EventBridge(const EventBridge &) = delete;
    EventBridge &operator=(const EventBridge &) = delete;

    // Starts the background subscription thread. Non-blocking; returns
    // immediately. Call stop() (or let the destructor do it) before the
    // process exits — an unstopped thread will keep the process alive.
    void start();

    // Requests the subscription to stop and joins the background thread.
    // Safe to call even if start() was never called, or this is a second
    // call. May block briefly (bounded) if it races a start() whose
    // subscription hasn't finished connecting yet — see firehose_watch.cpp.
    void stop();

    // Pops every event queued since the last call, in arrival order.
    // Thread-safe, non-blocking (never waits on the network) — meant to be
    // called once per game-loop turn.
    std::vector<RemoteEvent> drain();

  private:
    std::thread thread_;
    std::mutex mutex_;
    std::vector<RemoteEvent> queue_;
    std::atomic<wf_subscribe_handle *> handle_{nullptr};
    std::atomic<bool> stopRequested_{false};
};

} // namespace keepsake::sync

#endif
