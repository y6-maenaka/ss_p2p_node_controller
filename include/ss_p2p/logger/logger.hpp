#ifndef A3FBC2A6_E266_4588_B378_06DDF759EA56
#define A3FBC2A6_E266_4588_B378_06DDF759EA56


#include <sys/ioctl.h>
#include <mutex>
#include <ctime>
#include <thread>
#include <ss_p2p/logger/logger_utils.hpp>


#define S_LOG_EMERGENCY S_LOG_EMERGENCY_IMPL(true)
#define S_LOG_ALERT S_LOG_ALERT_IMPL(true)
#define S_LOG_CRITICAL S_LOG_CRITICAL_IMPL(true)
#define S_LOG_ERROR S_LOG_ERROR_IMPL(true)
#define S_LOG_WARNING S_LOG_WARNING_IMPL(true)
#define S_LOG_NOTICE S_LOG_NOTICE_IMPL(true)
#define S_LOG_INFO S_LOG_INFO_IMPL(true)
#define S_LOG_DEBUG S_LOG_DEBUG_IMPL(true)


namespace 
{


class s_logger
{
public:
  enum class log_level
  {
	EMERGENCY // system is unusable.
	 , ALERT // action must be taken immediately.
	 , CRITICAL // critical conditions.
	 , ERROR // runtime errors that do not require immediate action but should typically be logged and monitored.
	 , WARNING // exceptional occurrences that are not errors.
	 , NOTICE // normal but significant events.
	 , INFO // interesting events.
	 , DEBUG // detailed debug information
	 , INVALID // loggerがmutedもしくはprintが許可されいない時
  };
  enum class fg_color
  {
	RESET
	  , FG_RED
	  , FG_GREEN
	  , FG_YELLOW
	  , FG_BLUE
	  , FG_MAGENTA
	  , FG_GREY
  };
  enum class bg_color
  {
	RESET // 一応こっちにも作っとく
	  , BG_RED
	  , BG_GREEN
	  , BG_YELLOW
	  , BG_BLUE
	  , BG_MAGENTA
	  , BG_GREY
  };

  template < typename T > inline void log( const T& data ); // ログ情報はターミナル,ファイルどちらにも書き込まれる
  template < typename T > inline void log_console( const T& data, std::ostream &s = std::cout ); // 標準出力/エラー出力にのみ書き出す
  template < typename T > inline void log_file( const T& data ); // logファイルにのみ書き出す


  static inline bool is_muted();
  static inline void print_format( std::stringstream ss );
  static inline std::string get_prefix( bool is_graphical = false );
  static inline void new_line();

  inline s_logger( const s_logger::log_level &level, const char* file_name, const int &line_n );
  virtual inline ~s_logger();

protected:
  struct utils;
  struct console_io;
  struct outputfile_io;

  static inline std::string get_color_code( s_logger::bg_color c );
  static inline std::string get_color_code( s_logger::fg_color c );
  static inline std::string get_serity_str( s_logger::log_level ll );
  static inline std::string get_serity_color_code( s_logger::log_level ll );

private:
  static inline bool _is_newline = true;
  static inline bg_color _current_bg_color = bg_color::RESET;
  static inline fg_color _current_fg_color = fg_color::RESET;
  static inline std::string _current_file_name;
  static inline int _current_line_n = -1;
  static inline std::string _current_current_prefix;
  static inline log_level _current_log_level = log_level::INFO;
  static inline std::string _path_to_log_output_file;
  static inline std::ofstream _log_output_s;
  static inline bool _is_muted = false; // 基本的にtrueにしない
  static inline std::mutex _mtx;
  std::lock_guard<std::mutex> _lock_{_mtx};
};

struct s_logger::utils
{ 
  static inline std::string replace( std::string base, std::string rpl_from, std::string rpl_to );
  static inline std::string get_current_time_str();
};

struct s_logger::console_io
{
  static inline std::size_t get_console_width();
  static inline void write( std::string str, std::ostream &s = (std::cout) );
};

struct s_logger::outputfile_io
{
  static inline void write( std::ofstream ofs, std::string str );
};


};


#include "./logger.impl.hpp"


// [参考] (simple-cpp-logger) https://github.com/nadrino/simple-cpp-logger


#endif 
