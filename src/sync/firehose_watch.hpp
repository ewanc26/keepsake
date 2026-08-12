#ifndef KEEPSAKE_SYNC_FIREHOSE_WATCH_HPP
#define KEEPSAKE_SYNC_FIREHOSE_WATCH_HPP

#include <ostream>

namespace keepsake::sync {

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
