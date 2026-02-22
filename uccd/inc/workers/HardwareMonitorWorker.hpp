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

#include "DaemonWorker.hpp"
#include "../Utils.hpp"
#include <string>
#include <optional>
#include <memory>
#include <functional>
#include <vector>
#include <set>
#include <sstream>

/**
 * @brief GPU device counts by vendor/type
 */
struct GpuDeviceCounts
{
  int intelIGpuCount = 0;
  int amdIGpuCount = 0;
  int amdDGpuCount = 0;
  int nvidiaCount = 0;
};

/**
 * @brief Data structure for discrete GPU information
 */
struct DGpuInfo
{
  double m_temp = -1.0;
  double m_coreFrequency = -1.0;
  double m_maxCoreFrequency = -1.0;
  double m_powerDraw = -1.0;
  double m_maxPowerLimit = -1.0;
  double m_enforcedPowerLimit = -1.0;
  bool m_d0MetricsUsage = false;

  void print() const noexcept;
};

/**
 * @brief Data structure for integrated GPU information
 */
struct IGpuInfo
{
  double m_temp = -1.0;
  double m_coreFrequency = -1.0;
  double m_maxCoreFrequency = -1.0;
  double m_powerDraw = -1.0;
  std::string m_vendor = "unknown";

  void print() const noexcept;
};

// Forward declarations for internal implementation classes
class IntelRAPLController;
class PowerController;

/**
 * @brief Detects GPU devices by scanning PCI sysfs entries
 */
class GpuDeviceDetector
{
public:
  explicit GpuDeviceDetector() noexcept = default;

  [[nodiscard]] GpuDeviceCounts detectGpuDevices() const noexcept
  {
    GpuDeviceCounts counts;
    counts.intelIGpuCount = countDevicesMatchingPattern( getIntelIGpuPattern() );
    counts.amdIGpuCount = countDevicesMatchingPattern( getAmdIGpuPattern() );
    counts.amdDGpuCount = countDevicesMatchingPattern( getAmdDGpuPattern() );
    counts.nvidiaCount = countNvidiaDevices();
    return counts;
  }

private:
  [[nodiscard]] std::string getIntelIGpuPattern() const noexcept
  {
    return "8086:(6420|64B0|7D51|7D67|7D41|7DD5|7D45|7D40|"
           "A780|A781|A788|A789|A78A|A782|A78B|A783|A7A0|A7A1|A7A8|A7AA|"
           "A7AB|A7AC|A7AD|A7A9|A721|4680|4690|4688|468A|468B|4682|4692|"
           "4693|46D3|46D4|46D0|46D1|46D2|4626|4628|462A|46A2|46B3|46C2|"
           "46A3|46B2|46C3|46A0|46B0|46C0|46A6|46AA|46A8)";
  }

  [[nodiscard]] std::string getAmdIGpuPattern() const noexcept
  {
    return "1002:(164E|1506|15DD|15D8|15E7|1636|1638|164C|164D|1681|15BF|"
           "15C8|1304|1305|1306|1307|1309|130A|130B|130C|130D|130E|130F|"
           "1310|1311|1312|1313|1315|1316|1317|1318|131B|131C|131D|13C0|"
           "9830|9831|9832|9833|9834|9835|9836|9837|9838|9839|983a|983b|983c|"
           "983d|983e|983f|9850|9851|9852|9853|9854|9855|9856|9857|9858|"
           "9859|985A|985B|985C|985D|985E|985F|9870|9874|9875|9876|9877|"
           "98E4|13FE|143F|74A0|1435|163f|1900|1901|1114|150E)";
  }

  [[nodiscard]] std::string getAmdDGpuPattern() const noexcept
  {
    return "1002:(7480)";
  }

  [[nodiscard]] int countDevicesMatchingPattern( const std::string &pattern ) const noexcept
  {
    try
    {
      std::string command =
          "for f in /sys/bus/pci/devices/*/uevent; do "
          "grep -q 'PCI_CLASS=30000' \"$f\" && grep -q -P 'PCI_ID=" + pattern + "' \"$f\" && echo \"$f\"; "
          "done";

      std::string output = TccUtils::executeCommand( command );
      if ( output.empty() )
        return 0;

      int count = 0;
      for ( char c : output )
        if ( c == '\n' ) count++;
      if ( not output.empty() and output.back() != '\n' )
        count++;
      return count;
    }
    catch ( ... ) { return 0; }
  }

