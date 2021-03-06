#Activer le support c++11 
if(CMAKE_VERSION VERSION_LESS "3.1")
  set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
else ()
  set(CMAKE_CXX_STANDARD 11)
endif ()

#Librairie PTHREAD 
if(CMAKE_VERSION VERSION_GREATER "3.0")
  set(THREADS_PREFER_PTHREAD_FLAG ON)
endif ()
find_package(Threads)
find_package(CURL REQUIRED) 

find_library(LIBPAHO paho-mqtt3a REQUIRED)
add_library(paho-mqtt3a SHARED IMPORTED)
set_target_properties(paho-mqtt3a PROPERTIES
  IMPORTED_LOCATION "${LIBPAHO}"
)
message(STATUS "Library paho-mqtt3a found in ${LIBPAHO}")

find_library(LIBPAHOPP paho-mqttpp3 REQUIRED)
add_library(paho-mqttpp3 SHARED IMPORTED)
set_target_properties(paho-mqttpp3 PROPERTIES
  IMPORTED_LOCATION "${LIBPAHOPP}"
)
message(STATUS "Library paho-mqttpp3 found in ${LIBPAHOPP}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated") # various warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -O2") # no debug, active optimisation

include_directories(${CURL_INCLUDE_DIR})
include_directories(../dependencies/cpr/include)
include_directories(../dependencies/FragFramework/src)
include_directories(../dependencies/jsoncpp/include)
include_directories(../dependencies/MqttDaemon/src)
include_directories(../dependencies/ServiceSkeleton/src)
include_directories(../dependencies/SimpleFolder/src)
include_directories(../dependencies/SimpleIni/include)
include_directories(../dependencies/SimpleLog/src)
 
#Génération de la liste des fichiers sources
file(
  GLOB_RECURSE
  MqttDaemon_files
  ../dependencies/MqttDaemon/src/*
)

file(
  GLOB_RECURSE
  ServiceSkeleton_files
  ../dependencies/ServiceSkeleton/src/*
)

file(
  GLOB_RECURSE
  SimpleFolders_files
  ../dependencies/SimpleFolder/src/*
)

file(
  GLOB_RECURSE
  SimpleIni_files
  ../dependencies/SimpleIni/src/*
)

file(
  GLOB_RECURSE
  SimpleLog_files
  ../dependencies/SimpleLog/src/*
)

file(
  GLOB_RECURSE
  FragFramework_files
  ../dependencies/FragFramework/src/*
)

file(
  GLOB_RECURSE
  Cpr_files
  ../dependencies/cpr/cpr/*
)

list(REMOVE_ITEM
  Cpr_files
  "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/cpr/cpr/multipart.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/cpr/cpr/parameters.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/cpr/cpr/payload.cpp"
)

file(
  GLOB_RECURSE
  JsonCpp_files
  ../dependencies/jsoncpp/src/lib_json/*
)

#Déclaration de l'exécutable
set(EXECUTABLE_OUTPUT_PATH bin/${CMAKE_BUILD_TYPE})
add_executable(
  MqttEnergeasy
  ${MqttDaemon_files}
  ${ServiceSkeleton_files}
  ${SimpleFolders_files}
  ${SimpleIni_files}
  ${SimpleLog_files}
  ${SimpleSock_files}
  ${FragFramework_files}
  ${Cpr_files}
  ${JsonCpp_files}
  Energeasy.cpp
  MqttEnergeasy.cpp
  main.cpp
)

target_link_libraries(MqttEnergeasy paho-mqtt3a)
target_link_libraries(MqttEnergeasy paho-mqttpp3)
target_link_libraries(MqttEnergeasy ${CURL_LIBRARIES})
target_link_libraries(MqttEnergeasy ${CMAKE_THREAD_LIBS_INIT})

#Spécification WIN32
if(WIN32)
  target_link_libraries(MqttEnergeasy ws2_32)
endif()
