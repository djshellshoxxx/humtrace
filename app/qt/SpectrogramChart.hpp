#pragma once

#include "../presentation/findings.hpp"
#include "TimeChart.hpp"
#include "humtrace/view.hpp"

#include <QImage>
#include <vector>

class SpectrogramChart : public TimeChart {
    Q_OBJECT
public:
    explicit SpectrogramChart(ViewState* state, QWidget* parent = nullptr);
    void setData(const std::vector<humtrace::ChannelView>* views, const humtrace::AnalysisReport* report,
                 const std::vector<humtrace::ui::FindingRow>* findings);
    // Display-only options; they never alter analysis results.
    void setDisplay(bool logFrequency, humtrace::ui::Palette palette, double floorDb, double ceilingDb);
    void setSelectedFinding(int index) { selectedFinding_ = index; update(); }
    [[nodiscard]] QString columnDescription(int frame) const;

protected:
    void paintData(QPainter& p, const QRect& plot) override;
    void paintAxes(QPainter& p, const QRect& plot) override;

private:
    void rebuildImage();
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
