#include "pppps/poc/PocPipeline.h"
#include "pppps/poc/PocTaskController.h"

#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

using pppps::poc::CancellationFlag;
using pppps::poc::PocError;
using pppps::poc::PocPipeline;
using pppps::poc::PocTaskController;
using pppps::poc::TaskCanceled;

class PocPipelineTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsJpegAndPngSupport();
    void decodesPngAndRunsOpenCvAndOnnxRuntime();
    void decodesJpeg();
    void rejectsInvalidImage();
    void honorsCancellationBetweenStages();
    void controllerCompletesInBackground();
    void controllerReportsFailure();
    void controllerCanBeCanceled();

private:
    static QImage sampleImage();
    static QString saveImage(QTemporaryDir &directory, const QString &name, const char *format);
};

QImage PocPipelineTest::sampleImage()
{
    QImage image(96, 64, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor(x * 2, y * 3, (x + y) % 256));
        }
    }
    return image;
}

QString PocPipelineTest::saveImage(
    QTemporaryDir &directory,
    const QString &name,
    const char *format
)
{
    const auto path = directory.filePath(name);
    if (!sampleImage().save(path, format)) {
        return {};
    }
    return path;
}

void PocPipelineTest::reportsJpegAndPngSupport()
{
    const auto formats = PocPipeline::supportedImageFormats();
    QVERIFY2(formats.contains(QStringLiteral("jpeg")), qPrintable(formats.join(',')));
    QVERIFY2(formats.contains(QStringLiteral("png")), qPrintable(formats.join(',')));
}

void PocPipelineTest::decodesPngAndRunsOpenCvAndOnnxRuntime()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = saveImage(directory, QStringLiteral("sample.png"), "PNG");
    QVERIFY(!path.isEmpty());

    CancellationFlag cancellation;
    QList<int> progress;
    const auto result = PocPipeline{}.run(
        path,
        cancellation,
        [&progress](const int percent, const QString &) { progress.push_back(percent); }
    );

    QCOMPARE(result.imageSize, QSize(96, 64));
    QCOMPARE(result.imageFormat, QStringLiteral("png"));
    QVERIFY(std::isfinite(result.meanBgr[0]));
    QVERIFY(std::isfinite(result.meanBgr[1]));
    QVERIFY(std::isfinite(result.meanBgr[2]));
    QVERIFY(!result.onnxRuntimeVersion.isEmpty());
    QVERIFY(result.executionProviders.contains(QStringLiteral("CPUExecutionProvider")));
    QCOMPARE(progress.constLast(), 100);
}

void PocPipelineTest::decodesJpeg()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = saveImage(directory, QStringLiteral("sample.jpg"), "JPEG");
    QVERIFY(!path.isEmpty());

    CancellationFlag cancellation;
    const auto result = PocPipeline{}.run(path, cancellation);
    QCOMPARE(result.imageSize, QSize(96, 64));
    QCOMPARE(result.imageFormat, QStringLiteral("jpeg"));
}

void PocPipelineTest::rejectsInvalidImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("broken.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("not an image") > 0);
    file.close();

    CancellationFlag cancellation;
    bool didThrow = false;
    try {
        static_cast<void>(PocPipeline{}.run(path, cancellation));
    } catch (const PocError &error) {
        didThrow = true;
        QVERIFY(QString::fromUtf8(error.what()).contains(QStringLiteral("无法解码")));
    }
    QVERIFY(didThrow);
}

void PocPipelineTest::honorsCancellationBetweenStages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = saveImage(directory, QStringLiteral("cancel.png"), "PNG");
    QVERIFY(!path.isEmpty());

    CancellationFlag cancellation;
    bool didCancel = false;
    try {
        static_cast<void>(PocPipeline{}.run(
            path,
            cancellation,
            [&cancellation](const int percent, const QString &) {
                if (percent >= 45) {
                    cancellation.cancel();
                }
            }
        ));
    } catch (const TaskCanceled &) {
        didCancel = true;
    }
    QVERIFY(didCancel);
}

void PocPipelineTest::controllerCompletesInBackground()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = saveImage(directory, QStringLiteral("background.png"), "PNG");
    QVERIFY(!path.isEmpty());

    PocTaskController controller;
    QSignalSpy completed(&controller, &PocTaskController::completed);
    QSignalSpy failed(&controller, &PocTaskController::failed);
    QSignalSpy progress(&controller, &PocTaskController::progressChanged);

    controller.start(path);
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(progress.count() >= 3);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 5000);
}

void PocPipelineTest::controllerReportsFailure()
{
    PocTaskController controller;
    QSignalSpy failed(&controller, &PocTaskController::failed);
    QSignalSpy completed(&controller, &PocTaskController::completed);

    controller.start(QStringLiteral("/path/that/does/not/exist.png"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
    QCOMPARE(completed.count(), 0);
    QVERIFY(failed.constFirst().constFirst().toString().contains(QStringLiteral("不存在")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 5000);
}

void PocPipelineTest::controllerCanBeCanceled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = saveImage(directory, QStringLiteral("cancel-background.png"), "PNG");
    QVERIFY(!path.isEmpty());

    PocTaskController controller;
    QSignalSpy canceled(&controller, &PocTaskController::canceled);
    QSignalSpy completed(&controller, &PocTaskController::completed);

    controller.start(path);
    controller.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(canceled.count(), 1, 5000);
    QCOMPARE(completed.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 5000);
}

QTEST_GUILESS_MAIN(PocPipelineTest)

#include "tst_PocPipeline.moc"
