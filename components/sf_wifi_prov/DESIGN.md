# HLD — WiFi Provisioning (`sf_wifi_prov`)

**Status:** Approved for implementation (rev 18)
**Target:** ESP-IDF 5.4.2 / ESP32
**Author:** Evyatar (design assisted)

---

## 1. Goal

Let a deployed device receive its WiFi credentials (and, when needed, its clock
time) at runtime through a phone/laptop browser, instead of hard-coding them at
build time via `menuconfig`.

Hard requirements from design review:

- **The watering core must never be held hostage by the network.** A router
  outage, a changed WiFi password, or an un-provisioned radio must not stop the
  device from running its saved schedules — *as long as it has a valid clock*.
- **The device can never be permanently bricked** by a credential typo.
- **The provisioning radio (SoftAP) must not be open all the time.** It auto-
  starts only when the device is **non-operational** (`!connected && !time_ok`);
  otherwise it is **button-triggered only**.
- **The device self-heals.** Once it has credentials it keeps trying to (re)connect
  forever; when the router returns it reconnects automatically.
- **`sf_wifi_prov` is pure infrastructure.** It owns AP/HTTP/NVS plumbing; the
  *device* owns its UI (HTML, logo, fields) and its LED.

---

## 2. Key principle — *time* is the gate, not *network*

A cron schedule with no valid wall-clock time is not merely useless, it is
**harmful** (it would water at the wrong hours). The scheduler is gated on time
validity, not connectivity:

```
Cron scheduler fires a job  ONLY IF  time is currently valid
    valid   = SNTP succeeded  OR  user entered time manually  — recorded in an
              RTC-backed flag that survives soft reboot / sleep, lost only on
              power loss (§5.2)
    invalid = neither has happened since the last power loss  → scheduler idle
```

### Clock facts (accepted limitations)

| Event                              | RTC clock |
|------------------------------------|-----------|
| `esp_restart()` (software reboot)  | preserved |
| light / deep sleep                 | preserved |
| **power loss**                     | **lost**  |

Credentials live in **NVS and survive power loss**; only the RTC *time* is lost.
So after a power cut the device still has its credentials and will reconnect if
the router is up (SNTP then restores time). It only needs the prov page when it
can *neither* connect *nor* recover time. No battery-backed hardware RTC in scope.

> **Decided (O-1):** NVS time-persistence is **dropped**. Time validity is an
> RTC-backed flag (§5.2): cleared on power loss (RTC domain unpowered), preserved
> across `esp_restart` and sleep — matching the RTC clock's own persistence
> semantics exactly.

---

## 3. Components touched

| Component                 | Change                                                                          |
|---------------------------|---------------------------------------------------------------------------------|
| `components/sf_wifi`      | `sf_wifi_init(void)` → `sf_wifi_driver_init()` + `sf_wifi_connect(ssid, password)` + `sf_wifi_ap_start/stop()`; owns **all** `esp_wifi_*` calls; **perpetual reconnect-with-backoff** on disconnect |
| `components/sf_wifi_prov` | **New** — SoftAP (via `sf_wifi_ap_start/stop`) + HTTP server (UI from FatFS) + NVS credential store + state callback; never calls `esp_wifi_*` directly |
| `components/sf_time`      | RTC-backed **time-valid flag** (persists across restart/sleep, lost on power loss); `sf_time_set_manual()`; `sf_time_is_valid()` |
| `device/sf_watering`      | Authors UI files in the FatFS image; reorders `init_device`; drives **LED**; toggles `sf_pm`; wires button |

---

## 4. Driver ownership — `sf_wifi` owns the radio

`esp_wifi` is a single shared driver with a single mode register (STA / AP /
APSTA). Both `sf_wifi` (STA + reconnect) and `sf_wifi_prov` (SoftAP) must
mutate this driver — but **only `sf_wifi` may call `esp_wifi_*` directly**.
`sf_wifi_prov` requests AP operations through `sf_wifi` functions.

### 4.1 One-time stack init

```c
/* Initialises the network stack exactly once:
 *   esp_netif_init()
 *   esp_event_loop_create_default()
 *   esp_netif_create_default_wifi_sta()
 *   esp_wifi_init(WIFI_INIT_CONFIG_DEFAULT)
 *   registers all WiFi + IP event handlers
 * Guarded internally — safe to call again (no-op). Called by
 * sf_wifi_prov_init() before any STA or AP operation. */
sf_err_t sf_wifi_driver_init(void);
```

### 4.2 STA event notifications (new)

`sf_wifi_connect()` returns a one-shot result for the initial connection attempt.
For all subsequent STA state changes, `sf_wifi` posts custom events to the
**ESP-IDF default event loop** — keeping the driver generic and allowing any
number of independent subscribers without any registration API on `sf_wifi`.

```c
ESP_EVENT_DECLARE_BASE(SF_WIFI_EVENT);

typedef enum {
    SF_WIFI_EVENT_CONNECTED = 0, /* STA got IP — perpetual phase only            */
    SF_WIFI_EVENT_DISCONNECTED,  /* STA disconnected — perpetual phase only       */
    SF_WIFI_EVENT_AUTH_FAIL,     /* auth fail in perpetual reconnect — stop retry */
} sf_wifi_event_id_t;
```

Subscribers use the standard ESP-IDF API:
```c
esp_event_handler_register(SF_WIFI_EVENT, SF_WIFI_EVENT_CONNECTED, handler, ctx);
```

