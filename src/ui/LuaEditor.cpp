#include "LuaEditor.h"
#include "../highlight/LuaHighlighter.h"

#include <QFont>
#include <QFontDatabase>
#include <QTextCursor>
#include <QCompleter>
#include <QStringListModel>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QPainter>
#include <QTextBlock>

namespace {
QStringList completionList() {
    return {
        "zc.ChannelOn", "zc.ChannelOff", "zc.ChannelPulseMs",
        "zc.SetPower", "zc.SetFrequency", "zc.SetPulseWidth",
        "zc.SetMenuOption", "zc.DelayMs",
        "zc.EnableTriphase", "zc.LinkChannels", "zc.AccIoWrite",
        "Setup", "Loop", "MinMaxChange", "MultiChoiceChange",
        "SoftButton", "ExternalTrigger", "BluetoothRemoteKeypress",
        "BluetoothHidEvent", "AudioIntensityChange",
        "function", "local", "if", "then", "else", "elseif", "end",
        "for", "while", "do", "repeat", "until", "return", "break",
        "true", "false", "nil", "and", "or", "not", "in",
        "print",
        "\"TRIGGER1\"", "\"TRIGGER2\"", "\"A\"", "\"B\"",
        "\"KEY_BUTTON\"", "\"KEY_UP\"", "\"KEY_DOWN\"", "\"KEY_LEFT\"",
        "\"KEY_RIGHT\"", "\"KEY_SHUTTER\"", "\"KEY_UNKNOWN\"",
        "\"OFF\"", "\"AUDIO_INTENSITY\"",
        "\"MIN_MAX\"", "\"MULTI_CHOICE\"",
        "\"AUDIO_VIEW_INTENSITY_STEREO\"", "\"AUDIO_VIEW_INTENSITY_MONO\"",
    };
}
}

LuaEditor::LuaEditor(QWidget* parent) : QPlainTextEdit(parent) {
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);
    setFont(mono);
    setTabStopDistance(4 * QFontMetricsF(mono).horizontalAdvance(' '));
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_highlighter = new LuaHighlighter(document());

    m_completer = new QCompleter(completionList(), this);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setWrapAround(false);
    connect(m_completer, qOverload<const QString&>(&QCompleter::activated),
            this, &LuaEditor::insertCompletion);

    m_lineNumberArea = new LineNumberArea(this);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &LuaEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &LuaEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &LuaEditor::highlightCurrentLine);
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int LuaEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void LuaEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void LuaEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) m_lineNumberArea->scroll(0, dy);
    else m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void LuaEditor::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void LuaEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(45, 45, 48));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor(133, 133, 133));
            painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void LuaEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection s;
        s.format.setBackground(QColor(40, 40, 48));
        s.format.setProperty(QTextFormat::FullWidthSelection, true);
        s.cursor = textCursor();
        s.cursor.clearSelection();
        selections.append(s);
    }
    setExtraSelections(selections);
}

void LuaEditor::insertSnippet(const QString& code) {
    QTextCursor cur = textCursor();
    cur.insertText(code);
    setTextCursor(cur);
    setFocus();
}

QString LuaEditor::textUnderCursor() const {
    QTextCursor tc = textCursor();
    int pos = tc.position();
    QString text = toPlainText();
    int start = pos;
    while (start > 0) {
        QChar c = text[start - 1];
        if (c.isLetterOrNumber() || c == '_' || c == '.') --start;
        else break;
    }
    return text.mid(start, pos - start);
}

void LuaEditor::focusInEvent(QFocusEvent* e) {
    if (m_completer) m_completer->setWidget(this);
    QPlainTextEdit::focusInEvent(e);
}

void LuaEditor::insertCompletion(const QString& completion) {
    QString prefix = m_completer->completionPrefix();
    int extra = completion.length() - prefix.length();
    QTextCursor tc = textCursor();
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));
    setTextCursor(tc);
}

void LuaEditor::keyPressEvent(QKeyEvent* e) {
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (e->key()) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                e->ignore();
                return;
            default:
                break;
        }
    }

    bool ctrlSpace = (e->key() == Qt::Key_Space) && (e->modifiers() & Qt::ControlModifier);
    if (!ctrlSpace) {
        QPlainTextEdit::keyPressEvent(e);
    }

    const bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (!m_completer || (ctrlOrShift && e->text().isEmpty())) return;

    static const QString eow("~!@#$%^&*()+{}|:\"<>?,/;'[]\\-=");
    const bool hasModifier = (e->modifiers() != Qt::NoModifier) && !ctrlSpace;
    QString prefix = textUnderCursor();

    if (!ctrlSpace && (hasModifier || e->text().isEmpty()
                       || prefix.length() < 2
                       || eow.contains(e->text().right(1)))) {
        m_completer->popup()->hide();
        return;
    }

    if (prefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(prefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr);
}
