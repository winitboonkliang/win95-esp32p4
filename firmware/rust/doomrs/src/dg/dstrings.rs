extern "C" {
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
}
pub type size_t = usize;
static mut doom1_endmsg__b82i: [*mut ::core::ffi::c_char; 8] = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut doom1_endmsg: [*mut ::core::ffi::c_char; 8] =
    [::core::ptr::null::<::core::ffi::c_char>() as *mut ::core::ffi::c_char; 8];
static mut doom2_endmsg__b82i: [*mut ::core::ffi::c_char; 8] = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut doom2_endmsg: [*mut ::core::ffi::c_char; 8] =
    [::core::ptr::null::<::core::ffi::c_char>() as *mut ::core::ffi::c_char; 8];
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_dstrings() {
    memcpy(
        &raw mut doom1_endmsg as *mut ::core::ffi::c_void,
        &raw const doom1_endmsg__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<[*mut ::core::ffi::c_char; 8]>() as size_t,
    );
    memcpy(
        &raw mut doom2_endmsg as *mut ::core::ffi::c_void,
        &raw const doom2_endmsg__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<[*mut ::core::ffi::c_char; 8]>() as size_t,
    );
}

/// B83 (tools/fixdata.py): initial values, flash -> PSRAM .bss
#[optimize(size)]
pub unsafe fn b83i() {
    doom1_endmsg__b82i = [
    b"are you sure you want to\nquit this great game?\0" as *const u8 as *const ::core::ffi::c_char
        as *mut ::core::ffi::c_char,
    b"please don't leave, there's more\ndemons to toast!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"let's beat it -- this is turning\ninto a bloodbath!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"i wouldn't leave if i were you.\ndos is much worse.\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"you're trying to say you like dos\nbetter than me, right?\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"don't leave yet -- there's a\ndemon around that corner!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"ya know, next time you come in here\ni'm gonna toast ya.\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"go ahead and leave. see if i care.\0" as *const u8 as *const ::core::ffi::c_char
        as *mut ::core::ffi::c_char,
];
    doom2_endmsg__b82i = [
    b"are you sure you want to\nquit this great game?\0" as *const u8 as *const ::core::ffi::c_char
        as *mut ::core::ffi::c_char,
    b"you want to quit?\nthen, thou hast lost an eighth!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"don't go now, there's a \ndimensional shambler waiting\nat the dos prompt!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"get outta here and go back\nto your boring programs.\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"if i were your boss, i'd \n deathmatch ya in a minute!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"look, bud. you leave now\nand you forfeit your body count!\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"just leave. when you come\nback, i'll be waiting with a bat.\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
    b"you're lucky i don't smack\nyou for thinking about leaving.\0" as *const u8
        as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
];
}