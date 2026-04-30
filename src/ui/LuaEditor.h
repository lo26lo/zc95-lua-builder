#pragma once

#include <QPlainTextEdit>

class LuaHighlighter;
class QCompleter;
class QPaintEvent;
class QResizeEvent;

class LineNumberArea;

class LuaEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit LuaEditor(QWidget* parent = nullptr);

    void insertSnippet(const QString& code);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth();

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void insertCompletion(const QString& completion);
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();

private:
    QString textUnderCursor() const;

    LuaHighlighter* m_highlighter;
    QCompleter* m_completer;
    LineNumberArea* m_lineNumberArea;
};

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(LuaEditor* editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }
protected:
    void paintEvent(QPaintEvent* event) override { m_editor->lineNumberAreaPaintEvent(event); }
private:
    LuaEditor* m_editor;
};
