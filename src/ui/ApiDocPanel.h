#pragma once

#include <QWidget>

class QListWidget;
class QTextBrowser;

class ApiDocPanel : public QWidget {
    Q_OBJECT
public:
    explicit ApiDocPanel(QWidget* parent = nullptr);

public slots:
    // Show the doc for a given API name (e.g. "zc.ChannelOn"). If empty or
    // unknown, falls back to the first entry.
    void showEntry(const QString& name);

signals:
    void insertRequested(const QString& code);

private:
    void renderCurrent();

    QListWidget* m_list;
    QTextBrowser* m_doc;
};
