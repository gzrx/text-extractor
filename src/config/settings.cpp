// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config/settings.h"

#include <algorithm>

#include <KConfigGroup>

namespace textract {

namespace {
constexpr auto kGeneralGroup    = "General";
constexpr auto kPreprocessGroup = "Preprocess";
constexpr auto kModelsGroup     = "Models";
} // namespace

Settings loadSettings(const KConfigGroup &root)
{
    const Settings defaults;
    Settings s;

    const KConfigGroup general = root.group(QLatin1String(kGeneralGroup));
    s.langs = general.readEntry("Languages", defaults.langs);

    const KConfigGroup pre = root.group(QLatin1String(kPreprocessGroup));
    const int upscale = pre.readEntry("Upscale", defaults.preprocess.upscale);

    // Clamped, not rejected, and to exactly the range effectiveUpscale()
    // already enforces. Introducing a second, stricter rule here would let a
    // file load as one factor and preprocess as another.
    s.preprocess.upscale = std::clamp(upscale, kMinUpscale, kMaxUpscale);
    s.preprocess.binarize = pre.readEntry("Binarize", defaults.preprocess.binarize);

    const KConfigGroup models = root.group(QLatin1String(kModelsGroup));
    s.modelDir = models.readEntry("Directory", defaults.modelDir);

    return s;
}

void saveSettings(KConfigGroup &root, const Settings &settings,
                  KConfigBase::WriteConfigFlags flags)
{
    KConfigGroup general = root.group(QLatin1String(kGeneralGroup));
    general.writeEntry("Languages", settings.langs, flags);

    KConfigGroup pre = root.group(QLatin1String(kPreprocessGroup));
    pre.writeEntry("Upscale", settings.preprocess.upscale, flags);
    pre.writeEntry("Binarize", settings.preprocess.binarize, flags);

    KConfigGroup models = root.group(QLatin1String(kModelsGroup));
    models.writeEntry("Directory", settings.modelDir, flags);
}

} // namespace textract
