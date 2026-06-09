#include "drc/engine.h"
#include "tl/tlStream.h"
#include "db/dbReader.h"
#include "db/dbWriter.h"
#include "db/dbSaveLayoutOptions.h"
#include "db/dbEdgeProcessor.h"
#include "db/dbRegionUtils.h"
#include "db/dbRegionProcessors.h"
#include "rdb/rdb.h"

namespace drc {

// ---- DRCLayer ----

DRCLayer::DRCLayer() : m_type(Region) {}
DRCLayer::DRCLayer(db::Region* r) : m_type(Region), m_region(r) {}
DRCLayer::DRCLayer(db::Edges* e) : m_type(Edges), m_edges(e) {}
DRCLayer::DRCLayer(db::EdgePairs* ep) : m_type(EdgePairs), m_edge_pairs(ep) {}
DRCLayer::DRCLayer(db::Texts* t) : m_type(Texts), m_texts(t) {}

DRCLayer::DRCLayer(const DRCLayer& other) : m_type(other.m_type) {
    switch (m_type) {
        case Region: if (other.m_region) m_region = std::make_unique<db::Region>(*other.m_region); break;
        case Edges: if (other.m_edges) m_edges = std::make_unique<db::Edges>(*other.m_edges); break;
        case EdgePairs: if (other.m_edge_pairs) m_edge_pairs = std::make_unique<db::EdgePairs>(*other.m_edge_pairs); break;
        case Texts: if (other.m_texts) m_texts = std::make_unique<db::Texts>(*other.m_texts); break;
    }
}

DRCLayer::DRCLayer(DRCLayer&& other) noexcept : m_type(other.m_type) {
    m_region = std::move(other.m_region);
    m_edges = std::move(other.m_edges);
    m_edge_pairs = std::move(other.m_edge_pairs);
    m_texts = std::move(other.m_texts);
    other.m_type = Region;
}

DRCLayer& DRCLayer::operator=(DRCLayer other) {
    swap(other);
    return *this;
}

DRCLayer::~DRCLayer() = default;

void DRCLayer::swap(DRCLayer& other) noexcept {
    std::swap(m_type, other.m_type);
    std::swap(m_region, other.m_region);
    std::swap(m_edges, other.m_edges);
    std::swap(m_edge_pairs, other.m_edge_pairs);
    std::swap(m_texts, other.m_texts);
}

// Boolean ops (type-aware: Region, Edges, or EdgePairs)
DRCLayer DRCLayer::operator&(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(*m_region & *other.m_region));
    if (m_type == Edges && other.m_type == Edges && m_edges && other.m_edges)
        return DRCLayer(new db::Edges(*m_edges & *other.m_edges));
    return DRCLayer();
}

DRCLayer DRCLayer::operator|(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(*m_region | *other.m_region));
    if (m_type == Edges && other.m_type == Edges && m_edges && other.m_edges)
        return DRCLayer(new db::Edges(*m_edges | *other.m_edges));
    return DRCLayer();
}

DRCLayer DRCLayer::operator-(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(*m_region - *other.m_region));
    if (m_type == Edges && other.m_type == Edges && m_edges && other.m_edges)
        return DRCLayer(new db::Edges(*m_edges - *other.m_edges));
    return DRCLayer();
}

DRCLayer DRCLayer::operator^(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(*m_region ^ *other.m_region));
    if (m_type == Edges && other.m_type == Edges && m_edges && other.m_edges)
        return DRCLayer(new db::Edges(*m_edges ^ *other.m_edges));
    return DRCLayer();
}

// DRC checks
DRCLayer DRCLayer::width_check(double d) const {
    return DRCLayer(new db::EdgePairs(m_region->width_check(db::Coord(d))));
}

DRCLayer DRCLayer::space_check(double d) const {
    return DRCLayer(new db::EdgePairs(m_region->space_check(db::Coord(d))));
}

DRCLayer DRCLayer::notch_check(double d) const {
    return DRCLayer(new db::EdgePairs(m_region->notch_check(db::Coord(d))));
}

