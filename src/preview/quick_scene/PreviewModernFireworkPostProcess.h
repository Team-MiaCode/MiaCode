#pragma once

#include <QSGNode>
#include <QRectF>
#include <array>

class QQuickWindow;
class QSGTexture;

inline constexpr int kModernFireworkStarCount = 32;
struct ModernFireworkStarState {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float alpha = 0.0f;
};

// Render-thread owner of the composition, local stars and separable bloom targets.
class PreviewModernFireworkPostProcess final : public QSGNode
{
public:
    explicit PreviewModernFireworkPostProcess(QQuickWindow* window);
    ~PreviewModernFireworkPostProcess() override;
    QSGNode* sourceRoot() const;
    QSGTexture* starTexture() const;
    void update(const QRectF& rect, const QRectF& stageBounds, float clipRadius, float exitScale, float smoothBlendStart,
                const std::array<ModernFireworkStarState, kModernFireworkStarCount>& stars);
    void preprocess() override;
private:
    struct Data;
    Data* d;
};