Consumers in this project:
- `sf_wifi_prov`: subscribes to `SF_WIFI_EVENT_CONNECTED` — **sole owner of SNTP
  retry** (see §6.1 self-heal); calls `sf_time_sntp_restart()`.
- `sf_watering`: subscribes to all three events to drive the LED. May also call
  `sf_time_sntp_restart()` if it has independent reasons, but `sf_wifi_prov` is
  the authoritative trigger; `sf_time_sntp_restart()` is a no-op when already valid.

> **Event suppression during initial connect:**
> `sf_wifi` maintains an internal `s_initial_connect_phase` flag (set `true`
> before blocking on `xEventGroupWaitBits()`, cleared to `false` before
> `sf_wifi_connect()` returns). Events are posted **only when
> `!s_initial_connect_phase`**. The initial result is communicated solely via
> the return value of `sf_wifi_connect()`.

### 4.3 AP control (new — called by `sf_wifi_prov` only)

```c
/* Switch to APSTA mode, create the AP netif (once, guarded), configure and
 * start the AP. */
sf_err_t sf_wifi_ap_start(const char *ssid);

/* Stop the AP and switch back to STA-only mode. */
sf_err_t sf_wifi_ap_stop(void);
```

### 4.4 Driver ownership table

| Operation | Owner | Called by |
|---|---|---|
| `esp_netif_init()` | `sf_wifi_driver_init` | `sf_wifi_prov_init` (once) |
| `esp_event_loop_create_default()` | `sf_wifi_driver_init` | same |
| `esp_netif_create_default_wifi_sta()` | `sf_wifi_driver_init` | same |
| `esp_netif_create_default_wifi_ap()` | `sf_wifi_ap_start` | `sf_wifi_prov` via `sf_wifi` |
| `esp_wifi_init()` | `sf_wifi_driver_init` | same |
| `esp_wifi_set_mode()` | `sf_wifi` **only** | never called by `sf_wifi_prov` directly |
| `esp_wifi_connect()` | `sf_wifi` **only** | STA connect + reconnect handler |
| WiFi + IP event handlers | `sf_wifi` **only** | registered once in `sf_wifi_driver_init` |
| STA event notifications | `SF_WIFI_EVENT_*` on default event loop | `sf_wifi` → any subscriber |
| all AP config + start | `sf_wifi_ap_start` | `sf_wifi_prov` via `sf_wifi` |

---

## 5. `sf_wifi` and `sf_time` refactors

### 5.1 `sf_wifi` — full updated API

```c
typedef enum {
    SF_WIFI_CONN_OK = 0,    /* associated + got IP                       */
    SF_WIFI_CONN_AUTH_FAIL, /* wrong password / auth rejected            */
    SF_WIFI_CONN_NO_AP,     /* SSID not found / AP unreachable / timeout */
    SF_WIFI_CONN_FAIL,      /* any other failure                         */
} sf_wifi_conn_status_t;

/* Custom events posted to the default event loop after sf_wifi_connect()
 * returns — see §4.2 for event IDs and suppression rules. */
ESP_EVENT_DECLARE_BASE(SF_WIFI_EVENT);

sf_err_t              sf_wifi_driver_init(void);                          /* §4.1 */
sf_wifi_conn_status_t sf_wifi_connect(const char *ssid, const char *pwd); /* STA  */
sf_err_t              sf_wifi_ap_start(const char *ssid);                 /* §4.3 */
sf_err_t              sf_wifi_ap_stop(void);                              /* §4.3 */
/* sf_wifi_stop() exists in sf_wifi.h as a utility but has no call site in
 * this design — the device reboots on new credentials and self-heals forever. */
```

**Reason-code mapping.** `wifi_event_sta_disconnected_t.reason` drives retry and
classification. Only `WIFI_REASON_AUTH_FAIL` (202) is treated as a definitive
credential failure; all other reasons are treated as transient / NO_AP:

| Reason code | Value | Class | Rationale |
|---|---|---|---|
| `WIFI_REASON_AUTH_FAIL` | 202 | **AUTH_FAIL** | Only definitive wrong-password signal |
| `WIFI_REASON_NO_AP_FOUND` | 201 | NO_AP | SSID not visible |
| `WIFI_REASON_BEACON_TIMEOUT` | 200 | NO_AP | AP disappeared mid-session |
| `WIFI_REASON_ASSOC_FAIL` | 203 | NO_AP | Association rejected |
| `WIFI_REASON_CONNECTION_FAIL` | 205 | NO_AP | Generic transient |
| `WIFI_REASON_AP_TSF_RESET` | 206 | NO_AP | AP timing reset |
| `WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT` | 15 | **transient→NO_AP** | Ambiguous — AP busy or wrong pwd |
| `WIFI_REASON_HANDSHAKE_TIMEOUT` | 204 | **transient→NO_AP** | Same ambiguity |
| `WIFI_REASON_AUTH_EXPIRE` | 2 | **transient→NO_AP** | Auth window timeout |
| `WIFI_REASON_MIC_FAILURE` | 14 | **transient→NO_AP** | Packet corruption, not credential |
| everything else | — | NO_AP (safe fallback) | Retry is safer than declaring auth failure |

**Retry policy.** Base `esp_wifi` does *not* auto-reconnect — it only raises
`WIFI_EVENT_STA_DISCONNECTED`. `sf_wifi` owns all retry logic:

| Phase | `WIFI_REASON_AUTH_FAIL` (202) | Everything else |
|---|---|---|
| Initial `sf_wifi_connect()` call | **0 retries — return immediately** | Retry up to 5× |
| Perpetual reconnect handler | **Stop — surface via status callback** | Retry forever with backoff |

