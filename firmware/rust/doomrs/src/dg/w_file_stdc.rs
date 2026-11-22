extern "C" {
    pub type b83_FILE;
    fn fopen(_: *const ::core::ffi::c_char, _: *const ::core::ffi::c_char) -> *mut FILE;
    fn fclose(_: *mut FILE) -> ::core::ffi::c_int;
    fn fread(_: *mut ::core::ffi::c_void, _: size_t, _: size_t, _: *mut FILE) -> size_t;
    fn fseek(_: *mut FILE, _: ::core::ffi::c_long, _: ::core::ffi::c_int) -> ::core::ffi::c_int;
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn M_FileLength(handle: *mut FILE) -> ::core::ffi::c_long;
    fn Z_Malloc(
        size: ::core::ffi::c_int,
        tag: ::core::ffi::c_int,
        ptr: *mut ::core::ffi::c_void,
    ) -> *mut ::core::ffi::c_void;
    fn Z_Free(ptr: *mut ::core::ffi::c_void);
}
pub type size_t = usize;
pub type FILE = b83_FILE;
pub type uint8_t = u8;
pub type byte = uint8_t;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct _wad_file_s {
    pub file_class: *mut wad_file_class_t,
    pub mapped: *mut byte,
    pub length: ::core::ffi::c_uint,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct wad_file_class_t {
    pub OpenFile: Option<unsafe extern "C" fn(*mut ::core::ffi::c_char) -> *mut wad_file_t>,
    pub CloseFile: Option<unsafe extern "C" fn(*mut wad_file_t) -> ()>,
    pub Read: Option<
        unsafe extern "C" fn(
            *mut wad_file_t,
            ::core::ffi::c_uint,
            *mut ::core::ffi::c_void,
            size_t,
        ) -> size_t,
    >,
}
pub type wad_file_t = _wad_file_s;
pub type C2RustUnnamed = ::core::ffi::c_uint;
pub const PU_NUM_TAGS: C2RustUnnamed = 9;
pub const PU_CACHE: C2RustUnnamed = 8;
pub const PU_PURGELEVEL: C2RustUnnamed = 7;
pub const PU_LEVSPEC: C2RustUnnamed = 6;
pub const PU_LEVEL: C2RustUnnamed = 5;
pub const PU_FREE: C2RustUnnamed = 4;
pub const PU_MUSIC: C2RustUnnamed = 3;
pub const PU_SOUND: C2RustUnnamed = 2;
pub const PU_STATIC: C2RustUnnamed = 1;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct stdc_wad_file_t {
    pub wad: wad_file_t,
    pub fstream: *mut FILE,
}
pub const NULL: *mut ::core::ffi::c_void = ::core::ptr::null_mut::<::core::ffi::c_void>();
pub const SEEK_SET: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
#[optimize(size)]
unsafe extern "C" fn W_StdC_OpenFile(mut path: *mut ::core::ffi::c_char) -> *mut wad_file_t {
    let mut result: *mut stdc_wad_file_t = ::core::ptr::null_mut::<stdc_wad_file_t>();
    let mut fstream: *mut FILE = ::core::ptr::null_mut::<FILE>();
    fstream = fopen(path, b"rb\0" as *const u8 as *const ::core::ffi::c_char);
    if fstream.is_null() {
        return ::core::ptr::null_mut::<wad_file_t>();
    }
    result = Z_Malloc(
        ::core::mem::size_of::<stdc_wad_file_t>() as ::core::ffi::c_int,
        PU_STATIC as ::core::ffi::c_int,
        ::core::ptr::null_mut::<::core::ffi::c_void>(),
    ) as *mut stdc_wad_file_t;
    (*result).wad.file_class = &raw mut stdc_wad_file;
    (*result).wad.mapped = ::core::ptr::null_mut::<byte>();
    (*result).wad.length = M_FileLength(fstream) as ::core::ffi::c_uint;
    (*result).fstream = fstream;
    return &raw mut (*result).wad;
}
#[optimize(size)]
unsafe extern "C" fn W_StdC_CloseFile(mut wad: *mut wad_file_t) {
    let mut stdc_wad: *mut stdc_wad_file_t = ::core::ptr::null_mut::<stdc_wad_file_t>();
    stdc_wad = wad as *mut stdc_wad_file_t;
    fclose((*stdc_wad).fstream);
    Z_Free(stdc_wad as *mut ::core::ffi::c_void);
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn W_StdC_Read(
    mut wad: *mut wad_file_t,
    mut offset: ::core::ffi::c_uint,
    mut buffer: *mut ::core::ffi::c_void,
    mut buffer_len: size_t,
) -> size_t {
    let mut stdc_wad: *mut stdc_wad_file_t = ::core::ptr::null_mut::<stdc_wad_file_t>();
    let mut result: size_t = 0;
    stdc_wad = wad as *mut stdc_wad_file_t;
    fseek((*stdc_wad).fstream, offset as ::core::ffi::c_long, SEEK_SET);
    result = fread(buffer, 1 as size_t, buffer_len, (*stdc_wad).fstream);
    return result;
}
static mut stdc_wad_file__b82i: wad_file_class_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut stdc_wad_file: wad_file_class_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_w_file_stdc() {
    memcpy(
        &raw mut stdc_wad_file as *mut ::core::ffi::c_void,
        &raw const stdc_wad_file__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<wad_file_class_t>() as size_t,
    );
}

/// B83 (tools/fixdata.py): initial values, flash -> PSRAM .bss
#[optimize(size)]
pub unsafe fn b83i() {
    stdc_wad_file__b82i = unsafe {
    wad_file_class_t {
        OpenFile: Some(
            W_StdC_OpenFile as unsafe extern "C" fn(*mut ::core::ffi::c_char) -> *mut wad_file_t,
        ),
        CloseFile: Some(W_StdC_CloseFile as unsafe extern "C" fn(*mut wad_file_t) -> ()),
        Read: Some(
            W_StdC_Read
                as unsafe extern "C" fn(
                    *mut wad_file_t,
                    ::core::ffi::c_uint,
                    *mut ::core::ffi::c_void,
                    size_t,
                ) -> size_t,
        ),
    }
};
    stdc_wad_file = wad_file_class_t {
    OpenFile: None,
    CloseFile: None,
    Read: None,
};
}