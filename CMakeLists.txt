cmake_minimum_required( VERSION 3.27 )

project( SS_P2P CXX )

find_package( PkgConfig REQUIRED ) # 外部パッケージを検出する準備(opensslは外部パッケージ(?))
find_package( OpenSSL REQUIRED )
find_package( Boost REQUIRED COMPONENTS system ) # brew install boost でboost関連パッケージのインストールが必要

if( OpenSSL_FOUND )
  message( STATUS "Using OpenSSL" ${OPENSSL_VERSION} )
endif( OpenSSL_FOUND )
if( Boost_FOUND )
  message( STATUS "Using Boost" ${Boost_VERSION} )
endif( Boost_FOUND )

# .hpp, .cppをまとめる
set( basic_include_files
	  include/json.hpp
)
set( ss_p2p_include_files
    node_controller.hpp
	peer.hpp
	endpoint.hpp
	message.hpp
	message_pool.hpp
	socket_manager.hpp
	udp_server.hpp
)
set( ss_p2p_source_files
    node_controller.cpp
	peer.cpp
	endpoint.cpp
	message.cpp
	message_pool.cpp
	socket_manager.cpp
	udp_server.cpp
)
set( ss_p2p_kademlia_include_files
    k_routing_table.hpp
    k_bucket.hpp
    k_node.hpp
	dht_manager.hpp
	node_id.hpp
	observer.hpp
)
set( ss_p2p_kademlia_source_files
    k_routing_table.cpp
    k_bucket.cpp
    k_node.cpp
	dht_manager.cpp
	node_id.cpp
	observer.cpp
)

# リスト要素にプレフィックスを付与
list(TRANSFORM ss_p2p_include_files PREPEND "include/ss_p2p/" )
list(TRANSFORM ss_p2p_source_files PREPEND "src/ss_p2p/")
list(TRANSFORM ss_p2p_kademlia_include_files PREPEND "include/ss_p2p/kademlia/")
list(TRANSFORM ss_p2p_kademlia_source_files PREPEND "src/ss_p2p/kademlia/")


add_library( ss_p2p # ss_p2pをライブラリとしてまとめる
			${basic_include_files}
            ${ss_p2p_include_files}
            ${ss_p2p_source_files}

)
# 共通のインストールディレクトリとソールディレクトリの指定
target_include_directories( ss_p2p PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>

			${OPENSSL_INCLUDE_DIRS}
			${Boost_INCLUDE_DIRS}
			include/json.hpp # use nlohmann_json
)

# if KADEMLIA FLAG
target_sources( ss_p2p PRIVATE
              ${ss_p2p_kademlia_source_files}
              ${ss_p2p_kademlia_include_files}

)

if( _BUILD_SS )
  add_executable( ver_0 ./release/ver_0.cpp )
  target_link_libraries( ver_0 PRIVATE )
endif()

if( _BUILD_TEST )
  add_executable( run_0 ./run/run_0.cpp )
  target_link_libraries( run_0 PRIVATE
                        ss_p2p
						${OPENSSL_LIBRARIES}
   )
endif()



add_definitions( -std=c++20 )
