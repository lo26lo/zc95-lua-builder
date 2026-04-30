#include "FindReplaceBar.h"

#include <QPlainTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextDocument>

FindReplaceBar::FindReplaceBar(QPlainTextEdit* editor, QWidget* parent)
    : QWidget(parent), m_editor(editor) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel("Find:", this));
    m_findEdit = new QLineEdit(this);
    row1->addWidget(m_findEdit, 1);
    auto* prev = new QPushButton("◀ Prev", this);
    auto* next = new QPushButton("Next ▶", this);
    m_caseSensitive = new QCheckBox("Aa", this);
    m_caseSensitive->setToolTip("Case sensitive");
    m_wholeWords = new QCheckBox("W", this);
    m_wholeWords->setToolTip("Whole words only");
    row1->addWidget(prev);
    row1->addWidget(next);
    row1->addWidget(m_caseSensitive);
    row1->addWidget(m_wholeWords);
    auto* close = new QPushButton("✕", this);
    close->setFixedWidth(28);
    row1->addWidget(close);
    outer->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    m_replaceLabel = new QLabel("Replace:", this);
    row2->addWidget(m_replaceLabel);
    m_replaceEdit = new QLineEdit(this);
    row2->addWidget(m_replaceEdit, 1);
    m_replaceBtn = new QPushButton("Replace", this);
    m_replaceAllBtn = new QPushButton("All", this);
    row2->addWidget(m_replaceBtn);
    row2->addWidget(m_replaceAllBtn);
    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #888;");
    row2->addWidget(m_status);
    outer->addLayout(row2);

    connect(next, &QPushButton::clicked, this, &FindReplaceBar::findNext);
    connect(prev, &QPushButton::clicked, this, &FindReplaceBar::findPrev);
    connect(close, &QPushButton::clicked, this, &FindReplaceBar::hideBar);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::findNext);
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::replaceCurrent);
    connect(m_replaceBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceCurrent);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceAll);

    hide();
}

void FindReplaceBar::showFind() {
    m_replaceLabel->hide();
    m_replaceEdit->hide();
    m_replaceBtn->hide();
    m_replaceAllBtn->hide();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceBar::showReplace() {
    m_replaceLabel->show();
    m_replaceEdit->show();
    m_replaceBtn->show();
    m_replaceAllBtn->show();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceBar::hideBar() {
    hide();
    m_editor->setFocus();
}

bool FindReplaceBar::find(bool forward) {
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) return false;

    QTextDocument::FindFlags flags;
    if (m_caseSensitive->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWords->isChecked()) flags |= QTextDocument::FindWholeWords;
    if (!forward) flags |= QTextDocument::FindBackward;

    bool found = m_editor->find(needle, flags);
    if (!found) {
        // Wrap around
        QTextCursor c = m_editor->textCursor();
        c.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        m_editor->setTextCursor(c);
        found = m_editor->find(needle, flags);
        m_status->setText(found ? "wrapped" : "not found");
    } else {
        m_status->clear();
    }
    return found;
}

void FindReplaceBar::findNext() { find(true); }
void FindReplaceBar::findPrev() { find(false); }

void FindReplaceBar::replaceCurrent() {
    QTextCursor c = m_editor->textCursor();
    if (c.hasSelection()) {
        // Honor the bar's case-sensitivity flag, otherwise replace silently
        // refuses when the user does an Aa-off search and the case differs.
        Qt::CaseSensitivity cs = m_caseSensitive->isChecked()
                                     ? Qt::CaseSensitive : Qt::CaseInsensitive;
        if (QString::compare(c.selectedText(), m_findEdit->text(), cs) == 0) {
            c.insertText(m_replaceEdit->text());
        }
    }
    findNext();
}

void FindReplaceBar::replaceAll() {
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) return;
    QTextDocument::FindFlags flags;
    if (m_caseSensitive->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWords->isChecked()) flags |= QTextDocument::FindWholeWords;

    QTextCursor c = m_editor->textCursor();
    c.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(c);

    int count = 0;
    while (m_editor->find(needle, flags)) {
        QTextCursor cur = m_editor->textCursor();
        cur.insertText(m_replaceEdit->text());
        ++count;
    }
    m_status->setText(QString("%1 replaced").arg(count));
}

void FindReplaceBar::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hideBar();
        return;
    }
    QWidget::keyPressEvent(e);
}