Returning immediately on `WIFI_REASON_AUTH_FAIL` avoids the boot delay that
would occur if bad credentials were retried 5× on every power cycle.

In APSTA mode the reconnect handler is safe — `esp_wifi_connect()` operates on
the STA interface only and does not affect the running AP.

- Remove `CONFIG_SIM_FLOW_WIFI_SSID` / `CONFIG_SIM_FLOW_WIFI_PASSWORD` Kconfig.

### 5.2 `sf_time` — time-valid flag

```c
sf_err_t sf_time_set_manual(time_t epoch);                            /* settimeofday() + mark valid */
bool     sf_time_is_valid(void);                                      /* true after SNTP or manual set */
/* Custom event posted to the default event loop on invalid→valid transition.
 * Any number of handlers may subscribe — matches the SF_WIFI_EVENT_* pattern. */
ESP_EVENT_DECLARE_BASE(SF_TIME_EVENT); /* event ID: SF_TIME_EVENT_VALID = 0 */
sf_err_t sf_time_sntp_restart(void);  /* re-trigger SNTP sync; no-op if already valid or offline */
/* Note: the internal flag-flip on SNTP success is not a public API — it lives
 * entirely within sf_time's SNTP sync callback. */
```

**Implementation — RTC retained memory + magic canary:**

```c
#include "esp_attr.h"

#define SF_TIME_RTC_MAGIC 0x5F54494Du  /* "_TIM" */

RTC_NOINIT_ATTR static uint32_t s_rtc_magic;
RTC_NOINIT_ATTR static bool     s_rtc_time_valid;

/* Call once at the top of sf_time_init(), before any validity read. */
static void sf_time_rtc_guard(void)
{
    if (s_rtc_magic != SF_TIME_RTC_MAGIC) {
        s_rtc_magic      = SF_TIME_RTC_MAGIC;
        s_rtc_time_valid = false;  /* power was lost — RTC clock also invalid */
    }
    /* else: magic intact → s_rtc_time_valid carried over from before the reset */
}

bool sf_time_is_valid(void)        { return s_rtc_time_valid; }
static void sf_time_mark_valid(void) { s_rtc_time_valid = true; }
```

**Why `RTC_NOINIT_ATTR`, not `RTC_DATA_ATTR`:** `RTC_DATA_ATTR` variables can be
reloaded from their initializer by startup code, wiping the flag on a soft reboot.
`RTC_NOINIT_ATTR` is never auto-initialized, so it genuinely persists.

**Why the magic canary:** after power loss the RTC RAM comes up as random garbage —
a bare `bool` has a ~50% chance of reading back as `true` on a cold boot. The magic
word makes the bit trustworthy: if the sentinel is intact, the bit was written
intentionally; if not, the domain was unpowered and both are reset.

**Deep-sleep caveat:** assumes `ESP_PD_DOMAIN_RTC_SLOW_MEM` remains powered (the
default on ESP32). If it were ever set to power-off, the RTC clock would be lost
too — the flag and clock stay consistent by sharing the same power domain.

### 5.3 `sf_wifi_prov` — public API

```c
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "sf_err.h"

typedef enum {
    SF_WIFI_PROV_CLOSED = 0,   /* SoftAP off — normal operation             */
    SF_WIFI_PROV_PROVISIONING, /* SoftAP + HTTP server are up               */
} sf_wifi_prov_state_t;
/* The internal state machine runs three states (CLOSED / OPEN_INDEFINITE /
 * OPEN_TIMED — see §6.1). Both OPEN states collapse to PROVISIONING in this
 * public enum: the caller only needs to know whether the SoftAP is up. */

typedef struct {
    const char *ap_ssid;             /* SSID broadcast in provisioning mode    */

    /* UI content — served from the mounted FatFS. Absolute VFS paths. */
    const char *prov_html_path;      /* "/sf_fatfs/sf_watering/prov.html"       */
    const char *success_html_path;   /* ".../success.html"                      */
    const char *logo_path;           /* ".../logo.png"; NULL => no /logo.png    */

    /* Invoked on SoftAP state transitions only (CLOSED ↔ PROVISIONING).
     * Used to toggle sf_pm light-sleep and notify sf_watering for LED.
     * May be NULL. Runs in event-loop context — keep it short. */
    void (*on_state_change)(sf_wifi_prov_state_t state, void *ctx);
    void  *state_ctx;
} sf_wifi_prov_config_t;

/* Boot entry point. Blocks through the bounded initial sf_wifi_connect()
 * attempt. Returns once the network subsystem is in motion. Behaviour (§6, §7):
 *   - no credentials      → connect impossible; if !time_ok start SoftAP
 *   - credentials present → sf_wifi_connect():
 *        OK        → sf_wifi self-heals thereafter; SoftAP not started
 *        NO_AP/FAIL→ keep creds; sf_wifi self-heals; if !time_ok start SoftAP
 *        AUTH_FAIL → KEEP creds (see §7); if !time_ok start SoftAP, else
 *                    wait for button
 */
sf_err_t sf_wifi_prov_init(const sf_wifi_prov_config_t *config);

/* Manually open the provisioning server for a 5-min window (button press).
 * If already in OPEN_TIMED, this is a no-op — the window is not extended. */
sf_err_t sf_wifi_prov_start_server(void);

/* Erase stored credentials from NVS. Factory-reset hook — not called
 * automatically by the component (see §7). */
sf_err_t sf_wifi_prov_clear(void);
```

