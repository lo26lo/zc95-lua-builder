#pragma once

#include "../codegen/Linter.h"
#include <QWidget>

class QListWidget;
class QLabel;

class IssuesPanel : public QWidget {
    Q_OBJECT
public:
    explicit IssuesPanel(QWidget* parent = nullptr);

    void setIssues(const QVector<Issue>& issues);

signals:
    // Emitted when user double-clicks an issue with a known line number.
    void jumpToLine(int line);

private:
    QListWidget* m_list;
    QLabel* m_summary;
};
