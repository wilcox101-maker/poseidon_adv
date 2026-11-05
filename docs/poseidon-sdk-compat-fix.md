# POSEIDON — ESP-NOW recv-callback SDK compat fix

**Context:** `src/c5_cmd.cpp` and `src/mesh.cpp` were updated to the ESP-IDF 5.x ESP-NOW
callback signature (uses `esp_now_recv_info_t *`). The pioarduino migration config
(platformio.ini current) ships ESP-IDF 5.x headers, so that build path is
**correct**. But `platformio.ini.stable` (rollback to `espressif32@6.7.0`) still
uses ESP-IDF 4.x headers where `esp_now_recv_info_t` does not exist — so the
stable-config build fails.

Right now the migration config can't build either (toolchain binary rename issue,
separate problem). Until that's fixed, we need stable config to build so the
SaltyJack UI changes can be flashed. After migration is fixed, the fix below
keeps working on both — it just becomes dead code in the 5.x branch.

## Root cause

ESP-NOW callback signatures differ:

| SDK | Signature |
| --- | --- |
| IDF 4.x (current `espressif32@6.7.0`) | `void (*)(const uint8_t *mac, const uint8_t *data, int len)` |
| IDF 5.x+ (pioarduino migration) | `void (*)(const esp_now_recv_info_t *info, const uint8_t *data, int len)` |

`esp_now_recv_info_t` exposes `src_addr`, `des_addr`, and `rx_ctrl`. In 4.x we
only get the raw source MAC.

## Files to patch

- `src/c5_cmd.cpp` — function `on_recv`, lines ~140–147
- `src/mesh.cpp` — function `on_recv`, lines ~47–56

Both files register the callback identically via `esp_now_register_recv_cb(on_recv)`.

## Fix pattern

Guard the function signature on `ESP_IDF_VERSION_MAJOR` (defined by the SDK
header `esp_idf_version.h`, which is transitively included everywhere that
includes `esp_now.h`). Fall back to the old signature on IDF < 5. The body
is identical in both branches after extracting `mac` once.

### `src/c5_cmd.cpp`

**Before (lines 138–148):**

```cpp
/* IDF 5.x+ ESP-NOW recv callback signature: const esp_now_recv_info_t*
 * replaced the raw mac pointer. Source MAC is recv_info->src_addr. */
static void on_recv(const esp_now_recv_info_t *recv_info,
                    const uint8_t *data, int len)
{
    if (len < (int)sizeof(c5_msg_t)) return;
    const c5_msg_t *m = (const c5_msg_t *)data;
    if (m->magic != C5_MAGIC || m->version != C5_VERSION) return;

    const uint8_t *mac = recv_info->src_addr;
```

**After:**

```cpp
/* ESP-NOW recv callback signature differs by ESP-IDF major version.
 *   IDF 4.x: void(const uint8_t *mac, const uint8_t *data, int len)
 *   IDF 5.x: void(const esp_now_recv_info_t *info, ...)  (info->src_addr)
 * Unified under ESP_IDF_VERSION_MAJOR so both build paths compile. */
#include <esp_idf_version.h>

#if ESP_IDF_VERSION_MAJOR >= 5
static void on_recv(const esp_now_recv_info_t *recv_info,
                    const uint8_t *data, int len)
{
    const uint8_t *mac = recv_info->src_addr;
#else
static void on_recv(const uint8_t *mac,
                    const uint8_t *data, int len)
{
#endif
    if (len < (int)sizeof(c5_msg_t)) return;
    const c5_msg_t *m = (const c5_msg_t *)data;
    if (m->magic != C5_MAGIC || m->version != C5_VERSION) return;
```

Delete the original `const uint8_t *mac = recv_info->src_addr;` line below — it's
now hoisted into the version guard. The rest of the function body is unchanged.

### `src/mesh.cpp`

Same pattern. **Before (lines 45–56):**

```cpp
/* IDF 5.x+ ESP-NOW recv callback takes esp_now_recv_info_t* first; source
 * MAC is recv_info->src_addr. (Can now read RSSI from rx_ctrl if needed.) */
static void on_recv(const esp_now_recv_info_t *recv_info,
                    const uint8_t *data, int len)
{
    s_rx++;
    if (len < (int)sizeof(hello_frame_t)) return;
    const hello_frame_t *h = (const hello_frame_t *)data;
    if (h->magic != MESH_MAGIC || h->version != MESH_VERSION) return;
    if (h->type != MESH_TYPE_HELLO) return;

    const uint8_t *mac = recv_info->src_addr;
```

**After:**

```cpp
#include <esp_idf_version.h>

#if ESP_IDF_VERSION_MAJOR >= 5
static void on_recv(const esp_now_recv_info_t *recv_info,
                    const uint8_t *data, int len)
{
    const uint8_t *mac = recv_info->src_addr;
#else
static void on_recv(const uint8_t *mac,
                    const uint8_t *data, int len)
{
#endif
    s_rx++;
    if (len < (int)sizeof(hello_frame_t)) return;
    const hello_frame_t *h = (const hello_frame_t *)data;
    if (h->magic != MESH_MAGIC || h->version != MESH_VERSION) return;
    if (h->type != MESH_TYPE_HELLO) return;
```

Again, delete the original `const uint8_t *mac = recv_info->src_addr;` line below.

## Notes

- `#include <esp_idf_version.h>` can be placed near the top of the file with the
  other includes instead of right above the function if preferred. Either works.
- `ESP_IDF_VERSION_MAJOR` is defined in `esp_idf_version.h` for both 4.x and 5.x,
  so no `#ifdef ESP_IDF_VERSION_MAJOR` wrap is needed.
- The 4.x fallback loses RSSI (as the comment in `mesh.cpp` already notes). No
  behavior change from current stable-config code — that was never reading RSSI.
- If anything else in the codebase refers to `recv_info->des_addr` or `rx_ctrl`
  in these files, it needs to be guarded behind `#if ESP_IDF_VERSION_MAJOR >= 5`.
  Neither file does currently.

## Verification

From project root:

```bash
# stable path (IDF 4.x)
cp platformio.ini.stable platformio.ini
pio run

# migration path (IDF 5.x, once toolchain is unbroken)
cp platformio.ini.migration platformio.ini
pio run
```

Both should succeed. Only three translation units (`c5_cmd.cpp`, `mesh.cpp`,
and transitively their callers via the `.h` declarations — but the `.h` files
don't declare `on_recv`, it's `static`) are affected.

## After this lands

The SaltyJack UI changes (new sprite icons, boot splash, info pages, screensaver,
etc.) live entirely in `src/features/saltyjack/`. They compile on both SDK
versions already. Once the compat fix is merged, stable-config can build and we
can flash `-t upload --upload-port COM16`.

— Claude, 2026-04-18
