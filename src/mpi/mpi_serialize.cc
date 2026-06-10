#include "mpi/mpi_serialize.h"
#include "db/dbLayout.h"
#include "db/dbCell.h"
#include "db/dbRegion.h"
#include "db/dbEdges.h"
#include "db/dbEdgePairs.h"
#include "db/dbTexts.h"
#include "db/dbRecursiveShapeIterator.h"
#include "db/dbEdge.h"
#include "db/dbEdgePair.h"
#include "db/dbPoint.h"
#include "gds/dbGDS2Writer.h"
#include "gds/dbGDS2Reader.h"
#include "db/dbSaveLayoutOptions.h"
#include "db/dbWriter.h"
#include "db/dbReader.h"
#include "db/dbLoadLayoutOptions.h"
#include "tl/tlStream.h"
#include <cstring>

namespace drc {

// Custom binary serialization for Edges/EdgePairs (GDS2 doesn't preserve them natively).
// Region and Texts use GDS2 format (verified correct roundtrip).

static std::vector<char> serialize_gds2(const DRCLayer& layer) {
    db::Layout layout;
    layout.dbu(0.001);
    layout.add_cell("TEMP");
    layer.output(layout, 999, 0);

    tl::OutputMemoryStream mem;
    {
        tl::OutputStream stream(mem, false);
        db::SaveLayoutOptions options;
        options.set_format("GDS2");
        db::Writer writer(options);
        writer.write(layout, stream);
    }
    std::vector<char> result;
    int32_t gds_size = static_cast<int32_t>(mem.size());
    result.resize(4);
    std::memcpy(result.data(), &gds_size, 4);
    if (mem.size() > 0) {
        result.insert(result.end(), mem.data(), mem.data() + mem.size());
    }
    return result;
}

static std::vector<char> serialize_edges(const db::Edges* edges) {
    std::vector<char> buf;
    int32_t count = static_cast<int32_t>(edges->count());
    buf.resize(4);
    std::memcpy(buf.data(), &count, 4);
    for (auto it = edges->begin(); !it.at_end(); ++it) {
        db::Edge e = *it;
        int32_t coords[4] = { e.p1().x(), e.p1().y(), e.p2().x(), e.p2().y() };
        buf.insert(buf.end(), reinterpret_cast<char*>(coords),
                   reinterpret_cast<char*>(coords) + 16);
    }
    return buf;
}

static std::vector<char> serialize_edgepairs(const db::EdgePairs* eps) {
    std::vector<char> buf;
    int32_t count = static_cast<int32_t>(eps->count());
    buf.resize(4);
    std::memcpy(buf.data(), &count, 4);
    for (auto it = eps->begin(); !it.at_end(); ++it) {
        db::EdgePair ep = *it;
        int32_t coords[8] = {
            ep.first().p1().x(), ep.first().p1().y(),
            ep.first().p2().x(), ep.first().p2().y(),
            ep.second().p1().x(), ep.second().p1().y(),
            ep.second().p2().x(), ep.second().p2().y()
        };
        buf.insert(buf.end(), reinterpret_cast<char*>(coords),
                   reinterpret_cast<char*>(coords) + 32);
    }
    return buf;
}

std::vector<char> serialize_drclayer(const DRCLayer& layer) {
    std::vector<char> result;
    uint8_t type_tag = static_cast<uint8_t>(layer.type());
    result.push_back(static_cast<char>(type_tag));

    switch (layer.type()) {
        case DRCLayer::Region: {
            if (layer.region()) {
                auto gds = serialize_gds2(layer);
                result.insert(result.end(), gds.begin(), gds.end());
            } else {
                int32_t zero = 0;
                result.resize(result.size() + 4);
                std::memcpy(result.data() + 1, &zero, 4);
            }
            break;
        }
        case DRCLayer::Edges: {
            if (layer.edges()) {
                auto custom = serialize_edges(layer.edges());
                result.insert(result.end(), custom.begin(), custom.end());
            } else {
                int32_t zero = 0;
                result.resize(result.size() + 4);
                std::memcpy(result.data() + 1, &zero, 4);
            }
            break;
        }
        case DRCLayer::EdgePairs: {
            if (layer.edge_pairs()) {
                auto custom = serialize_edgepairs(layer.edge_pairs());
                result.insert(result.end(), custom.begin(), custom.end());
            } else {
                int32_t zero = 0;
                result.resize(result.size() + 4);
                std::memcpy(result.data() + 1, &zero, 4);
            }
            break;
        }
        case DRCLayer::Texts: {
            if (layer.texts()) {
                auto gds = serialize_gds2(layer);
                result.insert(result.end(), gds.begin(), gds.end());
            } else {
                int32_t zero = 0;
                result.resize(result.size() + 4);
                std::memcpy(result.data() + 1, &zero, 4);
            }
            break;
        }
    }
    return result;
}

static DRCLayer deserialize_gds2_region(const char* data, size_t gds_size) {
    tl::InputMemoryStream imem(data, gds_size, false);
    tl::InputStream istream(imem);
    db::Layout layout;
    db::Reader reader(istream);
    reader.read(layout);
    if (layout.begin_top_down() == layout.end_top_down()) return DRCLayer();
    db::cell_index_type top_cell = *layout.begin_top_down();
    unsigned int ll = static_cast<unsigned int>(layout.get_layer(db::LayerProperties(999, 0)));
    const auto& shapes = layout.cell(top_cell).shapes(ll);
    auto reg = std::make_unique<db::Region>();
    reg->reserve(shapes.size());
    for (auto s = shapes.begin(db::ShapeIterator::Regions); !s.at_end(); ++s) {
        reg->insert(*s);
    }
    return DRCLayer(reg.release());
}

static DRCLayer deserialize_gds2_texts(const char* data, size_t gds_size) {
    tl::InputMemoryStream imem(data, gds_size, false);
    tl::InputStream istream(imem);
    db::Layout layout;
    db::Reader reader(istream);
    reader.read(layout);
    if (layout.begin_top_down() == layout.end_top_down()) return DRCLayer();
    db::cell_index_type top_cell = *layout.begin_top_down();
    unsigned int ll = static_cast<unsigned int>(layout.get_layer(db::LayerProperties(999, 0)));
    const auto& shapes = layout.cell(top_cell).shapes(ll);
    auto texts = std::make_unique<db::Texts>();
    for (auto s = shapes.begin(db::ShapeIterator::Texts); !s.at_end(); ++s) {
        texts->insert(*s);
    }
    return DRCLayer(texts.release());
}

static DRCLayer deserialize_edges_custom(const char* data, size_t size) {
    if (size < 4) return DRCLayer();
    int32_t count;
    std::memcpy(&count, data, 4);
    if (count < 0) return DRCLayer();
    if (static_cast<size_t>(count) > size / 16) count = static_cast<int32_t>(size / 16);
    auto edges = std::make_unique<db::Edges>();
    for (int32_t i = 0; i < count; i++) {
        const int32_t* c = reinterpret_cast<const int32_t*>(data + 4 + i * 16);
        edges->insert(db::Edge(c[0], c[1], c[2], c[3]));
    }
    return DRCLayer(edges.release());
}

static DRCLayer deserialize_edgepairs_custom(const char* data, size_t size) {
    if (size < 4) return DRCLayer();
    int32_t count;
    std::memcpy(&count, data, 4);
    if (count < 0) return DRCLayer();
    if (static_cast<size_t>(count) > size / 32) count = static_cast<int32_t>(size / 32);
    auto eps = std::make_unique<db::EdgePairs>();
    for (int32_t i = 0; i < count; i++) {
        const int32_t* c = reinterpret_cast<const int32_t*>(data + 4 + i * 32);
        db::Edge a(c[0], c[1], c[2], c[3]);
        db::Edge b(c[4], c[5], c[6], c[7]);
        eps->insert(db::EdgePair(a, b));
    }
    return DRCLayer(eps.release());
}

DRCLayer deserialize_drclayer(const char* data, size_t size) {
    if (size < 5) return DRCLayer();

    uint8_t type_tag = static_cast<uint8_t>(data[0]);

    switch (static_cast<DRCLayer::Type>(type_tag)) {
        case DRCLayer::Region: {
            int32_t gds_size;
            std::memcpy(&gds_size, data + 1, 4);
            if (gds_size < 0 || static_cast<size_t>(gds_size) > size - 5) return DRCLayer();
            if (gds_size == 0) return DRCLayer();
            return deserialize_gds2_region(data + 5, static_cast<size_t>(gds_size));
        }
        case DRCLayer::Edges: {
            int32_t custom_size = static_cast<int32_t>(size - 5);
            if (custom_size < 4) return DRCLayer();
            int32_t count;
            std::memcpy(&count, data + 1, 4);
            if (count == 0) return DRCLayer(new db::Edges());
            return deserialize_edges_custom(data + 1, static_cast<size_t>(size - 1));
        }
        case DRCLayer::EdgePairs: {
            int32_t custom_size = static_cast<int32_t>(size - 5);
            if (custom_size < 4) return DRCLayer();
            int32_t count;
            std::memcpy(&count, data + 1, 4);
            if (count == 0) return DRCLayer(new db::EdgePairs());
            return deserialize_edgepairs_custom(data + 1, static_cast<size_t>(size - 1));
        }
        case DRCLayer::Texts: {
            int32_t gds_size;
            std::memcpy(&gds_size, data + 1, 4);
            if (gds_size < 0 || static_cast<size_t>(gds_size) > size - 5) return DRCLayer();
            if (gds_size == 0) return DRCLayer();
            return deserialize_gds2_texts(data + 5, static_cast<size_t>(gds_size));
        }
        default:
            return DRCLayer();
    }
}

} // namespace drc
