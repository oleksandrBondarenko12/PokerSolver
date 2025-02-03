#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include "RangeSelector.h"
#include <QTextEdit>

struct StreetControls {
    QLineEdit *betSizes;
    QLineEdit *raiseSizes;
    QCheckBox *addAllIn;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void showIPRangeSelector();
    void showOOPRangeSelector();
    void copyIPtoOOP();
    void buildTree();
    void estimateMemory();
    void startSolving();
    void stopSolving();
    void showResult();
    void clearLog();
    void appendToLog(const QString& text);

private:
    static const QString inputStyle;
    
    // Range Selection
    QPushButton *selectIPButton;
    QPushButton *selectOOPButton;
    
    // Board Input
    QLineEdit *boardInput;
    QPushButton *selectBoardButton;
    
    // Street Controls
    StreetControls flopIP;
    StreetControls turnIP;
    StreetControls riverIP;
    StreetControls flopOOP;
    StreetControls turnOOP;
    StreetControls riverOOP;
    
    // Solver Configuration
    QLineEdit *raiseLimitInput;
    QLineEdit *potInput;
    QLineEdit *effectiveStackInput;
    QLineEdit *allinThresholdInput;
    QCheckBox *useIsomorphismCheck;
    
    // Solver Parameters
    QLineEdit *iterationsInput;
    QLineEdit *exploitabilityInput;
    QLineEdit *logIntervalInput;
    QLineEdit *threadsInput;
    
    // Action Buttons
    QPushButton *startSolvingButton;
    QPushButton *stopSolvingButton;
    QPushButton *showResultButton;
    QPushButton *clearLogButton;
    QPushButton *buildTreeButton;
    QPushButton *estimateMemoryButton;
    
    // Log Area
    QTextEdit *logTextEdit;
    
    void setupUI();
    QWidget* createStreetControls(const QString& title, StreetControls& controls);
    void applyStyles();
};

#endif // MAINWINDOW_H 