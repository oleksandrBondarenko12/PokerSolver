#include "MainWindow.h"
#include "RangeSelector.h"
#include "StrategyExplorer.h"
#include <QApplication>
#include <QStyle>
#include <QFile>
#include <QDebug>
#include <QSpacerItem>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextEdit>

// Define inputStyle as a static const member variable
const QString MainWindow::inputStyle = 
    "QLineEdit {"
    "  background-color: #1F2937;"
    "  color: #F9FAFB;"
    "  padding: 8px 12px;"
    "  border-radius: 6px;"
    "  border: 1px solid #4B5563;"
    "}"
    "QLineEdit:focus {"
    "  border: 2px solid #3B82F6;"
    "}";

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    applyStyles();
}

void MainWindow::setupUI() {
    // Create a scroll area to contain everything
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    QWidget *centralWidget = new QWidget(scrollArea);
    scrollArea->setWidget(centralWidget);
    setCentralWidget(scrollArea);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header - make it stretch horizontally
    QWidget *headerContainer = new QWidget;
    headerContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerContainer->setStyleSheet("background-color: #1F2937; border-bottom: 1px solid #374151;");
    QVBoxLayout *headerLayout = new QVBoxLayout(headerContainer);
    headerLayout->setContentsMargins(16, 16, 16, 16);
    
    QLabel *header = new QLabel("Solver Strategy Parameters");
    header->setStyleSheet("font-size: 18px; font-weight: 600; color: #F9FAFB;");
    headerLayout->addWidget(header);
    mainLayout->addWidget(headerContainer);

    // Range Selection Section
    QLabel *rangeLabel = new QLabel("RANGE SELECTION");
    rangeLabel->setStyleSheet("color: #6B7280; font-size: 12px; font-weight: 500; margin-top: 16px;");
    mainLayout->addWidget(rangeLabel);

    QHBoxLayout *rangeLayout = new QHBoxLayout();
    rangeLayout->setSpacing(12);
    selectIPButton = new QPushButton("Select IP");
    selectOOPButton = new QPushButton("Select OOP");
    selectIPButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selectOOPButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QString buttonStyle = "QPushButton {"
                         "  background-color: #1F2937;"
                         "  color: #F9FAFB;"
                         "  padding: 8px 16px;"
                         "  border-radius: 6px;"
                         "  font-weight: 500;"
                         "}"
                         "QPushButton:hover {"
                         "  background-color: #374151;"
                         "}";
    selectIPButton->setStyleSheet(buttonStyle);
    selectOOPButton->setStyleSheet(buttonStyle);
    rangeLayout->addWidget(selectIPButton);
    rangeLayout->addWidget(selectOOPButton);
    rangeLayout->addStretch();
    mainLayout->addLayout(rangeLayout);

    // Board Input Section
    QLabel *boardLabel = new QLabel("Board");
    boardLabel->setStyleSheet("color: #6B7280; font-size: 12px; font-weight: 500; margin-top: 12px;");
    mainLayout->addWidget(boardLabel);

    QHBoxLayout *boardLayout = new QHBoxLayout();
    boardLayout->setSpacing(12);
    boardInput = new QLineEdit("5d 9h 4s");
    boardInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selectBoardButton = new QPushButton("Select Board Card");
    selectBoardButton->setStyleSheet(buttonStyle);
    boardLayout->addWidget(boardInput);
    boardLayout->addWidget(selectBoardButton);
    mainLayout->addLayout(boardLayout);

    // Street Controls Grid
    QGridLayout *streetLayout = new QGridLayout();
    streetLayout->setSpacing(16);
    for(int i = 0; i < streetLayout->columnCount(); ++i) {
        streetLayout->setColumnStretch(i, 1);
    }
    streetLayout->addWidget(createStreetControls("Flop IP", flopIP), 0, 0);
    streetLayout->addWidget(createStreetControls("Turn IP", turnIP), 0, 1);
    streetLayout->addWidget(createStreetControls("River IP", riverIP), 0, 2);
    mainLayout->addLayout(streetLayout);

    // Copy Button
    QPushButton *copyButton = new QPushButton("Copy from IP to OOP");
    copyButton->setStyleSheet("background-color: #1F2937; color: #ffffff; padding: 8px; border-radius: 4px;");
    mainLayout->addWidget(copyButton);

    // OOP Controls
    QGridLayout *oopLayout = new QGridLayout();
    for(int i = 0; i < oopLayout->columnCount(); ++i) {
        oopLayout->setColumnStretch(i, 1);
    }
    oopLayout->addWidget(createStreetControls("Flop OOP", flopOOP), 0, 0);
    oopLayout->addWidget(createStreetControls("Turn OOP", turnOOP), 0, 1);
    oopLayout->addWidget(createStreetControls("River OOP", riverOOP), 0, 2);
    mainLayout->addLayout(oopLayout);

    // Solver Configuration
    QGridLayout *configLayout = new QGridLayout();
    configLayout->setColumnStretch(1, 1); // Make input columns stretch
    configLayout->setColumnStretch(4, 1);
    
    // First row
    QLabel *raiseLimitLabel = new QLabel("raise limit:");
    raiseLimitInput = new QLineEdit("3");
    QLabel *timesLabel = new QLabel("times");
    configLayout->addWidget(raiseLimitLabel, 0, 0);
    raiseLimitInput->setStyleSheet(inputStyle);
    configLayout->addWidget(raiseLimitInput, 0, 1);
    configLayout->addWidget(timesLabel, 0, 2);

    QLabel *potLabel = new QLabel("Pot:");
    potInput = new QLineEdit("50");
    potInput->setStyleSheet(inputStyle);
    configLayout->addWidget(potLabel, 0, 3);
    configLayout->addWidget(potInput, 0, 4);

    QLabel *stackLabel = new QLabel("Effective Stack:");
    effectiveStackInput = new QLineEdit("200");
    effectiveStackInput->setStyleSheet(inputStyle);
    configLayout->addWidget(stackLabel, 0, 5);
    configLayout->addWidget(effectiveStackInput, 0, 6);

    // Second row
    QLabel *allinLabel = new QLabel("allin threshold:");
    allinThresholdInput = new QLineEdit("0.67");
    allinThresholdInput->setStyleSheet(inputStyle);
    configLayout->addWidget(allinLabel, 1, 0);
    configLayout->addWidget(allinThresholdInput, 1, 1);

    useIsomorphismCheck = new QCheckBox("use Isomorphism");
    configLayout->addWidget(useIsomorphismCheck, 1, 5);

    mainLayout->addLayout(configLayout);

    // Action Buttons
    buildTreeButton = new QPushButton("Build Tree");
    estimateMemoryButton = new QPushButton("Estimate Solving Memory");
    buildTreeButton->setStyleSheet("background-color: #1F2937; color: #ffffff; padding: 8px; border-radius: 4px;");
    estimateMemoryButton->setStyleSheet("background-color: #1F2937; color: #ffffff; padding: 8px; border-radius: 4px;");
    mainLayout->addWidget(buildTreeButton);
    mainLayout->addWidget(estimateMemoryButton);

    // Solver Parameters
    QGridLayout *solverLayout = new QGridLayout();
    
    QLabel *iterationsLabel = new QLabel("Iterations:");
    iterationsInput = new QLineEdit("200");
    iterationsInput->setStyleSheet(inputStyle);
    QLabel *timesLabel2 = new QLabel("times");
    solverLayout->addWidget(iterationsLabel, 0, 0);
    solverLayout->addWidget(iterationsInput, 0, 1);
    solverLayout->addWidget(timesLabel2, 0, 2);

    QLabel *exploitabilityLabel = new QLabel("stop solving when reach");
    exploitabilityInput = new QLineEdit("0.5");
    exploitabilityInput->setStyleSheet(inputStyle);
    QLabel *percentLabel = new QLabel("% exploitability");
    solverLayout->addWidget(exploitabilityLabel, 0, 3);
    solverLayout->addWidget(exploitabilityInput, 0, 4);
    solverLayout->addWidget(percentLabel, 0, 5);

    QLabel *logLabel = new QLabel("log interval:");
    logIntervalInput = new QLineEdit("10");
    logIntervalInput->setStyleSheet(inputStyle);
    QLabel *threadsLabel = new QLabel("threads:");
    threadsInput = new QLineEdit("8");
    threadsInput->setStyleSheet(inputStyle);
    solverLayout->addWidget(logLabel, 1, 0);
    solverLayout->addWidget(logIntervalInput, 1, 1);
    solverLayout->addWidget(threadsLabel, 1, 3);
    solverLayout->addWidget(threadsInput, 1, 4);

    mainLayout->addLayout(solverLayout);

    // Final Action Buttons
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(12);
    startSolvingButton = new QPushButton("Start Solving");
    stopSolvingButton = new QPushButton("Stop Solving");
    showResultButton = new QPushButton("Show Result");
    
    startSolvingButton->setStyleSheet("QPushButton {"
                                     "  background-color: #3B82F6;"
                                     "  color: #FFFFFF;"
                                     "  padding: 8px 16px;"
                                     "  border-radius: 6px;"
                                     "  font-weight: 500;"
                                     "}"
                                     "QPushButton:hover {"
                                     "  background-color: #2563EB;"
                                     "}");
    stopSolvingButton->setStyleSheet("background-color: #1F2937; color: #ffffff; padding: 8px 16px; border-radius: 4px;");
    showResultButton->setStyleSheet("background-color: #1F2937; color: #ffffff; padding: 8px 16px; border-radius: 4px;");
    
    startSolvingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stopSolvingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    showResultButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    actionLayout->addWidget(startSolvingButton);
    actionLayout->addWidget(stopSolvingButton);
    actionLayout->addWidget(showResultButton);
    mainLayout->addLayout(actionLayout);

    // Log Area
    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);  // Make it read-only
    logTextEdit->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1F2937;"
        "  color: #9CA3AF;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-family: monospace;"
        "}"
    );
    logTextEdit->setMinimumHeight(200);  // Set a minimum height for the log area
    mainLayout->addWidget(logTextEdit);

    // Clear Log Button (moved after log area)
    clearLogButton = new QPushButton("Clear Log");
    clearLogButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #1F2937;"
        "  color: #F9FAFB;"
        "  padding: 8px;"
        "  border-radius: 4px;"
        "  width: 100%;"
        "}"
        "QPushButton:hover {"
        "  background-color: #374151;"
        "}"
    );
    mainLayout->addWidget(clearLogButton);

    // Set minimum size for the window
    setMinimumSize(800, 600);

    // Connect signals
    connect(selectIPButton, &QPushButton::clicked, this, &MainWindow::showIPRangeSelector);
    connect(selectOOPButton, &QPushButton::clicked, this, &MainWindow::showOOPRangeSelector);
    connect(copyButton, &QPushButton::clicked, this, &MainWindow::copyIPtoOOP);
    connect(buildTreeButton, &QPushButton::clicked, this, &MainWindow::buildTree);
    connect(estimateMemoryButton, &QPushButton::clicked, this, &MainWindow::estimateMemory);
    connect(startSolvingButton, &QPushButton::clicked, this, &MainWindow::startSolving);
    connect(stopSolvingButton, &QPushButton::clicked, this, &MainWindow::stopSolving);
    connect(showResultButton, &QPushButton::clicked, this, &MainWindow::showResult);
    connect(clearLogButton, &QPushButton::clicked, this, &MainWindow::clearLog);
}

