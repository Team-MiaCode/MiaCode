#include <QByteArray>
#include <QCoreApplication>
#include <QTextStream>

#include "export/video_export/RawVideoPipeTransport.h"

#ifdef Q_OS_WIN
#include <QScopeGuard>
#include <chrono>
#include <future>
#include <windows.h>
#endif

namespace {

using miacode::video_export::raw_pipe::enqueueRawVideoFrame;
using miacode::video_export::raw_pipe::finishRawVideoPipePump;
using miacode::video_export::raw_pipe::RawVideoPipePump;

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << message << Qt::endl;
        return false;
    }
    return true;
}

QByteArray stubFrameBytes()
{
    return QByteArray(4, '\0');
}

// The pump is driven without a writer thread on purpose: enqueue only touches
// the queue plus its bookkeeping under the mutex, so the sequence rules are
// observable without a real FIFO/named pipe (and without ffmpeg) on the other
// end. `writerThreadStarted` stays false, which is also what lets the written
// total stay at zero for the conservation checks below.
bool verifyFrameSequenceConservation(QTextStream& err)
{
    {
        RawVideoPipePump pump;
        QString detail;
        for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
            if (!require(
                    enqueueRawVideoFrame(
                        &pump, stubFrameBytes(), frameIndex, nullptr, nullptr, &detail),
                    QStringLiteral("consecutive frame %1 must enqueue (%2)")
                        .arg(frameIndex)
                        .arg(detail),
                    err)) {
                return false;
            }
        }
    }

    // A readback pipeline that drops in-flight frames does not leave a hole in
    // the rawvideo stream: the pipe carries pixels only, so the next frame
    // silently takes the missing slot and every later frame runs ahead of the
    // audio/background for the rest of the export. The enqueue side is the last
    // place that still knows the real frame number, so it has to reject the gap.
    {
        RawVideoPipePump pump;
        QString detail;
        for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
            enqueueRawVideoFrame(&pump, stubFrameBytes(), frameIndex, nullptr, nullptr, &detail);
        }
        detail.clear();
        if (!require(
                !enqueueRawVideoFrame(&pump, stubFrameBytes(), 5, nullptr, nullptr, &detail),
                QStringLiteral("a gap in the frame sequence must be rejected"),
                err)
            || !require(
                detail.contains(QStringLiteral("expected=3"))
                    && detail.contains(QStringLiteral("frame=5")),
                QStringLiteral("gap rejection must name both frame numbers (%1)").arg(detail),
                err)) {
            return false;
        }
    }

    {
        RawVideoPipePump pump;
        QString detail;
        for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
            enqueueRawVideoFrame(&pump, stubFrameBytes(), frameIndex, nullptr, nullptr, &detail);
        }
        detail.clear();
        if (!require(
                !enqueueRawVideoFrame(&pump, stubFrameBytes(), 2, nullptr, nullptr, &detail),
                QStringLiteral("a repeated frame number must be rejected"),
                err)) {
            return false;
        }
    }

    return true;
}

bool verifyWrittenFrameConservation(QTextStream& err)
{
    // Frames were produced and queued, but none reached the pipe. A container
    // with the planned frame count proves nothing here (ffmpeg extends the
    // overlay input's last frame), so the pump itself has to compare what it
    // was handed against what it actually wrote.
    {
        RawVideoPipePump pump;
        QString detail;
        for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
            enqueueRawVideoFrame(&pump, stubFrameBytes(), frameIndex, nullptr, nullptr, &detail);
        }
        detail.clear();
        if (!require(
                !finishRawVideoPipePump(&pump, 3, &detail),
                QStringLiteral("finalize must fail when no frame reached the pipe"),
                err)
            || !require(
                detail.contains(QStringLiteral("planned=3"))
                    && detail.contains(QStringLiteral("written=0")),
                QStringLiteral("finalize failure must report the shortfall (%1)").arg(detail),
                err)) {
            return false;
        }
    }

    {
        RawVideoPipePump pump;
        QString detail;
        if (!require(
                finishRawVideoPipePump(&pump, 0, &detail),
                QStringLiteral("a pump with no planned frames must finalize clean (%1)").arg(detail),
                err)) {
            return false;
        }
    }

    return true;
}

#ifdef Q_OS_WIN
enum class PipeReaderCase { SlowRead, ExitEarly, Cancel };

