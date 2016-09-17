#include <boost/assert.hpp>
#include "utils.hpp"
#include "xlsx.hpp"

int main(int argc, char** argv) {
    using namespace xlsxconverter;
    using namespace xlsxconverter::utils;

    auto book = xlsx::Workbook("test/sample.xlsx");
    auto& sheet = book.sheet_by_name("test");

    utils::log("ncols: ", sheet.ncols());
    utils::log("nrows: ", sheet.nrows());
    for (int j = 0; j < sheet.nrows(); ++j) {
        for (int i = 0; i < sheet.ncols(); ++i) {
            auto& cell = sheet.cell(j, i);
            if (cell.type == xlsx::Cell::Type::kEmpty) continue;
            utils::log("cell[", cell.cellname(), "]=", cell.as_str());
        }
    }

    BOOST_ASSERT(sheet.cell("A1").as_str() == "1001");
    BOOST_ASSERT(sheet.cell("ZZ1").as_str() == "ZZ1");
    BOOST_ASSERT(sheet.cell("AAA1").as_str() == "AAA1");

    return 0;
}
