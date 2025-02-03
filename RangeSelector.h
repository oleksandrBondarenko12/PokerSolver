#ifndef RANGESELECTOR_H
#define RANGESELECTOR_H

#include <QDialog>
#include <QTableWidget>
#include <QTextEdit>
#include <QSlider>
#include <QPushButton>
#include <QLabel>

class RangeSelector : public QDialog {
    Q_OBJECT

public:
    RangeSelector(QWidget *parent = nullptr, bool isIP = true);

private slots:
    void confirmSelection();
    void clearRange();
    void exportRange();
    void importRange();
    void openRangeFolder();
    void updateRange();
    void onSliderValueChanged(int value);
    void onCellClicked(int row, int column);

private:
    QTextEdit *rangeText;
    QSlider *rangeSlider;
    QTableWidget *rangeTable;
    QLabel *sliderValueLabel;
    
    // Store the ranks for easy access
    const QStringList ranks = {"A", "K", "Q", "J", "T", "9", "8", "7", "6", "5", "4", "3", "2"};
    
    void setupUI();
    void setupRangeTable();
    void applyStyles();
    QString getCellId(int row, int column) const;
    bool isPair(int row, int column) const;
    bool isSuited(int row, int column) const;
};

#endif // RANGESELECTOR_H 