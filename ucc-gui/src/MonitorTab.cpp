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

#include "MonitorTab.hpp"
#include "../libucc-dbus/UccdClient.hpp"
#include <QDateTime>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QStackedWidget>
#include <QToolTip>
#include <QDir>
#include <QSettings>
#include <QMainWindow>
#include <QStatusBar>
#include <cstring>
#include <algorithm>

namespace ucc
{

// ---------------------------------------------------------------------------
// Metric definitions — colour palette inspired by CoolerControl
// ---------------------------------------------------------------------------
struct MetricDef
{
  const char *key;       // JSON key from MetricsHistoryStore
  const char *label;     // Human-readable label
  QColor      color;     // Line colour
  MetricGroup group;
};

static constexpr int METRIC_COUNT = 13;

// Order matches MetricId enum in MetricsHistoryStore.hpp
static const MetricDef kMetrics[ METRIC_COUNT ] =
{
  { "cpuTemp",             "CPU Temp",            QColor( 239,  83,  80 ), MetricGroup::Temp  },
  { "cpuFanDuty",          "CPU Fan Duty",        QColor(  66, 165, 245 ), MetricGroup::Duty  },
  { "cpuPower",            "CPU Power",           QColor( 255, 167,  38 ), MetricGroup::Power },
  { "cpuFrequency",        "CPU Frequency",       QColor( 171, 71,  188 ), MetricGroup::Freq  },
  { "gpuTemp",             "dGPU Temp",           QColor( 255,  82,  82 ), MetricGroup::Temp  },
  { "gpuFanDuty",          "dGPU Fan Duty",       QColor(  41, 182, 246 ), MetricGroup::Duty  },
  { "gpuPower",            "dGPU Power",          QColor( 255, 202,  40 ), MetricGroup::Power },
  { "gpuFrequency",        "dGPU Frequency",      QColor( 186, 104, 200 ), MetricGroup::Freq  },
  { "igpuTemp",            "iGPU Temp",           QColor( 255, 138, 101 ), MetricGroup::Temp  },
  { "igpuPower",           "iGPU Power",          QColor( 255, 213,  79 ), MetricGroup::Power },
  { "igpuFrequency",       "iGPU Frequency",      QColor( 206, 147, 216 ), MetricGroup::Freq  },
  { "waterCoolerFanDuty",  "WC Fan Duty",         QColor(  38, 198, 218 ), MetricGroup::Duty  },
  { "waterCoolerPumpLevel","WC Pump Level",       QColor( 129, 199, 132 ), MetricGroup::Duty  },
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * @brief Undo the normalisation applied to a metric value to restore its real value.
 * Inverse of metricToNormalisedScale().
 */
double MonitorTab::metricFromNormalisedScale( double normalisedValue, MetricGroup g )
{
  switch ( g )
  {
    case MetricGroup::Temp:  return normalisedValue / (100.0 / 105.0);  // 0–100 → 0–105 °C
    case MetricGroup::Duty:  return normalisedValue;                     // already %
    case MetricGroup::Power: return normalisedValue / (100.0 / m_maxPowerW);  // 0–100 → 0–m_maxPowerW W
    case MetricGroup::Freq:  return normalisedValue / (100.0 / 6000.0);  // 0–100 → 0–6000 MHz
  }
  return normalisedValue;
}

/**
 * @brief Scale factor for normalising a metric group value to [0, 100].
 * Used by the unified chart's shadow series (accesses this->m_maxPowerW for Power group).
 */
double MonitorTab::metricToNormalisedScale( MetricGroup g )
{
  switch ( g )
  {
    case MetricGroup::Temp:  return 100.0 / 105.0;  // 0–105 °C  → 0–100
    case MetricGroup::Duty:  return 1.0;             // already %
    case MetricGroup::Power: return 100.0 / m_maxPowerW;  // 0–m_maxPowerW W → 0–100
    case MetricGroup::Freq:  return 100.0 / 6000.0;  // 0–6000 MHz→ 0–100
  }
  return 1.0;
}

static const char *metricGroupUnit( MetricGroup g )
{
  switch ( g )
  {
    case MetricGroup::Temp:  return "°C";
    case MetricGroup::Duty:  return "%";
    case MetricGroup::Power: return "W";
    case MetricGroup::Freq:  return "MHz";
  }
  return "";
}

void MonitorTab::initializeMaxPowerFromHardware()
{
  // Get GPU TGP (cTGP) maximum
  int maxGpuTgp = 0;
  int maxBoostTdp = 0;

  if ( auto gpuMax = m_client->getNVIDIAPowerCTRLMaxPowerLimit() )
    maxGpuTgp = *gpuMax;

  // Get boost TDP maximum (index 1 from ODM power limits)
  if ( auto tdpLimits = m_client->getODMPowerLimits() )
  {
    // Index 1 is typically "Boost TDP"
    if ( tdpLimits->size() > 1 )
      maxBoostTdp = (*tdpLimits)[1];
  }

  if ( m_maxPowerW = std::max( maxGpuTgp, maxBoostTdp ); m_maxPowerW <= 0 )
    m_maxPowerW = 200;  // Fallback to default
}

static QChart *createChart()
{
  auto *chart = new QChart();
  // Don't set title — will save vertical space for graphs
  chart->setAnimationOptions( QChart::NoAnimation );
  chart->legend()->setVisible( false );
  chart->setMargins( QMargins( 4, 4, 4, 4 ) );

  // Black chart background
  chart->setBackgroundBrush( QBrush( Qt::black ) );
  chart->setPlotAreaBackgroundBrush( QBrush( Qt::black ) );
  chart->setPlotAreaBackgroundVisible( true );
  chart->setTitleBrush( QBrush( Qt::white ) );

  return chart;
}

static void styleAxis( QAbstractAxis *axis )
{
  axis->setLabelsBrush( QBrush( Qt::white ) );
  axis->setTitleBrush( QBrush( Qt::white ) );
  QPen linePen( QColor( 100, 100, 100 ) );
  axis->setLinePen( linePen );
  axis->setGridLinePen( QPen( QColor( 45, 45, 45 ) ) );
  axis->setShadesBrush( QBrush( Qt::transparent ) );
}

static QDateTimeAxis *createXAxis()
{
  auto *axis = new QDateTimeAxis();
  axis->setFormat( "HH:mm:ss" );
  axis->setTickCount( 6 );
  styleAxis( axis );
  return axis;
}

static QValueAxis *createYAxis( const QString &title, double min, double max )
{
  auto *axis = new QValueAxis();
  axis->setTitleText( title );
  axis->setRange( min, max );
  axis->setLabelFormat( "%.0f" );
  styleAxis( axis );
  return axis;
}

static QChartView *createChartView( QChart *chart )
{
  auto *view = new QChartView( chart );
  view->setRenderHint( QPainter::Antialiasing );
  view->setMinimumHeight( 180 );
  return view;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MonitorTab::MonitorTab( UccdClient *client, QWidget *parent )
  : QWidget( parent )
  , m_client( client )
{
  initializeMaxPowerFromHardware();
  setupUI();

  setFocusPolicy( Qt::StrongFocus );  // Enable keyboard events for spacebar pause

  m_fetchTimer.setInterval( 1000 );
  connect( &m_fetchTimer, &QTimer::timeout, this, &MonitorTab::fetchData );
}

void MonitorTab::setMonitoringActive( bool active )
{
  if ( active )
  {
    // Clear all in-memory buffers and series to avoid overlapping time ranges
    // (which cause crossed lines when the same timestamps appear twice).
    for ( auto &[key, info] : m_seriesMap )
    {
      info.buffer.clear();
      info.series->clear();

      QVariant uv = info.series->property( "_uniSeries" );
      if ( uv.isValid() )
      {
        auto *uni = qobject_cast< QLineSeries * >( uv.value< QObject * >() );
        if ( uni )
          uni->clear();
      }
    }

    // Only fetch data that fits in the current visible window — not the full
    // daemon history horizon (which can be 30 minutes).  This bounds the
    // initial render cost to m_windowSeconds worth of points.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastTimestamp = now - static_cast< qint64 >( m_windowSeconds ) * 1000;
    fetchData();
    m_fetchTimer.start();
  }
  else
  {
    m_fetchTimer.stop();
  }
}

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void MonitorTab::setupUI()
{
  auto *mainLayout = new QVBoxLayout( this );

  setupControls();

  // ── (1) Legend / series-toggle group box — full width at top ──────────
  auto *legendBox = new QGroupBox();
  auto *legendLayout = new QGridLayout( legendBox );
  legendLayout->setContentsMargins( 4, 4, 4, 4 );
  int col = 0, row = 0;
  for ( int i = 0; i < METRIC_COUNT; ++i )
  {
    const auto &md = kMetrics[ i ];
    auto *cb = new QCheckBox( md.label );
    cb->setChecked( true );
    cb->setStyleSheet( QStringLiteral( "QCheckBox { color: %1; }" ).arg( md.color.name() ) );

    legendLayout->addWidget( cb, row, col );
    if ( ++col >= 5 ) { col = 0; ++row; }

    // Create series
    auto *series = new QLineSeries();
    series->setName( md.label );
    QPen pen( md.color );
    pen.setWidth( 1 );
    series->setPen( pen );
    series->setProperty( "_unit", QString::fromUtf8( metricGroupUnit( md.group ) ) );

    connect( cb, &QCheckBox::toggled, series, &QLineSeries::setVisible );

    m_seriesMap[ md.key ] = { series, cb, md.label, md.color, {} };
  }

  m_unifiedCheckBox = new QCheckBox( "Unified Graph" );
  m_unifiedCheckBox->setChecked( false );
  connect( m_unifiedCheckBox, &QCheckBox::toggled, this, &MonitorTab::setUnifiedMode );
  legendLayout->addWidget( m_unifiedCheckBox, row, col );

  if ( ++col >= 5 ) { col = 0; ++row; }
  // Pause indicator (hidden by default, shown when spacebar pauses updates)
  m_pauseLabel = new QLabel( "⏸ PAUSED" );
  m_pauseLabel->setStyleSheet( "QLabel { color: #FF6B6B; font-weight: bold; padding: 0 8px; }" );
  m_pauseLabel->hide();
  legendLayout->addWidget( m_pauseLabel, row, col );

  mainLayout->addWidget( legendBox );

  // ---------- Per-group page (4 charts filling available space) ----------
  auto *chartsWidget = new QWidget();
  auto *chartsLayout = new QVBoxLayout( chartsWidget );
  chartsLayout->setContentsMargins( 0, 0, 0, 0 );
  chartsLayout->setSpacing( 2 );

  setupTemperatureChart();
  setupDutyChart();
  setupPowerChart();
  setupFrequencyChart();

  chartsLayout->addWidget( m_tempChartView, 1 );
  chartsLayout->addWidget( m_dutyChartView, 1 );
  chartsLayout->addWidget( m_powerChartView, 1 );
  chartsLayout->addWidget( m_freqChartView, 1 );

  m_perGroupPage = chartsWidget;  // stacked page 0

  // ---------- Unified "all-in-one" chart (page 1) ----------
  setupUnifiedChart();

  // ---------- Stacked widget to switch between the two views ----------
  m_chartStack = new QStackedWidget();
  m_chartStack->addWidget( m_perGroupPage );      // index 0
  m_chartStack->addWidget( m_unifiedChartView );   // index 1
  m_chartStack->setCurrentIndex( 0 );

  mainLayout->addWidget( m_chartStack, 1 );

  // ---------- Install hover callout on every chart ----------
  installHoverCallout( m_tempChart );
  installHoverCallout( m_dutyChart );
  installHoverCallout( m_powerChart );
  installHoverCallout( m_freqChart );
  // Unified chart hover is installed lazily in setUnifiedMode()
  // because its series don't exist yet at this point.

  // ---------- Load saved checkbox states ----------
  loadCheckboxStates();

  // Connect all checkboxes to auto-save on toggle and update group visibility
  for ( auto &[key, info] : m_seriesMap )
  {
    connect( info.toggle, &QCheckBox::toggled,
             this, &MonitorTab::saveCheckboxStates );
    connect( info.toggle, &QCheckBox::toggled,
             this, &MonitorTab::updateGroupChartVisibility );
  }

  // Apply initial group visibility after loading saved states
  updateGroupChartVisibility();
}

void MonitorTab::setupControls()
{
  // Time window is controlled by mouse scroll; no combo box needed.
}

void MonitorTab::setTimeWindow( int seconds )
{
  m_windowSeconds = std::clamp( seconds, 60, 1800 );

  // Update the main window status bar
  if ( auto *mw = qobject_cast< QMainWindow * >( window() ) )
  {
    const int mins = m_windowSeconds / 60;
    const int secs = m_windowSeconds % 60;
    QString text;
    if ( secs == 0 )
      text = QStringLiteral( "Time window: %1 min" ).arg( mins );
    else
      text = QStringLiteral( "Time window: %1:%2" )
          .arg( mins ).arg( secs, 2, 10, QLatin1Char( '0' ) );
    mw->statusBar()->showMessage( text, 3000 );
  }

  // Clear all in-memory buffers and re-fetch from the new horizon so that
  // widening the window loads fresh history and narrowing immediately trims.
  for ( auto &[key, info] : m_seriesMap )
  {
    info.buffer.clear();
    info.series->clear();
    QVariant uv = info.series->property( "_uniSeries" );
    if ( uv.isValid() )
      if ( auto *uni = qobject_cast< QLineSeries * >( uv.value< QObject * >() ) )
        uni->clear();
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  m_lastTimestamp = now - static_cast< qint64 >( m_windowSeconds ) * 1000;
  fetchData();
  updateAxes();

  // Persist only the time window value
  QSettings settings( QDir::homePath() + "/.config/uccrc", QSettings::IniFormat );
  settings.beginGroup( "MonitorTab" );
  settings.setValue( "TimeWindowSeconds", m_windowSeconds );
  settings.endGroup();
  settings.sync();
}

void MonitorTab::wheelEvent( QWheelEvent *event )
{
  // Each 120-unit notch changes the window by 30 seconds
  const int delta = event->angleDelta().y();
  if ( delta == 0 )
    return;

  // Scroll up = zoom in (shorter window), scroll down = zoom out (longer)
  const int step = ( delta > 0 ) ? -30 : 30;
  setTimeWindow( m_windowSeconds + step );
  event->accept();
}

// ---------------------------------------------------------------------------
// Unified chart — normalised 0–100 % Y axis
// ---------------------------------------------------------------------------

void MonitorTab::setupUnifiedChart()
{
  m_unifiedChart = createChart();
  m_unifiedXAxis = createXAxis();
  m_unifiedYAxis = createYAxis( "%", 0, 100 );
  m_unifiedChart->addAxis( m_unifiedXAxis, Qt::AlignBottom );
  m_unifiedChart->addAxis( m_unifiedYAxis, Qt::AlignLeft );
  m_unifiedChart->legend()->setVisible( false );

  // Add an invisible anchor series so that the axes render their labels
  // even before the real (lazy) shadow series are created.
  auto *anchor = new QLineSeries();
  anchor->setVisible( false );
  anchor->setPen( QPen( Qt::transparent ) );
  m_unifiedChart->addSeries( anchor );
  anchor->attachAxis( m_unifiedXAxis );
  anchor->attachAxis( m_unifiedYAxis );

  // Shadow series created lazily in createUnifiedSeries() when the
  // unified view is first activated.
  m_unifiedChartView = createChartView( m_unifiedChart );
}

void MonitorTab::createUnifiedSeries()
{
  if ( m_unifiedSeriesActive )
    return;  // Already created

  // Create a shadow normalised series for each metric and synchronise it
  // with the original series via property mirroring in applyBinaryData().
  for ( int i = 0; i < METRIC_COUNT; ++i )
  {
    const auto &md = kMetrics[ i ];
    auto *ns = new QLineSeries();
    ns->setName( md.label );
    QPen pen( md.color );
    pen.setWidth( 1 );
    ns->setPen( pen );

    m_unifiedChart->addSeries( ns );
    ns->attachAxis( m_unifiedXAxis );
    ns->attachAxis( m_unifiedYAxis );

    // Store reverse-scale and unit on the shadow series
    const double fwdScale = metricToNormalisedScale( md.group );
    ns->setProperty( "_realScale", 1.0 / fwdScale );
    ns->setProperty( "_unit", QString::fromUtf8( metricGroupUnit( md.group ) ) );

    // Store the shadow series on the original for applyBinaryData/trimSeries
    m_seriesMap[ md.key ].series->setProperty( "_uniSeries",
        QVariant::fromValue( static_cast< QObject * >( ns ) ) );
    m_seriesMap[ md.key ].series->setProperty( "_uniScale",
        metricToNormalisedScale( md.group ) );

    // Respect the toggle checkbox (connect and sync initial state)
    connect( m_seriesMap[ md.key ].toggle, &QCheckBox::toggled,
             ns, &QLineSeries::setVisible );
    ns->setVisible( m_seriesMap[ md.key ].toggle->isChecked() );

    // Copy existing raw data into the shadow series (normalised)
    // so the unified chart is immediately populated with all visible history.
    auto &rawBuffer = m_seriesMap[ md.key ].buffer;
    const double scale = metricToNormalisedScale( md.group );
    if ( !rawBuffer.isEmpty() )
    {
      QList< QPointF > pts;
      pts.reserve( rawBuffer.size() );
      for ( const auto &pt : rawBuffer )
        pts.append( QPointF( pt.x(), pt.y() * scale ) );
      ns->replace( pts );
    }
  }

  m_unifiedSeriesActive = true;
}

void MonitorTab::destroyUnifiedSeries()
{
  if ( !m_unifiedSeriesActive )
    return;  // Already destroyed

  // Remove all shadow series from the unified chart (keep invisible anchor).
  // The anchor has no name and is not visible; shadow series always have names.
  const auto allSeries = m_unifiedChart->series();   // snapshot (QList copy)
  for ( auto *abstractSeries : allSeries )
  {
    if ( !abstractSeries->isVisible() && abstractSeries->name().isEmpty() )
      continue;   // invisible anchor — keep
    m_unifiedChart->removeSeries( abstractSeries );
    delete abstractSeries;
  }

  // Clear the property references
  for ( auto &[key, info] : m_seriesMap )
  {
    info.series->setProperty( "_uniSeries", QVariant() );
    info.series->setProperty( "_uniScale", QVariant() );
  }

  m_unifiedSeriesActive = false;
}

void MonitorTab::setUnifiedMode( bool unified )
{
  if ( unified && !m_unifiedSeriesActive )
  {
    // Lazily create shadow series on first activation
    createUnifiedSeries();
    // Install hover callout now that the series actually exist
    installHoverCallout( m_unifiedChart );
  }
  else if ( !unified && m_unifiedSeriesActive )
  {
    // Reclaim memory when switching back to per-group view
    destroyUnifiedSeries();
  }
  m_chartStack->setCurrentIndex( unified ? 1 : 0 );
  updateAxes();
}

// ---------------------------------------------------------------------------
// Hover callout — shows exact value under the pointer
// ---------------------------------------------------------------------------

void MonitorTab::installHoverCallout( QChart *chart )
{
  // Clean up any previous callout for this chart (important for the
  // unified chart whose series are destroyed and recreated).
  auto it = m_callouts.find( chart );
  if ( it != m_callouts.end() )
  {
    delete it->second.bg;
    delete it->second.text;
    m_callouts.erase( it );
  }

  // Create a semi-transparent background rect + text item living in the
  // chart's scene.  Hidden by default; shown on hover.
  auto *bg   = new QGraphicsRectItem( chart );
  auto *text = new QGraphicsSimpleTextItem( chart );
  bg->setBrush( QBrush( QColor( 30, 30, 30, 200 ) ) );
  bg->setPen( QPen( QColor( 200, 200, 200 ) ) );
  bg->setZValue( 100 );
  text->setBrush( Qt::white );
  text->setZValue( 101 );
  bg->hide();
  text->hide();

  m_callouts[ chart ] = { bg, text };

  // Connect hovered() on every series belonging to this chart.
  for ( auto *abstractSeries : chart->series() )
  {
    auto *ls = qobject_cast< QLineSeries * >( abstractSeries );
    if ( !ls )
      continue;

    connect( ls, &QLineSeries::hovered,
             this, [this, ls, chart]( const QPointF &point, bool state )
    {
      auto &co = m_callouts[ chart ];
      if ( !state )
      {
        co.bg->hide();
        co.text->hide();
        return;
      }

      // Format the timestamp and value.
      // _realScale is set on unified shadow series to invert the normalisation;
      // _unit is set on all series for the physical unit suffix.
      const QDateTime dt = QDateTime::fromMSecsSinceEpoch(
                              static_cast< qint64 >( point.x() ) );
      const QVariant rvProp = ls->property( "_realScale" );
      const double realVal  = rvProp.isValid()
                              ? point.y() * rvProp.toDouble()
                              : point.y();
      const QString unit    = ls->property( "_unit" ).toString();

      const QString label = unit.isEmpty()
          ? QStringLiteral( "%1\n%2: %3" )
              .arg( dt.toString( "HH:mm:ss" ) )
              .arg( ls->name() )
              .arg( realVal, 0, 'f', 1 )
          : QStringLiteral( "%1\n%2: %3 %4" )
              .arg( dt.toString( "HH:mm:ss" ) )
              .arg( ls->name() )
              .arg( realVal, 0, 'f', 1 )
              .arg( unit );

      co.text->setText( label );

      // Position near the data point (in scene coordinates)
      const QPointF scenePos = chart->mapToPosition( point );
      constexpr qreal pad = 4.0;
      const QRectF textRect = co.text->boundingRect();
      co.text->setPos( scenePos.x() + 10, scenePos.y() - textRect.height() - 6 );
      co.bg->setRect( co.text->pos().x() - pad,
                      co.text->pos().y() - pad,
                      textRect.width() + 2 * pad,
                      textRect.height() + 2 * pad );

      co.bg->show();
      co.text->show();
    } );
  }
}

// ---------------------------------------------------------------------------
// Chart setup helpers — attach series to charts
// ---------------------------------------------------------------------------

void MonitorTab::setupTemperatureChart()
{
  m_tempChart = createChart();
  m_tempXAxis = createXAxis();
  m_tempYAxis = createYAxis( "Temperature (°C)", 0, 105 );
  m_tempChart->addAxis( m_tempXAxis, Qt::AlignBottom );
  m_tempChart->addAxis( m_tempYAxis, Qt::AlignLeft );

  for ( const auto &md : kMetrics )
  {
    if ( md.group != MetricGroup::Temp ) continue;
    auto *s = m_seriesMap[ md.key ].series;
    m_tempChart->addSeries( s );
    s->attachAxis( m_tempXAxis );
    s->attachAxis( m_tempYAxis );
  }
  m_tempChartView = createChartView( m_tempChart );
}

void MonitorTab::setupDutyChart()
{
  m_dutyChart = createChart();
  m_dutyXAxis = createXAxis();
  m_dutyYAxis = createYAxis( "Fan Duty (%)", 0, 100 );
  m_dutyChart->addAxis( m_dutyXAxis, Qt::AlignBottom );
  m_dutyChart->addAxis( m_dutyYAxis, Qt::AlignLeft );

  for ( const auto &md : kMetrics )
  {
    if ( md.group != MetricGroup::Duty ) continue;
    auto *s = m_seriesMap[ md.key ].series;
    m_dutyChart->addSeries( s );
    s->attachAxis( m_dutyXAxis );
    s->attachAxis( m_dutyYAxis );
  }
  m_dutyChartView = createChartView( m_dutyChart );
}

void MonitorTab::setupPowerChart()
{
  m_powerChart = createChart();
  m_powerXAxis = createXAxis();
  m_powerYAxis = createYAxis( QStringLiteral( "Power (W)" ), 0, m_maxPowerW );
  m_powerChart->addAxis( m_powerXAxis, Qt::AlignBottom );
  m_powerChart->addAxis( m_powerYAxis, Qt::AlignLeft );

  for ( const auto &md : kMetrics )
  {
    if ( md.group != MetricGroup::Power ) continue;
    auto *s = m_seriesMap[ md.key ].series;
    m_powerChart->addSeries( s );
    s->attachAxis( m_powerXAxis );
    s->attachAxis( m_powerYAxis );
  }
  m_powerChartView = createChartView( m_powerChart );
}

void MonitorTab::setupFrequencyChart()
{
  m_freqChart = createChart();
  m_freqXAxis = createXAxis();
  m_freqYAxis = createYAxis( "Frequency (MHz)", 0, 6000 );
  m_freqChart->addAxis( m_freqXAxis, Qt::AlignBottom );
  m_freqChart->addAxis( m_freqYAxis, Qt::AlignLeft );

  for ( const auto &md : kMetrics )
  {
    if ( md.group != MetricGroup::Freq ) continue;
    auto *s = m_seriesMap[ md.key ].series;
    m_freqChart->addSeries( s );
    s->attachAxis( m_freqXAxis );
    s->attachAxis( m_freqYAxis );
  }
  m_freqChartView = createChartView( m_freqChart );
}

// ---------------------------------------------------------------------------
// Incremental data fetch
// ---------------------------------------------------------------------------

void MonitorTab::fetchData()
{
  if ( !m_client || m_paused )
    return;

  auto result = m_client->getMonitorDataSince( m_lastTimestamp );
  if ( !result.has_value() || result->isEmpty() )
    return;

  // ── Suspend painting on ALL chart views during the batch update ────
  m_tempChartView->setUpdatesEnabled( false );
  m_dutyChartView->setUpdatesEnabled( false );
  m_powerChartView->setUpdatesEnabled( false );
  m_freqChartView->setUpdatesEnabled( false );
  m_unifiedChartView->setUpdatesEnabled( false );

  applyBinaryData( *result );
  trimSeries();
  commitSeries();
  updateAxes();

  // ── Resume painting — triggers a single composite repaint ─────────
  m_tempChartView->setUpdatesEnabled( true );
  m_dutyChartView->setUpdatesEnabled( true );
  m_powerChartView->setUpdatesEnabled( true );
  m_freqChartView->setUpdatesEnabled( true );
  m_unifiedChartView->setUpdatesEnabled( true );
}

// ---------------------------------------------------------------------------
// Pause / resume via spacebar
// ---------------------------------------------------------------------------

void MonitorTab::keyPressEvent( QKeyEvent *event )
{
  if ( event->key() == Qt::Key_Space )
  {
    m_paused = !m_paused;
    if ( m_pauseLabel )
    {
      m_pauseLabel->setVisible( m_paused );
    }
    event->accept();
    return;
  }
  QWidget::keyPressEvent( event );
}

void MonitorTab::applyBinaryData( const QByteArray &data )
{
  // Wire layout (native endian — same-host IPC):
  //   per non-empty metric: uint8_t metricId, uint32_t count,
  //                         count × { int64_t timestampMs, double value }  (16 bytes each)
  //
  // Points are appended to in-memory buffers only.  The actual QLineSeries
  // objects are updated in commitSeries() via a single replace() call,
  // which emits exactly one signal per series instead of one per append +
  // one per trim.

  static constexpr size_t kPointSize = sizeof( int64_t ) + sizeof( double );  // 16

  const auto *p   = reinterpret_cast< const uint8_t * >( data.constData() );
  const auto *end = p + data.size();
  qint64 maxTs = m_lastTimestamp;

  while ( p < end )
  {
    if ( p + 1 + sizeof( uint32_t ) > end )
      break;

    const uint8_t metricId = *p++;

    uint32_t count = 0;
    std::memcpy( &count, p, sizeof( count ) );
    p += sizeof( count );

    if ( p + static_cast< size_t >( count ) * kPointSize > end )
      break;

    const bool valid = ( metricId < METRIC_COUNT )
                       && ( m_seriesMap.count( kMetrics[ metricId ].key ) > 0 );

    for ( uint32_t j = 0; j < count; ++j )
    {
      int64_t ts  = 0;
      double  val = 0.0;
      std::memcpy( &ts,  p,             sizeof( ts ) );
      std::memcpy( &val, p + sizeof(ts), sizeof( val ) );
      p += kPointSize;

      if ( valid )
      {
        m_seriesMap[ kMetrics[ metricId ].key ].buffer.append(
            QPointF( static_cast< qreal >( ts ), val ) );
        if ( ts > maxTs )
          maxTs = ts;
      }
    }
  }

  // Advance cursor so the next fetch only returns new points.
  if ( maxTs > m_lastTimestamp )
    m_lastTimestamp = maxTs + 1;
}

void MonitorTab::trimSeries()
{
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const qreal cutoff = static_cast< qreal >( now - static_cast< qint64 >( m_windowSeconds ) * 1000 );

  // Trim leading stale points from in-memory buffers only.
  // The QLineSeries objects are updated in commitSeries().
  for ( auto &[key, info] : m_seriesMap )
  {
    auto &buf = info.buffer;
    int stale = 0;
    while ( stale < buf.size() && buf[ stale ].x() < cutoff )
      ++stale;
    if ( stale > 0 )
      buf.remove( 0, stale );
  }
}

void MonitorTab::commitSeries()
{
  // Push in-memory buffers into QLineSeries via a single replace() call.
  // This emits exactly ONE pointsReplaced signal per series, instead of
  // separate pointsAdded + pointsRemoved signals from append() + removePoints().

  for ( int i = 0; i < METRIC_COUNT; ++i )
  {
    auto &info = m_seriesMap[ kMetrics[ i ].key ];
    info.series->replace( info.buffer );

    // Update unified shadow series if active — build normalised list on the fly
    QVariant uv = info.series->property( "_uniSeries" );
    if ( uv.isValid() )
    {
      auto *uni = qobject_cast< QLineSeries * >( uv.value< QObject * >() );
      if ( uni )
      {
        const double scale = metricToNormalisedScale( kMetrics[ i ].group );
        QList< QPointF > scaled;
        scaled.reserve( info.buffer.size() );
        for ( const auto &pt : info.buffer )
          scaled.append( QPointF( pt.x(), pt.y() * scale ) );
        uni->replace( scaled );
      }
    }
  }
}

void MonitorTab::updateAxes()
{
  const QDateTime now = QDateTime::currentDateTime();
  const QDateTime start = now.addSecs( -m_windowSeconds );

  // Only update axes for the currently visible chart view — the invisible
  // ones will be updated when they become visible again.
  const bool perGroup = ( m_chartStack->currentIndex() == 0 );

  if ( perGroup )
  {
    auto setRange = [&]( QDateTimeAxis *axis ) {
      if ( axis ) axis->setRange( start, now );
    };
    setRange( m_tempXAxis );
    setRange( m_dutyXAxis );
    setRange( m_powerXAxis );
    setRange( m_freqXAxis );
  }
  else
  {
    if ( m_unifiedXAxis )
      m_unifiedXAxis->setRange( start, now );
  }
}

// ---------------------------------------------------------------------------
// Group chart visibility — hide empty groups, stretch remaining
// ---------------------------------------------------------------------------

void MonitorTab::updateGroupChartVisibility()
{
  // Map each MetricGroup to its per-group chart view
  const std::pair< MetricGroup, QChartView * > groupViews[] = {
    { MetricGroup::Temp,  m_tempChartView  },
    { MetricGroup::Duty,  m_dutyChartView  },
    { MetricGroup::Power, m_powerChartView },
    { MetricGroup::Freq,  m_freqChartView  },
  };

  for ( const auto &[group, view] : groupViews )
  {
    bool anyEnabled = false;
    for ( int i = 0; i < METRIC_COUNT; ++i )
    {
      if ( kMetrics[ i ].group == group )
      {
        auto it = m_seriesMap.find( kMetrics[ i ].key );
        if ( it != m_seriesMap.end() && it->second.toggle->isChecked() )
        {
          anyEnabled = true;
          break;
        }
      }
    }
    view->setVisible( anyEnabled );
  }
}

void MonitorTab::saveCheckboxStates()
{
  QSettings settings( QDir::homePath() + "/.config/uccrc", QSettings::IniFormat );
  settings.beginGroup( "MonitorTab" );

  // Save individual metric visibility states
  for ( const auto &[key, info] : m_seriesMap )
  {
    settings.setValue( QString::fromStdString( key ), info.toggle->isChecked() );
  }

  // Save unified mode checkbox state
  settings.setValue( "UnifiedMode", m_unifiedCheckBox->isChecked() );

  settings.endGroup();
  settings.sync();
}

void MonitorTab::loadCheckboxStates()
{
  QSettings settings( QDir::homePath() + "/.config/uccrc", QSettings::IniFormat );
  settings.beginGroup( "MonitorTab" );

  // Load individual metric visibility states (default to checked)
  for ( auto &[key, info] : m_seriesMap )
  {
    const bool checked = settings.value( QString::fromStdString( key ), true ).toBool();
    info.toggle->setChecked( checked );
  }

  // Load time window in seconds (default 5 min = 300s)
  m_windowSeconds = std::clamp( settings.value( "TimeWindowSeconds", 300 ).toInt(), 60, 1800 );

  // Load unified mode checkbox state (default to false)
  const bool unifiedMode = settings.value( "UnifiedMode", false ).toBool();
  m_unifiedCheckBox->setChecked( unifiedMode );

  settings.endGroup();
}

} // namespace ucc
