#include "StrategyExplorer.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QStringList>
#include <QDebug>
#include <QBrush>
#include <QColor>
#include <QEvent>

StrategyExplorer::StrategyExplorer(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Strategy Explorer");
    setMinimumSize(800, 600);
    setupUI();
}

StrategyExplorer::~StrategyExplorer() {}

void StrategyExplorer::setupUI() {
    // Main grid layout: 2 columns (left, right) similar to HTML grid-cols-[1fr,1.2fr]
    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->setHorizontalSpacing(20);
    mainLayout->setVerticalSpacing(20);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setColumnStretch(1, 12); // using 12 to simulate 1.2 fraction (scale factor)

    // ============================
    // LEFT COLUMN
    // ============================
    QWidget *leftWidget = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setSpacing(20);

    // --- Game Tree Panel ---
    QFrame *gameTreeFrame = new QFrame;
    gameTreeFrame->setFrameShape(QFrame::StyledPanel);
    gameTreeFrame->setStyleSheet("background-color: #374151; border-radius: 8px; padding: 8px;");
    QVBoxLayout *gameTreeLayout = new QVBoxLayout(gameTreeFrame);
    QLabel *gameTreeTitle = new QLabel("Game Tree");
    gameTreeTitle->setStyleSheet("font-size: 12px; margin-bottom: 8px;");
    gameTreeLayout->addWidget(gameTreeTitle);
    // "⊟ FLOP begin" label
    QLabel *flopBeginLabel = new QLabel("⊟ FLOP begin");
    flopBeginLabel->setStyleSheet("background-color: #2563EB; padding: 4px;");
    gameTreeLayout->addWidget(flopBeginLabel);
    // OOP strategies list with left margin
    QWidget *oopStrategies = new QWidget;
    QVBoxLayout *oopLayout = new QVBoxLayout(oopStrategies);
    oopLayout->setContentsMargins(20, 0, 0, 0); // mimic ml-4
    oopLayout->setSpacing(4);
    oopLayout->addWidget(new QLabel("⊞ OOP CHECK"));
    oopLayout->addWidget(new QLabel("⊞ OOP BET 25"));
    oopLayout->addWidget(new QLabel("⊞ OOP BET 200"));
    gameTreeLayout->addWidget(oopStrategies);
    leftLayout->addWidget(gameTreeFrame);

    // --- Card Selectors ---
    QWidget *cardSelectorsWidget = new QWidget;
    QGridLayout *cardSelectorsLayout = new QGridLayout(cardSelectorsWidget);
    cardSelectorsLayout->setHorizontalSpacing(10);
    cardSelectorsLayout->setVerticalSpacing(10);
    // Turn card
    QLabel *turnLabel = new QLabel("Turn card:");
    turnLabel->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardSelectorsLayout->addWidget(turnLabel, 0, 0);
    QFrame *turnSelector = new QFrame;
    turnSelector->setFrameShape(QFrame::StyledPanel);
    turnSelector->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
    QHBoxLayout *turnSelectorLayout = new QHBoxLayout(turnSelector);
    turnSelectorLayout->setContentsMargins(8, 4, 8, 4);
    QLabel *turnCardLabel = new QLabel("2♠");
    QLabel *turnArrow = new QLabel("▼");
    turnArrow->setStyleSheet("color: #9CA3AF;");
    turnSelectorLayout->addWidget(turnCardLabel);
    turnSelectorLayout->addStretch();
    turnSelectorLayout->addWidget(turnArrow);
    cardSelectorsLayout->addWidget(turnSelector, 1, 0);
    // River card
    QLabel *riverLabel = new QLabel("River card:");
    riverLabel->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardSelectorsLayout->addWidget(riverLabel, 0, 1);
    QFrame *riverSelector = new QFrame;
    riverSelector->setFrameShape(QFrame::StyledPanel);
    riverSelector->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
    QHBoxLayout *riverSelectorLayout = new QHBoxLayout(riverSelector);
    riverSelectorLayout->setContentsMargins(8, 4, 8, 4);
    QLabel *riverCardLabel = new QLabel("2♦");
    QLabel *riverArrow = new QLabel("▼");
    riverArrow->setStyleSheet("color: #9CA3AF;");
    riverSelectorLayout->addWidget(riverCardLabel);
    riverSelectorLayout->addStretch();
    riverSelectorLayout->addWidget(riverArrow);
    cardSelectorsLayout->addWidget(riverSelector, 1, 1);
    leftLayout->addWidget(cardSelectorsWidget);

    // --- Hand Matrix ---
    handMatrix = new QTableWidget(13, 13);
    handMatrix->horizontalHeader()->setVisible(false);
    handMatrix->verticalHeader()->setVisible(false);
    handMatrix->setEditTriggers(QAbstractItemView::NoEditTriggers);
    handMatrix->setSelectionMode(QAbstractItemView::NoSelection);
    handMatrix->setFocusPolicy(Qt::NoFocus);

    // Set overall table styling to match bg-gray-700 (#374151)
    handMatrix->setStyleSheet("QTableWidget { background-color: #374151; }");

    QStringList ranks = {"A", "K", "Q", "J", "T", "9", "8", "7", "6", "5", "4", "3", "2"};
    for (int row = 0; row < 13; ++row) {
        for (int col = 0; col < 13; ++col) {
            QString hand;
            if (row == col) {
                hand = ranks[row] + ranks[col];
            } else if (row < col) {
                hand = ranks[row] + ranks[col] + "s";
            } else {
                hand = ranks[col] + ranks[row] + "o";
            }
            QTableWidgetItem *item = new QTableWidgetItem(hand);
            item->setTextAlignment(Qt::AlignCenter);
            // Base background: bg-gray-600 (#4B5563)
            QColor bgColor = QColor("#4B5563");
            // Special cells get red-500 (#EF4444)
            if (hand == "22" || hand == "K2s") {
                bgColor = QColor("#EF4444");
            }
            item->setBackground(QBrush(bgColor));
            item->setForeground(QBrush(QColor("#ffffff")));
            handMatrix->setItem(row, col, item);
        }
    }

    // Instead of setting a fixed size (48 pixels), install an event filter
    // so that the cell size is computed based on the available width.
    handMatrix->installEventFilter(this);
    // Optionally, set a minimum size so the grid is not too small
    handMatrix->setMinimumWidth(300);

    leftLayout->addWidget(handMatrix);

    mainLayout->addWidget(leftWidget, 0, 0);

    // ============================
    // RIGHT COLUMN
    // ============================
    QWidget *rightWidget = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(20);

    // --- Hand Strategy Grid ---
    QWidget *handStrategyWidget = new QWidget;
    QGridLayout *handStrategyLayout = new QGridLayout(handStrategyWidget);
    handStrategyLayout->setSpacing(5);
    // Create 4 strategy boxes (static examples)
    QStringList handNames = {"A♣7♣", "A♣7♥", "A♣7♦", "A♣7♠"};
    for (int i = 0; i < 4; ++i) {
        QFrame *strategyBox = new QFrame;
        strategyBox->setFrameShape(QFrame::StyledPanel);
        strategyBox->setStyleSheet("background-color: #10B981; border-radius: 4px; padding: 4px;");
        QVBoxLayout *boxLayout = new QVBoxLayout(strategyBox);
        QLabel *handLabel = new QLabel(handNames[i]);
        handLabel->setStyleSheet("margin-bottom: 4px; font-size: 10px;");
        boxLayout->addWidget(handLabel);
        boxLayout->addWidget(new QLabel("CHECK: 100.0%"));
        boxLayout->addWidget(new QLabel("BET 25: 0.0%"));
        boxLayout->addWidget(new QLabel("BET 200: 0.0%"));
        handStrategyLayout->addWidget(strategyBox, 0, i);
    }
    rightLayout->addWidget(handStrategyWidget);

    // --- Rough Strategy, Board Info & Range Controls ---
    QWidget *miscWidget = new QWidget;
    QVBoxLayout *miscLayout = new QVBoxLayout(miscWidget);
    miscLayout->setSpacing(10);

    // Rough Strategy Title
    QLabel *roughStrategyTitle = new QLabel("Rough Strategy");
    roughStrategyTitle->setStyleSheet("font-size: 12px; margin-bottom: 4px;");
    miscLayout->addWidget(roughStrategyTitle);

    // Rough Strategy Grid (3 cells)
    QWidget *roughStrategyWidget = new QWidget;
    QGridLayout *roughStrategyLayout = new QGridLayout(roughStrategyWidget);
    roughStrategyLayout->setSpacing(5);
    {
        // Cell 1: CHECK
        QFrame *cell = new QFrame;
        cell->setFrameShape(QFrame::StyledPanel);
        cell->setStyleSheet("background-color: #10B981; padding: 4px; border-radius: 4px;");
        QVBoxLayout *cellLayout = new QVBoxLayout(cell);
        QLabel *cellTitle = new QLabel("CHECK");
        cellTitle->setStyleSheet("font-weight: bold;");
        cellLayout->addWidget(cellTitle);
        cellLayout->addWidget(new QLabel("98.9%"));
        cellLayout->addWidget(new QLabel("273.2 combos"));
        roughStrategyLayout->addWidget(cell, 0, 0);
    }
    {
        // Cell 2: BET 25.0
        QFrame *cell = new QFrame;
        cell->setFrameShape(QFrame::StyledPanel);
        cell->setStyleSheet("background-color: #F87171; padding: 4px; border-radius: 4px;");
        QVBoxLayout *cellLayout = new QVBoxLayout(cell);
        QLabel *cellTitle = new QLabel("BET 25.0");
        cellTitle->setStyleSheet("font-weight: bold;");
        cellLayout->addWidget(cellTitle);
        cellLayout->addWidget(new QLabel("1.1%"));
        cellLayout->addWidget(new QLabel("3.0 combos"));
        roughStrategyLayout->addWidget(cell, 0, 1);
    }
    {
        // Cell 3: BET 200.0
        QFrame *cell = new QFrame;
        cell->setFrameShape(QFrame::StyledPanel);
        cell->setStyleSheet("background-color: #F87171; padding: 4px; border-radius: 4px;");
        QVBoxLayout *cellLayout = new QVBoxLayout(cell);
        QLabel *cellTitle = new QLabel("BET 200.0");
        cellTitle->setStyleSheet("font-weight: bold;");
        cellLayout->addWidget(cellTitle);
        cellLayout->addWidget(new QLabel("0.0%"));
        cellLayout->addWidget(new QLabel("0.0 combos"));
        roughStrategyLayout->addWidget(cell, 0, 2);
    }
    miscLayout->addWidget(roughStrategyWidget);

    // Board Info
    QWidget *boardInfoWidget = new QWidget;
    QVBoxLayout *boardInfoLayout = new QVBoxLayout(boardInfoWidget);
    QLabel *boardLabel = new QLabel("board: Q♠ J♥ 2♥");
    boardLabel->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    QLabel *decisionNodeLabel = new QLabel("OOP decision node");
    decisionNodeLabel->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    boardInfoLayout->addWidget(boardLabel);
    boardInfoLayout->addWidget(decisionNodeLabel);
    miscLayout->addWidget(boardInfoWidget);

    // Range & Strategy Controls
    QWidget *rangeStrategyWidget = new QWidget;
    QGridLayout *rangeStrategyLayout = new QGridLayout(rangeStrategyWidget);
    rangeStrategyLayout->setSpacing(10);
    {
        // Left: Range controls
        QVBoxLayout *leftRangeLayout = new QVBoxLayout;
        QLabel *rangeTitle = new QLabel("Range:");
        rangeTitle->setStyleSheet("color: #9CA3AF; font-size: 12px; margin-bottom: 4px;");
        leftRangeLayout->addWidget(rangeTitle);
        QPushButton *ipButton = new QPushButton("IP");
        QPushButton *oopButton = new QPushButton("OOP");
        ipButton->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
        oopButton->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
        leftRangeLayout->addWidget(ipButton);
        leftRangeLayout->addWidget(oopButton);
        rangeStrategyLayout->addLayout(leftRangeLayout, 0, 0);
    }
    {
        // Right: Strategy & EV controls
        QVBoxLayout *rightStrategyLayout = new QVBoxLayout;
        QLabel *strategyTitle = new QLabel("Strategy & EVs:");
        strategyTitle->setStyleSheet("color: #9CA3AF; font-size: 12px; margin-bottom: 4px;");
        rightStrategyLayout->addWidget(strategyTitle);
        QPushButton *strategyButton = new QPushButton("strategy");
        QPushButton *evStrategyButton = new QPushButton("EV + strategy");
        QPushButton *evButton = new QPushButton("EV");
        strategyButton->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
        evStrategyButton->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
        evButton->setStyleSheet("background-color: #374151; border-radius: 4px; padding: 4px;");
        rightStrategyLayout->addWidget(strategyButton);
        rightStrategyLayout->addWidget(evStrategyButton);
        rightStrategyLayout->addWidget(evButton);
        rangeStrategyLayout->addLayout(rightStrategyLayout, 0, 1);
    }
    miscLayout->addWidget(rangeStrategyWidget);

    rightLayout->addWidget(miscWidget);

    mainLayout->addWidget(rightWidget, 0, 1);

    setLayout(mainLayout);
}

bool StrategyExplorer::eventFilter(QObject *obj, QEvent *event)
{
    // If the event is a resize event on the hand matrix, recalc cell sizes
    if (obj == handMatrix && event->type() == QEvent::Resize) {
        // Get available width from the viewport (cells area)
        int newWidth = handMatrix->viewport()->width();
        // Compute cell size by dividing the available width by 13 columns
        int cellSize = newWidth / 13;
        // Apply new size to each column and row
        for (int i = 0; i < 13; ++i) {
            handMatrix->setColumnWidth(i, cellSize);
            handMatrix->setRowHeight(i, cellSize);
        }
    }
    // Pass the event on to the base class
    return QDialog::eventFilter(obj, event);
} 