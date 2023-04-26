/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

namespace Headless {

struct ViewBackend
{
};

} // namespace Headless

extern "C" {

struct wpe_view_backend_interface headless_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new Headless::ViewBackend();
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<Headless::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        return -1;
    },
};

}