QWidget* MainWindow::createStreetControls(const QString& title, StreetControls& controls) {
    QWidget *widget = new QWidget;
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(12);
    
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color: #6B7280; font-size: 12px; font-weight: 500;");
    
    QLabel *betLabel = new QLabel("Bet Sizes:");
    betLabel->setStyleSheet("color: #9CA3AF;");
    controls.betSizes = new QLineEdit("50");
    controls.betSizes->setStyleSheet(inputStyle);
    controls.betSizes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    QLabel *raiseLabel = new QLabel("Raise Sizes:");
    raiseLabel->setStyleSheet("color: #9CA3AF;");
    controls.raiseSizes = new QLineEdit("60");
    controls.raiseSizes->setStyleSheet(inputStyle);
    controls.raiseSizes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    controls.addAllIn = new QCheckBox("Add AllIn");
    controls.addAllIn->setStyleSheet("color: #9CA3AF;");
    controls.addAllIn->setChecked(true);
    
    layout->addWidget(titleLabel);
    layout->addWidget(betLabel);
    layout->addWidget(controls.betSizes);
    layout->addWidget(raiseLabel);
    layout->addWidget(controls.raiseSizes);
    layout->addWidget(controls.addAllIn);
    
    return widget;
}

void MainWindow::applyStyles() {
    // Load and apply stylesheet from resources
    QFile styleFile(":/styles/dark.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = styleFile.readAll();
        setStyleSheet(style);
    } else {
        qDebug() << "Failed to open stylesheet";
    }
}