  [[nodiscard]] int countNvidiaDevices() const noexcept
  {
    try
    {
      const std::string nvidiaVendorId = "0x10de";
      std::string command =
          "grep -lx '" + nvidiaVendorId + "' /sys/bus/pci/devices/*/vendor 2>/dev/null || echo ''";
      std::string output = TccUtils::executeCommand( command );
      if ( output.empty() )
        return 0;

      std::set< std::string > uniqueDevices;
      std::istringstream iss( output );
      std::string line;

      while ( std::getline( iss, line ) )
      {
        if ( not line.empty() )
        {
          size_t lastSlash = line.rfind( '/' );
          if ( lastSlash != std::string::npos and lastSlash > 0 )
          {
            std::string devicePath = line.substr( 0, lastSlash );
            size_t lastDot = devicePath.rfind( '.' );
            if ( lastDot != std::string::npos )
              devicePath = devicePath.substr( 0, lastDot );
            uniqueDevices.insert( devicePath );
          }
        }
      }
      return static_cast< int >( uniqueDevices.size() );
    }
    catch ( ... ) { return 0; }
  }
};

/**
 * @brief HardwareMonitorWorker - Unified hardware monitoring
 *
 * Merges GPU information collection, CPU power monitoring, and NVIDIA Prime
 * state detection into a single worker thread.
 *
 * GPU monitoring (every cycle, 800ms):
 *   - Intel iGPU via RAPL energy counters and DRM frequency
 *   - AMD iGPU via hwmon sysfs interface
 *   - AMD dGPU via hwmon sysfs interface
 *   - NVIDIA dGPU via nvidia-smi command
 *
 * CPU power monitoring (every 3rd cycle ≈ 2400ms):
 *   - Intel RAPL power data for CPU package
 *   - Power constraints (PL1/PL2/PL4)
 *
 * Prime state monitoring (every 12th cycle ≈ 9600ms):
 *   - NVIDIA Prime GPU switching status
 *   - Requires prime-select utility (Ubuntu/TUXEDO OS)
 */
class HardwareMonitorWorker : public DaemonWorker
{
public:
  /**
   * @brief Callback function type for GPU data updates
   */
  using GpuDataCallback = std::function< void( const IGpuInfo &, const DGpuInfo & ) >;

  /**
   * @brief Callback function type for CPU power data updates
   * @param json JSON string with power data
   * @param cpuPowerWatts Current CPU power draw in watts (or -1.0 if unavailable)
   */
  using CpuPowerCallback = std::function< void( const std::string &json, double cpuPowerWatts ) >;

  /**
   * @brief Callback function type for CPU frequency updates (MHz)
   */
  using CpuFrequencyCallback = std::function< void( int frequencyMHz ) >;

  /**
   * @brief Constructor
   * @param cpuPowerUpdateCallback Called with CPU power JSON + raw watts when updated
   * @param getSensorDataCollectionStatus Returns whether sensor data collection is enabled
   * @param setPrimeStateCallback Called with prime state string when updated
   */
  explicit HardwareMonitorWorker(
    CpuPowerCallback cpuPowerUpdateCallback,
    std::function< bool() > getSensorDataCollectionStatus,
    std::function< void( const std::string & ) > setPrimeStateCallback );

  ~HardwareMonitorWorker() override;

  // Prevent copy and move
  HardwareMonitorWorker( const HardwareMonitorWorker & ) = delete;
  HardwareMonitorWorker( HardwareMonitorWorker && ) = delete;
  HardwareMonitorWorker &operator=( const HardwareMonitorWorker & ) = delete;
  HardwareMonitorWorker &operator=( HardwareMonitorWorker && ) = delete;

  /**
   * @brief Callback function type for webcam status updates
   * @param available Whether webcam switch hardware is present
   * @param status    Current webcam on/off state
   */
  using WebcamStatusCallback = std::function< void( bool available, bool status ) >;

  /**
   * @brief Hardware reader that returns (available, status) from the webcam switch
   */
  using WebcamHwReader = std::function< std::pair< bool, bool >() >;

  /**
   * @brief Set callback for GPU data updates
   * @param callback Function called with integrated and discrete GPU info
   */
  void setGpuDataCallback( GpuDataCallback callback ) noexcept;

  /**
   * @brief Set callbacks for webcam monitoring
   *
   * Must be called before start(). The reader queries hardware for the
   * webcam switch state; the callback pushes the result to DBus data.
   *
   * @param reader   Returns (available, status) from hardware
   * @param callback Called with (available, status) on each poll
   */
  void setWebcamCallbacks( WebcamHwReader reader, WebcamStatusCallback callback ) noexcept;