DRCLayer DRCLayer::enclosure_check(const DRCLayer& inner, double d) const {
    return DRCLayer(new db::EdgePairs(m_region->enclosing_check(*inner.m_region, db::Coord(d))));
}

DRCLayer DRCLayer::separation_check(const DRCLayer& other, double d) const {
    return DRCLayer(new db::EdgePairs(m_region->separation_check(*other.m_region, db::Coord(d))));
}

DRCLayer DRCLayer::overlap_check(const DRCLayer& other, double d) const {
    return DRCLayer(new db::EdgePairs(m_region->overlap_check(*other.m_region, db::Coord(d))));
}

// Geometry transforms
DRCLayer DRCLayer::sized(double d) const {
    return DRCLayer(new db::Region(m_region->sized(db::Coord(d))));
}

DRCLayer DRCLayer::sized(double dx, double dy) const {
    return DRCLayer(new db::Region(m_region->sized(db::Coord(dx), db::Coord(dy))));
}

DRCLayer DRCLayer::merge() const {
    return DRCLayer(new db::Region(m_region->merged()));
}

DRCLayer DRCLayer::edges_op() const {
    return DRCLayer(new db::Edges(m_region->edges()));
}

DRCLayer DRCLayer::corners_dots(double angle_min, double angle_max) const {
    return DRCLayer(new db::Edges(m_region->processed(
        db::CornersAsDots(angle_min, true, angle_max, true, false, false))));
}

DRCLayer DRCLayer::corners_boxes(double dim, double angle_min, double angle_max) const {
    return DRCLayer(new db::Region(m_region->processed(
        db::CornersAsRectangles(angle_min, true, angle_max, true, false, false, db::Coord(dim)))));
}

// Selection (type-aware: Region or Edges)
DRCLayer DRCLayer::interacting(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(m_region->selected_interacting(*other.m_region)));
    if (m_type == Edges && other.m_type == Region && m_edges && other.m_region)
        return DRCLayer(new db::Edges(m_edges->selected_interacting(*other.m_region)));
    if (m_type == Edges && other.m_type == Edges && m_edges && other.m_edges)
        return DRCLayer(new db::Edges(m_edges->selected_interacting(*other.m_edges)));
    return DRCLayer();
}

DRCLayer DRCLayer::inside(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(m_region->selected_inside(*other.m_region)));
    if (m_type == Edges && other.m_type == Region && m_edges && other.m_region)
        return DRCLayer(new db::Edges(m_edges->selected_inside(*other.m_region)));
    return DRCLayer();
}

DRCLayer DRCLayer::outside(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(m_region->selected_outside(*other.m_region)));
    if (m_type == Edges && other.m_type == Region && m_edges && other.m_region)
        return DRCLayer(new db::Edges(m_edges->selected_outside(*other.m_region)));
    return DRCLayer();
}

DRCLayer DRCLayer::enclosing(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region && m_region && other.m_region)
        return DRCLayer(new db::Region(m_region->selected_enclosing(*other.m_region)));
    // Edges don't have enclosing - not meaningful
    return DRCLayer();
}

// Edge operations (only on Edges type)
DRCLayer DRCLayer::extended_out(double d) const {
    db::Region out;
    m_edges->extended(out, 0, 0, db::Coord(d), 0, false);
    return DRCLayer(new db::Region(out));
}

DRCLayer DRCLayer::extended_in(double d) const {
    db::Region out;
    m_edges->extended(out, 0, 0, 0, db::Coord(d), false);
    return DRCLayer(new db::Region(out));
}

DRCLayer DRCLayer::extended(double b, double e, double o, double i, bool join) const {
    db::Region out;
    m_edges->extended(out, db::Coord(b), db::Coord(e), db::Coord(o), db::Coord(i), join);
    return DRCLayer(new db::Region(out));
}

double DRCLayer::length() const {
    return m_edges->length();
}

DRCLayer DRCLayer::centers(double l, double f) const {
    return DRCLayer(new db::Edges(m_edges->centers(db::Edges::length_type(l), f)));
}

DRCLayer DRCLayer::start_segments(double l, double f) const {
    return DRCLayer(new db::Edges(m_edges->start_segments(db::Edges::length_type(l), f)));
}

