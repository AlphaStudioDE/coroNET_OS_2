// Compile the actual BSP flush/TE callbacks against a controllable DMA endpoint.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
function method(file, signature) {
  const text = fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
  const start = text.lastIndexOf(signature);
  const end = text.indexOf('\n}', start);
  if (start < 0 || end < 0) throw Error(signature);
  return text.slice(start, end + 2);
}
const source = String.raw`
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
using lv_color_t = uint16_t;
using esp_err_t = int;
constexpr int ESP_OK = 0, pdTRUE = 1;
constexpr int LV_DISP_ROT_NONE = 0, LV_DISP_ROT_90 = 1, LV_DISP_ROT_180 = 2, LV_DISP_ROT_270 = 3;
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(n) (n)
const char* esp_err_to_name(int) { return "test"; }
uint32_t esp_log_timestamp() { return 100; }
int maxWait = 0;
int xSemaphoreTake(int* sem, int timeout) {
  assert(timeout <= 100);
  if (timeout > maxWait) maxWait = timeout;
  if (*sem) { --*sem; return pdTRUE; }
  return 0;
}
void xSemaphoreGive(int* sem) { assert(*sem == 0); ++*sem; }
struct bsp_lcd_tear_t { int* te_catch_sem; int* te_v_sync_sem; };
struct Panel { void* user_data = nullptr; };
struct lv_area_t { int x1, y1, x2, y2; };
struct lv_disp_drv_t { void* user_data; int hor_res = 4, ver_res = 4; };
struct lvgl_port_display_ctx_t {
  int trans_size = 8;
  lv_color_t *trans_buf_1, *trans_buf_2, *trans_act = nullptr;
  int* trans_done_sem;
  bool transport_fault = false;
  int sw_rotate = LV_DISP_ROT_NONE;
  bool (*draw_wait_cb)(void*) = nullptr;
  Panel* panel_handle;
};
struct { bool refresh_pending = false; } lvgl_port_ctx;
int ready = 0, calls = 0, token = 1;
bool completeImmediately = true, failSubmit = false;
std::vector<std::vector<lv_color_t>> submitted;
void lv_disp_flush_ready(lv_disp_drv_t*) { ++ready; }
int esp_lcd_panel_draw_bitmap(Panel*, int x1, int y1, int x2, int y2, lv_color_t* pixels) {
  ++calls;
  if (failSubmit) return -1;
  submitted.emplace_back(pixels, pixels + (x2-x1)*(y2-y1));
  if (completeImmediately) xSemaphoreGive(&token);
  return ESP_OK;
}
${method('src/bsp/lv_port.c', 'static void lvgl_port_flush_callback(')}
${method('src/bsp/esp_bsp.c', 'static bool bsp_display_sync_cb(')}
int main() {
  lv_color_t a[8] = {}, b[8] = {}, pixels[16];
  for (int i=0; i<16; ++i) pixels[i] = i + 1;
  Panel panel;
  lvgl_port_display_ctx_t ctx{8, a, b, nullptr, &token, false, LV_DISP_ROT_NONE, nullptr, &panel};
  lv_disp_drv_t driver{&ctx};
  lv_area_t area{0,0,3,3};
  lvgl_port_flush_callback(&driver, &area, pixels);
  assert(ready == 1 && calls == 2 && token == 1);
  assert(submitted[0] == std::vector<lv_color_t>(pixels,pixels+8));
  assert(submitted[1] == std::vector<lv_color_t>(pixels+8,pixels+16));
  std::cout << "PASS: normal double-buffer submission preserves all pixels\n";

  completeImmediately = false;
  lvgl_port_flush_callback(&driver, &area, pixels);
  assert(calls == 3 && token == 0 && ctx.transport_fault && lvgl_port_ctx.refresh_pending);
  const auto owned = std::vector<lv_color_t>(b,b+8);
  for (auto& pixel : pixels) pixel = 99;
  lvgl_port_flush_callback(&driver, &area, pixels);
  assert(calls == 3 && std::vector<lv_color_t>(b,b+8) == owned);
  std::cout << "PASS: timeout does not reuse a buffer still owned by DMA\n";

  xSemaphoreGive(&token); // A late hardware completion releases the owned buffer.
  completeImmediately = true;
  lvgl_port_flush_callback(&driver, &area, pixels);
  assert(calls == 5 && token == 1 && !ctx.transport_fault);
  assert(submitted.back() == std::vector<lv_color_t>(8,99));
  std::cout << "PASS: late DMA completion permits a fresh redraw\n";

  failSubmit = true;
  lvgl_port_flush_callback(&driver, &area, pixels);
  assert(calls == 6 && token == 1);
  failSubmit = false;
  lvgl_port_flush_callback(&driver, &area, pixels);
  assert(calls == 8 && token == 1);
  std::cout << "PASS: failed submission releases its unused token for retry\n";

  for (int rotate : {LV_DISP_ROT_90, LV_DISP_ROT_180, LV_DISP_ROT_270}) {
    ctx.sw_rotate = rotate;
    lvgl_port_flush_callback(&driver, &area, pixels);
    assert(token == 1 && !ctx.transport_fault);
  }
  std::cout << "PASS: all supported rotations submit bounded transfers\n";
  int catchSem = 0, syncSem = 0;
  bsp_lcd_tear_t tear{&catchSem,&syncSem};
  assert(bsp_display_sync_cb(&tear));
  assert(catchSem == 1 && maxWait == 100);
  std::cout << "PASS: missing TE edge returns after a bounded wait\n";
}
`;
const dir = path.join(root, '.pio/host-tests');
fs.mkdirSync(dir, { recursive: true });
const cpp = path.join(dir, 'display-transfer.cpp');
const exe = path.join(dir, process.platform === 'win32' ? 'display-transfer.exe' : 'display-transfer');
fs.writeFileSync(cpp, source);
for (const [command, args] of [[process.env.CXX || 'g++', ['-std=c++17', '-O0', cpp, '-o', exe]], [exe, []]]) {
  const result = spawnSync(command, args, { encoding: 'utf8', timeout: 30000 });
  process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
  if (result.error) throw result.error;
  if (result.status) process.exit(result.status);
}
