extern "C" {
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
}
pub type size_t = usize;
pub type boolean = ::core::ffi::c_uint;
pub const false_0: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
static mut net_client_connected__b82i: boolean = false_0 as boolean;
#[no_mangle]
pub static mut net_client_connected: boolean = 0;
static mut drone__b82i: boolean = false_0 as boolean;
#[no_mangle]
pub static mut drone: boolean = 0;
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_dummy() {
    memcpy(
        &raw mut net_client_connected as *mut ::core::ffi::c_void,
        &raw const net_client_connected__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<boolean>() as size_t,
    );
    memcpy(
        &raw mut drone as *mut ::core::ffi::c_void,
        &raw const drone__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<boolean>() as size_t,
    );
}
