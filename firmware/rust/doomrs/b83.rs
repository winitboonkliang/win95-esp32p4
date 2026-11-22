//! B83 crate glue (hand-written, not generated): panic -> the host's engine error path (Doom boot: back to Windows).
extern "C" {
    fn b82_error(m: *const core::ffi::c_char);
}

#[panic_handler]
fn panic(_i: &core::panic::PanicInfo) -> ! {
    // shortcut: no message formatting (no alloc/fmt in the engine); file:line goes to the console once we need it
    unsafe { b82_error(b"Rust engine panic\0".as_ptr() as *const core::ffi::c_char) };
    loop {}
}
