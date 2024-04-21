#ifndef A026CD10_BE1A_4B41_AAAC_E0E3C1E4D5FB
#define A026CD10_BE1A_4B41_AAAC_E0E3C1E4D5FB


// loggerが止めてあるか,is_print=falseでない時に任意のloggerを作成する
// #define S_LOG_DISPATCHER( log_level, is_print )  (s_logger(((!(is_print) || s_logger::is_muted()) ? s_logger::log_level::INVALID : log_level), __FILE__, __LINE__ ))
#define S_LOG_DISPATCHER( log_level, is_print )  (s_logger( log_level, FILENAME, __LINE__ ))

#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define S_LOG_EMERGENCY_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::EMERGENCY, is_print))
#define S_LOG_ALERT_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::ALERT, is_print))
#define S_LOG_CRITICAL_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::CRITICAL, is_print))
#define S_LOG_ERROR_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::ERROR, is_print))
#define S_LOG_WARNING_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::WARNING, is_print))
#define S_LOG_NOTICE_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::NOTICE, is_print))
#define S_LOG_INFO_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::INFO, is_print))
#define S_LOG_DEBUG_IMPL(is_print) (S_LOG_DISPATCHER(s_logger::log_level::DEBUG, is_print))


#endif 
