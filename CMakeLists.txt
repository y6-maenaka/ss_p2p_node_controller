cmake_minimum_required( VERSION 3.20 )

project( SS_P2P CXX )

# find_package( PkgConfig REQUIRED ) # 外部パッケージを検出する準備(opensslは外部パッケージ(?))
find_package( OpenSSL REQUIRED )
find_package( Boost REQUIRED )

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
	  include/hash.hpp
	  include/utils.hpp
)
set( basic_source_files 
	  src/utils.cpp
)
set( ss_p2p_include_files
    node_controller.hpp
	peer.hpp
	endpoint.hpp
	message.hpp
	message_pool.hpp
	socket_manager.hpp
	udp_server.hpp
	observer.hpp
	observer_strage.hpp
	sender.hpp
)
set( ss_p2p_source_files
    node_controller.cpp
	peer.cpp
	endpoint.cpp
	message.cpp
	message_pool.cpp
	socket_manager.cpp
	udp_server.cpp
	observer.cpp
	observer_strage.cpp
	sender.cpp
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
	k_observer_strage.cpp
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
	ice_observer_strage.cpp
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

if( _BUILD_SS )
  add_executable( host_node ./run/host_node.cpp )
  target_link_libraries( host_node PRIVATE 
						  ss_p2p
						  ${OPENSSL_LIBRARIES}
	)
endif()

if( _BUILD_DEBUG )
  add_executable( node_0 ./run/node_0.cpp )
   target_link_libraries( node_0 PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )

  add_executable( node_1 ./run/node_1.cpp )
   target_link_libraries( node_1 PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )

  add_executable( node_2 ./run/node_2.cpp )
   target_link_libraries( node_2 PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )

  add_executable( node_3 ./run/node_3.cpp )
   target_link_libraries( node_3 PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )

  add_executable( node_4 ./run/node_4.cpp )
   target_link_libraries( node_4 PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )

  add_executable( stable_host ./run/stable_host.cpp )
   target_link_libraries( stable_host PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )

  add_compile_definitions( SS_VERBOSE )
  add_compile_definitions( SS_CAPTURE_PACKET )
  add_compile_definitions( SS_DEBUG )
endif()


add_definitions( -std=c++20 )
