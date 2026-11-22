/* B85: USB game input in the Doom boot (lab:LEAD/B85/REPORT.md).
 *  - generic HID gamepads: any HID interface that is not a boot keyboard/mouse; the report descriptor decides (b85hid.c);
 *  - XInput (vendor class 0xFF/0x5D/0x01: Xbox 360 wired, F310 X mode 046D:C21D): our own USB host client, interrupt IN + OUT
 *    (player LED, rumble);
 *  - mouse: boot reports, every delta summed (tiny386's |d|>50 drop is a Windows-side filter, not used here);
 *  - hot-plug: connect/disconnect of all of them; presence counts hide the touch overlay.
 * Everything here is Doom-boot only (b85_solo): the Windows boot keeps tiny386's paths. State in PSRAM (internal RAM rule). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "b85hid.h"
#include "b85.h"

/* ---- shared with the engine (b85.c): last pad state, mouse sums, presence */
EXT_RAM_BSS_ATTR volatile b85pad_t b85_padst;
EXT_RAM_BSS_ATTR volatile int64_t b85_pad_t;   /* time of the last pad change (report -> tic latency) */
EXT_RAM_BSS_ATTR volatile int32_t b85_mdx, b85_mdy, b85_mbtn; EXT_RAM_BSS_ATTR volatile int64_t b85_mouse_t;
EXT_RAM_BSS_ATTR volatile int b85_npad, b85_nkbd, b85_nmouse;
EXT_RAM_BSS_ATTR volatile uint32_t b85_nrep;   /* pad reports seen */
EXT_RAM_BSS_ATTR char b85_padname[32];
EXT_RAM_BSS_ATTR volatile int b85_padlog;

static void pad_in(const b85pad_t *p)
{
	b85_nrep++;
	if (!memcmp((const void *)&b85_padst, p, sizeof *p)) return;
	/* padlog: one line per button change or stick/trigger move of >= 1/4 travel (owner's press-sequence test) */
	if (b85_padlog && (p->btn != b85_padst.btn || abs(p->lx - b85_padst.lx) > 31 || abs(p->ly - b85_padst.ly) > 31 || abs(p->rx - b85_padst.rx) > 31 ||
	    abs(p->ry - b85_padst.ry) > 31 || abs(p->lt - b85_padst.lt) > 63 || abs(p->rt - b85_padst.rt) > 63))
		fprintf(stderr, "[b85] pad btn %05x lx %d ly %d rx %d ry %d lt %d rt %d\r\n", (unsigned)p->btn, p->lx, p->ly, p->rx, p->ry, p->lt, p->rt);
	memcpy((void *)&b85_padst, p, sizeof *p); b85_pad_t = esp_timer_get_time();
}
static void pad_clear(void) { memset((void *)&b85_padst, 0, sizeof b85_padst); b85_pad_t = esp_timer_get_time(); }

/* ---- mouse (usb_input.c boot mouse callback) */
int b85_mouse(int dx, int dy, int btn)
{
	if (!b85_solo()) return 0;
	b85_mdx += dx; b85_mdy += dy;
	if (dx || dy || btn != b85_mbtn) b85_mouse_t = esp_timer_get_time();
	b85_mbtn = btn; return 1;
}
void b85_dev(int kind, int d)
{
	if (kind == 1) b85_nkbd += d; else if (kind == 2) b85_nmouse += d;
	if (b85_nkbd < 0) { b85_nkbd = 0; }
	if (b85_nmouse < 0) { b85_nmouse = 0; }
	if (b85_solo()) fprintf(stderr, "[b85] %s %s (kbd %d mouse %d pad %d)\r\n", kind == 1 ? "keyboard" : "mouse", d > 0 ? "in" : "out", b85_nkbd, b85_nmouse, b85_npad);
}

