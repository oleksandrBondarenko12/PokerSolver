#ifndef STRATEGYEXPLORER_H
#define STRATEGYEXPLORER_H

#include <QDialog>

// Forward declarations
class QTableWidget;
class QEvent;

class StrategyExplorer : public QDialog {
    Q_OBJECT

public:
    explicit StrategyExplorer(QWidget *parent = nullptr);
    ~StrategyExplorer();

protected:
    // Declare the event filter override so it can be defined out-of-line.
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();

    // Member variable for the hand matrix
    QTableWidget *handMatrix;
};

#endif // STRATEGYEXPLORER_H 