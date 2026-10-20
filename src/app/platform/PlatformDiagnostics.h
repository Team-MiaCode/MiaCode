#pragma once

// Process and GPU diagnostics the process entry, the UI and the runtime share.
// Implemented in this directory (process_identity.cpp, gpu_adapter_probe.cpp,
// gpu_device_provider.cpp); kept out of app/MainEntrypoints.h so layers below
// the process entry do not include the entry-point header.

#include <QString>

class QQuickWindow;

namespace miacode::app::entry {

// ===== Startup diagnostics bundle (process_identity.cpp) =====
// Emits startup/process_identity (P0) + startup/gpu_hint (P2) + startup/gpu_policy
// (P3) in one call, deriving argv/role from QCoreApplication. Gated on --debug.
// `phase` tags the batch ("boot" early in main(), "log_dir_rebound" after the
// runtime log directory rebinds to a chart's .miacode/logs/) so these process-
// invariant facts are visible in whichever log file a user collects. Call after
// QGuiApplication is constructed.
void logProcessStartupDiagnostics(const QString& phase);

// ===== Actual GPU adapter / renderer probe (gpu_adapter_probe.cpp) =====
// Schedules a one-shot render-thread probe of the window's live RHI device and
// logs the live device for D3D11, D3D12, Metal, Vulkan, OpenGL, and software
// rendering. Gated on --debug. Safe to call before scene-graph initialization.
void logQuickWindowGpuDevice(QQuickWindow* window, const QString& surfaceLabel);

// ===== High-performance Quick graphics device provider (gpu_device_provider.cpp) =====
// P4 — before a Quick window/view initializes its scene graph, optionally bind
// it to the resolved high-performance DXGI adapter via
// QQuickGraphicsDevice::fromAdapter (Qt owns the device lifetime). Behaviour:
//  - preferVideoShareDevice=true (preview composite / video surfaces): try the
//    H2 single-device video-share device; else keep Qt's DEFAULT adapter. Never
//    fromAdapter — the D3D11VA two-device keyed-mutex bridge is same-adapter only.
//  - preferVideoShareDevice=false (root window): fromAdapter(high-perf LUID),
//    but ONLY when MIACODE_GPU_BIND_HIGH_PERFORMANCE=1 (opt-in until validated),
//    the RHI is D3D11, the policy yields a hardware LUID, and it differs from the
//    default adapter (else redundant). Any miss leaves Qt on its default adapter.
// Logs the decision (startup/gpu_provider). Returns true iff a device was bound.
// Must be called on the GUI thread BEFORE the window's scene graph initializes.
bool bindHighPerformanceQuickGraphicsDevice(
    QQuickWindow* window, const QString& surfaceLabel, bool preferVideoShareDevice);

}  // namespace miacode::app::entry