/* ---- generic HID gamepad (usb_input.c: CONNECTED with protocol NONE) */
#define NHID 2
static EXT_RAM_BSS_ATTR struct { hid_host_device_handle_t h; b85hid_t *d; } hid[NHID];
static void hid_cb(hid_host_device_handle_t h, const hid_host_interface_event_t ev, void *arg)
{
	int i = (int)(intptr_t)arg;
	if (ev == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
		uint8_t r[64]; size_t n = 0; b85pad_t p;
		if (hid_host_device_get_raw_input_report_data(h, r, sizeof r, &n) == ESP_OK && hid[i].d && b85hid_decode(hid[i].d, r, (int)n, &p)) pad_in(&p);
	} else if (ev == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
		hid_host_device_close(h); heap_caps_free(hid[i].d); hid[i].d = NULL; hid[i].h = NULL; b85_npad--; pad_clear();
		fprintf(stderr, "[b85] HID pad out (pads %d)\r\n", b85_npad);
	}
}
int b85_hid_open(hid_host_device_handle_t h)
{
	if (!b85_solo()) return 0;
	int i = 0; while (i < NHID && hid[i].h) i++;
	if (i == NHID) return 0;
	const hid_host_device_config_t cfg = { .callback = hid_cb, .callback_arg = (void *)(intptr_t)i };
	if (hid_host_device_open(h, &cfg) != ESP_OK) return 0;
	size_t n = 0; uint8_t *desc = hid_host_get_report_descriptor(h, &n);
	b85hid_t *d = heap_caps_calloc(1, sizeof *d, MALLOC_CAP_SPIRAM);
	if (!desc || !d || !b85hid_parse(desc, (int)n, d)) {
		fprintf(stderr, "[b85] HID interface (desc %u B) is not a pad\r\n", (unsigned)n);
		heap_caps_free(d); hid_host_device_close(h); return 1;   /* ours anyway: the Doom boot has no other use for it */
	}
	hid_host_dev_info_t inf; memset(&inf, 0, sizeof inf); hid_host_get_device_info(h, &inf);
	hid[i].h = h; hid[i].d = d; b85_npad++;
	snprintf(b85_padname, sizeof b85_padname, "HID %04x:%04x", inf.VID, inf.PID);
	fprintf(stderr, "[b85] HID pad in: %04x:%04x desc %u B, %d buttons, id %d (pads %d)\r\n", inf.VID, inf.PID, (unsigned)n, d->nbtn[d->has_id ? 1 : 0], d->has_id, b85_npad);
	hid_host_device_start(h);
	return 1;
}

