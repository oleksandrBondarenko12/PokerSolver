#include "RangeSelector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QTextEdit>
#include <QSlider>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QColor>
#include <QDebug>
#include <QIcon>
#include <QFile>

RangeSelector::RangeSelector(QWidget *parent, bool isIP)
    : QDialog(parent)
{
    // Set the window title based on type (IP/OOP)
    setWindowTitle(QString("Range Selector – %1").arg(isIP ? "IP" : "OOP"));
    setupUI();
    setStyleSheet("QDialog { background-color: #111827; }");
    setMinimumWidth(800);
}

void RangeSelector::setupUI() {
    // Create the main layout with proper margins and spacing
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ======================================================================
    // Header Area - Mimic HTML header with title and a close button
    // ======================================================================
    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(5);
    
    QLabel *titleLabel = new QLabel(windowTitle(), headerWidget);
    titleLabel->setStyleSheet("color: #F9FAFB; font-size: 20px; font-weight: 600;");
    
    // Close button: using a simple "X" text
    QPushButton *closeButton = new QPushButton("×");
    closeButton->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  color: #9CA3AF;"
        "  font-size: 24px;"
        "  border: none;"
        "  padding: 4px 8px;"
        "}"
        "QPushButton:hover {"
        "  color: #F9FAFB;"
        "}"
    );
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);
    headerWidget->setStyleSheet("background-color: #374151; padding: 16px; border-bottom: 1px solid #4b5563;");
    mainLayout->addWidget(headerWidget);

    // ======================================================================
    // Range Text Input Area
    // ======================================================================
    rangeText = new QTextEdit();
    rangeText->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1F2937;"
        "  color: #F9FAFB;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-family: monospace;"
        "}"
    );
    rangeText->setMaximumHeight(80);
    mainLayout->addWidget(rangeText);

    // ======================================================================
    // Slider Layout
    // ======================================================================
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderValueLabel = new QLabel(".000");
    sliderValueLabel->setStyleSheet("color: #9CA3AF; min-width: 50px;");
    
    rangeSlider = new QSlider(Qt::Horizontal);
    rangeSlider->setRange(0, 1000);
    rangeSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  background: #374151;"
        "  height: 4px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #3B82F6;"
        "  width: 16px;"
        "  margin: -6px 0;"
        "  border-radius: 8px;"
        "}"
    );
    connect(rangeSlider, &QSlider::valueChanged, this, &RangeSelector::onSliderValueChanged);
    
    sliderLayout->addWidget(sliderValueLabel);
    sliderLayout->addWidget(rangeSlider);
    mainLayout->addLayout(sliderLayout);

    // ======================================================================
    // Control Buttons Layout
    // ======================================================================
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    
    QString buttonStyle = 
        "QPushButton {"
        "  background-color: #1F2937;"
        "  color: #F9FAFB;"
        "  padding: 8px 16px;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #374151;"
        "}";

    QString primaryButtonStyle = 
        "QPushButton {"
        "  background-color: #3B82F6;"
        "  color: #FFFFFF;"
        "  padding: 8px 16px;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #2563EB;"
        "}";

    QPushButton *confirmButton = new QPushButton("Confirm");
    QPushButton *cancelButton = new QPushButton("Cancel");
    QPushButton *clearButton = new QPushButton("Clear range");
    QPushButton *exportButton = new QPushButton("Export Range");
    QPushButton *importButton = new QPushButton("Import Range");
    QPushButton *folderButton = new QPushButton("Open Range Folder");

    confirmButton->setStyleSheet(primaryButtonStyle);
    cancelButton->setStyleSheet(buttonStyle);
    clearButton->setStyleSheet(buttonStyle);
    exportButton->setStyleSheet(buttonStyle);
    importButton->setStyleSheet(buttonStyle);
    folderButton->setStyleSheet(buttonStyle);
    
    buttonLayout->addWidget(confirmButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(folderButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(confirmButton, &QPushButton::clicked, this, &RangeSelector::confirmSelection);
    connect(cancelButton, &QPushButton::clicked, this, &RangeSelector::reject);
    connect(clearButton, &QPushButton::clicked, this, &RangeSelector::clearRange);
    connect(exportButton, &QPushButton::clicked, this, &RangeSelector::exportRange);
    connect(importButton, &QPushButton::clicked, this, &RangeSelector::importRange);
    connect(folderButton, &QPushButton::clicked, this, &RangeSelector::openRangeFolder);

    // ======================================================================
    // Range Table
    // ======================================================================
    setupRangeTable();
    mainLayout->addWidget(rangeTable);

    // ======================================================================
    // Footer Area - a simple footer with an icon or symbol (chevron)
    // ======================================================================
    QWidget *footerWidget = new QWidget(this);
    QHBoxLayout *footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *footerLabel = new QLabel("/", footerWidget);
    footerLabel->setStyleSheet("color: #f3f4f6; font-size: 16px;");
    footerLayout->addStretch();
    footerLayout->addWidget(footerLabel);
    mainLayout->addWidget(footerWidget);
}

void RangeSelector::setupRangeTable() {
    // Create table with 13 rows and 13 columns (for hand range display)
    rangeTable = new QTableWidget(13, 13, this);
    QStringList ranks = {"A", "K", "Q", "J", "T", "9", "8", "7", "6", "5", "4", "3", "2"};
    rangeTable->setHorizontalHeaderLabels(ranks);
    rangeTable->setVerticalHeaderLabels(ranks);
    
    // Style the table
    rangeTable->setStyleSheet(
        "QTableWidget {"
        "  background-color: #1F2937;"
        "  gridline-color: #374151;"
        "  border: none;"
        "}"
        "QHeaderView::section {"
        "  background-color: #1F2937;"
        "  color: #9CA3AF;"
        "  border: none;"
        "  padding: 4px;"
        "}"
        "QTableWidget::item {"
        "  background-color: #374151;"
        "  color: #F9FAFB;"
        "}"
    );

    // Set up the cells
    for (int i = 0; i < 13; i++) {
        for (int j = 0; j < 13; j++) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            
            QString cellId = getCellId(i, j);
            item->setText(cellId);
            
            if (isPair(i, j)) {
                item->setBackground(QColor("#1F2937"));
            } else {
                item->setBackground(QColor("#374151"));
            }
            
            rangeTable->setItem(i, j, item);
        }
    }

    // Set fixed cell size
    const int cellSize = 48;
    for (int i = 0; i < 13; i++) {
        rangeTable->setColumnWidth(i, cellSize);
        rangeTable->setRowHeight(i, cellSize);
    }

    rangeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    rangeTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    
    connect(rangeTable, &QTableWidget::cellClicked, this, &RangeSelector::onCellClicked);
}

