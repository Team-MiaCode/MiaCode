#pragma once

#include <QJsonObject>

// Bundled slide-shape reference dataset (`:/data/slide_data.json`, registered
// by resources/slide_data.qrc in this library). The ~17 MB JSON expands to
// roughly 90 MB of QJsonObject storage, so the Simai parser and the Muri
// analyzer share this single process-wide copy instead of each parsing their
// own.
namespace miacode::slide_reference {

// Lazily parsed on first use (thread-safe); empty object if the resource is
// missing or malformed. Holds the "slides", "wifi" and "config" sections.
const QJsonObject& root();

}  // namespace miacode::slide_reference