### 5.4 Where the UI lives — FatFS

Pages and logo are **not** embedded in firmware; they are files in the existing
FatFS image (`sf_fatfs/sf_watering/`), built into the `storage` partition by
`fatfs_create_spiflash_image()` and mounted at `/sf_fatfs`:

```
sf_fatfs/sf_watering/
├── prov.html
├── success.html
└── logo.png
```

The component opens these with `fopen`/`fread` and streams them via
`httpd_resp_send_chunk()` (memory-friendly for the PNG). Content types fixed per
endpoint (`text/html`, `image/png`).

**Why FatFS:** UI updatable without reflashing; smaller firmware; **future
security:** files can be **signed** and verified before serving / before
accepting credentials (§17).

**Trade-off:** provisioning depends on a mounted FS — but schedules already do,
so it matches existing reliance. *(Optional built-in fallback page → O-8.)*

---

## 6. Behavioural model

The watering core **always proceeds**. SoftAP and reconnect run concurrently and
never halt the application.

### 6.1 SoftAP state machine

The SoftAP has three states:

```
  ┌──────────────────────────────────────────────────────────────────┐
  │                           CLOSED                                 │
  │                        (SoftAP off)                              │
  └───────────┬──────────────────────────────────┬───────────────────┘
              │                                  │
              │ !connected && !time_ok            │ button
              │ (boot or AUTH_FAIL)               │ 5s press
              ▼                                  ▼
  ┌───────────────────────┐            ┌───────────────────────┐
  │    OPEN_INDEFINITE    │            │      OPEN_TIMED        │
  │                       │  POST /time│                        │
  │  SoftAP up            ├───────────►│  SoftAP up             │
  │  no timeout           │  time_ok=T │  5-min countdown       │
  └──────────┬────────────┘  start 5min└──────────┬────────────┘
             │                                    │
             │ POST /connect                      │ POST /connect
             │ (save NVS)                         │ (save NVS)
             └──────────────┬─────────────────────┘
                            │
                      esp_restart()         5-min timeout
                            │                    │
                    ┌───────┴──────┐             │
                    │   [REBOOT]   │         ┌───▼──────┐
                    └───────┬──────┘         │  CLOSED  │
                            │                └──────────┘
               ┌────────────┴─────────────┐
               │                          │
          connected                  !connected
               │                     && !time_ok
               ▼                          │
            CLOSED                        ▼
                                   OPEN_INDEFINITE
```

> **Post-reboot fan-out (third outcome not drawn):** after the `POST /connect`
> reboot the device re-runs the boot auto-SoftAP rule. Besides the two arrows
> shown, the case `!connected && time_ok` (e.g. NO_AP but the RTC time survived
> the soft reboot, §5.2) lands in `CLOSED` and waits for the button — identical
> to the boot row in §6.2.

**State descriptions:**

| State | SoftAP | Timeout | Entered by |
|---|---|---|---|
| `CLOSED` | off | — | boot + connected; 5-min timeout fires; reboot + connected |
| `OPEN_INDEFINITE` | on | none | `!connected && !time_ok` at boot or after AUTH_FAIL |
| `OPEN_TIMED` | on | 5 min absolute | button 5s press; or `POST /time` from `OPEN_INDEFINITE` |

> **`OPEN_TIMED` timer is absolute — it does not reset or extend.**
> A button re-press while already in `OPEN_TIMED` is a no-op (window not
> extended). A `POST /time` while in `OPEN_TIMED` sets the time but does not
> reset the countdown. The window closes 5 minutes after it first opened,
> regardless of subsequent activity.

**Key transition — `POST /time` during `OPEN_INDEFINITE`:**
When the user sets the time (but not WiFi credentials), `time_ok` becomes `true`
and the device is no longer non-operational. The SoftAP does not close immediately
(the user may still want to enter WiFi credentials), but a **5-minute countdown
starts** from that moment. If no `POST /connect` arrives within 5 minutes, the
SoftAP closes automatically.

**Self-heal during `OPEN_INDEFINITE`:**
While the auto-started SoftAP is up, `sf_wifi` keeps retrying STA in the background —
`sf_wifi_prov` does not drive the reconnect. When the router returns:
`sf_wifi` posts `SF_WIFI_EVENT_CONNECTED` → `sf_wifi_prov` calls `sf_time_sntp_restart()`
→ SNTP success → `sf_time` marks flag valid internally → posts `SF_TIME_EVENT_VALID`
→ `sf_wifi_prov` evaluates: if `OPEN_INDEFINITE` → `sf_wifi_ap_stop()` → state = `CLOSED`.

> **Hardware constraint — APSTA channel coupling (ESP32):**
> The ESP32 has a single radio. In APSTA mode the SoftAP is forced onto the STA's
> channel. If STA connects to a router on a different channel than the AP is
> currently broadcasting, the AP hops channel and **drops any phone client that is
> currently connected to it** (e.g. a user mid-way through typing credentials).
> This is an accepted hardware limitation. Consequence: when `OPEN_INDEFINITE`
> self-heals, the AP is torn down **immediately** on STA connect — no graceful
> overlap is attempted, because the channel hop would drop the client anyway.
> The user must button-press to re-open a new AP window if they still need to
> change credentials.
>
> **Exception — button-triggered `OPEN_TIMED` (§6.4):** the device is already
> connected to the router on channel X before the AP starts. The AP is created
> on the same channel X — no hop occurs — APSTA overlap works cleanly.

### 6.2 Auto-SoftAP trigger verification

