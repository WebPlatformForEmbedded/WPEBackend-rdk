/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <wpe/wpe.h>

#include <utility>
#include <map>
#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>
#include <functional>

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stddef.h>
#include <time.h>
#include <limits.h>

#include <glib.h>
#include <glib-unix.h>

#define BITS_PER_LONG (sizeof(unsigned long) * CHAR_BIT)
#define NBITS(x) (((x) + BITS_PER_LONG - 1) / BITS_PER_LONG)

#define INVALID_SOURCE_ID (-1U)

static bool getEvDevBits(int fd, unsigned int type, void* buf, unsigned int size)
{
    if (ioctl(fd, EVIOCGBIT(type, size), buf) < 0)
        return false;
    return true;
}

static bool testBitIsSet(const unsigned long* data, int bit)
{
    return data[bit / BITS_PER_LONG] & (1UL << (bit % BITS_PER_LONG));
}

static bool hasGamepadButtons(const unsigned long* ev_bits, const unsigned long* key_bits)
{
    if (!testBitIsSet(ev_bits, EV_KEY))
        return false;
    for (int key = BTN_GAMEPAD; key <= BTN_THUMBR; ++key) {
        if (testBitIsSet(key_bits, key)) {
            return true;
        }
    }
    return false;
}

static int32_t mapButtonId(int32_t code)
{
  // buttons[0]	Bottom button in right cluster
  // buttons[1]	Right button in right cluster
  // buttons[2]	Left button in right cluster
  // buttons[3]	Top button in right cluster
  // buttons[4]	Top left front button
  // buttons[5]	Top right front button
  // buttons[6]	Bottom left front button (trigger)
  // buttons[7]	Bottom right front button (trigger)
  // buttons[8]	Left button in center cluster
  // buttons[9]	Right button in center cluster
  // buttons[10]	Left stick pressed button
  // buttons[11]	Right stick pressed button
  // buttons[12]	Top button in left cluster
  // buttons[13]	Bottom button in left cluster
  // buttons[14]	Right  button in left cluster
  // buttons[15]	Left   button in left cluster

  struct MapEntry {
    const int32_t code;
    const int32_t id;
  };

  static const MapEntry map[] = {
    { BTN_A,      0 },
    { BTN_B,      1 },
    { BTN_X,      2 },
    { BTN_Y,      3 },
    { BTN_TL,     4 },
    { BTN_TR,     5 },
    { BTN_TL2,    6 },
    { BTN_TR2,    7 },
    { BTN_SELECT, 8 },
    { BTN_START,  9 },
    { BTN_THUMBL, 10 }, // stick pressed button
    { BTN_THUMBR, 11 }, // stick pressed button
    { ABS_HAT0Y,  12 },
    { ABS_HAT0X,  14 },

    // TODO: Add configurable re-mapping for any controller
    //
    // Append:  XBOX Wireless Controller
    //
    { ABS_BRAKE,   6 },  // trigger
    { ABS_GAS,     7 },  // trigger
    { KEY_BACK,    8 }   // Left center "menu/start" button

  };

  static const size_t mapSize = sizeof(map)/sizeof(map[0]);

  for (int i = 0; i < mapSize; ++i)
  {
    if (code == map[i].code)
      return map[i].id;
  }
  return -1;
}

static int mapAxisId(int32_t code)
{
  // axes[0]	Horizontal axis for left stick (negative left/positive right)
  // axes[1]	Vertical axis for left stick (negative up/positive down)
  // axes[2]	Horizontal axis for right stick (negative left/positive right)
  // axes[3]	Vertical axis for right stick (negative up/positive down)
  struct MapEntry {
    const int32_t code;
    const int32_t id;
  };

  static const MapEntry map[] = {
    { ABS_X,  0 },
    { ABS_Y,  1 },
    { ABS_Z,  2 },
    { ABS_RZ, 3 },
  };

  static const size_t mapSize = sizeof(map)/sizeof(map[0]);

  for (int i = 0; i < mapSize; ++i)
    if (code == map[i].code)
      return map[i].id;

  return -1;
}

static double clamp(double d, double min, double max)
{
  const double t = d < min ? min : d;
  return t > max ? max : t;
}

static double mapAxisValue(int32_t val, int32_t min, int32_t max)
{
    const double kMappedMax = 1.0;
    const double kMappedMin = -1.0;

    double scale = (kMappedMax - kMappedMin) / (max - min);
    double offset = (max + min) / (kMappedMax - kMappedMin) * scale * kMappedMin;
    double mappedVal = val * scale + offset;
    mappedVal = clamp(mappedVal, kMappedMin, kMappedMax);

    if (mappedVal < 0.009 && mappedVal > -0.009)
        mappedVal = 0.0;

    return mappedVal;
}

