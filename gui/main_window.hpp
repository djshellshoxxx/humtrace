// Purpose: HumTrace main window. Analysis runs on a worker thread; the UI only
// displays results and calls the UI-independent core in humtrace/analysis.hpp.
#pragma once

#include "humtrace/analysis.hpp"
#include "timeline_widget.hpp"

#include <QMainWindow>
#include <atomic>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QThread;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void openFile(const QString& path);          // starts background analysis
    bool analysisRunning() const { return running_; }
    int tableRowCount() const;
    QString statusText() const;
    const humtrace::AnalysisReport* report() const { return report_.get(); }
    bool exportJson(const QString& path);        // returns false and shows message on error
signals:
    void analysisFinished(bool success);
public slots:
    void cancelAnalysis();
private slots:
    void chooseFile();
    void exportReport();
    void onProgress(int percent);
    void onDone(bool ok, const QString& message);
    void selectChannel(int index);
    void selectFrame(int frame);
private:
    void setBusy(bool busy);
    void populateTable();
    QString path_;
    std::unique_ptr<humtrace::AnalysisReport> report_;
    std::atomic<bool> cancel_{false};
    bool running_{false};
    QThread* thread_{nullptr};
    quint64 generation_{0};
    QPushButton *openButton_, *cancelButton_, *exportButton_;
    QComboBox* channelBox_;
    QDoubleSpinBox *thresholdSpin_, *prominenceSpin_;
    QComboBox* frameSizeBox_;
    QProgressBar* progress_;
    QLabel* summary_;
    QTableWidget* table_;
    TimelineWidget* timeline_;
    SpectrumWidget* spectrum_;
};