The `!connected && !time_ok` rule correctly handles all scenarios:

| Scenario | connected | time_ok | Auto SoftAP |
|---|---|---|---|
| Fresh device | no | no | **yes** |
| Power loss, router up | yes (after SNTP) | yes | no |
| Power loss, router down | no | no | **yes** |
| Router drops mid-run (no power loss) | no | yes (RTC kept) | no — keep watering, self-heal |
| AUTH_FAIL, time still valid (soft reboot) | no | yes | no — wait for button |
| AUTH_FAIL after power loss | no | no | **yes** |

### 6.3 Reconnect / self-heal

Handled entirely by `sf_wifi`'s disconnect handler (§5.1): perpetual
reconnect-with-backoff for non-auth failures. No separate retry task in the prov
component.

### 6.4 Button trigger

- Long-press detection (GPIO + timing) lives in `sf_watering` via `sf_gpio`.
- A **5-second** hold calls `sf_wifi_prov_start_server()` → enters `OPEN_TIMED`.
- The **only** way to open a provisioning window on a device that has valid time
  but bad/changed credentials (AUTH_FAIL case).
- If WiFi is already connected, runs **APSTA** for the window.
- If the button opens `OPEN_TIMED` while **disconnected** (NO_AP + time_ok) and
  `sf_wifi` self-heals mid-window, the APSTA channel hop drops the AP client
  (§6.1). `OPEN_TIMED` has **no connect-triggered teardown** — the window runs to
  its 5-min timeout; the user re-presses for a fresh window if still needed.

### 6.5 State machine synchronization

The SoftAP state is mutated from four independent execution contexts: the
WiFi/IP event task (STA connect → self-heal teardown), the HTTP server task
(`POST /time`, `POST /connect`), the `esp_timer` task (5-min timeout, 3s reboot
timer), and the button/GPIO context. Concurrent transitions — e.g. a self-heal
connect racing a 5-min timeout racing a `POST /connect` — must be serialized.

**Chosen approach: funnel all transitions through the ESP-IDF default event loop.**
Each context posts a custom `SF_PROV_EVENT_*` event via `esp_event_post()` and
returns immediately (fire-and-forget). The default event loop task, which is
already running (created in `sf_wifi_driver_init`, §4.1), dispatches them one
at a time to a single `prov_state_handler()`. Because the handler is always
called from the same task, no mutex is needed and logical races are impossible.

**Mandatory ordering rule for HTTP handlers — respond before posting:**
If a handler posts the event before sending the response, the event loop task
may call `httpd_stop()` (via an AP-teardown path — 5-min timeout or self-heal →
CLOSED), which waits for all active handlers to complete — but the handler is
still running → **deadlock**. (The `POST /connect` reboot defers to an
`esp_timer` → `esp_restart()` and does not itself call `httpd_stop()`, but the
same respond-first rule applies uniformly.)

Every HTTP handler in `sf_wifi_prov` must follow this pattern without exception:

```c
static esp_err_t post_connect_handler(httpd_req_t *req)
{
    /* 1. Do all work — parse, save NVS, etc. */
    /* ... */

    /* 2. Send the HTTP response FIRST — before any event post. */
    httpd_resp_send(req, response, len);

    /* 3. ONLY THEN post the event — handler is effectively done. */
    esp_event_post(SF_PROV_EVENT, EVT_POST_CONNECT, NULL, 0, portMAX_DELAY);

    /* 4. Return immediately — do not touch req or server state after this. */
    return ESP_OK;
}
```

The same rule applies to `POST /time` → `EVT_POST_TIME`. A comment marking
steps 2 and 3 must appear in every handler implementation as a violation guard.

Custom events defined by `sf_wifi_prov` (internal, funnelled through default event loop):

| Event | Posted by |
|---|---|
| `EVT_TIME_VALID` | `SF_TIME_EVENT_VALID` handler |
| `EVT_POST_TIME` | HTTP `POST /time` handler |
| `EVT_POST_CONNECT` | HTTP `POST /connect` handler |
| `EVT_TIMEOUT` | 5-min `esp_timer` callback |
| `EVT_BUTTON` | `sf_wifi_prov_start_server()` |

`SF_WIFI_EVENT_CONNECTED` from `sf_wifi` is subscribed to directly by both
`sf_wifi_prov` (SNTP retry trigger) and `sf_watering` (LED). It is not re-posted
as a prov-internal event.

---

## 7. Connection-failure decision tree

```
No credentials (fresh device, or explicit factory reset via sf_wifi_prov_clear())
   └── cannot connect
        ├── !time_ok → auto-start SoftAP            → PROVISIONING
        └──  time_ok → wait for button; keep watering → CLOSED (LED: solid red)

Credentials present
   └── sf_wifi_connect(ssid, password)
         ├── OK ───────► SNTP → time valid → operational; sf_wifi keeps link up
         │               → CLOSED (LED: solid green)
         │
         ├── NO_AP/FAIL► router down / transient
         │       • KEEP credentials (never wipe on NO_AP!)
         │       • sf_wifi self-heals (perpetual reconnect, §5.1)
         │       • !time_ok → auto-start SoftAP (APSTA); time_ok → keep watering
         │       • SF_WIFI_EVENT_DISCONNECTED posted → LED: solid red
         │
         └── AUTH_FAIL ► WIFI_REASON_AUTH_FAIL (202) — 0 retries, returns immediately
                 • KEEP credentials in NVS — do NOT clear (decided)
                 • !time_ok → auto-start SoftAP; time_ok → wait for button
                 • watering continues IF time valid, else scheduler idle
                 • result communicated via sf_wifi_connect() return value only
                   (events suppressed during initial connect per §4.2);
                   SF_WIFI_EVENT_AUTH_FAIL is the perpetual-phase signal only
                 • button press → SoftAP → POST /connect overwrites creds
```

