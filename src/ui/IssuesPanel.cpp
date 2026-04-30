#include "IssuesPanel.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QLabel>

IssuesPanel::IssuesPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_summary = new QLabel("No issues.", this);
    m_summary->setStyleSheet("padding: 4px 8px; background: #2d2d30; color: #ccc;");
    layout->addWidget(m_summary);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        int line = item->data(Qt::UserRole).toInt();
        if (line > 0) emit jumpToLine(line);
    });
}

void IssuesPanel::setIssues(const QVector<Issue>& issues) {
    m_list->clear();
    if (issues.isEmpty()) {
        m_summary->setText("✓ No issues found.");
        m_summary->setStyleSheet("padding: 4px 8px; background: #2d2d30; color: #6c6;");
        return;
    }

    int errors = 0, warnings = 0, infos = 0;
    for (const auto& iss : issues) {
        QString prefix;
        QColor color;
        switch (iss.severity) {
            case IssueSeverity::Error:   prefix = "✗ ERROR"; color = QColor("#ff6b6b"); ++errors; break;
            case IssueSeverity::Warning: prefix = "⚠ WARN ";  color = QColor("#ffd166"); ++warnings; break;
            case IssueSeverity::Info:    prefix = "ℹ INFO ";   color = QColor("#88ccff"); ++infos; break;
        }
        QString lineText = (iss.line > 0) ? QString("L%1").arg(iss.line) : "  ";
        QString text = QString("%1  %2  %3").arg(prefix, lineText, iss.message);
        if (!iss.fixHint.isEmpty()) text += "  →  " + iss.fixHint;
        auto* item = new QListWidgetItem(text);
        item->setForeground(color);
        item->setData(Qt::UserRole, iss.line);
        m_list->addItem(item);
    }
    m_summary->setText(QString("%1 error(s), %2 warning(s), %3 info").arg(errors).arg(warnings).arg(infos));
    m_summary->setStyleSheet(QString("padding: 4px 8px; background: #2d2d30; color: %1;")
        .arg(errors > 0 ? "#ff6b6b" : (warnings > 0 ? "#ffd166" : "#ccc")));
}
