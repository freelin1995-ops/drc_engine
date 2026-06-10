#include "mpi/mpi_serialize.h"
#include "db/dbLayout.h"
#include "db/dbCell.h"
#include "db/dbRegion.h"
#include "db/dbEdges.h"
#include "db/dbEdgePairs.h"
#include "db/dbTexts.h"
#include "db/dbRecursiveShapeIterator.h"
#include "gds/dbGDS2Writer.h"
#include "gds/dbGDS2Reader.h"
#include "db/dbSaveLayoutOptions.h"
#include "db/dbWriter.h"
#include "db/dbReader.h"
#include "db/dbLoadLayoutOptions.h"
#include "tl/tlStream.h"

namespace drc {

std::vector<char> serialize_drclayer(const DRCLayer& layer) {
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
    uint8_t type_tag = static_cast<uint8_t>(layer.type());
    result.push_back(static_cast<char>(type_tag));

    int32_t gds_size = static_cast<int32_t>(mem.size());
    result.resize(result.size() + 4);
    std::memcpy(result.data() + 1, &gds_size, 4);

    if (mem.size() > 0) {
        result.insert(result.end(), mem.data(), mem.data() + mem.size());
    }
    return result;
}

DRCLayer deserialize_drclayer(const char* data, size_t size) {
    if (size < 5) {
        return DRCLayer();
    }

    uint8_t type_tag = static_cast<uint8_t>(data[0]);
    int32_t gds_size;
    std::memcpy(&gds_size, data + 1, 4);

    if (gds_size < 0 || static_cast<size_t>(gds_size) > size - 5) {
        return DRCLayer();
    }

    tl::InputMemoryStream imem(data + 5, static_cast<size_t>(gds_size), false);
    tl::InputStream istream(imem);

    db::Layout layout;
    db::Reader reader(istream);
    reader.read(layout);

    if (layout.begin_top_down() == layout.end_top_down()) {
        return DRCLayer();
    }
    db::cell_index_type top_cell = *layout.begin_top_down();

    unsigned int ll = static_cast<unsigned int>(layout.get_layer(db::LayerProperties(999, 0)));

    const auto& shapes = layout.cell(top_cell).shapes(ll);

    switch (static_cast<DRCLayer::Type>(type_tag)) {
        case DRCLayer::Region: {
            auto reg = std::make_unique<db::Region>();
            reg->reserve(shapes.size());
            for (auto s = shapes.begin(db::ShapeIterator::Regions); !s.at_end(); ++s) {
                reg->insert(*s);
            }
            return DRCLayer(reg.release());
        }
        case DRCLayer::Edges: {
            auto edges = std::make_unique<db::Edges>();
            for (auto s = shapes.begin(db::ShapeIterator::Edges); !s.at_end(); ++s) {
                edges->insert(*s);
            }
            return DRCLayer(edges.release());
        }
        case DRCLayer::EdgePairs: {
            auto eps = std::make_unique<db::EdgePairs>();
            for (auto s = shapes.begin(db::ShapeIterator::EdgePairs); !s.at_end(); ++s) {
                eps->insert(*s);
            }
            return DRCLayer(eps.release());
        }
        case DRCLayer::Texts: {
            auto texts = std::make_unique<db::Texts>();
            for (auto s = shapes.begin(db::ShapeIterator::Texts); !s.at_end(); ++s) {
                texts->insert(*s);
            }
            return DRCLayer(texts.release());
        }
        default:
            return DRCLayer();
    }
}

} // namespace drc
