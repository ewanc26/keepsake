// Round-trips a synthetic click.croft.rpg.event record through the exact
// CAR/CBOR pipeline sync::firehose_watch.cpp's onEvent() uses, without any
// network access. This is the one part of the firehose reader that could
// not be verified against live data during development (wss://
// connections are blocked in that environment) — this test closes that
// gap by encoding a real record with wolfram's own JSON->CBOR encoder,
// wrapping it in a real CAR block, and decoding it back with
// sync::decodeEventRecord, the identical function the live path calls.
//
// Must pass in Release builds, where -DNDEBUG strips <cassert>'s assert(),
// so checks use a CHECK macro that always evaluates its expression.

#include <cstdlib>
#include <iostream>
#include <string>

#include <wolfram/repo/car.h>
#include <wolfram/repo/cbor.h>
#include <wolfram/repo/cid.h>

#include "sync/firehose_watch.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at firehose_decode_test.cpp:" << __LINE__ << "\n";  \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

const wf_record_schema kStringSchema{WF_RECORD_STRING, nullptr, 0, nullptr};

const wf_record_property kEventProps[] = {
    {"kind", &kStringSchema, 1},
    {"locationId", &kStringSchema, 1},
    {"detail", &kStringSchema, 0},
    {"createdAt", &kStringSchema, 1},
};

const wf_record_schema kEventSchema{WF_RECORD_OBJECT, kEventProps, 4, nullptr};

} // namespace

int main() {
    using keepsake::sync::DecodedEvent;
    using keepsake::sync::decodeEventRecord;
    using keepsake::sync::formatRemoteEvent;
    using keepsake::sync::RemoteEvent;

    // 1) Encode a real click.croft.rpg.event record to DAG-CBOR the same
    //    way a genuine WolframRecordStore::recordEvent() call would (via
    //    wolfram's own encoder, not keepsake's save::Json).
    const char *json =
        "{\"kind\":\"enemyDefeated\",\"locationId\":\"undercroft\","
        "\"detail\":\"The Hollow Knight has fallen.\","
        "\"createdAt\":\"2026-01-01T00:00:00Z\"}";

    unsigned char *cbor = nullptr;
    size_t cborLen = 0;
    wf_status encodeStatus = wf_record_encode_json(
        "click.croft.rpg.event", &kEventSchema, json, &cbor, &cborLen);
    CHECK(encodeStatus == WF_OK);
    CHECK(cbor != nullptr);
    CHECK(cborLen > 0);

    // 2) Compute its real CID and wrap it in a one-block CAR, exactly the
    //    shape a commit's `blocks` carries on the real firehose.
    wf_cid cid{};
    CHECK(wf_cid_of_block(cbor, cborLen, &cid) == WF_OK);

    wf_car_block block{};
    block.cid = cid;
    block.data = cbor;
    block.data_len = cborLen;

    wf_car car{};
    car.roots = &cid;
    car.root_count = 1;
    car.blocks = &block;
    car.block_count = 1;

    unsigned char *carBytes = nullptr;
    size_t carBytesLen = 0;
    CHECK(wf_car_write(&car, &carBytes, &carBytesLen) == WF_OK);
    CHECK(carBytes != nullptr);

    // 3) Parse the CAR bytes back and find the block by CID — the exact
    //    two calls onEvent() makes on a real commit's `blocks`/op.cid.
    wf_car parsedCar{};
    CHECK(wf_car_parse(carBytes, carBytesLen, &parsedCar) == WF_OK);

    wf_car_block *found = wf_car_find_block(&parsedCar, &cid);
    CHECK(found != nullptr);

    // 4) Decode with the identical function the live path calls.
    if (found != nullptr) {
        DecodedEvent decoded;
        bool ok = decodeEventRecord(found->data, found->data_len, decoded);
        CHECK(ok);
        CHECK(decoded.kind == "enemyDefeated");
        CHECK(decoded.locationId == "undercroft");
        CHECK(decoded.detail == "The Hollow Knight has fallen.");
    }

    // 5) A malformed block (not CBOR at all) must decode as false, not
    //    crash — this is untrusted data from other clients.
    {
        const unsigned char garbage[] = {0xff, 0x00, 0xff, 0x00};
        DecodedEvent decoded;
        CHECK(decodeEventRecord(garbage, sizeof(garbage), decoded) == false);
    }

    // 6) formatRemoteEvent — the line shape EventBridge's drained events
    //    print as, and `keepsake events` has always printed.
    {
        RemoteEvent remote{
            "did:plc:example",
            {"enemyDefeated", "undercroft", "The Hollow Knight has fallen."}};
        CHECK(formatRemoteEvent(remote) ==
              "[did:plc:example] enemyDefeated at undercroft: The Hollow "
              "Knight has fallen.");

        // Optional detail omitted entirely, not printed as ": ".
        RemoteEvent noDetail{"did:plc:example",
                             {"enemyDefeated", "undercroft", ""}};
        CHECK(formatRemoteEvent(noDetail) ==
              "[did:plc:example] enemyDefeated at undercroft");

        // Missing kind/locationId fall back rather than printing blank.
        RemoteEvent blank{"did:plc:example", {"", "", ""}};
        CHECK(formatRemoteEvent(blank) ==
              "[did:plc:example] (unknown event) at ?");
    }

    wf_car_free(&parsedCar);
    std::free(carBytes);
    std::free(cbor);

    std::cout << "firehose_decode_test " << (g_failures ? "FAILED" : "OK")
              << "\n";
    return g_failures ? 1 : 0;
}
