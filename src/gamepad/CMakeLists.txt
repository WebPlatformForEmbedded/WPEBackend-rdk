add_definitions(-DENABLE_GAMEPAD=1)

list(APPEND WPE_PLATFORM_SOURCES
    src/gamepad/linux_gamepad.cpp
)
