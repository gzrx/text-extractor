// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocr/onnxpaddleengine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QStringView>
#include <QTextStream>

#include <onnxruntime_cxx_api.h>

#include "ocr/paddledet.h"
#include "ocr/paddlerec.h"
#include "ocr/paddlewords.h"

namespace textract {

namespace {

/// Eight beats all 32 cores by 18% on this model size -- past eight, thread
/// contention costs more than the parallelism buys. Measured; see HANDOFF 12.2.
constexpr int kIntraOpThreads = 8;

/// The detector's longest side, in pixels. Larger inputs are scaled down.
constexpr int kDetLimitSide = 960;

/// Every recognition crop is resized to this height.
constexpr int kRecHeight = 48;

/// ImageNet normalisation, which is what the detector was trained with.
constexpr float kMean[3] = {0.485f, 0.456f, 0.406f};
constexpr float kStd[3] = {0.229f, 0.224f, 0.225f};

/// Removes leading ASCII space and tab only.
///
/// NOT QString::trimmed(), and this is load-bearing rather than fussy.
/// trimmed() strips every character QChar::isSpace() accepts, which includes
/// U+3000 IDEOGRAPHIC SPACE -- and entry 1748 of PP-OCRv6's dictionary is an
/// unquoted U+3000. Trimming it away drops one entry and shifts every class
/// index after it by one, silently corrupting all CJK recognition, which is
/// the single thing tier 2 exists to fix. Verified against a real YAML parse:
/// this yields 18708 entries, trimmed() yields 18707.
QStringView leadingAsciiTrimmed(QStringView line)
{
    qsizetype i = 0;
    while (i < line.size()
           && (line.at(i) == QLatin1Char(' ') || line.at(i) == QLatin1Char('\t'))) {
        ++i;
    }
    return line.mid(i);
}

/// Reads the `character_dict` list out of a PaddleOCR inference.yml.
///
/// Deliberately a line scanner rather than a YAML parse: the file's only
/// interesting part is a flat list of scalars under a known key, and this
/// avoids taking a YAML dependency for one field. Entries are emitted in file
/// order, which is the order the class indices use.
QStringList readCharacterDict(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList dict;
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    bool inDict = false;
    while (!stream.atEnd()) {
        const QString raw = stream.readLine();
        const QStringView body = leadingAsciiTrimmed(raw);

        if (!inDict) {
            if (body.startsWith(QLatin1String("character_dict:"))) {
                inDict = true;
            }
            continue;
        }
        if (!body.startsWith(QLatin1String("- "))) {
            break; // The first non-entry line ends the block.
        }

        QString value = body.mid(2).toString();
        // Scalars may be single-quoted, and a literal quote is doubled.
        if (value.size() >= 2 && value.startsWith(QLatin1Char('\''))
            && value.endsWith(QLatin1Char('\''))) {
            value = value.mid(1, value.size() - 2).replace(QLatin1String("''"),
                                                           QLatin1String("'"));
        }
        dict << value;
    }
    return dict;
}

QImage toRgb888(const QImage &image)
{
    return image.format() == QImage::Format_RGB888
               ? image
               : image.convertToFormat(QImage::Format_RGB888);
}

} // namespace

struct OnnxPaddleEngine::Private {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "textract"};
    Ort::SessionOptions options;
    std::unique_ptr<Ort::Session> det;
    std::unique_ptr<Ort::Session> rec;
    QStringList charset;
    DetectOptions detect;
    int  upscale{1};
    bool ready{false};
};

