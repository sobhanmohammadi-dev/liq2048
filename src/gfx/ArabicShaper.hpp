#pragma once
// Minimal Arabic-script text preparation for Persian and Arabic, independent of HarfBuzz:
//   1. contextual shaping (isolated / initial / medial / final forms, lam-alef ligatures),
//   2. a simple bidirectional pass (Latin letters and digits stay left-to-right inside RTL text),
//   3. the result is in *visual* order, so it can be drawn left to right with any font that has
//      the Arabic presentation forms (Vazirmatn does).
#include <string>

namespace liq {

std::string shapeArabicVisual(const std::string& utf8);

}  // namespace liq
