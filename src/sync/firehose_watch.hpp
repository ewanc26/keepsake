#ifndef KEEPSAKE_SYNC_FIREHOSE_WATCH_HPP
#define KEEPSAKE_SYNC_FIREHOSE_WATCH_HPP

#include <cstddef>
#include <ostream>
#include <string>

namespace keepsake::sync {

struct DecodedEvent {
    std::string kind;
    std::string locationId;
    std::string detail; // may be empty — the field is optional
};

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
// Blocks until interrupted with Ctrl+C. This is deliberately a standalone
// command (`keepsake events`), not folded into the live game loop — doing
// that safely means sharing World/Progress state across the firehose's
// callback and the interactive command loop, which needs real
// synchronization design this phase doesn't have yet. See AGENTS.md.
void watchEvents(std::ostream &out);

} // namespace keepsake::sync

#endif