OnnxPaddleEngine::OnnxPaddleEngine(const QString &modelDir)
    : d(std::make_unique<Private>())
{
    d->options.SetIntraOpNumThreads(kIntraOpThreads);
    d->options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    const QString detPath = modelDir + QStringLiteral("/ppocrv6_small_det.onnx");
    const QString recPath = modelDir + QStringLiteral("/ppocrv6_small_rec.onnx");
    const QString recYml = modelDir + QStringLiteral("/ppocrv6_small_rec.yml");

    if (!QFile::exists(detPath) || !QFile::exists(recPath)) {
        return; // Stays unavailable. Not an error: tier 2 is optional.
    }

    const QStringList dict = readCharacterDict(recYml);
    if (dict.isEmpty()) {
        return;
    }
    // PaddleOCR's CTCLabelDecode order: blank, then the dictionary, then space.
    d->charset.reserve(dict.size() + 2);
    d->charset << QStringLiteral("<blank>");
    d->charset << dict;
    d->charset << QStringLiteral(" ");

    try {
        d->det = std::make_unique<Ort::Session>(
            d->env, detPath.toLocal8Bit().constData(), d->options);
        d->rec = std::make_unique<Ort::Session>(
            d->env, recPath.toLocal8Bit().constData(), d->options);
    } catch (const Ort::Exception &) {
        // A corrupt or truncated model must not take the daemon down with it.
        d->det.reset();
        d->rec.reset();
        return;
    }

    // The charset and the model must agree exactly, or every class index is
    // off and the output is confident nonsense rather than an obvious failure.
    // This is not hypothetical: parsing the dictionary with QString::trimmed()
    // silently drops the unquoted U+3000 at entry 1748 and lands here one
    // short. An off-by-one in a charset is invisible in Latin text and
    // catastrophic in CJK, so it is checked rather than trusted.
    const auto recShape =
        d->rec->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (recShape.size() < 3 || recShape.back() != int64_t(d->charset.size())) {
        std::fprintf(stderr,
                     "textract: charset has %lld entries but %s expects %lld; "
                     "refusing to run tier 2\n",
                     static_cast<long long>(d->charset.size()),
                     qPrintable(recPath),
                     recShape.empty()
                         ? -1LL
                         : static_cast<long long>(recShape.back()));
        d->det.reset();
        d->rec.reset();
        return;
    }

    d->ready = true;
}

OnnxPaddleEngine::~OnnxPaddleEngine() = default;

bool OnnxPaddleEngine::available() const
{
    return d->ready;
}

bool OnnxPaddleEngine::isWarm() const
{
    return d->ready;
}

void OnnxPaddleEngine::setUpscaleFactor(int factor)
{
    d->upscale = factor > 0 ? factor : 1;
}

QString OnnxPaddleEngine::defaultModelDir()
{
    // TEXTRACT_MODELS mirrors TEXTRACT_FIXTURES, which the corpus gate already
    // honours, so a checkout can point at models outside the user's data dir.
    if (const QByteArray override = qgetenv("TEXTRACT_MODELS");
        !override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }

    // GenericDataLocation + a literal "textract", NOT AppDataLocation.
    // AppDataLocation is derived from the running executable's name, so the
    // daemon would look in ~/.local/share/textract while test_fixtures looked
    // in ~/.local/share/test_fixtures and silently found nothing. The models
    // belong to the project, not to whichever binary opened them.
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/textract/models");
}

