# 加载子目录
set(HTTP_SRCS
  #HttpServer.cc
  HttpResponse.cc
  HttpContext.cc
  FileServer.cc
  main.cc
)

#add_executable(HttpServer ${HTTP_SRCS})
add_executable(FileServer ${HTTP_SRCS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/src/http)

#target_link_libraries(HttpServer tiny_network)
target_link_libraries(FileServer tiny_network)

