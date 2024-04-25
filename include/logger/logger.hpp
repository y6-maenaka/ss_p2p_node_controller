#ifndef DB3FFA53_9B67_4240_B685_D9ACE209F838
#define DB3FFA53_9B67_4240_B685_D9ACE209F838


#if !defined(LOGGING_DISABLE) || !defined(LOGGER_OUTPUT_FILENAME)

#ifndef LOGGER_OUTFILE_DIR
#define LOGGER_OUTFILE_DIR "."
#endif 

#define LOGGER_OUTFILE_NAME "d_ss"

#endif

#include <iostream>
#include <mutex>
#include <fstream>
#include <iostream>
#include <stdarg.h>


namespace 
{


class logger
{
public:
  enum log_level
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

  template <typename... Args > inline void log( const log_level &ll, Args&&... args ); // 書き出しファイル設定がここで決定するので純粋仮想の方がいいかも
  template <typename... Args > void code_log( const log_level &ll, const char* file_name, const int &line_n, Args&&... args );
  
  void set_max_log_level( const logger::log_level &level ); // このログレベル以下のログに関しては何も処理しない
  void set_custom_header( std::string target );
  inline bool is_muted();

protected:
  bool inline should_log(); // ログを書き出す必要があるか否かを判定する
  
  inline std::string get_prefix( const log_level& ll, bool is_graphical = false /*ファイルに書き込む時は文字コードを挿入しない*/ ); // logのprefixを取得
  struct utils;
  struct console_io;
  struct file_io;

private:
  bool _is_muted;
  log_level _max_log_level = log_level::DEBUG;
  std::string _custom_header = "";
  std::ofstream _output_of;

  // 排他制御
  std::mutex _mtx;
  std::unique_lock<std::mutex> _lock{_mtx};
};

struct logger::utils
{
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

  static inline std::string replace( std::string base, std::string rpl_from, std::string rpl_to );
  static inline std::string get_current_time_str( std::string fmt = "" );
  static inline std::string get_serity_str( const log_level &ll );
  static inline std::string get_serity_color_code( const log_level &ll );
  static inline std::string get_color_code( const fg_color &fc );
  static inline std::string get_color_code( const bg_color &bc );
  static inline std::string get_cerity_color_code( const log_level &ll );
};

struct logger::console_io
{
  void print( std::string target, std::ostream &stream = (std::cout) );
};

struct logger::file_io
{
  void print( std::string target, std::ofstream &ofs );
};


}; // namesapce ""


#include "logger.impl.hpp"


#endif 