QString RangeSelector::getCellId(int row, int column) const {
    QString cellId;
    if (row == column) {
        cellId = ranks[row] + ranks[column];  // Pair
    } else if (row < column) {
        cellId = ranks[row] + ranks[column] + "s";  // Suited
    } else {
        cellId = ranks[column] + ranks[row] + "o";  // Offsuit
    }
    return cellId;
}

bool RangeSelector::isPair(int row, int column) const {
    return row == column;
}

bool RangeSelector::isSuited(int row, int column) const {
    return row < column;
}

void RangeSelector::onSliderValueChanged(int value) {
    sliderValueLabel->setText(QString(".%1").arg(value, 3, 10, QChar('0')));
}

void RangeSelector::onCellClicked(int row, int column) {
    QTableWidgetItem *item = rangeTable->item(row, column);
    if (item) {
        if (item->background().color() == QColor("#374151")) {
            item->setBackground(QColor("#F59E0B")); // Selected color
        } else {
            item->setBackground(QColor("#374151")); // Default color
        }
    }
}

void RangeSelector::applyStyles() {
    // Additional overall style for the dialog if needed
    setStyleSheet("QDialog { background-color: #1F2937; }");
}

void RangeSelector::confirmSelection() {
    // Accept the dialog (you can add further processing here)
    accept();
}

void RangeSelector::clearRange() {
    // Clear the text input and reset cell backgrounds in the table
    rangeText->clear();
    for (int i = 0; i < rangeTable->rowCount(); i++) {
        for (int j = 0; j < rangeTable->columnCount(); j++) {
            QTableWidgetItem *item = rangeTable->item(i, j);
            if (item)
                item->setBackground(QColor("#2d2d2d"));
        }
    }
}

void RangeSelector::exportRange() {
    // Export the range text (placeholder)
    QString range = rangeText->toPlainText();
    qDebug() << "Exporting range:" << range;
}

void RangeSelector::importRange() {
    // Import range (placeholder for file dialog or other logic)
    qDebug() << "Importing range...";
}

void RangeSelector::openRangeFolder() {
    // Placeholder for opening the folder containing range files
    qDebug() << "Opening range folder...";
}

void RangeSelector::updateRange() {
    // Placeholder for updating the table based on range text changes
    qDebug() << "Updating range...";
}

// ... Implement other methods ... 