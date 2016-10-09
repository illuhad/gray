#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <ctime>

namespace gray {
  
class timer
{
public:
  timer()
  : _is_running{false}
  {}

  inline
  bool is_running() const 
  {return _is_running;}

  void start()
  {
    _is_running = true;
    _start = std::chrono::high_resolution_clock::now();
  }

  double stop()
  {
    if(!_is_running)
      return 0.0;

    _stop = std::chrono::high_resolution_clock::now();
    _is_running = false;

    auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(_stop - _start).count();
    return static_cast<double>(ticks) * 1.e-9;
  }

private:
  using time_point_type = 
    std::chrono::time_point<std::chrono::high_resolution_clock>;
  time_point_type _start;
  time_point_type _stop;

  bool _is_running;
};

}

#endif