/* ---- XInput: our own USB host client */
static EXT_RAM_BSS_ATTR struct {
	usb_host_client_handle_t cl; usb_device_handle_t dev; uint8_t addr, ifn, ep_in, ep_out, mps_in, live, gone, busy_out;
	usb_transfer_t *tin, *tout; int inflight;
} xi;
static void xi_in_cb(usb_transfer_t *t)
{
	b85pad_t p;
	if (t->status == USB_TRANSFER_STATUS_COMPLETED) {
		if (b85x360_decode(t->data_buffer, t->actual_num_bytes, &p)) pad_in(&p);
		if (!xi.gone && usb_host_transfer_submit(t) == ESP_OK) return;
	}
	xi.inflight--;
}
static void xi_out_cb(usb_transfer_t *t) { (void)t; xi.busy_out = 0; xi.inflight--; }
static void xi_out(const uint8_t *b, int n)
{
	if (!xi.live || xi.gone || !xi.tout || xi.busy_out || !xi.ep_out) return;
	memcpy(xi.tout->data_buffer, b, n); xi.tout->num_bytes = n; xi.busy_out = 1; xi.inflight++;
	if (usb_host_transfer_submit(xi.tout) != ESP_OK) { xi.busy_out = 0; xi.inflight--; }
}
void b85_rumble(int big, int small)
{
	static EXT_RAM_BSS_ATTR uint8_t last[2];
	if (last[0] == big && last[1] == small) return;
	uint8_t b[8] = { 0x00, 0x08, 0x00, (uint8_t)big, (uint8_t)small, 0, 0, 0 };
	xi_out(b, 8); if (!xi.busy_out) return;
	last[0] = (uint8_t)big; last[1] = (uint8_t)small;
}
static void xi_open(uint8_t addr)
{
	usb_device_handle_t dev; const usb_config_desc_t *c; const usb_device_desc_t *dd;
	if (xi.live || usb_host_device_open(xi.cl, addr, &dev) != ESP_OK) return;
	if (usb_host_get_device_descriptor(dev, &dd) != ESP_OK || usb_host_get_active_config_descriptor(dev, &c) != ESP_OK) { usb_host_device_close(xi.cl, dev); return; }
	fprintf(stderr, "[b85] usb dev addr %d %04x:%04x class %02x/%02x/%02x\r\n", addr, dd->idVendor, dd->idProduct, dd->bDeviceClass, dd->bDeviceSubClass, dd->bDeviceProtocol);
	const uint8_t *p = (const uint8_t *)c, *e = p + c->wTotalLength; int ifn = -1, in = 0, out = 0, mps = 32;
	for (int cur = 0; p + 2 <= e && p[0]; p += p[0]) {
		if (p[1] == USB_B_DESCRIPTOR_TYPE_INTERFACE) { cur = p[5] == 0xFF && p[6] == 0x5D && p[7] == 0x01 && p[3] == 0 && ifn < 0; if (cur) ifn = p[2]; }
		else if (p[1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT && cur && (p[3] & 3) == 3) {
			if (p[2] & 0x80) { if (!in) { in = p[2]; mps = p[4]; } } else if (!out) out = p[2];
		}
	}
	if (ifn < 0 || !in || usb_host_interface_claim(xi.cl, dev, ifn, 0) != ESP_OK) { usb_host_device_close(xi.cl, dev); return; }
	xi.dev = dev; xi.addr = addr; xi.ifn = ifn; xi.ep_in = in; xi.ep_out = out; xi.mps_in = mps > 64 ? 64 : mps; xi.gone = 0;
	if (!xi.tin) usb_host_transfer_alloc(64, 0, &xi.tin);
	if (!xi.tout) usb_host_transfer_alloc(32, 0, &xi.tout);
	xi.tin->device_handle = dev; xi.tin->bEndpointAddress = in; xi.tin->callback = xi_in_cb; xi.tin->num_bytes = xi.mps_in; xi.tin->timeout_ms = 0;
	xi.tout->device_handle = dev; xi.tout->bEndpointAddress = out; xi.tout->callback = xi_out_cb; xi.tout->timeout_ms = 0;
	xi.live = 1; xi.busy_out = 0; xi.inflight = 1;
	if (usb_host_transfer_submit(xi.tin) != ESP_OK) xi.inflight = 0;
	{ const uint8_t led[3] = { 0x01, 0x03, 0x06 }; xi_out(led, 3); }   /* ring: player 1 */
	b85_npad++;
	snprintf(b85_padname, sizeof b85_padname, "XInput %04x:%04x", dd->idVendor, dd->idProduct);
	fprintf(stderr, "[b85] XInput pad in: %04x:%04x if %d ep in %02x out %02x mps %d (pads %d)\r\n", dd->idVendor, dd->idProduct, ifn, in, out, mps, b85_npad);
}
static void xi_event(const usb_host_client_event_msg_t *m, void *arg)
{
	(void)arg;
	if (m->event == USB_HOST_CLIENT_EVENT_NEW_DEV) xi_open(m->new_dev.address);
	else if (m->event == USB_HOST_CLIENT_EVENT_DEV_GONE && xi.live && m->dev_gone.dev_hdl == xi.dev) {
		xi.gone = 1; usb_host_endpoint_halt(xi.dev, xi.ep_in); usb_host_endpoint_flush(xi.dev, xi.ep_in);
		if (xi.ep_out) { usb_host_endpoint_halt(xi.dev, xi.ep_out); usb_host_endpoint_flush(xi.dev, xi.ep_out); }
	}
}
static void xi_task(void *a)
{
	(void)a;
	const usb_host_client_config_t cfg = { .is_synchronous = false, .max_num_event_msg = 5, .async = { .client_event_callback = xi_event, .callback_arg = NULL } };
	while (usb_host_client_register(&cfg, &xi.cl) != ESP_OK) vTaskDelay(pdMS_TO_TICKS(100));   /* until usb_setup installed the host */
	/* below 35 fps the engine (prio 5, core 0) never sleeps: every lower task on core 0 starves (c1 on board A: console lines lost,
	 * HID connects never handled). These tasks block on events, so lifting them above the engine costs nothing */
	static const char *const lift[] = { "cardsvc", "usb_events", "usb_task" };
	for (int i = 0; i < 3; i++) {
		TaskHandle_t h = NULL;
		for (int k = 0; k < 50 && !(h = xTaskGetHandle(lift[i])); k++) vTaskDelay(pdMS_TO_TICKS(20));
		if (h) vTaskPrioritySet(h, 6);
		fprintf(stderr, "[b85] task %s -> prio 6 %s\r\n", lift[i], h ? "ok" : "MISSING");
	}
	for (;;) {
		usb_host_client_handle_events(xi.cl, pdMS_TO_TICKS(50));
		if (xi.live && xi.gone && xi.inflight <= 0) {   /* every transfer came back: let go of the device */
			usb_host_interface_release(xi.cl, xi.dev, xi.ifn); usb_host_device_close(xi.cl, xi.dev);
			xi.live = 0; b85_npad--; pad_clear(); fprintf(stderr, "[b85] XInput pad out (pads %d)\r\n", b85_npad);
		}
	}
}
void b85_pad_start(void) { xTaskCreatePinnedToCore(xi_task, "b85_xi", 4096, NULL, 5, NULL, 0); }
