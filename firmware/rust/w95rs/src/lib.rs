#![no_std]
// b40 + fwhost export their C ABI (b40_*, fw_*); snapcodec = B61 snapshot codec (snap_*)
extern crate b40;
extern crate fwhost;
pub mod snapcodec;
