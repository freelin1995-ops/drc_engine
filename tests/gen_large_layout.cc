#include <iostream>
#include <cstdlib>
#include "db/dbLayout.h"
#include "db/dbCell.h"
#include "db/dbRegion.h"
#include "db/dbBox.h"
#include "db/dbTrans.h"
#include "db/dbWriter.h"
#include "db/dbSaveLayoutOptions.h"
#include "tl/tlStream.h"

int main(int argc, char* argv[]) {
    int num_cells = 100;
    int shapes_per_cell = 1000;
    if (argc > 1) num_cells = std::atoi(argv[1]);
    if (argc > 2) shapes_per_cell = std::atoi(argv[2]);

    db::Layout layout;
    layout.dbu(0.001);

    // Create layers
    unsigned int l1 = layout.insert_layer(db::LayerProperties(10, 0));
    unsigned int l2 = layout.insert_layer(db::LayerProperties(20, 0));
    unsigned int l3 = layout.insert_layer(db::LayerProperties(30, 0));

    // Create top cell
    db::cell_index_type top = layout.add_cell("TOP");

    // Create many sub-cells with polygons
    for (int c = 0; c < num_cells; c++) {
        std::string cell_name = "CELL_" + std::to_string(c);
        db::cell_index_type ci = layout.add_cell(cell_name.c_str());
        db::Cell& cell = layout.cell(ci);

        // Add rectangles to each layer in this cell
        for (int s = 0; s < shapes_per_cell; s++) {
            int x = (c * 10000 + s * 37) % 500000;
            int y = (c * 10000 + s * 53) % 500000;
            int w = 10 + (s % 200);
            int h = 10 + (s % 150);
            db::Box box(x, y, x + w, y + h);
            cell.shapes(l1).insert(box);
        }

        // Additional shapes on layer 20 for boolean ops
        for (int s = 0; s < shapes_per_cell / 2; s++) {
            int x = (c * 10000 + s * 71) % 500000;
            int y = (c * 10000 + s * 97) % 500000;
            int w = 5 + (s % 100);
            int h = 5 + (s % 100);
            db::Box box(x, y, x + w, y + h);
            cell.shapes(l2).insert(box);
        }

        // Layer 30 with some overlap for DRC checks
        for (int s = 0; s < shapes_per_cell / 3; s++) {
            int x = (c * 10000 + s * 41) % 500000;
            int y = (c * 10000 + s * 67) % 500000;
            int w = 100 + (s % 50);
            int h = 100 + (s % 50);
            db::Box box(x, y, x + w, y + h);
            cell.shapes(l3).insert(box);
        }

        // Place each cell in top with an array
        db::CellInstArray arr(ci, db::Trans(db::Vector(c * 10000, c * 10000)));
        layout.cell(top).insert(arr);
    }

    // Write output
    std::string path = argc > 3 ? argv[3] : "/tmp/test_large.gds";
    db::SaveLayoutOptions opts;
    opts.set_format_from_filename(path);
    db::Writer writer(opts);
    tl::OutputStream stream(path);
    writer.write(layout, stream);

    double total_shapes = (double)num_cells * (double)shapes_per_cell * 1.83;
    std::cout << "Generated " << path << ": "
              << num_cells << " cells x ~"
              << shapes_per_cell << " shapes = ~"
              << (int)total_shapes << " total shapes" << std::endl;
    return 0;
}
