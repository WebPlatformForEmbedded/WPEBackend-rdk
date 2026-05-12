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
#include <glib.h>
#include <glib-unix.h>
#include <vector>
#include <algorithm>
#include <cassert>

static GMainLoop *loop;
static struct wpe_gamepad_provider* provider;
static std::vector <struct wpe_gamepad*> gamepads;

static struct wpe_gamepad_client s_gamepadClient =
{
  // button_values_changed
  [](struct wpe_gamepad* gamepad, void *) {
    uint32_t button_count = wpe_gamepad_get_button_count(gamepad);
    std::vector<double> button_values;
    button_values.resize(button_count, 0.0);
    wpe_gamepad_copy_button_values(gamepad, button_values.data(), button_values.size());
    g_print("button_values_changed:\n");
    for (const auto v : button_values) {
      g_print(" %.3f", v);
    }
    g_print("\n");
  },
  // axis_values_changed
  [] (struct wpe_gamepad* gamepad, void *) {
    uint32_t axis_count = wpe_gamepad_get_axis_count(gamepad);
    std::vector<double> axis_values;
    axis_values.resize(axis_count, 0.0);
    wpe_gamepad_copy_axis_values(gamepad, axis_values.data(), axis_values.size());
    g_print("axis_values_changed:\n");
    for (const auto v : axis_values) {
      g_print(" %.3f", v);
    }
    g_print("\n");
  }
};

static struct wpe_gamepad_provider_client s_providerClient =
{
  // gamepad_connected
  [](void*, uint32_t gamepad_id) {
    g_print("connected %u \n", gamepad_id);

    struct wpe_gamepad* gamepad = wpe_gamepad_create(provider, gamepad_id);
    wpe_gamepad_set_client(gamepad, &s_gamepadClient, nullptr);
    gamepads.push_back(gamepad);

    const char* name = wpe_gamepad_get_device_name(gamepad);
    uint32_t button_count = wpe_gamepad_get_button_count(gamepad);
    uint32_t axis_count = wpe_gamepad_get_axis_count(gamepad);
    g_print("gamepad name = %s, button count = %u, axis count = %u \n", name, button_count, axis_count);
  },
  // gamepad_disconnected
  [](void*, uint32_t gamepad_id) {
    g_print("disconnected %u \n", gamepad_id);

    auto iter = std::find_if(
      gamepads.begin(), gamepads.end(),
      [gamepad_id] (struct wpe_gamepad* gamepad) {
        return gamepad_id == wpe_gamepad_get_id(gamepad);
      });

    if (iter != gamepads.end()) {
      struct wpe_gamepad* gamepad = *iter;
      gamepads.erase(iter);
      wpe_gamepad_destroy(gamepad);
    }
  }
};

static gboolean terminate_signal(gpointer)
{
  g_print("\nGot terminate signal\n");
  g_main_loop_quit(loop);
  return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
  loop = g_main_loop_new(nullptr, FALSE);

  g_unix_signal_add(SIGINT, terminate_signal, nullptr);
  g_unix_signal_add(SIGTERM, terminate_signal, nullptr);

  provider = wpe_gamepad_provider_create();
  assert(provider != nullptr);

  wpe_gamepad_provider_set_client(provider, &s_providerClient, nullptr);
  wpe_gamepad_provider_start(provider);

  g_main_loop_run(loop);

  wpe_gamepad_provider_stop(provider);
  wpe_gamepad_provider_destroy(provider);

  g_main_loop_unref(loop);

  return 0;
}
