#ifndef E5188D95_D8A9_4E6C_A813_A3A531B512A3
#define E5188D95_D8A9_4E6C_A813_A3A531B512A3


namespace ss
{


template <typename... Args > void ss_logger::log_packet( const logger::log_level &ll, const packet_direction &pd, const ip::udp::endpoint &dest_ep, Args&& ... args )
{
  std::string logger_outfile_format = LOGGER_OUTFILE_FORMAT;
  std::string current_time_str = logger::utils::get_current_time_str( LOGGER_OUTFILE_FORMAT );
  std::string logger_outfile_name = logger::utils::replace( logger_outfile_format, "{EXE}", SS_LOGGER_PACKET_OUTFILE_NAME);
  logger_outfile_name = logger::utils::replace( logger_outfile_name, "{DATE}", current_time_str );

  std::string console_prefix_str = get_prefix( ll, true );
  std::string outfile_prefix_str = get_prefix( ll, false );

  std::ofstream ofs;
  ofs.open( logger_outfile_name, std::ios::app );

  ofs << outfile_prefix_str;
  ((ofs << std::forward<Args>(args) << ' '), ... ); ofs << "\n";

  std::cout << console_prefix_str;
  ((std::cout << std::forward<Args>(args) << ' '), ... ); std::cout << "\n";
}


};


#endif
