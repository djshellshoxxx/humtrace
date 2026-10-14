#pragma once

#include "../presentation/findings.hpp"
#include "../presentation/job_state.hpp"
#include "humtrace/pipeline.hpp"
#include "humtrace/session.hpp"

#include <QMainWindow>
#include <functional>
#include <memory>

class AnalysisWorker;
class FindingsModel;
class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QSortFilterProxyModel;
class QTableView;
class QTableWidget;
class QTextBrowser;
class SpectrogramChart;
class SpectrumChart;
class ViewState;
class WaveformChart;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Test/automation hooks (the same code paths the UI actions use).
    void openRecording(const QString& path);
    void startAnalysis();
    [[nodiscard]] humtrace::ui::JobState state() const { return state_; }
    [[nodiscard]] const humtrace::AnalysisReport* report() const { return result_ ? &result_->report : nullptr; }
    [[nodiscard]] ViewState* viewState() const { return view_; }
    [[nodiscard]] FindingsModel* findingsModel() const { return model_; }
    [[nodiscard]] QTableView* findingsTable() const { return table_; }
    // Replace the modal error dialog (used by automated tests). Receives (stage, message).
    void setErrorHandler(std::function<void(const QString&, const QString&)> handler) { errorHandler_ = std::move(handler); }
    // Programmatic equivalents of the settings dialog / display controls (also used by tests).
    void setPipelineSettings(const humtrace::PipelineSettings& settings);
    void setDisplayOptions(int scaleIndex, int paletteIndex, double floorDb, double ceilingDb);
    [[nodiscard]] QString exportReport(const QString& path, bool redactPath);

protected:
    void closeEvent(QCloseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private slots:
    void chooseAndOpen();
    void cancelAnalysis();
    void chooseAndExport();
    void chooseAndCompare();
    void showSettings();
    void showHelp();
    void onFindingSelected();
    void onCursorChanged();
    void stepFinding(int delta);
    void applyDisplayOptions();     // debounced (spin boxes)
    void applyDisplayOptionsNow();

private:
    void setState(humtrace::ui::JobState next);
    void buildUi();
    void buildActions();
    void updateActions();
    void onFinished(std::shared_ptr<humtrace::AnalysisResult> result);
    void showError(const QString& stage, const QString& message);
    void addRecent(const QString& path);
    void loadRecent();
    void clearResults();
    void releaseWorker();
    int currentFrame() const;
    void updateFrameViews();

    humtrace::ui::JobState state_{humtrace::ui::JobState::Empty};
    QString path_;
    humtrace::PipelineSettings settings_;
    std::unique_ptr<humtrace::AnalysisResult> result_;
    std::vector<humtrace::ui::FindingRow> findings_;
    AnalysisWorker* worker_{nullptr};
    bool compareBusy_{false};   // a comparison's nested event loop is running
    int restoreChannel_{0};     // channel to re-select after re-analysis
    class QTimer* displayTimer_{nullptr};
    std::function<void(const QString&, const QString&)> errorHandler_;

    ViewState* view_;
    WaveformChart* waveform_{nullptr};
    SpectrogramChart* spectrogram_{nullptr};
    SpectrumChart* spectrum_{nullptr};
    QTableWidget* frameTable_{nullptr};
    FindingsModel* model_{nullptr};
    QSortFilterProxyModel* proxy_{nullptr};
    QTableView* table_{nullptr};
    QTextBrowser* inspector_{nullptr};
    QLabel* header_{nullptr};
    QLabel* summary_{nullptr};
    QLabel* stateLabel_{nullptr};
    QProgressBar* progress_{nullptr};
    QListWidget* channels_{nullptr};
    QListWidget* recent_{nullptr};
    QComboBox* scaleBox_{nullptr};
    QComboBox* paletteBox_{nullptr};
    QDoubleSpinBox* floorBox_{nullptr};
    QDoubleSpinBox* ceilingBox_{nullptr};
    QAction* openAct_{nullptr};
    QAction* analyzeAct_{nullptr};
    QAction* cancelAct_{nullptr};
    QAction* compareAct_{nullptr};
    QAction* exportAct_{nullptr};
    QAction* settingsAct_{nullptr};
    QAction* nextAct_{nullptr};
    QAction* prevAct_{nullptr};
    QAction* contrastAct_{nullptr};
};
