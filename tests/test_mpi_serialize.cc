#include <iostream>
#include <cassert>
#include <memory>
#include "drc/engine.h"
#include "mpi/mpi_serialize.h"
#include "db/dbRegion.h"
#include "db/dbEdges.h"
#include "db/dbEdgePairs.h"
#include "db/dbTexts.h"
#include "db/dbEdge.h"
#include "db/dbEdgePair.h"
#include "db/dbTrans.h"
#include "db/dbVector.h"

static void test_region_roundtrip() {
    auto reg = std::make_unique<db::Region>();
    reg->insert(db::Box(0, 0, 1000, 2000));
    reg->insert(db::Box(5000, 5000, 6000, 7000));
    drc::DRCLayer original(reg.release());
    assert(original.region()->count() == 2);

    auto bytes = drc::serialize_drclayer(original);
    assert(!bytes.empty());

    auto restored = drc::deserialize_drclayer(bytes.data(), bytes.size());
    assert(restored.type() == drc::DRCLayer::Region);
    assert(restored.region() != nullptr);
    assert(restored.region()->count() == 2);

    auto it = restored.region()->begin();
    db::Box b0 = (*it).box();
    assert(b0.left() == 0);
    assert(b0.bottom() == 0);
    assert(b0.right() == 1000);
    assert(b0.top() == 2000);
    ++it;
    db::Box b1 = (*it).box();
    assert(b1.left() == 5000);
    assert(b1.bottom() == 5000);
    assert(b1.right() == 6000);
    assert(b1.top() == 7000);

    std::cout << "PASS: test_region_roundtrip" << std::endl;
}

static void test_empty_region() {
    auto reg = std::make_unique<db::Region>();
    drc::DRCLayer original(reg.release());
    assert(original.region()->count() == 0);

    auto bytes = drc::serialize_drclayer(original);
    auto restored = drc::deserialize_drclayer(bytes.data(), bytes.size());
    assert(restored.type() == drc::DRCLayer::Region);
    assert(restored.region() != nullptr);
    assert(restored.region()->count() == 0);

    std::cout << "PASS: test_empty_region" << std::endl;
}

static void test_edges_roundtrip() {
    auto edges = std::make_unique<db::Edges>();
    edges->insert(db::Edge(0, 0, 1000, 0));
    drc::DRCLayer original(edges.release());
    assert(original.edges() != nullptr);

    auto bytes = drc::serialize_drclayer(original);
    assert(bytes.size() > 5);
    printf("  edges serialized %zu bytes, first=0x%02x\n", bytes.size(), (unsigned char)bytes[0]);
    auto restored = drc::deserialize_drclayer(bytes.data(), bytes.size());
    assert(restored.type() == drc::DRCLayer::Edges);
    assert(restored.edges() != nullptr);
    assert(restored.edges()->count() == 1);
    if (restored.edges()->count() != 1) {
        printf("  FAIL: edges count = %zu (expected 1)\n", restored.edges()->count());
    }

    std::cout << "PASS: test_edges_roundtrip" << std::endl;
}

static void test_edgepairs_roundtrip() {
    auto eps = std::make_unique<db::EdgePairs>();
    eps->insert(db::EdgePair(db::Edge(0, 0, 1000, 0), db::Edge(0, 100, 1000, 100)));
    drc::DRCLayer original(eps.release());

    auto bytes = drc::serialize_drclayer(original);
    assert(bytes.size() > 5);
    printf("  edgepairs serialized %zu bytes, first=0x%02x\n", bytes.size(), (unsigned char)bytes[0]);
    auto restored = drc::deserialize_drclayer(bytes.data(), bytes.size());
    assert(restored.type() == drc::DRCLayer::EdgePairs);
    assert(restored.edge_pairs() != nullptr);
    assert(restored.edge_pairs()->count() == 1);
    if (restored.edge_pairs()->count() != 1) {
        printf("  FAIL: edgepairs count = %zu (expected 1)\n", restored.edge_pairs()->count());
    }

    std::cout << "PASS: test_edgepairs_roundtrip" << std::endl;
}

static void test_texts_roundtrip() {
    auto texts = std::make_unique<db::Texts>();
    texts->insert(db::Text("hello", db::simple_trans<db::Coord>(), 10));
    drc::DRCLayer original(texts.release());
    assert(original.texts() != nullptr);
    assert(original.texts()->count() == 1);

    auto bytes = drc::serialize_drclayer(original);
    assert(!bytes.empty());

    auto restored = drc::deserialize_drclayer(bytes.data(), bytes.size());
    assert(restored.type() == drc::DRCLayer::Texts);
    assert(restored.texts() != nullptr);
    assert(restored.texts()->count() == 1);

    std::cout << "PASS: test_texts_roundtrip" << std::endl;
}

static void test_invalid_data() {
    // too short
    char bad1[] = {0x01, 0x00, 0x00, 0x00};
    auto r1 = drc::deserialize_drclayer(bad1, 4);
    assert(r1.is_null());

    // negative gds_size
    char bad2[] = {0x00, char(0xff), char(0xff), char(0xff), char(0xff), 0x00};
    auto r2 = drc::deserialize_drclayer(bad2, 6);
    assert(r2.is_null());

    std::cout << "PASS: test_invalid_data" << std::endl;
}

int main() {
    test_region_roundtrip();
    test_empty_region();
    test_edges_roundtrip();
    test_edgepairs_roundtrip();
    test_texts_roundtrip();
    test_invalid_data();
    std::cout << "All MPI serialize tests passed!" << std::endl;
    return 0;
}
