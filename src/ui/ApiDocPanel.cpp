#include "ApiDocPanel.h"
#include "../codegen/ApiDoc.h"

#include <QListWidget>
#include <QTextBrowser>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

ApiDocPanel::ApiDocPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    auto* split = new QSplitter(Qt::Vertical, this);

    m_list = new QListWidget(this);
    for (const auto& e : ApiDoc::entries()) {
        m_list->addItem(e.name);
    }
    split->addWidget(m_list);

    m_doc = new QTextBrowser(this);
    m_doc->setOpenExternalLinks(false);
    split->addWidget(m_doc);
    split->setSizes({200, 400});

    outer->addWidget(split, 1);

    auto* row = new QHBoxLayout();
    auto* insertBtn = new QPushButton("Insert example", this);
    row->addStretch();
    row->addWidget(insertBtn);
    outer->addLayout(row);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { renderCurrent(); });
    connect(insertBtn, &QPushButton::clicked, this, [this]() {
        auto* item = m_list->currentItem();
        if (!item) return;
        const ApiEntry* e = ApiDoc::find(item->text());
        if (e && !e->example.isEmpty()) emit insertRequested(e->example + "\n");
    });

    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void ApiDocPanel::showEntry(const QString& name) {
    auto items = m_list->findItems(name, Qt::MatchExactly);
    if (items.isEmpty()) {
        if (m_list->count() > 0) m_list->setCurrentRow(0);
    } else {
        m_list->setCurrentItem(items.first());
    }
}

void ApiDocPanel::renderCurrent() {
    auto* item = m_list->currentItem();
    if (!item) { m_doc->clear(); return; }
    const ApiEntry* e = ApiDoc::find(item->text());
    if (!e) { m_doc->clear(); return; }

    QString html;
    html += "<h3 style='color:#dcb;'>" + e->name.toHtmlEscaped() + "</h3>";
    html += "<pre style='background:#1e1e1e;padding:6px;color:#d4d4d4;'>"
            + e->signature.toHtmlEscaped() + "</pre>";
    html += "<p>" + e->description.toHtmlEscaped().replace("\n", "<br>") + "</p>";

    if (!e->parameters.isEmpty()) {
        html += "<h4>Parameters</h4><ul>";
        for (const auto& p : e->parameters) {
            html += "<li>" + p.toHtmlEscaped() + "</li>";
        }
        html += "</ul>";
    }
    if (!e->example.isEmpty()) {
        html += "<h4>Example</h4>";
        html += "<pre style='background:#1e1e1e;padding:6px;color:#d4d4d4;'>"
                + e->example.toHtmlEscaped() + "</pre>";
    }
    m_doc->setHtml(html);
}
