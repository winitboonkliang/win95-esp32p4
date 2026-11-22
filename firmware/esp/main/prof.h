/* B50_PROF: board profiler (tools/bench/prof). Off unless the build defines B50_PROF (cmake -DB50_PROF=1).
 *  - sampler: per-core 2 kHz timer interrupt records the interrupted PC (mepc) in a 64-byte-granule histogram (PSRAM);
 *    buckets come from prof_map.inc, generated from the ELF by tools/bench/prof/mkmap.py
 *  - explicit wait counters: PROF_W_BEGIN/PROF_W_END around the places where a core waits (disk, PPA, halt sleep, lock):
 *    two mcycle reads + one add per event, so they also say WHY a core was not executing guest code
 *  - report every 10 s on the console as "[prof]" lines and on demand: type "prof" + Enter on the console */
#pragma once
#include <stdint.h>
#ifdef B50_PROF
enum { PW_DISK, PW_PPA, PW_IDLE, PW_LOCK, PW_N };
extern uint64_t prof_wait_cyc[2][PW_N];
extern uint32_t prof_wait_cnt[2][PW_N];
static inline uint32_t prof_cyc(void) { uint32_t v; __asm__ volatile("csrr %0, mcycle" : "=r"(v)); return v; }
static inline uint32_t prof_core(void) { uint32_t v; __asm__ volatile("csrr %0, mhartid" : "=r"(v)); return v & 1; }
#define PROF_W_BEGIN(id) uint32_t _pw_##id = prof_cyc()
#define PROF_W_END(id) do { uint32_t _c = prof_core(); prof_wait_cyc[_c][id] += (uint32_t)(prof_cyc() - _pw_##id); prof_wait_cnt[_c][id]++; } while (0)
void prof_init(void);
void prof_request(void);   /* one-shot report (console command "prof") */
#else
#define PROF_W_BEGIN(id) do { } while (0)
#define PROF_W_END(id) do { } while (0)
static inline void prof_init(void) {}
static inline void prof_request(void) {}
#endif
