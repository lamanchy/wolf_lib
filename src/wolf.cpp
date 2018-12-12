#include <base/plugin.h>
#include <base/pipeline.h>
#include <plugins/cout.h>
#include <plugins/generator.h>
#include <plugins/tcp_in.h>
#include <serializers/line.h>
#include <plugins/string_to_json.h>
#include <plugins/kafka_out.h>
#include <plugins/collate.h>
#include <date/tz.h>
#include <plugins/ysoft/add_local_info.h>
#include <plugins/json_to_string.h>
#include <plugins/ysoft/normalize_nlog_logs.h>
#include <plugins/ysoft/normalize_serilog_logs.h>
#include <plugins/ysoft/normalize_log4j2_logs.h>
#include <plugins/tcp_out.h>
#include <plugins/lambda.h>
#include <whereami/whereami.h>
#include <extras/logger.h>
#include <plugins/kafka_in.h>
#include <plugins/http_out.h>
#include <plugins/stats.h>


std::string get_dir_path() {

  char *path = NULL;
  int length, dirname_length;
  std::string dir_path;

  length = wai_getExecutablePath(NULL, 0, &dirname_length);
  if (length > 0) {
    path = (char *) malloc(length + 1);
    if (!path)
      abort();
    wai_getExecutablePath(path, length, &dirname_length);
    path[length] = '\0';

//    printf("executable path: %s\n", path);
    char separator = path[dirname_length];
    path[dirname_length] = '\0';
//    printf("  dirname: %s\n", path);
//    printf("  basename: %s\n", path + dirname_length + 1);
    dir_path = std::string(path) + separator;
    free(path);
  } else {
    throw std::runtime_error("coulndt get path");
  }
  return dir_path;
}

int main(int argc, char *argv[]) {
  using namespace wolf;

  Logger::setupLogger(get_dir_path());
  Logger &logger = Logger::getLogger();
  logger.info("Configuring STXXL");
  stxxl::config *cfg = stxxl::config::get_instance();
  // create a disk_config structure.
  stxxl::disk_config
      disk(get_dir_path() + "queue.tmp", 0, get_dir_path()[get_dir_path().size() - 1] == '/' ? "syscall" : "wincall");
  disk.autogrow = true;
  disk.unlink_on_open = true;
  disk.delete_on_exit = true;
  disk.direct = stxxl::disk_config::DIRECT_TRY;
  cfg->add_disk(disk);

//  json_to_string jts = json_to_string();
//  test_create();

//  std::shared_ptr<plugin> test = test_create(std::move(jts));
//  test->print_name();

//  test->print_name();

//  pipeline test = pipeline(argc, argv).add(
//      create<tcp_in<line>>("nlog"),
//      create<string_to_json>(),
//      create<normalize_nlog_logs>(),
//      create<add_local_info>(),
//      create<json_to_string>(),
//      create<kafka_out>("test", 1)
//  );

  logger.info("Parsing command line arguments");
  cxxopts::Options opts(argv[0], " - example command line options");

  std::string output, output_ip, group;
  opts.add_options()
      ("output", "Type of output, kafka/logstash", cxxopts::value<std::string>(output)->default_value("kafka"))
      ("output_ip", "Ip address of output", cxxopts::value<std::string>(output_ip)->default_value("10.0.11.162"))
      ("group", "Define the group name", cxxopts::value<std::string>(group)->default_value("default"));
  opts.parse(argc, argv);

  logger.info("Parsed arguments:");
  logger.info("output:    " + output);
  logger.info("output_ip: " + output_ip);
  logger.info("group:     " + group);


  std::function<plugin::pointer(std::string)> out;
  plugin::pointer tcp = create<tcp_out>(output_ip, "9070");

  if (output == "kafka") {
    out = [&](std::string type) { return create<kafka_out>(type + "-" + group, 1, output_ip + ":9092"); };
  } else if (output == "logstash") {
    out = [&](std::string type) { return tcp; };
  } else {
    throw std::runtime_error("output is not kafka nor logstash but " + output);
  }
//  out = [&](std::string type) { return create<cout>(); };

  plugin::pointer common_processing =
      create<add_local_info>(group)->register_output(
          create<json_to_string>()->register_output(
              out("unified_logs")
          )
      );

  pipeline p = pipeline(argc, argv, true).register_plugin(
      create<tcp_in<line>>("nlog", 9556)->register_output(
          create<string_to_json>()->register_output(
              create<normalize_nlog_logs>()->register_output(
                  common_processing
              )
          )
      )
  ).register_plugin(
      create<tcp_in<line>>("log4j2", 9555)->register_output(
          create<string_to_json>()->register_output(
              create<normalize_log4j2_logs>()->register_output(
                  common_processing
              )
          )
      )
  ).register_plugin(
      create<tcp_in<line>>("serilog", 9559)->register_output(
          create<string_to_json>()->register_output(
              create<normalize_serilog_logs>()->register_output(
                  common_processing
              )
          )
      )
  ).register_plugin(
      create<tcp_in<line>>("metrics", 9557)->register_output(
          create<lambda>(
              [group](json &message) {
                message.assign_object(
                    {
                        {"message", message},
                        {"group", group},
                        {"type", "metrics"}
                    });
              })->register_output(
              create<json_to_string>()
                  ->register_output(
                      out("metrics")
                  )
          )
      )
  );
  logger.info("Starting");
  p.run();
  logger.info("Stopped");


  return 0;
}
