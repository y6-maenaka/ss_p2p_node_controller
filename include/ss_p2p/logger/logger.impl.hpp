#ifndef AF69C01A_46AB_4042_87F0_754B376288C5
#define AF69C01A_46AB_4042_87F0_754B376288C5


#include "./logger_params.hpp"


namespace 
{


inline s_logger::s_logger( const s_logger::log_level &level, const char* file_name, const int &line_n )
{
  _current_log_level = level;
  _current_file_name = file_name;
  _current_line_n = line_n;

  // 出力ファイルの設定
  _path_to_log_output_file = LOGGER_OUTFILE_FOLDER;
  _path_to_log_output_file += "/";
  _path_to_log_output_file += LOGGER_OUTFILE_NAME_FORMAT;
  _path_to_log_output_file = s_logger::utils::replace( _path_to_log_output_file, "{EXE}", "ss_p2p_node_controller" );
}

inline s_logger::~s_logger()
{
  return;
}

inline bool s_logger::is_muted()
{
  return _is_muted;
}

template < typename T > inline void s_logger::log( const T& data )
{
  std::stringstream ss;
  ss << data;

  if( ss.str().empty() ) return;
  return print_format( std::move(ss) );
}

inline void s_logger::print_format( std::stringstream ss )
{
  std::string prefix_str = get_prefix();
  std::cout << prefix_str << "  " << ss.str() << "\n";
}

inline void s_logger::new_line()
{
  std::cout << "\n";
}

inline std::string s_logger::get_prefix()
{
  std::string ret = LOGGER_PREFIX_FORMAT;

  std::string current_time_str = s_logger::utils::get_current_time_str();
  std::stringstream current_time_ss; current_time_ss << "[" << current_time_str << "]";

  std::stringstream serity_ss;
  std::string serity_str = get_serity_str( _current_log_level );
  std::string serity_color_code = get_serity_color_code( _current_log_level );
  serity_ss << "[" << serity_color_code << serity_str << get_color_code( s_logger::bg_color::RESET ) << "]";

  std::stringstream file_info_ss;
  std::string file_name = _current_file_name;
  file_info_ss << file_name << ":" << _current_line_n;

  /*std::stringstream thread_ss;
  auto thread_id = std::this_thread::get_id();
  thread_ss << thread_id; */

  ret = s_logger::utils::replace( ret, "{TIME}", current_time_ss.str() );
  ret = s_logger::utils::replace( ret, "{USER_HEADER}", "" );
  ret = s_logger::utils::replace( ret, "{SERITY}", serity_ss.str() );
  ret = s_logger::utils::replace( ret, "{FILEINFO}", file_info_ss.str() );
  // ret = s_logger::utils::replace( ret , "{THREAD}", thread_ss.str() );

  return ret;
}

inline std::string s_logger::get_color_code( s_logger::bg_color c )
{
  switch( c )
  {
	case s_logger::bg_color::RESET : return "\x1b[0m";
	case s_logger::bg_color::BG_RED : return "\x1b[41m";
	case s_logger::bg_color::BG_GREEN : return "\x1b[42m";
	case s_logger::bg_color::BG_YELLOW : return "\x1b[43m";
	case s_logger::bg_color::BG_BLUE : return "\x1b[44m";
	case s_logger::bg_color::BG_MAGENTA : return "\x1b[45m";
	case s_logger::bg_color::BG_GREY : return "\x1b[46m";
  }
  return "";
}

inline std::string s_logger::get_color_code( s_logger::fg_color c )
{
  switch( c )
  {
	case s_logger::fg_color::RESET : return "\x1b[0m";
	case s_logger::fg_color::FG_RED : return "\x1b[31m";
	case s_logger::fg_color::FG_GREEN : return "\x1b[32m";
	case s_logger::fg_color::FG_YELLOW : return "\x1b[33m";
	case s_logger::fg_color::FG_BLUE : return "\x1b[34m";
	case s_logger::fg_color::FG_MAGENTA : return "\x1b[35m";
	case s_logger::fg_color::FG_GREY : return "\x1b[36m";
  }
  return "";
}

inline std::string s_logger::get_serity_str( s_logger::log_level ll )
{
  switch( ll )
  {
	case s_logger::log_level::EMERGENCY : return "EMERGENCY";
	case s_logger::log_level::ALERT : return "ALERT";
	case s_logger::log_level::CRITICAL : return "CRITICAL";
	case s_logger::log_level::ERROR : return "ERROR";
	case s_logger::log_level::WARNING : return "WARNING";
	case s_logger::log_level::NOTICE : return "NOTICE";
	case s_logger::log_level::INFO : return "INFO";
	case s_logger::log_level::DEBUG : return "DEBUG";
	case s_logger::log_level::INVALID : return "INVALID";
  }
  return "";
}

inline std::string s_logger::get_serity_color_code( s_logger::log_level ll )
{
  std::stringstream ret_ss;
  switch( ll )
  {
	case s_logger::log_level::EMERGENCY :
	  ret_ss << get_color_code(s_logger::bg_color::BG_RED);
	  break;
	case s_logger::log_level::ALERT :
	  ret_ss << get_color_code(s_logger::bg_color::BG_YELLOW);
	  break;
	case s_logger::log_level::CRITICAL : 
	  ret_ss << get_color_code(s_logger::bg_color::BG_YELLOW);
	  break;
  }

  switch( ll )
  {
	case s_logger::log_level::CRITICAL : 
	  ret_ss << get_color_code( s_logger::fg_color::FG_RED );
	  break;
	case s_logger::log_level::ERROR :
	  ret_ss << get_color_code( s_logger::fg_color::FG_RED );
	  break;
	case s_logger::log_level::WARNING : 
	  ret_ss << get_color_code( s_logger::fg_color::FG_YELLOW );
	  break;
	case s_logger::log_level::INFO :
	  ret_ss << get_color_code( s_logger::fg_color::FG_GREEN );
	  break;
	case s_logger::log_level::DEBUG : 
	  ret_ss << get_color_code( s_logger::fg_color::FG_MAGENTA );
	  break;
  }

  return ret_ss.str();
}



inline std::string s_logger::utils::replace( std::string base, std::string rpl_from, std::string rpl_to )
{
  std::string ret = base;
  std::size_t idx = 0;
  while( (idx = ret.find(rpl_from, idx)) != std::string::npos ){
	ret.replace( idx, rpl_from.length(), rpl_to );
  }
  
  return ret;
}

inline std::string s_logger::utils::get_current_time_str()
{
  std::time_t current_time_t = std::time(nullptr);
  struct std::tm* time_info = std::localtime(&current_time_t);

  char temp[256];
  std::strftime( temp, sizeof(temp), LOGGER_TIME_FORMAT, time_info );

  std::string ret(temp);
  return ret;
}

inline std::size_t s_logger::term_io::get_console_width()
{
  struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  return ws.ws_col;
}



};


#endif 