struct GamepadProxy;

struct GamepadProvider
{
  struct GamepadInfo
  {
    int32_t gamepad_id;
    int fd;
    std::string name;
    std::string path;
    guint source_id;
    std::map<int, input_absinfo> axisInfo;
    std::vector<double> buttonValues;
    std::vector<double> axisValues;
  };

  struct wpe_gamepad_provider* provider { nullptr };

  std::map<uint32_t /* id */, struct GamepadProxy*> gamepadProxies;
  std::map<uint32_t /* id */, struct GamepadInfo > gamepadsInfo;
  guint discoverySourceId { INVALID_SOURCE_ID };
  bool running { false };

  GamepadProvider(struct wpe_gamepad_provider* provider)
    : provider(provider)
  { }

  ~GamepadProvider();

  void addGamepadProxy(uint32_t id, struct GamepadProxy* gamepad)
  {
    gamepadProxies[id] = gamepad;
  }

  void removeGamepadProxy(uint32_t id, struct GamepadProxy*)
  {
    gamepadProxies.erase(id);
  }

  const char* getDeviceName(uint32_t gamepad_id)
  {
    auto it = gamepadsInfo.find(gamepad_id);
    if (it == gamepadsInfo.end())
      return nullptr;
    return it->second.name.c_str();
  }

  gboolean discoverGamePads()
  {
    if (!running) {
      discoverySourceId = INVALID_SOURCE_ID;
      return G_SOURCE_REMOVE;
    }

    if (gamepadsInfo.empty()) {
      GDir* dir;
      GPatternSpec* pspec;
      const char* basePath = "/dev/input/";

      dir = g_dir_open(basePath, 0, nullptr);
      if (dir) {
        pspec = g_pattern_spec_new("event*");
        while (const char* name = g_dir_read_name(dir)) {
          if (!g_pattern_match_string(pspec, name))
            continue;

          gchar* devPath = g_build_filename(basePath, name, nullptr);
          tryOpenGamePad(devPath);
          g_free(devPath);

          if (!gamepadsInfo.empty())
            break;
        }
        g_pattern_spec_free(pspec);
        g_dir_close(dir);
      }
    }
    return G_SOURCE_CONTINUE;
  }

  void startDiscovery()
  {
    if (discoverySourceId != INVALID_SOURCE_ID)
      return;
    discoverySourceId = g_timeout_add_seconds(
      1,
      [](gpointer data) {
        GamepadProvider* impl = static_cast<GamepadProvider*>(data);
        return impl->discoverGamePads();
      },
      this);
    discoverGamePads();
  }

  void stopDiscovery()
  {
    if (discoverySourceId != INVALID_SOURCE_ID) {
      g_source_remove(discoverySourceId);
      discoverySourceId = INVALID_SOURCE_ID;
    }
  }

  void start()
  {
    if (running)
      return;
    running = true;
    startDiscovery();
  }

  void stop()
  {
    if (!running)
      return;
    running = false;
    stopDiscovery();
    while(!gamepadsInfo.empty())
      closeGamepad(gamepadsInfo.begin()->first);
  }