DRCLayer DRCLayer::end_segments(double l, double f) const {
    return DRCLayer(new db::Edges(m_edges->end_segments(db::Edges::length_type(l), f)));
}

// Filters
DRCLayer DRCLayer::with_area(double min_a, double max_a) const {
    return DRCLayer(new db::Region(m_region->filtered(
        db::RegionAreaFilter(db::Region::area_type(min_a), db::Region::area_type(max_a), false))));
}

DRCLayer DRCLayer::with_perimeter(double min_p, double max_p) const {
    return DRCLayer(new db::Region(m_region->filtered(
        db::RegionPerimeterFilter(db::Region::perimeter_type(min_p), db::Region::perimeter_type(max_p), false))));
}

// Output
void DRCLayer::output(db::Layout& target, int layer, int dtype) const {
    unsigned int ll = target.get_layer(db::LayerProperties(layer, dtype));
    db::cell_index_type ci = *target.begin_top_down();

    if (m_type == Region && m_region) {
        m_region->insert_into(&target, ci, ll);
    } else if (m_type == EdgePairs && m_edge_pairs) {
        m_edge_pairs->insert_into(&target, ci, ll);
    } else if (m_type == Edges && m_edges) {
        m_edges->insert_into(&target, ci, ll);
    } else if (m_type == Texts && m_texts) {
        m_texts->insert_into(&target, ci, ll);
    }
}

void DRCLayer::output_rdb(rdb::Database& rdb, const std::string& category_name) const {
    rdb::Category* cat = rdb.category_by_name_non_const(category_name);
    if (!cat) {
        cat = rdb.create_category(category_name);
    }
    if (m_type == EdgePairs && m_edge_pairs) {
        for (auto it = m_edge_pairs->begin(); !it.at_end(); ++it) {
            rdb::Item* item = rdb.create_item(0, cat->id());
            if (item) {
                item->add_value(it->first());
                item->add_value(it->second());
            }
        }
    }
}

// ---- DRCEngine ----

DRCEngine::DRCEngine()
    : m_layout(std::make_unique<db::Layout>())
    , m_target(std::make_unique<db::Layout>())
{
    m_target->add_cell("TOP");
}

DRCEngine::~DRCEngine() = default;

bool DRCEngine::load_layout(const std::string& path) {
    m_layout = std::make_unique<db::Layout>();
    m_target = std::make_unique<db::Layout>();
    m_target->add_cell("TOP");
    tl::InputStream stream(path);
    db::Reader reader(stream);
    reader.read(*m_layout);
    m_dbu = m_layout->dbu();
    return true;
}

void DRCEngine::set_target_path(const std::string& path) {
    m_target_path = path;
}

void DRCEngine::write_output(const std::string& path) {
    const std::string& p = path.empty() ? m_target_path : path;
    if (p.empty()) return;
    db::SaveLayoutOptions options;
    options.set_format_from_filename(p);
    db::Writer writer(options);
    tl::OutputStream stream(p);
    writer.write(*m_target, stream);
}

DRCLayer DRCEngine::input(int layer, int dtype) const {
    int ll = m_layout->get_layer_maybe(db::LayerProperties(layer, dtype));
    if (ll < 0) {
        return DRCLayer(new db::Region());
    }
    db::cell_index_type top_cell = *m_layout->begin_top_down();
    db::RecursiveShapeIterator si(*m_layout, m_layout->cell(top_cell), (unsigned int)ll);
    return DRCLayer(new db::Region(si));
}

DRCLayer DRCEngine::input(const std::string& name) const {
    int ll = -1;
    for (auto l = m_layout->begin_layers(); l != m_layout->end_layers(); ++l) {
        auto lp = *l;
        if (lp.second && lp.second->name == name) {
            ll = (int)lp.first;
            break;
        }
    }
    if (ll < 0) {
        return DRCLayer(new db::Region());
    }
    db::cell_index_type top_cell = *m_layout->begin_top_down();
    db::RecursiveShapeIterator si(*m_layout, m_layout->cell(top_cell), (unsigned int)ll);
    return DRCLayer(new db::Region(si));
}

} // namespace drc
