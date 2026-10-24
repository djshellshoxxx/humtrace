#pragma once

#include "../presentation/findings.hpp"
#include "TimeChart.hpp"
#include "humtrace/view.hpp"

#include <QImage>
#include <vector>
#include "PagedData.hpp"

class SpectrogramChart : public TimeChart {
    Q_OBJECT
public:
    explicit SpectrogramChart(ViewState* state, QWidget* parent = nullptr);
    void setData(const std::vector<humtrace::ChannelView>* views, const humtrace::AnalysisReport* report,
                 const std::vector<humtrace::ui::FindingRow>* findings);
    void setPagedData(PagedData* data, const std::vector<humtrace::ChannelView>* views,
                      const humtrace::AnalysisReport* report, const std::vector<humtrace::ui::FindingRow>* findings);
    [[nodiscard]] std::size_t imageBytes() const { return static_cast<std::size_t>(image_.sizeInBytes()); }
signals:
    void readFailed(QString message);
public:
    // Display-only options; they never alter analysis results.
    void setDisplay(bool logFrequency, humtrace::ui::Palette palette, double floorDb, double ceilingDb);
    void setSelectedFinding(int index) { selectedFinding_ = index; update(); }
    [[nodiscard]] QString columnDescription(int frame) const;

protected:
    void paintData(QPainter& p, const QRect& plot) override;
    void paintAxes(QPainter& p, const QRect& plot) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildImage();
    void rebuildPagedImage();
    PagedData* paged_{nullptr};
    std::uint64_t imageFirst_{0}, imageStep_{1};
    const std::vector<humtrace::ChannelView>* views_{nullptr};
    const humtrace::AnalysisReport* report_{nullptr};
    const std::vector<humtrace::ui::FindingRow>* findings_{nullptr};
    QImage image_;
    int imageChannel_{-1};
    bool log_{true};
    humtrace::ui::Palette palette_{humtrace::ui::Palette::Default};
    double floor_{-100.0};
    double ceiling_{-10.0};
    int selectedFinding_{-1};
};
