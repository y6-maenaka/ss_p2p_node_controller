#ifndef E5188D95_D8A9_4E6C_A813_A3A531B512A3
#define E5188D95_D8A9_4E6C_A813_A3A531B512A3


namespace ss
{


template <typename... Args > void ss_logger::log_packet( const logger::log_level &ll, const packet_direction &pd, const ip::udp::endpoint &ep, Args&& ... args )
{
  std::string logger_outfile_format = LOGGER_OUTFILE_FORMAT;

  std::string current_time_str = logger::utils::get_current_time_str( LOGGER_OUTFILE_TIME_FORMAT );
  std::string logger_outfile_name = logger::utils::replace( logger_outfile_format, "{EXE}", SS_LOGGER_PACKET_OUTFILE_NAME);
  std::cout << logger_outfile_name  << "\n";
  logger_outfile_name = logger::utils::replace( logger_outfile_name, "{DATE}", current_time_str );

  std::string console_prefix_str = get_prefix( ll, true ); console_prefix_str += "({PACK_DIRECTION})({PACK_PROTOCOL})[{IP}] ";
  std::string outfile_prefix_str = get_prefix( ll, false ); outfile_prefix_str += "({PACK_DIRECTION})({PACK_PROTOCOL})[{IP}] ";

  if( pd == ss_logger::packet_direction::INCOMING ){
	console_prefix_str = logger::utils::replace( console_prefix_str, "{PACK_DIRECTION}", "RECEIVE" );
	outfile_prefix_str = logger::utils::replace( outfile_prefix_str , "{PACK_DIRECTION}", "RECEIVE" );
  } 
  else {
	console_prefix_str = logger::utils::replace( console_prefix_str, "{PACK_DIRECTION}", "SEND" );
	outfile_prefix_str = logger::utils::replace( outfile_prefix_str , "{PACK_DIRECTION}", "SEND" );
  }

  console_prefix_str = logger::utils::replace( console_prefix_str, "{PACK_PROTOCOL}", "UDP" ); // 一旦仮で
  outfile_prefix_str = logger::utils::replace( outfile_prefix_str, "{PACK_PROTOCOL}", "UDP" );
  
  console_prefix_str = logger::utils::replace( console_prefix_str, "{IP}", ep.address().to_string() );
  outfile_prefix_str = logger::utils::replace( outfile_prefix_str, "{IP}", ep.address().to_string() );

  std::stringstream logger_outfile_path; logger_outfile_path << LOGGER_OUTFILE_DIR << "/" << logger_outfile_name;
  std::ofstream ofs;
  ofs.open( logger_outfile_path.str(), std::ios::app );

  ofs << outfile_prefix_str;
  ((ofs << std::forward<Args>(args) << ' '), ... ); ofs << "\n";

  std::cout << console_prefix_str;
  ((std::cout << std::forward<Args>(args) << ' '), ... ); std::cout << "\n";
}

/*
 [INFO][22:10:54] [SEND](UDP) 
*/


};


#endif
