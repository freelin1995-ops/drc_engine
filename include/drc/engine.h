#pragma once

#include "db/dbLayout.h"
#include "db/dbRegion.h"
#include "db/dbEdges.h"
#include "db/dbEdgePairs.h"
#include "db/dbTexts.h"
#include <string>
#include <memory>
#include <vector>
#include <limits>

namespace rdb {
class Database;
}

namespace drc {

class DRCLayer {
public:
    enum Type { Region, Edges, EdgePairs, Texts };

    DRCLayer();
    explicit DRCLayer(db::Region* r);
    explicit DRCLayer(db::Edges* e);
    explicit DRCLayer(db::EdgePairs* ep);
    explicit DRCLayer(db::Texts* t);
    DRCLayer(const DRCLayer& other);
    DRCLayer(DRCLayer&& other) noexcept;
    DRCLayer& operator=(DRCLayer other);
    ~DRCLayer();

    Type type() const { return m_type; }
    bool is_null() const { return m_type == Region && !m_region; }

    db::Region* region() { return m_region.get(); }
    db::Edges* edges() { return m_edges.get(); }
    db::EdgePairs* edge_pairs() { return m_edge_pairs.get(); }
    db::Texts* texts() { return m_texts.get(); }

    const db::Region* region() const { return m_region.get(); }
    const db::Edges* edges() const { return m_edges.get(); }
    const db::EdgePairs* edge_pairs() const { return m_edge_pairs.get(); }
    const db::Texts* texts() const { return m_texts.get(); }

    // Boolean operations (on Region)
    DRCLayer operator&(const DRCLayer& other) const;
    DRCLayer operator|(const DRCLayer& other) const;
    DRCLayer operator-(const DRCLayer& other) const;
    DRCLayer operator^(const DRCLayer& other) const;

    // DRC checks (Region -> EdgePairs)
    DRCLayer width_check(double d) const;
    DRCLayer space_check(double d) const;
    DRCLayer notch_check(double d) const;
    DRCLayer enclosure_check(const DRCLayer& inner, double d) const;
    DRCLayer separation_check(const DRCLayer& other, double d) const;
    DRCLayer overlap_check(const DRCLayer& other, double d) const;

    // Geometry transforms
    DRCLayer sized(double d) const;
    DRCLayer sized(double dx, double dy) const;
    DRCLayer merge() const;
    DRCLayer edges_op() const;  // Region to Edges
    DRCLayer corners_dots(double angle_min = -180.0, double angle_max = 180.0) const;  // Region -> Edges
    DRCLayer corners_boxes(double dim, double angle_min = -180.0, double angle_max = 180.0) const;  // Region -> Region

    // Selection filters
    DRCLayer interacting(const DRCLayer& other) const;
    DRCLayer inside(const DRCLayer& other) const;
    DRCLayer outside(const DRCLayer& other) const;
    DRCLayer enclosing(const DRCLayer& other) const;

    // Edge operations (only when type is Edges)
    DRCLayer extended_out(double d) const;  // Edges -> Region
    DRCLayer extended_in(double d) const;   // Edges -> Region
    double length() const;                  // Edges -> double (total length)
    DRCLayer centers(double l, double f) const;       // Edges -> Edges
    DRCLayer start_segments(double l, double f) const; // Edges -> Edges
    DRCLayer end_segments(double l, double f) const;   // Edges -> Edges
    DRCLayer extended(double b, double e, double o, double i, bool join) const; // Edges -> Region (generic)

    // Area/length filters
    DRCLayer with_area(double min_area, double max_area = std::numeric_limits<double>::max()) const;
    DRCLayer with_perimeter(double min_perim, double max_perim = std::numeric_limits<double>::max()) const;

    // Output
    void output(db::Layout& target, int layer, int dtype) const;
    void output_rdb(rdb::Database& rdb, const std::string& category) const;

private:
    Type m_type;
    std::unique_ptr<db::Region> m_region;
    std::unique_ptr<db::Edges> m_edges;
    std::unique_ptr<db::EdgePairs> m_edge_pairs;
    std::unique_ptr<db::Texts> m_texts;

    void swap(DRCLayer& other) noexcept;
};

class DRCEngine {
public:
    DRCEngine();
    ~DRCEngine();

    bool load_layout(const std::string& path);
    void write_output(const std::string& path);
    void set_target_path(const std::string& path);

    DRCLayer input(int layer, int dtype) const;
    DRCLayer input(const std::string& name) const;

    double dbu() const { return m_dbu; }
    db::Coord to_db(double um) const { return db::Coord(um / m_dbu); }

    db::Layout* layout() { return m_layout.get(); }
    const db::Layout* layout() const { return m_layout.get(); }
    db::Layout* target() { return m_target.get(); }
    const db::Layout* target() const { return m_target.get(); }

private:
    std::unique_ptr<db::Layout> m_layout;
    std::unique_ptr<db::Layout> m_target;
    std::string m_target_path;
    double m_dbu = 0.001;
};

} // namespace drc
