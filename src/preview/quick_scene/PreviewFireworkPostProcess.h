#pragma once

#include <QSGNode>
#include <QRectF>
#include <array>

class QQuickWindow;
class QSGTexture;

inline constexpr int kFireworkStarCount = 32;
struct FireworkStarState {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float alpha = 0.0f;
};

// Render-thread owner of the composition, local stars and separable bloom targets.
class PreviewFireworkPostProcess final : public QSGNode
{
public:
    explicit PreviewFireworkPostProcess(QQuickWindow* window);
    ~PreviewFireworkPostProcess() override;
    QSGNode* sourceRoot() const;
    QSGTexture* starTexture() const;
    void update(const QRectF& rect, const QRectF& stageBounds, float clipRadius, float exitScale, float smoothBlendStart,
                const std::array<FireworkStarState, kFireworkStarCount>& stars);
    void preprocess() override;
private:
    struct Data;
    Data* d;
};
