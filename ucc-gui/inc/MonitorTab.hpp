/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QByteArray>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsRectItem>
#include <QLabel>
#include <QStackedWidget>
#include <QSettings>
#include <QStatusBar>
#include <QKeyEvent>
#include <QWheelEvent>
#include <map>
#include <cstdint>

namespace ucc
{

class UccdClient;

/**
 * @brief Metric group categories for normalisation and colouring
 */
enum class MetricGroup
{
  Temp,   ///< Temperature (°C)
  Duty,   ///< Fan duty cycle (%)
  Power,  ///< Power consumption (W)
  Freq    ///< Clock frequency (MHz)
};

/**
 * @brief Monitoring tab with real-time hardware graphs.
 *
 * Periodically fetches incremental metric data from the daemon via
 * UccdClient::getMonitorDataSince() and plots it on dual-Y-axis charts.
 */
class MonitorTab : public QWidget
{
  Q_OBJECT

public:
  explicit MonitorTab( UccdClient *client, QWidget *parent = nullptr );
  ~MonitorTab() override = default;

  /** Start / stop the incremental fetch timer. */
  void setMonitoringActive( bool active );

protected:
  void keyPressEvent( QKeyEvent *event ) override;
  void wheelEvent( QWheelEvent *event ) override;

private slots:
  void fetchData();

private:
  // --- Setup helpers ---
  void setupUI();
  void setupTemperatureChart();
  void setupDutyChart();
  void setupPowerChart();
  void setupFrequencyChart();
  void setupControls();
  void setupUnifiedChart();

  /** Apply a new time window (clears series, re-fetches, updates label). */
  void setTimeWindow( int seconds );

  /** Toggle between per-group charts and a single unified chart. */
  void setUnifiedMode( bool unified );

  /** Create shadow series for unified chart (lazy, on-demand). */
  void createUnifiedSeries();

  /** Destroy shadow series for unified chart (reclaim memory). */
  void destroyUnifiedSeries();

  /** Install hover callout on every series in the given chart view. */
  void installHoverCallout( QChart *chart );

  /** Decode the binary payload returned by GetMonitorDataSince and append to series. */
  void applyBinaryData( const QByteArray &data );

  /** Hide per-group chart views when all metrics in that group are disabled. */
  void updateGroupChartVisibility();

  /** Save metric visibility checkboxes to ~/.config/uccrc. */
  void saveCheckboxStates();

  /** Load metric visibility checkboxes from ~/.config/uccrc. */
  void loadCheckboxStates();

  /** Initialize m_maxPowerW from hardware TDP and GPU limits. */
  void initializeMaxPowerFromHardware();

  /** Trim series points that fall outside the visible time window. */
  void trimSeries();

  /** Normalise a metric group value to [0, 100] for unified chart display. */
  double metricToNormalisedScale( MetricGroup g );

  /** Undo normalisation to restore real value (inverse operation). */
  double metricFromNormalisedScale( double normalisedValue, MetricGroup g );

  /** Update the X-axis range to [now - window, now]. */
  void updateAxes();

  // --- Data model ---
  struct SeriesInfo
  {
    QLineSeries *series = nullptr;
    QCheckBox   *toggle = nullptr;
    QString      label;
    QColor       color;
  };

  // One entry per metric key string (e.g. "cpuTemp")
  std::map< std::string, SeriesInfo > m_seriesMap;

  // --- Charts ---
  QChart *m_tempChart     = nullptr;
  QChart *m_dutyChart     = nullptr;
  QChart *m_powerChart    = nullptr;
  QChart *m_freqChart     = nullptr;

  QChartView *m_tempChartView  = nullptr;
  QChartView *m_dutyChartView  = nullptr;
  QChartView *m_powerChartView = nullptr;
  QChartView *m_freqChartView  = nullptr;

  QDateTimeAxis *m_tempXAxis  = nullptr;
  QDateTimeAxis *m_dutyXAxis  = nullptr;
  QDateTimeAxis *m_powerXAxis = nullptr;
  QDateTimeAxis *m_freqXAxis  = nullptr;

  QValueAxis *m_tempYAxis  = nullptr;
  QValueAxis *m_dutyYAxis  = nullptr;
  QValueAxis *m_powerYAxis = nullptr;
  QValueAxis *m_freqYAxis  = nullptr;

  // --- Unified "all-in-one" chart ---
  QChart          *m_unifiedChart     = nullptr;
  QChartView      *m_unifiedChartView = nullptr;
  QDateTimeAxis   *m_unifiedXAxis     = nullptr;
  QValueAxis      *m_unifiedYAxis     = nullptr;
  QStackedWidget  *m_chartStack       = nullptr;  ///< index 0 = per-group, 1 = unified
  QWidget         *m_perGroupPage     = nullptr;

  // --- Hover callout ---
  struct Callout
  {
    QGraphicsRectItem       *bg   = nullptr;
    QGraphicsSimpleTextItem *text = nullptr;
  };
  std::map< QChart *, Callout > m_callouts;

  // --- Controls ---
  QCheckBox *m_unifiedCheckBox = nullptr;
  QLabel    *m_pauseLabel      = nullptr;  ///< Status indicator for pause mode

  // --- State ---
  UccdClient *m_client = nullptr;
  QTimer      m_fetchTimer;
  qint64      m_lastTimestamp = 0;    ///< Last fetched timestamp (ms since epoch)
  int         m_windowSeconds = 300;  ///< Visible time window (default 5 min)
  bool        m_unifiedSeriesActive = false;  ///< Shadow series created?
  bool        m_paused = false;                ///< Pause mode active?
  int         m_maxPowerW = 150;               ///< Platform max power (TDP); adjust for your hardware
};

} // namespace ucc
