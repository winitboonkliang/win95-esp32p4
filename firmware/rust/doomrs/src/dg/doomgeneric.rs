extern "C" {
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    static mut myargc: ::core::ffi::c_int;
    static mut myargv: *mut *mut ::core::ffi::c_char;
    fn M_FindResponseFile();
    fn malloc(_: size_t) -> *mut ::core::ffi::c_void;
    fn DG_Init();
    fn D_DoomMain();
}
pub type size_t = usize;
pub type uint8_t = u8;
pub type pixel_t = uint8_t;
pub const NULL: *mut ::core::ffi::c_void = ::core::ptr::null_mut::<::core::ffi::c_void>();
static mut DG_ScreenBuffer__b82i: *mut pixel_t = ::core::ptr::null::<pixel_t>() as *mut pixel_t;
#[no_mangle]
pub static mut DG_ScreenBuffer: *mut pixel_t = ::core::ptr::null::<pixel_t>() as *mut pixel_t;
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn doomgeneric_Create(
    mut argc: ::core::ffi::c_int,
    mut argv: *mut *mut ::core::ffi::c_char,
) {
    myargc = argc;
    myargv = argv;
    M_FindResponseFile();
    if DG_ScreenBuffer.is_null() {
        DG_ScreenBuffer = malloc(
            ((DOOMGENERIC_RESX * DOOMGENERIC_RESY) as size_t)
                .wrapping_mul(::core::mem::size_of::<pixel_t>() as size_t),
        ) as *mut pixel_t;
    }
    DG_Init();
    D_DoomMain();
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_doomgeneric() {
    memcpy(
        &raw mut DG_ScreenBuffer as *mut ::core::ffi::c_void,
        &raw const DG_ScreenBuffer__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<*mut pixel_t>() as size_t,
    );
}
pub const DOOMGENERIC_RESX: ::core::ffi::c_int = 640 as ::core::ffi::c_int;
pub const DOOMGENERIC_RESY: ::core::ffi::c_int = 400 as ::core::ffi::c_int;