  bool tryOpenGamePad(const char* path)
  {
    using ValueType = decltype(gamepadsInfo)::value_type;
    bool alreadyOpen = std::any_of(
      gamepadsInfo.begin(), gamepadsInfo.end(),
      [path](const ValueType& kv) {
        return kv.second.path == path;
      });
    if (alreadyOpen)
      return false;

    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
      return false;

    unsigned long  ev_bits[NBITS( EV_CNT)] = {0};
    unsigned long key_bits[NBITS(KEY_CNT)] = {0};
    unsigned long abs_bits[NBITS(ABS_CNT)] = {0};

    if (!getEvDevBits(fd, 0, ev_bits, sizeof(ev_bits))) {
      close(fd);
      return false;
    }
    if (!getEvDevBits(fd, EV_KEY, key_bits, sizeof(key_bits))) {
      close(fd);
      return false;
    }
    if (!getEvDevBits(fd, EV_ABS, abs_bits, sizeof(abs_bits))) {
      close(fd);
      return false;
    }
    if (!hasGamepadButtons(ev_bits, key_bits)) {
      close(fd);
      return false;
    }

    std::map<int, input_absinfo> axisInfo;
    for (int32_t code = ABS_X; code < ABS_CNT; ++code) {
      if (!testBitIsSet(abs_bits, code))
        continue;

      struct input_absinfo absinfo;
      if (ioctl(fd, EVIOCGABS(code), &absinfo) < 0)
        continue;

      axisInfo[code] = absinfo;
    }

    char name[256] = {'\0'};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
      strncpy(name, "Unknown", sizeof(name));

    // todo: figureout a better way to identify gamepad
    int32_t gamepad_id = std::hash<std::string>{}(path) ^ std::hash<std::string>{}(name);

    // oopsie
    if (gamepadsInfo.find(gamepad_id) != gamepadsInfo.end()) {
      close(fd);
      g_warning("Gamepad ID already taken, ignoring '%s' \n", path);
      return false;
    }

    auto &info = gamepadsInfo[gamepad_id];
    info = GamepadInfo { gamepad_id, fd, name, path, INVALID_SOURCE_ID, axisInfo, {}, {} };

    // fixme: figureout the actual mapping from the device
    info.buttonValues.resize(16, 0.0);
    info.axisValues.resize(4, 0.0);

    using SourceData = std::pair<GamepadProvider*, int32_t>;
    SourceData *sourcedata = new SourceData(this, gamepad_id);

    info.source_id = g_unix_fd_add_full(
      G_PRIORITY_HIGH,
      fd,
      static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
      [](gint fd, GIOCondition condition, gpointer data) -> gboolean
      {
        SourceData *sourcedata = static_cast<SourceData*>(data);
        GamepadProvider& self = *(sourcedata->first);

        int32_t gamepad_id = sourcedata->second;
        if (condition & (G_IO_HUP | G_IO_ERR)) {
          self.closeGamepad(gamepad_id);
          return G_SOURCE_REMOVE;
        }
        self.readAndDispatchEvents(fd, gamepad_id);
        return G_SOURCE_CONTINUE;
      },
      sourcedata,
      [](gpointer data)
      {
        SourceData *sourcedata = static_cast<SourceData*>(data);
        delete sourcedata;
      });

    wpe_gamepad_provider_dispatch_gamepad_connected(provider, gamepad_id);
    return true;
  }

  void closeGamepad(uint32_t gamepad_id)
  {
    auto it = gamepadsInfo.find(gamepad_id);
    if (it == gamepadsInfo.end())
      return;

    const auto& info = it->second;
    if (info.fd > 0)
      close(info.fd);
    g_source_remove(info.source_id);
    gamepadsInfo.erase(it);

    wpe_gamepad_provider_dispatch_gamepad_disconnected(provider, gamepad_id);
  }

  void readAndDispatchEvents(int fd, uint32_t gamepad_id)
  {
    std::vector<input_event> events;
    bool keepReading = true;
    while(keepReading)
    {
      input_event input;
      ssize_t read_size = read(fd, &input, sizeof(input));
      if (read_size != sizeof(input)) {
        if (errno == EINTR)
          continue;
        if (errno != EWOULDBLOCK)
          perror("Gamepad read failed");
        break;
      }

      switch(input.type) {
        case EV_SYN:
        {
          if (input.code == SYN_REPORT)
            keepReading = false;
          break;
        }
        case EV_KEY:
        case EV_ABS:
        {
          events.push_back(input);
          break;
        }
        default:
          break;
      }
    }

    auto it = gamepadsInfo.find(gamepad_id);
    if (it != gamepadsInfo.end()) {
      auto &info = it->second;
      updateGamepad(info, std::move(events));
    }
  }

  uint32_t getGamepadButtonCount(uint32_t gamepad_id) {
    auto it = gamepadsInfo.find(gamepad_id);
    if (it != gamepadsInfo.end())
      return it->second.buttonValues.size();
    return 0;
  }

  uint32_t copyGamepadButtonValues(uint32_t gamepad_id, double *array, uint32_t length) {
    auto it = gamepadsInfo.find(gamepad_id);
    if (it != gamepadsInfo.end()) {
      auto &info = it->second;
      uint32_t buttonCount = info.buttonValues.size();
      uint32_t min = std::min(buttonCount, length);
      memcpy(array, info.buttonValues.data(), min * sizeof(double));
      return min;
    }
    return 0;
  }

  uint32_t getGamepadAxisCount(uint32_t gamepad_id) {
    auto it = gamepadsInfo.find(gamepad_id);
    if (it != gamepadsInfo.end())
      return it->second.axisValues.size();
    return 0;
  }

