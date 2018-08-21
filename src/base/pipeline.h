#ifndef WOLF_PIPELINE_H
#define WOLF_PIPELINE_H

#include <csignal>
#include <thread>
#include "plugin.h"
#include <cxxopts.hpp>

namespace wolf {

class pipeline {
public:
  using pointer = plugin::pointer;
  using options = plugin::options;
  using parse_result = plugin::parse_result;

  pipeline(int argc, char *argv[]) :
      opts(argv[0], " - example command line options"),
      argc(argc),
      argv(argv) { }

  pipeline(pipeline const &) = delete;

  pipeline &operator=(pipeline const &) = delete;

  pipeline(pipeline &&) = default;

  pipeline &operator=(pipeline &&) = default;

  pipeline register_plugin(pointer plugin) {
    plugins.push_back(plugin);
    return std::move(*this);
  }

  void run() {
    std::signal(SIGINT, catch_signal);
    evaluate_options();
    start();
    wait();
    stop();
  }

private:
  std::vector<pointer> plugins;
  std::vector<std::thread> processors;
  unsigned number_of_processors = std::thread::hardware_concurrency();

  int argc;
  char **argv;
  options opts;

  void evaluate_options() {
    opts.add_options()
        ("h,help", "Print help");

    opts.add_options("Pipeline options")
        ("t,threads", "Number of processors", cxxopts::value<unsigned>(number_of_processors))
        ("b,buffer_size", "Size of buffer in records", cxxopts::value<unsigned>(plugin::buffer_size));


    for_each_plugin([this](plugin &p) { p.register_options(opts); });

    try {
      parse_result result = opts.parse(argc, argv);

      if (result.count("help")) {
        std::cout << opts.help(opts.groups()) << std::endl;
        exit(0);
      }

      for_each_plugin([& result] (plugin &p) {p.validate_options(result); });

    } catch (const cxxopts::OptionException &e) {
      std::cerr << "error parsing options: " << e.what() << std::endl;
      exit(1);
    }
  }

  template<typename T>
  std::vector<T> for_each_plugin(const std::function<T(plugin &)>& function) {
    std::set<plugin::id_type> visited;
    std::queue<pointer> to_process;
    std::for_each(plugins.begin(), plugins.end(), [&to_process](pointer &ptr) { to_process.push(ptr); });
    std::vector<T> results;

    while (not to_process.empty()) {
      pointer &ptr = to_process.front();
      auto it = visited.find(ptr->id);
      if (it == visited.end()) {
        visited.insert(ptr->id);
        results.push_back(function(*ptr));
        for (auto &pair : ptr->outputs) to_process.push(pair.second);
      }
      to_process.pop();
    }
    return results;
  }

  void for_each_plugin(const std::function<void(plugin &)>& function) {
    const std::function<int(plugin &)> fn([&function] (plugin & p) {
      function(p);
      return 0;
    });
    for_each_plugin<int>(fn);
  }

  void process() {
    plugin::is_thread_processor = true;
    while (plugins_running()) {
      for_each_plugin([](plugin &p) { while (p.process_buffer()) { }; });
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      // TODO build up sleep time
//      std::this_thread::yield();
    }
    std::for_each(plugins.begin(), plugins.end(), [this](pointer &) {
      for_each_plugin([](plugin &p) { while (p.process_buffer()) { }; });
    });

    std::cerr << "ending processor" << std::endl;
  }

  static void catch_signal(int signal) {
    ++interrupt_received;

    if (interrupt_received >= 2) {
      std::quick_exit(EXIT_FAILURE);
    }
  }

  static std::atomic<int> interrupt_received;

  void start() {
    for_each_plugin([](plugin &p) { p.start(); });

    for (unsigned i = 0; i < number_of_processors; ++i) {
      processors.emplace_back(&pipeline::process, this);
    }
  }

  bool plugins_running() {
    // TODO fuck fuck fuck
    std::vector<bool> results = for_each_plugin<bool>([](plugin &p) { return p.is_running(); } );
    return std::any_of(results.begin(), results.end(), [](bool r) { return r; } );
  }

  void wait() {
    while (plugins_running()) {
      sleep(1);
      if (interrupt_received) {
        break;
      }
    }
  }

  void stop() {
    for_each_plugin([](plugin &p) { p.stop(); });
    std::for_each(processors.begin(), processors.end(), [](std::thread &thread) { thread.join(); });
  }

};

std::atomic<int> pipeline::interrupt_received{0};

}

#endif //WOLF_PIPELINE_H