**Why NOT clear on AUTH_FAIL (decided):**
With the reason-code mapping in §5.1, `WIFI_REASON_AUTH_FAIL` (202) returns
immediately with 0 retries — there is no boot delay from keeping bad creds.
Clearing gains nothing for the genuine bad-password case (next boot: kept creds
→ 1 immediate-fail attempt → same SoftAP / wait-for-button state). But clearing
is **strictly harmful** for any false-positive case (202 can surface on transient
glitches on a valid config → erasing working creds requires re-provisioning).
Keeping is reversible (button → overwrite); clearing is not.
`sf_wifi_prov_clear()` is reserved as explicit factory-reset hook only.

---

## 8. Device state → LED indication

LED is **derived** in `sf_watering`. It is evaluated once explicitly after
`sf_wifi_prov_init()` returns (covers boot terminal states that produce no
edge event), then re-evaluated on every `on_state_change`, every
`SF_WIFI_EVENT_*`, and on `SF_TIME_EVENT_VALID`:

```
prov    = (prov_state == SF_WIFI_PROV_PROVISIONING)
conn    = s_wifi_connected  /* local bool, toggled by SF_WIFI_EVENT_CONNECTED/DISCONNECTED */
time_ok = sf_time_is_valid()
```

| Priority | Condition                  | Meaning                          | LED               |
|----------|----------------------------|----------------------------------|-------------------|
| 1        | `prov`                     | provisioning / setup window open | **slow blink red**|
| 2        | `conn`                     | all good (connected)             | **solid green**   |
| 3        | `!time_ok`                 | no valid time → cannot schedule  | **fast blink red**|
| 4        | else (`!conn && time_ok`)  | offline but running on schedule  | **solid red**     |

LED driver stays in `sf_watering` (board-specific).

---

## 9. HTTP endpoints (served only while SoftAP is up)

| Method | Path        | Action                                                          | Restart? |
|--------|-------------|-----------------------------------------------------------------|----------|
| GET    | `/`         | stream `prov_html_path` from FatFS                              | no       |
| GET    | `/logo.png` | stream `logo_path` from FatFS (if non-NULL)                     | no       |
| GET    | `/api/time` | `sf_time_is_valid()` → `{"time":"<UTC Z>"}` or `{"time":null}` | no       |
| POST   | `/time`     | parse `time` → `sf_time_set_manual()` (posts `SF_TIME_EVENT_VALID`) | no       |
| POST   | `/connect`  | parse `ssid`+`password` (req), `time` (opt) → save NVS → reboot | **yes**  |

### 9.1 `POST /connect` fields

| Field      | Required | Action                       |
|------------|----------|------------------------------|
| `ssid`     | yes      | NVS `sf_prov/ssid`           |
| `password` | yes      | NVS `sf_prov/password`       |
| `time`     | no       | `sf_time_set_manual()`       |

> **Known limitation:** `password` is mandatory, so **open (passwordless)
> networks cannot be provisioned** in this revision. Acceptable for the target
> deployment; revisit if an open-SSID use case appears.

### 9.2 Time field contract — `POST /time` and `POST /connect`

**Format sent by the browser:** naive local datetime string, as produced by
`<input type="datetime-local">`: `"2025-05-31T14:35"` — no timezone, no UTC
offset.

**Why not JS UTC epoch or ISO-8601 with offset:**
- JS epoch (`new Date(val).getTime()/1000`) interprets the naive string in the
  **phone's** TZ. If phone TZ ≠ device TZ the result is silently wrong.
- ISO-8601 with offset (`+HH:MM`) from `getTimezoneOffset()` has the same flaw.
- The user is physically at the device location (to press the button), so device
  TZ = user's local TZ in all normal cases. The safest source of TZ is the
  **device itself**, not the phone.

**Device-side conversion (in the HTTP handler, not in `sf_time`):**
```c
struct tm tm = {0};
strptime(time_str, "%Y-%m-%dT%H:%M", &tm);
tm.tm_isdst = -1;           /* let mktime determine DST */
time_t epoch = mktime(&tm); /* interprets in device TZ — set at init step 5 */
sf_time_set_manual(epoch);
```

> **Assumption:** correctness depends on the device's TZ being set to the
> deployment region, so `mktime` interprets the wall-clock string correctly.
> Init step 5 (`sf_time_set_timezone(NULL)`) must resolve to that region; if it
> resolves to UTC while the user is not in UTC, manual time is offset by the
> local UTC delta. Provisioning the device TZ is out of scope here but is a
> prerequisite for accurate manual time.

**`GET /api/time` response:**
- If `sf_time_is_valid()` is true: `{"time":"2025-05-31T11:35:00Z"}` (UTC, Z suffix)
- If time not yet set: `{"time":null}`

The provisioning page JS must handle both:
```js
fetch('/api/time').then(r => r.json()).then(d => {
    el.innerText = d.time ? new Date(d.time).toLocaleString() : "not set";
});
```
Without this guard, an unset clock returns a ~1970 epoch, which renders as a
nonsensical date rather than signalling that time entry is needed.

### 9.3 Graceful reboot on `POST /connect`

Do **not** call `esp_restart()` or `httpd_stop()` inside the handler (cuts TCP
before flush / unsafe from own handler). Correct sequence:

