#pragma once

#include <QWidget>

class SnippetsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SnippetsPanel(QWidget* parent = nullptr);

signals:
    void snippetRequested(const QString& code);
};
