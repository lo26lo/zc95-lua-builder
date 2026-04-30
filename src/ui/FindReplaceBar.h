#pragma once

#include <QWidget>

class QPlainTextEdit;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;

class FindReplaceBar : public QWidget {
    Q_OBJECT
public:
    explicit FindReplaceBar(QPlainTextEdit* editor, QWidget* parent = nullptr);

public slots:
    void showFind();
    void showReplace();
    void hideBar();
    void findNext();
    void findPrev();
    void replaceCurrent();
    void replaceAll();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    bool find(bool forward);

    QPlainTextEdit* m_editor;
    QLineEdit* m_findEdit;
    QLineEdit* m_replaceEdit;
    QCheckBox* m_caseSensitive;
    QCheckBox* m_wholeWords;
    QPushButton* m_replaceBtn;
    QPushButton* m_replaceAllBtn;
    QLabel* m_replaceLabel;
    QLabel* m_status;
};
