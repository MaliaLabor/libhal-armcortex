#pragma once

#include <array>
#include <cstdint>

#include <libembeddedhal/config.hpp>
#include <libembeddedhal/counter/counter.hpp>
#include <libembeddedhal/overflow_counter.hpp>

namespace embed::cortex_m {
/**
 * @brief A counter with a frequency fixed to the CPU clock rate.
 *
 * This driver is supported for Cortex M3 devices and above.
 *
 */
class dwt_counter : public embed::counter
{
public:
  /// Structure type to access the Data Watchpoint and Trace Register (DWT).
  struct dwt_registers_t
  {
    /// Offset: 0x000 (R/W)  Control Register
    volatile uint32_t ctrl;
    /// Offset: 0x004 (R/W)  Cycle Count Register
    volatile uint32_t cyccnt;
    /// Offset: 0x008 (R/W)  CPI Count Register
    volatile uint32_t cpicnt;
    /// Offset: 0x00C (R/W)  Exception Overhead Count Register
    volatile uint32_t exccnt;
    /// Offset: 0x010 (R/W)  Sleep Count Register
    volatile uint32_t sleepcnt;
    /// Offset: 0x014 (R/W)  LSU Count Register
    volatile uint32_t lsucnt;
    /// Offset: 0x018 (R/W)  Folded-instruction Count Register
    volatile uint32_t foldcnt;
    /// Offset: 0x01C (R/ )  Program Counter Sample Register
    volatile const uint32_t pcsr;
    /// Offset: 0x020 (R/W)  Comparator Register 0
    volatile uint32_t comp0;
    /// Offset: 0x024 (R/W)  Mask Register 0
    volatile uint32_t mask0;
    /// Offset: 0x028 (R/W)  Function Register 0
    volatile uint32_t function0;
    /// Reserved 0
    std::array<uint32_t, 1> reserved0;
    /// Offset: 0x030 (R/W)  Comparator Register 1
    volatile uint32_t comp1;
    /// Offset: 0x034 (R/W)  Mask Register 1
    volatile uint32_t mask1;
    /// Offset: 0x038 (R/W)  Function Register 1
    volatile uint32_t function1;
    /// Reserved 1
    std::array<uint32_t, 1> reserved1;
    /// Offset: 0x040 (R/W)  Comparator Register 2
    volatile uint32_t comp2;
    /// Offset: 0x044 (R/W)  Mask Register 2
    volatile uint32_t mask2;
    /// Offset: 0x048 (R/W)  Function Register 2
    volatile uint32_t function2;
    /// Reserved 2
    std::array<uint32_t, 1> reserved2;
    /// Offset: 0x050 (R/W)  Comparator Register 3
    volatile uint32_t comp3;
    /// Offset: 0x054 (R/W)  Mask Register 3
    volatile uint32_t mask3;
    /// Offset: 0x058 (R/W)  Function Register 3
    volatile uint32_t function3;
  };

  /// Structure type to access the Core Debug Register (CoreDebug)
  struct core_debug_registers_t
  {
    /// Offset: 0x000 (R/W)  Debug Halting Control and Status Register
    volatile uint32_t dhcsr;
    /// Offset: 0x004 ( /W)  Debug Core Register Selector Register
    volatile uint32_t dcrsr;
    /// Offset: 0x008 (R/W)  Debug Core Register Data Register
    volatile uint32_t dcrdr;
    /// Offset: 0x00C (R/W)  Debug Exception and Monitor Control Register
    volatile uint32_t demcr;
  };

  /**
   * @brief This bit must be set to 1 to enable use of the trace and debug
   * blocks:
   *
   *   - Data Watchpoint and Trace (DWT)
   *   - Instrumentation Trace Macrocell (ITM)
   *   - Embedded Trace Macrocell (ETM)
   *   - Trace Port Interface Unit (TPIU).
   */
  static constexpr unsigned core_trace_enable = 1 << 24U;

  /// Mask for turning on cycle counter.
  static constexpr unsigned enable_cycle_count = 1 << 0;

  /// Address of the hardware DWT registers
  static constexpr intptr_t dwt_address = 0xE0001000UL;

  /// Address of the Cortex M CoreDebug module
  static constexpr intptr_t core_debug_address = 0xE000EDF0UL;

  /// @return auto* - Address of the DWT peripheral
  static auto* dwt() noexcept
  {
    if constexpr (embed::is_a_test()) {
      static dwt_registers_t dummy_dwt{};
      return &dummy_dwt;
    }
    return reinterpret_cast<dwt_registers_t*>(dwt_address);
  }

  /// @return auto* - Address of the Core Debug module
  static auto* core() noexcept
  {
    if constexpr (embed::is_a_test()) {
      static core_debug_registers_t dummy_core{};
      return &dummy_core;
    }
    return reinterpret_cast<core_debug_registers_t*>(core_debug_address);
  }

  /**
   * @brief Construct a new dwt counter object
   *
   * @param p_cpu_frequency - the operating frequency of the CPU
   */
  dwt_counter(frequency p_cpu_frequency) noexcept
    : m_cpu_frequency(p_cpu_frequency)
    , m_count{}
  {
    core()->demcr = (core()->demcr | core_trace_enable);

    // No need to check return value since this function never fails
    (void)driver_control(controls::reset);
    (void)driver_control(controls::start);
  }

  /**
   * @brief Inform the driver of the operating frequency of the CPU in order to
   * generate the correct uptime.
   *
   * Use this when the CPU's operating frequency has changed and no longer
   * matches the frequency supplied to the constructor. Care should be taken
   * when execting this function when there is the potentially other parts of
   * the system that depend on this counter's uptime to operate.
   *
   * @param p_cpu_frequency - the operating frequency of the CPU
   */
  void register_cpu_frequency(frequency p_cpu_frequency) noexcept
  {
    m_cpu_frequency = p_cpu_frequency;
  }

  /**
   * @return boost::leaf::result<bool> returns true if the counter is running.
   * Never returns an error.
   */
  bool driver_is_running() noexcept override
  {
    return (dwt()->ctrl & enable_cycle_count) != 0;
  }

  /**
   * @brief Control the behavior of the counter
   *
   * @param p_control - counter control
   * @return boost::leaf::result<void> - this driver's implementation never
   * returns an error.
   */
  boost::leaf::result<void> driver_control(controls p_control) noexcept override
  {
    switch (p_control) {
      case controls::start:
        dwt()->ctrl = (dwt()->ctrl | enable_cycle_count);
        break;
      case controls::stop:
        dwt()->ctrl = (dwt()->ctrl & ~enable_cycle_count);
        break;
      case controls::reset:
        dwt()->cyccnt = 0;
        m_count.reset();
        break;
    }
    return {};
  }

  /**
   * @brief Return the number of nanoseconds since the counter has started.
   *
   * @return std::chrono::nanoseconds - nanoseconds since the counter has
   * started.
   */
  std::chrono::nanoseconds driver_uptime() noexcept override
  {
    return m_cpu_frequency.duration_from_cycles(m_count.update(dwt()->cyccnt));
  }

private:
  frequency m_cpu_frequency{ 1'000'000 };
  overflow_counter<32> m_count{};
};
}  // namespace embed::cortex_m
