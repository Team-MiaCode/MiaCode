#include <QCoreApplication>
#include <QHash>
#include <QTextStream>
#include <QVector>

#include "preview/quick_scene/PreviewQuickSpriteBatchPolicy.h"

namespace {

struct VertexStub {
    int x = 0;
    int y = 0;
    int u = 0;
    int v = 0;
    int opacity = 0;
    int effect = 0;

    bool operator==(const VertexStub& other) const
    {
        return x == other.x
            && y == other.y
            && u == other.u
            && v == other.v
            && opacity == other.opacity
            && effect == other.effect;
    }
};

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << message << Qt::endl;
        return false;
    }
    return true;
}

bool verifyOrderedRunGrouping(QTextStream& err)
{
    const QVector<miacode::preview::quick_scene::SpriteFamilyKey> familyKeys = {1, 1, 2, 2, 1, 3, 3};
    const auto runs = miacode::preview::quick_scene::buildPreviewQuickOrderedRunRanges(familyKeys);
    if (!require(runs.size() == 4, QStringLiteral("ordered runs split on non-contiguous family transitions"), err)) {
        return false;
    }
    if (!require(runs.at(0).familyKey == 1 && runs.at(0).startSpriteIndex == 0 && runs.at(0).spriteCount == 2, QStringLiteral("run 0 matches expected family and count"), err)) {
        return false;
    }
    if (!require(runs.at(1).familyKey == 2 && runs.at(1).startSpriteIndex == 2 && runs.at(1).spriteCount == 2, QStringLiteral("run 1 matches expected family and count"), err)) {
        return false;
    }
    if (!require(runs.at(2).familyKey == 1 && runs.at(2).startSpriteIndex == 4 && runs.at(2).spriteCount == 1, QStringLiteral("run 2 preserves later re-entry as a new run"), err)) {
        return false;
    }
    if (!require(runs.at(3).familyKey == 3 && runs.at(3).startSpriteIndex == 5 && runs.at(3).spriteCount == 2, QStringLiteral("run 3 matches expected family and count"), err)) {
        return false;
    }
    return true;
}

bool verifyDegenerateTailFillPreservesPrefix(QTextStream& err)
{
    QVector<VertexStub> vertices(12);
    for (int index = 0; index < 6; ++index) {
        vertices[index] = VertexStub{index, index + 1, index + 2, index + 3, 255, index};
    }
    const QVector<VertexStub> expectedPrefix = vertices.first(6);
    const VertexStub filler{99, 88, 77, 66, 0, 0};
    miacode::preview::quick_scene::fillPreviewQuickDegenerateVertexTail(vertices.data(), 6, vertices.size(), filler);

    for (int index = 0; index < 6; ++index) {
        if (!require(vertices[index] == expectedPrefix[index], QStringLiteral("degenerate tail fill preserves valid prefix"), err)) {
            return false;
        }
    }
    for (int index = 6; index < vertices.size(); ++index) {
        if (!require(vertices[index] == filler, QStringLiteral("degenerate tail fill writes the requested filler to the tail"), err)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);

    if (!verifyOrderedRunGrouping(err)) {
        return 1;
    }
    if (!verifyDegenerateTailFillPreservesPrefix(err)) {
        return 1;
    }

    out << "preview_quick_sprite_batch_spec ok" << Qt::endl;
    return 0;
}