bool verifyNamedPipeClose(PipeReaderCase scenario, QTextStream& err)
{
    using namespace miacode::video_export::raw_pipe;
    RawVideoPipePump pump;
    pump.plan = chooseRawVideoPipePlan(QSize(640, 360));
    pump.plan.connectTimeoutMs = 1000;
    QString detail;
    if (!require(startRawVideoPipe(&pump.pipe, {}, pump.plan, &detail), detail, err)) {
        return false;
    }
    HANDLE client = INVALID_HANDLE_VALUE;
    const auto closeClient = [&]() {
        if (client != INVALID_HANDLE_VALUE) {
            CloseHandle(client);
            client = INVALID_HANDLE_VALUE;
        }
    };
    const auto cleanup = qScopeGuard([&]() {
        closeClient();
        shutdownRawVideoPipePump(&pump);
    });
    if (!require(startRawVideoPipePumpThread(&pump, &detail), detail, err)) {
        return false;
    }
    client = CreateFileW(reinterpret_cast<LPCWSTR>(pump.pipe.inputPath.utf16()),
        GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (!require(client != INVALID_HANDLE_VALUE, QStringLiteral("local pipe client must connect"), err)) {
        return false;
    }

    const QByteArray first(pump.plan.frameBytes, 'Q');
    const QByteArray second(pump.plan.frameBytes, 'r');
    const auto enqueue = [&](const QByteArray& bytes, int index) {
        const bool queued = enqueueRawVideoFrame(&pump, bytes, index, nullptr, nullptr, &detail);
        return require(queued, QStringLiteral("frame %1 must enqueue: %2").arg(index).arg(detail), err);
    };
    if (!enqueue(first, 0)) return false;

    const auto read = [&](qsizetype limit) {
        QByteArray output;
        char buffer[65536];
        DWORD readBytes = 0;
        while (output.size() < limit && ReadFile(client, buffer,
            static_cast<DWORD>(qMin<qsizetype>(sizeof(buffer), limit - output.size())), &readBytes, nullptr)
            && readBytes != 0) {
            output.append(buffer, readBytes);
        }
        return output;
    };
    if (scenario == PipeReaderCase::ExitEarly) {
        // Prove the connection was live before the reader exits.
        if (!require(read(first.size()) == first, QStringLiteral("the early client reads frame zero"), err)) {
            return false;
        }
        closeClient();
    }
    if (!enqueue(second, 1)) return false;

    if (scenario == PipeReaderCase::Cancel) {
        // Wait for bytes to be buffered without letting the client read them.
        DWORD available = 0;
        for (int i = 0; i < 100 && available == 0; ++i) {
            if (!PeekNamedPipe(client, nullptr, 0, nullptr, &available, nullptr)) break;
            if (available == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!require(available > 0, QStringLiteral("cancel begins with unread data in the pipe"), err)) {
            return false;
        }
    }

    auto finish = std::async(std::launch::async, [&]() {
        if (scenario == PipeReaderCase::Cancel) {
            shutdownRawVideoPipePump(&pump);
            return true;
        }
        return finishRawVideoPipePump(&pump, 2, &detail);
    });
    if (finish.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        closeClient();
        finish.wait();
        return require(false, QStringLiteral("closing the writer must not wait for a stalled client"), err);
    }

    const bool finished = finish.get();
    if (scenario == PipeReaderCase::ExitEarly) {
        return require(!finished && detail.contains(QStringLiteral("WriteFile")),
            QStringLiteral("a broken pipe fails rather than reporting a complete stream: %1").arg(detail), err);
    }
    if (scenario == PipeReaderCase::Cancel) {
        return require(read(1).isEmpty(), QStringLiteral("cancel discards unread data and closes the pipe"), err);
    }
    // Keep both frames unread until the writer has closed its handle. A
    // forced disconnect loses these bytes even though WriteFile succeeded.
    const QByteArray output = read(first.size() + second.size() + 1);
    const DWORD eof = GetLastError();
    return require(finished && output == first + second && eof == ERROR_BROKEN_PIPE,
        QStringLiteral("the late reader receives both buffered frames and EOF (%1 bytes)").arg(output.size()), err);
}
#endif

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);

    if (!verifyFrameSequenceConservation(err) || !verifyWrittenFrameConservation(err)) {
        return 1;
    }

#ifdef Q_OS_WIN
    if (!verifyNamedPipeClose(PipeReaderCase::SlowRead, err)
        || !verifyNamedPipeClose(PipeReaderCase::ExitEarly, err)
        || !verifyNamedPipeClose(PipeReaderCase::Cancel, err)) {
        return 1;
    }
#endif

    out << "raw_video_pipe_frame_conservation_spec ok" << Qt::endl;
    return 0;
}