```
POST /connect handler:
    parse body; save creds (+ optional time)
    stream success_html_path           ; self-contained — no external refs
    return ESP_OK                      ; httpd flushes TCP to send buffer
        └── one-shot esp_timer fires at 3 s:
              esp_restart()
```

**Reboot timer = JS countdown = 3 seconds.** They must match exactly:
- The 3 s window gives TCP time to fully transmit `success.html` to the browser
  before the device disappears.
- The browser's JS countdown reaches zero at the same moment the device reboots,
  so the user sees a clean "done" rather than an unexpected connection drop.
- **`success.html` must be entirely self-contained** (inline CSS, no logo, no
  external resources). A follow-up request for `/logo.png` after the countdown
  page loads would fail if the device reboots before serving it.

> **Why not 1 s:** `httpd_resp_send()` writes to the TCP send buffer; actual
> over-the-air transmission is not complete at `return ESP_OK`. A 1 s window
> is too short to guarantee the page reaches the browser over an open AP link,
> especially on a congested 2.4 GHz channel. 3 s is conservative and robust.

---

## 10. Provisioning page (authored in FatFS by `sf_watering`)

```
┌─────────────────────────────┐
│        [ SimFlow Logo ]     │   <img src="/logo.png">
│   Device time: 14:35:22 ←JS │   fetch('/api/time') + tick every 1 s
│                             │
│  ── WiFi Setup ──────────   │
│  Network:  [____________]   │   form → POST /connect
│  Password: [____________]   │
│            [   Connect   ]  │
│                             │
│  ── Set Time ────────────   │
│  Date/Time:[____________]   │   form → POST /time   (no reboot)
│            [  Set Time   ]  │
└─────────────────────────────┘
```

Palette from the logo: bg `#F2F2F0`, primary `#2EC9C9`, accent `#1E7A82`.

---

## 11. NVS layout

| Namespace | Key        | Type   | Max  | Notes                 |
|-----------|------------|--------|------|-----------------------|
| `sf_prov` | `ssid`     | string | 32 B | WiFi SSID             |
| `sf_prov` | `password` | string | 64 B | WiFi password (plain) |

Plaintext — encryption deferred (§14). No time in NVS (O-1).

---

## 12. `sf_watering` init re-ordering

```
init_device():
   1. sf_gpio_init()
   2. nvs_flash_init()              (existing erase-on-corrupt recovery)
   3. sf_file_init_fs("/sf_fatfs")  ── MOVED UP   (also provides UI files)
   4. sf_watering_load_from_file()  ── MOVED UP   (schedules ready offline)
   5. sf_time_set_timezone(NULL)
   6. sf_wifi_prov_init(&prov_cfg)  ── bounded initial connect, then returns  ← WDT note below
   6a. sf_time_init()               ── must follow prov_init (needs esp_netif_init from step 6)
   7. sf_time_sntp_restart()         ── unconditional; no-op when offline; failure NOT fatal
   8. sf_pm light-sleep             ── toggled by state callback (§13)
   device_start():
      scheduler runs; each job gated by sf_time_is_valid() (§2)
```

SNTP failure no longer `goto FAIL` — it just means "time not valid yet"; self-
heal reconnect or manual entry resolves it later.

> **Task WDT constraint (step 6):**
> `sf_wifi_prov_init()` blocks during the initial `sf_wifi_connect()` — worst
> case ~25–30 s (5 retries × ~5 s association timeout each, NO_AP path). The
> default Task WDT timeout is 5 s (`CONFIG_ESP_TASK_WDT_TIMEOUT_S`). If the
> task running `init_device()` is subscribed to the WDT and the WDT is
> initialized, it will reset the device before the retries complete.
>
> **Current project status:** `CONFIG_ESP_TASK_WDT_INIT is not set` — WDT is
> compiled in but not auto-started, so the risk is low now. This becomes real
> when WDT is enabled for production.
>
> **Mitigation options (decide at impl):**
> 1. Do not subscribe `init_device()`'s task to the WDT during step 6; subscribe
>    after `sf_wifi_prov_init()` returns.
> 2. Feed the WDT (`esp_task_wdt_reset()`) between retries inside
>    `sf_wifi_connect()`.

### Integration sketch

```c
#define WEBUI_DIR "/sf_fatfs/sf_watering"

static bool s_wifi_connected = false;

/* Fired on SoftAP up/down — drives light-sleep and LED. */
static void on_prov_state(sf_wifi_prov_state_t state, void *ctx)
{
    watering_update_led();                                                /* §8  */
    sf_pm_set_light_sleep_power_mode(state != SF_WIFI_PROV_PROVISIONING); /* §13 */
}

/* Subscribed via esp_event_handler_register(SF_TIME_EVENT, SF_TIME_EVENT_VALID, ...) */
static void on_time_valid(void *ctx, esp_event_base_t base,
                          int32_t id, void *data)
{
    watering_update_led();   /* §8 — time_ok is now true */
}

/* Subscribed via esp_event_handler_register(SF_WIFI_EVENT, ...) in init. */
static void on_wifi_event(void *ctx, esp_event_base_t base,
                          int32_t id, void *data)
{
    s_wifi_connected = (id == SF_WIFI_EVENT_CONNECTED);
    watering_update_led();
}

static const sf_wifi_prov_config_t prov_cfg = {
    .ap_ssid           = "SIMFLOW-Watering",
    .prov_html_path    = WEBUI_DIR "/prov.html",
    .success_html_path = WEBUI_DIR "/success.html",
    .logo_path         = WEBUI_DIR "/logo.png",
    .on_state_change   = on_prov_state,
    .state_ctx         = NULL,
};

static sf_err_t init_network(void)
{
#ifdef CONFIG_SIM_FLOW_EMULATION_BUILD
    return sf_eth_init();
#else
    /* sf_wifi_prov_init() calls sf_wifi_driver_init() internally, which
     * creates the default event loop. SF_WIFI_EVENT and SF_TIME_EVENT handlers
     * must be registered AFTER this call. SF_WIFI_EVENT_* events are suppressed
     * during the blocking initial connect, so nothing is missed. */
    sf_err_t ret = sf_wifi_prov_init(&prov_cfg);

    esp_event_handler_register(SF_WIFI_EVENT,  ESP_EVENT_ANY_ID,
                                on_wifi_event, NULL);
    esp_event_handler_register(SF_TIME_EVENT, SF_TIME_EVENT_VALID,
                                on_time_valid, NULL);

    /* Boot terminal states (no-creds+time_ok, AUTH_FAIL+time_ok, etc.) produce
     * no edge event — evaluate LED once explicitly to cover them all. */
    watering_update_led();
    return ret;
#endif
}
```