  uint32_t copyGamepadAxisValues(uint32_t gamepad_id, double *array, uint32_t length) {
    auto it = gamepadsInfo.find(gamepad_id);
    if (it != gamepadsInfo.end()) {
      auto &info = it->second;
      uint32_t axisCount = info.axisValues.size();
      uint32_t min = std::min(axisCount, length);
      memcpy(array, info.axisValues.data(), min * sizeof(double));
      return min;
    }
    return 0;
  }

  void updateGamepad(GamepadInfo& info, std::vector<input_event> events);
};

struct GamepadProxy
{
  struct GamepadProvider* provider;
  struct wpe_gamepad* gamepad;
  uint32_t gamepad_id;

  GamepadProxy(GamepadProvider* provider, struct wpe_gamepad* gamepad, uint32_t id)
    : provider(provider)
    , gamepad(gamepad)
    , gamepad_id(id)
  {
    provider->addGamepadProxy(gamepad_id, this);
  }

  ~GamepadProxy()
  {
    if (provider)
      provider->removeGamepadProxy(gamepad_id, this);
  }

  void cleanup()
  {
    provider = nullptr;
    gamepad = nullptr;
    gamepad_id = 0;
  }

  const char* get_device_name() {
    const char* result = nullptr;
    if (provider)
      result = provider->getDeviceName(gamepad_id);
    return result ? result: "Unknown";
  }

  uint32_t get_button_count() {
    if (provider)
      return provider->getGamepadButtonCount(gamepad_id);
    return 0;
  }

  uint32_t copy_button_values(double *array, uint32_t length) {
    if (provider)
      return provider->copyGamepadButtonValues(gamepad_id, array, length);
    return 0;
  }

  uint32_t get_axis_count() {
    if (provider)
      return provider->getGamepadAxisCount(gamepad_id);
    return 0;
  }

  uint32_t copy_axis_values(double *array, uint32_t length) {
    if (provider)
      return provider->copyGamepadAxisValues(gamepad_id, array, length);
    return 0;
  }

  void dispatchButtonValuesChanged()
  {
    if (gamepad)
      wpe_gamepad_dispatch_button_values_changed(gamepad);
  }

  void dispatchAxisValuesChanged()
  {
    if (gamepad)
      wpe_gamepad_dispatch_axis_values_changed(gamepad);
  }
};

GamepadProvider::~GamepadProvider()
{
  stop();
  for(const auto &kv : gamepadProxies)
    kv.second->cleanup();
}

