// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config/configdialog.h"

#include "config/settings.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace textract {

bool runConfigDialog(Settings *settings, const QStringList &languages)
{
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("Text Extractor Settings"));

    auto *layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel(
        QStringLiteral("Languages for tier 1 (Tesseract). Tier 2 (PP-OCRv6) "
                       "uses one fixed character set and takes no language."),
        &dialog));

    // Built from installed tessdata, so the dialog cannot offer a language
    // whose warmUp() would then fail.
    const QStringList selected =
        settings->langs.split(QLatin1Char('+'), Qt::SkipEmptyParts);

    auto *list = new QListWidget(&dialog);
    for (const QString &lang : languages) {
        auto *item = new QListWidgetItem(lang, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selected.contains(lang) ? Qt::Checked : Qt::Unchecked);
    }
    layout->addWidget(list);

    auto *form = new QFormLayout;
    auto *modelDir = new QLineEdit(settings->modelDir, &dialog);
    modelDir->setPlaceholderText(QStringLiteral("default location"));
    form->addRow(QStringLiteral("Tier 2 model directory:"), modelDir);
    layout->addLayout(form);

    layout->addWidget(new QLabel(
        QStringLiteral("Shortcuts are configured in "
                       "System Settings → Shortcuts → textract."),
        &dialog));

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // With nothing checked, an empty Languages would be written, fail warmUp(),
    // and be reported as missing langdata -- which names the wrong cause.
    // Refusing it at the only place it can be created is cheaper than
    // explaining it afterwards.
    auto *okButton = buttons->button(QDialogButtonBox::Ok);
    const auto updateOkState = [list, okButton] {
        bool any = false;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->checkState() == Qt::Checked) {
                any = true;
                break;
            }
        }
        okButton->setEnabled(any);
    };
    QObject::connect(list, &QListWidget::itemChanged, &dialog, updateOkState);
    updateOkState();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QStringList chosen;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() == Qt::Checked) {
            chosen << list->item(i)->text();
        }
    }

    settings->langs = chosen.join(QLatin1Char('+'));
    settings->modelDir = modelDir->text().trimmed();
    return true;
}

} // namespace textract