std::vector<Word> OnnxPaddleEngine::recognize(const QImage &image,
                                              const QString &langs,
                                              Segmentation mode)
{
    // PP-OCRv6_small is one multilingual model covering 50 languages, so the
    // language selection has nothing to select. Segmentation is likewise
    // meaningless here, which is what providesReadingOrder() == false declares.
    Q_UNUSED(langs);
    Q_UNUSED(mode);

    if (!d->ready || image.isNull()) {
        return {};
    }

    const QImage source = toRgb888(image);
    const int width = source.width();
    const int height = source.height();

    // --- detection -------------------------------------------------------
    const double ratio =
        std::min(1.0, double(kDetLimitSide) / double(std::max(width, height)));
    const int detWidth = std::max(32, int(std::lround(width * ratio / 32.0)) * 32);
    const int detHeight = std::max(32, int(std::lround(height * ratio / 32.0)) * 32);

    const QImage detImage = source.scaled(detWidth, detHeight,
                                          Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation);

    std::vector<float> input(size_t(3) * size_t(detWidth) * size_t(detHeight));
    const size_t plane = size_t(detWidth) * size_t(detHeight);
    for (int y = 0; y < detHeight; ++y) {
        const uchar *row = detImage.constScanLine(y);
        for (int x = 0; x < detWidth; ++x) {
            for (int c = 0; c < 3; ++c) {
                const float value = float(row[x * 3 + c]) / 255.0f;
                input[size_t(c) * plane + size_t(y) * size_t(detWidth) + size_t(x)] =
                    (value - kMean[c]) / kStd[c];
            }
        }
    }

    const std::array<int64_t, 4> detShape{1, 3, detHeight, detWidth};
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<QRect> boxes;
    try {
        Ort::Value detTensor = Ort::Value::CreateTensor<float>(
            memory, input.data(), input.size(), detShape.data(), detShape.size());

        Ort::AllocatorWithDefaultOptions allocator;
        const auto inName = d->det->GetInputNameAllocated(0, allocator);
        const auto outName = d->det->GetOutputNameAllocated(0, allocator);
        const char *inNames[] = {inName.get()};
        const char *outNames[] = {outName.get()};

        auto outputs = d->det->Run(Ort::RunOptions{nullptr}, inNames, &detTensor,
                                   1, outNames, 1);
        const float *map = outputs.front().GetTensorData<float>();
        const auto shape =
            outputs.front().GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() < 4) {
            return {};
        }
        boxes = detectTextLines(map, int(shape[3]), int(shape[2]), d->detect);
    } catch (const Ort::Exception &) {
        return {};
    }

    // Detection ran on a scaled image; put the boxes back in source pixels.
    const double sx = double(width) / double(detWidth);
    const double sy = double(height) / double(detHeight);

    std::vector<TextLine> lines;
    lines.reserve(boxes.size());

    // --- recognition, one crop at a time ---------------------------------
    for (const QRect &box : boxes) {
        const QRect inSource(int(box.left() * sx), int(box.top() * sy),
                             std::max(1, int(box.width() * sx)),
                             std::max(1, int(box.height() * sy)));
        const QRect clamped = inSource & QRect(0, 0, width, height);
        if (clamped.width() < 2 || clamped.height() < 2) {
            continue;
        }

        const int cropWidth = std::max(
            8, std::min(3200, int(double(kRecHeight) * double(clamped.width())
                                  / double(clamped.height()))));
        const QImage crop = source.copy(clamped).scaled(
            cropWidth, kRecHeight, Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);

        std::vector<float> recInput(size_t(3) * size_t(cropWidth)
                                    * size_t(kRecHeight));
        const size_t recPlane = size_t(cropWidth) * size_t(kRecHeight);
        for (int y = 0; y < kRecHeight; ++y) {
            const uchar *row = crop.constScanLine(y);
            for (int x = 0; x < cropWidth; ++x) {
                for (int c = 0; c < 3; ++c) {
                    const float value = float(row[x * 3 + c]) / 255.0f;
                    recInput[size_t(c) * recPlane + size_t(y) * size_t(cropWidth)
                             + size_t(x)] = (value - 0.5f) / 0.5f;
                }
            }
        }

        const std::array<int64_t, 4> recShape{1, 3, kRecHeight, cropWidth};
        try {
            Ort::Value recTensor = Ort::Value::CreateTensor<float>(
                memory, recInput.data(), recInput.size(), recShape.data(),
                recShape.size());

            Ort::AllocatorWithDefaultOptions allocator;
            const auto inName = d->rec->GetInputNameAllocated(0, allocator);
            const auto outName = d->rec->GetOutputNameAllocated(0, allocator);
            const char *inNames[] = {inName.get()};
            const char *outNames[] = {outName.get()};

            auto outputs = d->rec->Run(Ort::RunOptions{nullptr}, inNames,
                                       &recTensor, 1, outNames, 1);
            const auto shape =
                outputs.front().GetTensorTypeAndShapeInfo().GetShape();
            if (shape.size() < 3) {
                continue;
            }
            const Recognition decoded =
                decodeCtc(outputs.front().GetTensorData<float>(), int(shape[1]),
                          int(shape[2]), d->charset);
            if (decoded.text.trimmed().isEmpty()) {
                continue;
            }

            // Back to ORIGINAL crop coordinates. analyze/ and assemble/ must
            // never learn that preprocessing upscaled anything.
            const QRect reported(clamped.left() / d->upscale,
                                 clamped.top() / d->upscale,
                                 std::max(1, clamped.width() / d->upscale),
                                 std::max(1, clamped.height() / d->upscale));
            lines.push_back({decoded.text, reported, decoded.confidence});
        } catch (const Ort::Exception &) {
            continue;
        }
    }

    return wordsFromLines(lines);
}

} // namespace textract