void GamepadProvider::updateGamepad(GamepadInfo& info, std::vector<input_event> events)
{
  bool buttonValuesChanged = false;
  bool axisValuesChanged = false;
  const int32_t gamepad_id = info.gamepad_id;

  const auto updateButtonValue = [&info, &buttonValuesChanged] (int32_t code, int32_t value)
  {
    const int32_t button_id = mapButtonId(code);

    if (button_id < 0 || button_id >= info.buttonValues.size()) {
      g_warning("unmapped button code: %d    0x%04X\n", code, code);
      return;
    }
    info.buttonValues[button_id] = value == 0 ? 0.0 : 1.0;
    buttonValuesChanged = true;
  };

  const auto updateDpadButtonValue = [&info, &buttonValuesChanged] (int32_t code, int32_t value)
  {
    // todo: figure out a better to handle dpad
    if (code != ABS_HAT0X && code != ABS_HAT0Y) {
      g_warning("not supported code: %d    0x%04X\n", code, code);
      return;
    }
    const int32_t idx_base = mapButtonId(code);
    if ((idx_base < 0) || (idx_base + 1 >= info.buttonValues.size())) {
      g_warning("unmapped button code: %d    0x%04X\n", code, code);
      return;
    }
    info.buttonValues[idx_base]     = 0.0;
    info.buttonValues[idx_base + 1] = 0.0;
    if (value != 0)
      info.buttonValues[(value < 0 ? idx_base : idx_base + 1)] = 1.0;
    buttonValuesChanged = true;
  };

  const auto updateTriggerValue = [&info, &buttonValuesChanged] (int32_t code, int32_t value)
  {
    const int32_t idx_base = mapButtonId(code);
    if ((idx_base < 0) || (idx_base + 1 >= info.buttonValues.size())) {
      g_warning("unmapped trigger code: %d    0x%04X\n", code, code);
      return;
    }

    auto info_iter = info.axisInfo.find(code);
    if (info_iter == info.axisInfo.end()) {
      g_warning("no trigger info: %d \n", code);
      return;
    }

    const input_absinfo& axisInfo = info_iter->second;
    double mapped = mapAxisValue(value, axisInfo.minimum, axisInfo.maximum);

    // Need to Normalize values in [ -1 <-> 1 ] to [ 0 <-> 1 ]
    //
    // First [ -1 <-> 1 ] / 2      =  [ -0.5 <-> 0.5 ]
    // Then  [ -0.5 <-> 0.5] + 0.5 =  [  0.0 <-> 1.0 ]
    //
    info.buttonValues[idx_base] = (mapped/2) + 0.5;
    buttonValuesChanged = true;
  };

  const auto updateAxisValue = [&info, &axisValuesChanged] (int32_t code, int32_t value)
  {
    auto info_iter = info.axisInfo.find(code);
    if (info_iter == info.axisInfo.end()) {
      g_warning("no axis info: %d \n", code);
      return;
    }
    const int32_t axis_id = mapAxisId(code);
    if (axis_id < 0 || axis_id >= info.axisValues.size()) {
      g_warning("unmapped axis code: %d \n", code);
      return;
    }

    const input_absinfo& axisInfo = info_iter->second;
    double mapped = mapAxisValue(value, axisInfo.minimum, axisInfo.maximum);

    info.axisValues[axis_id] = mapped;
    axisValuesChanged = true;
  };

  for (const auto& event : events)
  {
    switch(event.type)
    {
      case EV_KEY:
      {
        updateButtonValue(event.code, event.value);
        break;
      }
      case EV_ABS:
      {
        if (event.code == ABS_GAS || event.code == ABS_BRAKE) // L/R triggers
          updateTriggerValue(event.code, event.value);
        else
        if (event.code == ABS_HAT0X || event.code == ABS_HAT0Y)
          updateDpadButtonValue(event.code, event.value);
        else
          updateAxisValue(event.code, event.value);
        break;
      }
      default:
        break;
    }
  }

  if (buttonValuesChanged) {
    auto iter = gamepadProxies.find(gamepad_id);
    if (iter != gamepadProxies.end()) {
      auto *proxy = iter->second;
      proxy->dispatchButtonValuesChanged();
    }
  }

  if (axisValuesChanged) {
    auto iter = gamepadProxies.find(gamepad_id);
    if (iter != gamepadProxies.end()) {
      auto *proxy = iter->second;
      proxy->dispatchAxisValuesChanged();
    }
  }
}


struct wpe_gamepad_provider_interface gamepad_provider_interface = {
  // create
  [](struct wpe_gamepad_provider* provider) -> void* {
    GamepadProvider* impl = new GamepadProvider(provider);
    return impl;
  },
  // destroy
  [](void *data) {
    GamepadProvider* impl = (struct GamepadProvider*)data;
    delete impl;
  },
  // start
  [](void *data) {
    GamepadProvider* impl = (struct GamepadProvider*)data;
    impl->start();
  },
  // stop
  [](void *data) {
    GamepadProvider* impl = (struct GamepadProvider*)data;
    impl->stop();
  },
  // get_view_for_gamepad_input
  [](void*, void*) -> struct wpe_view_backend* {
    return nullptr;
  }
};

struct wpe_gamepad_interface gamepad_interface = {
  // create
  [](void* data, struct wpe_gamepad* gamepad, uint32_t id) -> void* {
    GamepadProvider* provider = (GamepadProvider*) data;
    GamepadProxy* proxy = new GamepadProxy(provider, gamepad, id);
    return proxy;
  },
  // destroy
  [](void* data) {
    GamepadProxy* proxy = (GamepadProxy*) data;
    delete proxy;
  },
  // get_id
  [](void* data) -> uint32_t {
    GamepadProxy* proxy = (GamepadProxy*) data;
    return proxy->gamepad_id;
  },
  // get_device_name
  [](void *data) -> const char* {
    GamepadProxy* proxy = (GamepadProxy*) data;
    return proxy->get_device_name();
  },
  // get_button_count
  [](void* data) -> uint32_t {
    GamepadProxy* proxy = (GamepadProxy*) data;
    return proxy->get_button_count();
  },
  // copy_button_values
  [] (void* data, double* array, uint32_t length) -> uint32_t {
    GamepadProxy* proxy = (GamepadProxy*) data;
    return proxy->copy_button_values(array, length);
  },
  // get_axis_count
  [](void* data) -> uint32_t {
    GamepadProxy* proxy = (GamepadProxy*) data;
    return proxy->get_axis_count();
  },
  // copy_axis_values
  [] (void* data, double* array, uint32_t length) -> uint32_t {
    GamepadProxy* proxy = (GamepadProxy*) data;
    return proxy->copy_axis_values(array, length);
  }
};
