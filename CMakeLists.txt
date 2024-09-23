cmake_minimum_required( VERSION 3.20 )

project( SS_P2P CXX )

# find_package( PkgConfig REQUIRED ) # 外部パッケージを検出する準備(opensslは外部パッケージ(?))
find_package( OpenSSL REQUIRED )
find_package( Boost COMPONENTS chrono date_time timer thread REQUIRED )


if( OpenSSL_FOUND )
  message( STATUS "Using OpenSSL" ${OPENSSL_VERSION} )
  message("OpenSSL include dir : " ${OPENSSL_INCLUDE_DIR})
endif( OpenSSL_FOUND )
if( Boost_FOUND )
  message( STATUS "Using Boost" ${Boost_VERSION} )
endif( Boost_FOUND )

# .hpp, .cppをまとめる
set( basic_include_files
	  include/json.hpp
	  # include/hash.hpp
	  include/utils.hpp
	  include/logger/logger.hpp
	  include/logger/logger.impl.hpp
	  include/logger/logger.utils.hpp
	  include/logger/logger.params.hpp

	  include/crypto_utils/crypto_utils.hpp
)
set( basic_source_files 
	  src/utils.cpp
)
set( ss_p2p_include_files
    node_controller.hpp
	peer.hpp
	message.hpp
	message_pool.hpp
	message_pool.fwd.hpp
	socket_manager.hpp
	udp_server.hpp
	observer.hpp
	observer_strage.hpp
	sender.hpp
	multicast_manager.hpp
	interface.hpp
	ss_logger.hpp
	ss_logger.impl.hpp
)
set( ss_p2p_source_files
    node_controller.cpp
	peer.cpp
	message.cpp
	message_pool.cpp
	socket_manager.cpp
	udp_server.cpp
	observer.cpp
	sender.cpp
	multicast_manager.cpp
	interface.cpp
)
set( ss_p2p_kademlia_include_files
    k_routing_table.hpp
    k_bucket.hpp
    k_node.hpp
	dht_manager.hpp
	node_id.hpp
	k_observer.hpp
	k_observer_strage.hpp
	k_message.hpp
	rpc_manager.hpp
	direct_routing_table_controller.hpp
)
set( ss_p2p_kademlia_source_files
    k_routing_table.cpp
    k_bucket.cpp
    k_node.cpp
	dht_manager.cpp
	node_id.cpp
	k_observer.cpp
	k_message.cpp
	rpc_manager.cpp
	direct_routing_table_controller.cpp
	connection_maintainer.cpp
)
set( ss_p2p_ice_agent_include_files 
	ice_agent.hpp
	ice_observer.hpp
	ice_observer_strage.hpp
	signaling_server.hpp
	stun_server.hpp
	ice_message.hpp
	ice_sender.hpp
)
set( ss_p2p_ice_agent_source_files 
	ice_agent.cpp
	ice_observer.cpp
	signaling_server.cpp
	stun_server.cpp
	ice_message.cpp
	ice_sender.cpp
)

# リスト要素にプレフィックスを付与
list(TRANSFORM ss_p2p_include_files PREPEND "include/ss_p2p/" )
list(TRANSFORM ss_p2p_source_files PREPEND "src/ss_p2p/")
list(TRANSFORM ss_p2p_kademlia_include_files PREPEND "include/ss_p2p/kademlia/")
list(TRANSFORM ss_p2p_kademlia_source_files PREPEND "src/ss_p2p/kademlia/")
list(TRANSFORM ss_p2p_ice_agent_include_files PREPEND "include/ss_p2p/ice_agent/")
list(TRANSFORM ss_p2p_ice_agent_source_files PREPEND "src/ss_p2p/ice_agent/")

add_library( ss_p2p # ss_p2pをライブラリとしてまとめる
			${basic_include_files}
			${basic_source_files}
            ${ss_p2p_include_files}
            ${ss_p2p_source_files}

)
# 共通のインストールディレクトリとソールディレクトリの指定
target_include_directories( ss_p2p PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>

			${OPENSSL_INCLUDE_DIR} # 不要
			${Boost_INCLUDE_DIRS}
)

# if KADEMLIA FLAG
target_sources( ss_p2p PRIVATE
              ${ss_p2p_kademlia_source_files}
              ${ss_p2p_kademlia_include_files}

)
target_sources( ss_p2p PRIVATE 
			  ${ss_p2p_ice_agent_source_files}
			  ${ss_p2p_ice_agent_include_files}
)

add_compile_definitions(DEFAULT_LOGGER_OUTFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/log") # あまりよくないかも
# add_compile_definitions( SS_LOGGING_DISABLE )


function( build_node NODE_SYMBOL ENABLE_DEBUG ENABLE_VERBOSE ENABLE_CAPTURE_PACKET )
  message( "@ BUILD NODE :: " ${NODE_SYMBOL} )

  set( BUILD_TARGET_NODE "${NODE_SYMBOL}" )
  set( BUILD_TARGET_NODE_PATH "./run/${NODE_SYMBOL}.cpp" )

  add_executable( ${BUILD_TARGET_NODE} ${BUILD_TARGET_NODE_PATH} )
  target_link_libraries( ${BUILD_TARGET_NODE} PRIVATE ss_p2p ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} )

  # 良い方法ではない
  add_compile_definitions( SS_VERBOSE ) 
  add_compile_definitions( SS_CAPTURE_PACKET ) 
  add_compile_definitions( SS_DEBUG )
endfunction()


if( _BUILD_DEBUG_NODE )
  build_node( ${_TARGET} FALSE FALSE FALSE )
endif() # BUILD_NODE


if( _BUILD_DEBUG_ALL_NODE )
  build_node( "node_0" FALSE FALSE FALSE )
  build_node( "node_1" FALSE FALSE FALSE )
  build_node( "node_2" FALSE FALSE FALSE )
  build_node( "node_3" FALSE FALSE FALSE )
  build_node( "node_4" FALSE FALSE FALSE )
  build_node( "stable_host" FALSE FALSE FALSE )
endif()


add_definitions( -std=c++20 )