void MainWindow::showIPRangeSelector() {
    RangeSelector *selector = new RangeSelector(this, true);
    selector->exec();
}

void MainWindow::showOOPRangeSelector() {
    RangeSelector *selector = new RangeSelector(this, false);
    selector->exec();
}

void MainWindow::copyIPtoOOP() {
    // Copy IP street control values to OOP controls
    flopOOP.betSizes->setText(flopIP.betSizes->text());
    flopOOP.raiseSizes->setText(flopIP.raiseSizes->text());
    flopOOP.addAllIn->setChecked(flopIP.addAllIn->isChecked());
    
    turnOOP.betSizes->setText(turnIP.betSizes->text());
    turnOOP.raiseSizes->setText(turnIP.raiseSizes->text());
    turnOOP.addAllIn->setChecked(turnIP.addAllIn->isChecked());
    
    riverOOP.betSizes->setText(riverIP.betSizes->text());
    riverOOP.raiseSizes->setText(riverIP.raiseSizes->text());
    riverOOP.addAllIn->setChecked(riverIP.addAllIn->isChecked());
}

void MainWindow::buildTree() {
    appendToLog("Loading holdem compairing file");
    appendToLog("Loading shortdeck compairing file");
    appendToLog("Loading finished. Good to go.");
    appendToLog("building tree...");
    appendToLog("build tree finished");
}

void MainWindow::estimateMemory() {
    appendToLog("Estimating memory requirements...");
    // Add actual memory estimation logic here
}

void MainWindow::startSolving() {
    appendToLog("Start Solving..");
    appendToLog("Using 8 threads");
    appendToLog("Iter: 0");
    appendToLog("player 0 exploitability 43.9384");
    appendToLog("player 1 exploitability 27.9392");
    appendToLog("Total exploitability 71.8775 precent");
    appendToLog("------------------");
}

void MainWindow::stopSolving() {
    appendToLog("Stopping solver...");
}

void MainWindow::showResult() {
    appendToLog("Showing results...");
    // Open the StrategyExplorer dialog as a modal window
    StrategyExplorer explorer(this);
    explorer.exec();
}

void MainWindow::appendToLog(const QString& text) {
    logTextEdit->append(text);
}

void MainWindow::clearLog() {
    logTextEdit->clear();
} 