// Purpose: HumTrace main window. Analysis runs on a worker thread; the UI only
// displays results and calls the UI-independent core in humtrace/analysis.hpp.
#pragma once

#include "humtrace/analysis.hpp"
#include "display_widgets.hpp"
#include "humtrace/compare.hpp"
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
class QTabWidget;
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
    int trackRowCount() const;
    const WaveformWidget* waveformView() const { return waveform_; }
    const SpectrogramWidget* spectrogramView() const { return spectrogram_; }
    QTabWidget* viewTabs() const { return viewTabs_; }
    void selectTrackRow(int row);
    // Analyzes >= 2 files on a worker thread (no display products) and emits
    // compareFinished. Shows no dialog; chooseCompareFiles() adds the dialog.
    void compareFiles(const QStringList& paths);
    QString comparisonText() const { return comparisonText_; }
    // Writes the last comparison as JSON; refuses any path equal to an input.
    bool exportComparisonJson(const QString& path);
signals:
    void analysisFinished(bool success);
    void compareFinished(bool success);
public slots:
    void cancelAnalysis();
private slots:
    void chooseFile();
    void chooseCompareFiles();
    void showComparisonDialog();
    void exportReport();
    void onProgress(int percent);
    void onDone(bool ok, const QString& message);
    void selectChannel(int index);
    void selectFrame(int frame);
private:
    void setBusy(bool busy);
    void populateTable();
    void populateTracks();
    void onCompareDone(bool ok, const QString& message);
    QString path_;
    std::unique_ptr<humtrace::AnalysisReport> report_;
    std::atomic<bool> cancel_{false};
    bool running_{false};
    QThread* thread_{nullptr};
    std::vector<humtrace::FileSummary> comparison_;
    QStringList comparePaths_;
    QString comparisonText_;
    bool showCompareDialog_{false};
    bool comparing_{false};
    quint64 generation_{0};
    QPushButton *openButton_, *cancelButton_, *exportButton_, *compareButton_;
    QComboBox* channelBox_;
    QDoubleSpinBox *thresholdSpin_, *prominenceSpin_;
    QComboBox* frameSizeBox_;
    QProgressBar* progress_;
    QLabel* summary_;
    QTableWidget* table_;
    QTableWidget* tracksTable_;
    QTabWidget *viewTabs_, *tableTabs_;
    WaveformWidget* waveform_;
    SpectrogramWidget* spectrogram_;
    TimelineWidget* timeline_;
    SpectrumWidget* spectrum_;
};