  /**
   * @brief Set callback for CPU frequency updates
   *
   * Called every cycle (~800ms) with the current CPU frequency in MHz.
   * Must be called before start().
   *
   * @param callback Function called with frequency in MHz (or -1 if unavailable)
   */
  void setCpuFrequencyCallback( CpuFrequencyCallback callback ) noexcept;

  /**
   * @brief Check if NVIDIA Prime is supported on this system
   * @return true if Prime is supported
   */
  [[nodiscard]] bool isPrimeSupported() const noexcept;

protected:
  void onStart() override;
  void onWork() override;
  void onExit() override;

private:
  // --- GPU state ---
  GpuDeviceDetector m_gpuDetector;
  GpuDeviceCounts m_deviceCounts;
  bool m_isNvidiaSmiInstalled;
  GpuDataCallback m_gpuDataCallback;
  std::optional< std::string > m_amdIGpuHwmonPath;
  std::optional< std::string > m_amdDGpuHwmonPath;
  std::optional< std::string > m_intelIGpuDrmPath;
  int m_hwmonIGpuRetryCount;
  int m_hwmonDGpuRetryCount;

  // GPU RAPL for Intel iGPU power
  std::unique_ptr< IntelRAPLController > m_intelRAPLGpu;
  std::unique_ptr< PowerController > m_intelGpuPowerController;

  // --- CPU power state ---
  std::unique_ptr< IntelRAPLController > m_intelRAPLCpu;
  std::unique_ptr< PowerController > m_cpuPowerController;
  bool m_RAPLConstraint0Status;
  bool m_RAPLConstraint1Status;
  bool m_RAPLConstraint2Status;
  CpuPowerCallback m_cpuPowerUpdateCallback;
  std::function< bool() > m_getSensorDataCollectionStatus;

  // --- CPU frequency callback ---
  CpuFrequencyCallback m_cpuFrequencyCallback;

  // --- Prime state ---
  std::function< void( const std::string & ) > m_setPrimeState;
  bool m_primeSupported;

  // --- Webcam state ---
  WebcamHwReader m_webcamHwReader;
  WebcamStatusCallback m_webcamStatusCallback;

  // --- Cycle counters for staggered polling ---
  uint32_t m_cycleCounter;

  // GPU methods
  void initGpu();
  [[nodiscard]] bool checkAmdIGpuHwmonPath() noexcept;
  [[nodiscard]] bool checkAmdDGpuHwmonPath() noexcept;
  [[nodiscard]] std::string getAmdIGpuHwmonPathImpl() const noexcept;
  [[nodiscard]] std::string getAmdDGpuHwmonPathImpl() const noexcept;
  [[nodiscard]] std::string getIntelIGpuDrmPathImpl() const noexcept;
  [[nodiscard]] bool checkNvidiaSmiInstalledImpl() const noexcept;
  [[nodiscard]] IGpuInfo getIGpuValues() noexcept;
  [[nodiscard]] IGpuInfo getIntelIGpuValues( const IGpuInfo &base ) const noexcept;
  [[nodiscard]] IGpuInfo getAmdIGpuValues( const IGpuInfo &base ) const noexcept;
  [[nodiscard]] DGpuInfo getDGpuValues() noexcept;
  [[nodiscard]] DGpuInfo getNvidiaDGpuValues() const noexcept;
  [[nodiscard]] DGpuInfo getAmdDGpuValues( const DGpuInfo &base ) const noexcept;
  [[nodiscard]] DGpuInfo parseNvidiaOutput( const std::string &output ) const noexcept;
  [[nodiscard]] double parseNumberWithMetric( const std::string &value ) const noexcept;
  [[nodiscard]] double parseMaxAmdFreq( const std::string &frequencyString ) const noexcept;

  // CPU power methods
  void initCpuPower();
  void updateCpuPower();
  [[nodiscard]] double getCpuCurrentPower();
  [[nodiscard]] double getCpuMaxPowerLimit();

  // Prime methods
  void initPrime();
  void updatePrimeStatus() noexcept;
  [[nodiscard]] bool checkPrimeSupported() const noexcept;
  [[nodiscard]] std::string checkPrimeStatus() const noexcept;
  [[nodiscard]] std::string transformPrimeStatus( const std::string &status ) const noexcept;

  // Webcam methods
  void updateWebcamStatus() noexcept;

  // CPU frequency methods
  void updateCpuFrequency() noexcept;
};
