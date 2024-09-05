add_executable(gamepad_test src/gamepad_test/gamepad_test.cpp)

target_include_directories(gamepad_test PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${WPE_INCLUDE_DIRS})

target_link_libraries(gamepad_test
  ${GLIB_GIO_LIBRARIES}
  ${GLIB_GOBJECT_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${WPE_LIBRARIES})

install(TARGETS gamepad_test DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
