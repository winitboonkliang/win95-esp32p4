//! B83 hand-written engine parts (not generated). Each one replaces a generated function of the same C symbol; the generated one stays
//! as `<name>_c2r` (tools/fixown.py OWN list) for A/B. Bit-exact rule: lab:LEAD/B83 oracle (demo1..3 frame + state hashes).
pub mod par;
pub mod wadmap;

/// called at the end of b82_data_init (tools/fixown.py): install the own parts that are reached through data (function tables)
pub unsafe fn init() {
    wadmap::install();
}