---

## 13. Power-management interaction (resolves O-2)

A running **SoftAP cannot light-sleep**. Power mode is toggled by the **same**
`on_state_change` callback that drives the LED:

```
state == PROVISIONING  → disable light sleep
otherwise              → enable light sleep
self-heal reconnect     → light-sleep compatible (event/timer wake)
```

---

## 14. Security posture

- SoftAP comes up **only** when the device is non-operational (`!connected &&
  !time_ok`) or via the **5-min** button window — never as a surprise on a
  healthy, operating unit.
- HTTP server torn down on reboot immediately after `POST /connect`.
- AP is **open (no password)** for now → optional WPA2 tracked in
  [issue #60](https://github.com/evyatarv/SimFlow/issues/60) (O-3).
- **Future:** signed UI files served from FatFS (§5.4).
- **Deferred to production:** Secure Boot v2, Flash Encryption, NVS encryption.

---

## 15. IDF dependencies & build

```cmake
# sf_wifi
REQUIRES esp_wifi esp_netif esp_event log

# sf_wifi_prov  (uses VFS/newlib fopen on mounted FatFS — no fatfs REQUIRE)
REQUIRES esp_wifi esp_netif esp_event esp_http_server nvs_flash esp_timer log sf_wifi sf_time

# sf_time  (esp_netif needed by esp_netif_sntp; esp_event for SF_TIME_EVENT)
REQUIRES esp_netif esp_event log
```

UI files ship in the FatFS image (no `EMBED_*`):

```
sf_fatfs/sf_watering/prov.html
sf_fatfs/sf_watering/success.html
sf_fatfs/sf_watering/logo.png        (optimised SIM_FLOW_LOGO.png, O-6)
```

Baked into `storage` by the existing `fatfs_create_spiflash_image(storage
../sf_fatfs ...)` in `main/CMakeLists.txt`.

We deliberately do **not** use ESP-IDF's `wifi_provisioning` manager (needs the
Espressif phone app; we want a plain browser flow).

---

## 16. Component / file layout

```
components/sf_wifi_prov/
├── CMakeLists.txt
├── sf_wifi_prov.h
└── sf_wifi_prov.c

sf_fatfs/sf_watering/
├── prov.html
├── success.html
└── logo.png
```

---

## 17. Open items / decisions

| ID  | Item                                                            | Status / decision                          |
|-----|-----------------------------------------------------------------|--------------------------------------------|
| O-1 | Drop NVS time persistence                                       | **Decided: dropped**                       |
| O-2 | `sf_wifi_prov` ↔ `sf_pm` light-sleep toggle                     | **Resolved:** unified state callback       |
| O-3 | Password-protect the SoftAP                                     | **Tracked:** issue #60                     |
| O-4 | Button GPIO pin & debounce                                      | decide at impl in `sf_watering`            |
| O-5 | Reconnect backoff policy (interval/cap)                         | **In `sf_wifi`**; propose e.g. 1→30 s backoff |
| O-6 | Logo size / optimisation target                                 | resize ~150 px, target ≤ ~15 KB            |
| O-7 | SoftAP inactivity timeout (button window)                       | **5 min absolute**                         |
| O-8 | Minimal built-in fallback page if FatFS unavailable             | **Open** — accept FS dependency for now?   |
| O-9 | Captive-portal DNS responder                                    | **Open** — see below                       |

**O-9 — Captive-portal DNS responder:**
Modern iOS and Android detect "no internet" APs aggressively — they show "No
internet connection" warnings and may auto-disconnect before the user can open a
browser. A lightweight UDP DNS server that answers **every query** with the AP
IP (`192.168.4.1`) triggers the OS captive-portal detection flow, which opens
the browser and navigates to the provisioning page automatically — no manual URL
required. Implementation: a small FreeRTOS task bound to UDP port 53, running
only while the SoftAP is up. Recommend adding before the first real-device test.

**Future enhancement:** sign the FatFS-hosted UI files and verify before serving
/ before accepting credentials (ties into the Secure Boot story).

---

## 18. What stays unchanged

- NVS init in `sf_watering.c` (already present).
- `sf_cron_job`, `sf_files` internals; `sf_gpio` gains a button read.
- Emulation path (`sf_eth_init()` under `CONFIG_SIM_FLOW_EMULATION_BUILD`).
</content>
