extern "C" {
    pub type b83_FILE;
    fn printf(_: *const ::core::ffi::c_char, ...) -> ::core::ffi::c_int;
    fn vsnprintf(
        _: *mut ::core::ffi::c_char,
        _: size_t,
        _: *const ::core::ffi::c_char,
        _: ::core::ffi::VaList,
    ) -> ::core::ffi::c_int;
    fn sscanf(
        _: *const ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
        ...
    ) -> ::core::ffi::c_int;
    fn fopen(_: *const ::core::ffi::c_char, _: *const ::core::ffi::c_char) -> *mut FILE;
    fn fclose(_: *mut FILE) -> ::core::ffi::c_int;
    fn fread(_: *mut ::core::ffi::c_void, _: size_t, _: size_t, _: *mut FILE) -> size_t;
    fn fwrite(_: *const ::core::ffi::c_void, _: size_t, _: size_t, _: *mut FILE) -> size_t;
    fn fseek(_: *mut FILE, _: ::core::ffi::c_long, _: ::core::ffi::c_int) -> ::core::ffi::c_int;
    fn ftell(_: *mut FILE) -> ::core::ffi::c_long;
    fn malloc(_: size_t) -> *mut ::core::ffi::c_void;
    fn memset(
        _: *mut ::core::ffi::c_void,
        _: ::core::ffi::c_int,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn strlen(_: *const ::core::ffi::c_char) -> size_t;
    fn strcmp(_: *const ::core::ffi::c_char, _: *const ::core::ffi::c_char) -> ::core::ffi::c_int;
    fn strncmp(
        _: *const ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
        _: size_t,
    ) -> ::core::ffi::c_int;
    fn strncpy(
        _: *mut ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
        _: size_t,
    ) -> *mut ::core::ffi::c_char;
    fn strstr(
        _: *const ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
    ) -> *mut ::core::ffi::c_char;
    fn strdup(_: *const ::core::ffi::c_char) -> *mut ::core::ffi::c_char;
    fn strncasecmp(
        _: *const ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
        _: size_t,
    ) -> ::core::ffi::c_int;
    fn toupper(_: ::core::ffi::c_int) -> ::core::ffi::c_int;
    fn b83_errno() -> *mut ::core::ffi::c_int;
    fn mkdir(_: *const ::core::ffi::c_char, _: mode_t) -> ::core::ffi::c_int;
    fn I_Error(error: *mut ::core::ffi::c_char, ...);
    fn Z_Malloc(
        size: ::core::ffi::c_int,
        tag: ::core::ffi::c_int,
        ptr: *mut ::core::ffi::c_void,
    ) -> *mut ::core::ffi::c_void;
}
pub type __builtin_va_list = [__va_list_tag; 1];
#[derive(Copy, Clone)]
#[repr(C)]
pub struct __va_list_tag {
    pub gp_offset: ::core::ffi::c_uint,
    pub fp_offset: ::core::ffi::c_uint,
    pub overflow_arg_area: *mut ::core::ffi::c_void,
    pub reg_save_area: *mut ::core::ffi::c_void,
}
pub type size_t = usize;
pub type va_list = __builtin_va_list;
pub type FILE = b83_FILE;
pub type mode_t = ::core::ffi::c_uint;
pub type uint8_t = u8;
pub type boolean = ::core::ffi::c_uint;
pub type byte = uint8_t;
pub const PU_STATIC: C2RustUnnamed = 1;
pub type C2RustUnnamed = ::core::ffi::c_uint;
pub const PU_NUM_TAGS: C2RustUnnamed = 9;
pub const PU_CACHE: C2RustUnnamed = 8;
pub const PU_PURGELEVEL: C2RustUnnamed = 7;
pub const PU_LEVSPEC: C2RustUnnamed = 6;
pub const PU_LEVEL: C2RustUnnamed = 5;
pub const PU_FREE: C2RustUnnamed = 4;
pub const PU_MUSIC: C2RustUnnamed = 3;
pub const PU_SOUND: C2RustUnnamed = 2;
pub const NULL: *mut ::core::ffi::c_void = ::core::ptr::null_mut::<::core::ffi::c_void>();
pub const SEEK_SET: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
pub const SEEK_END: ::core::ffi::c_int = 2 as ::core::ffi::c_int;
pub const EISDIR: ::core::ffi::c_int = 21 as ::core::ffi::c_int;
pub const true_0: ::core::ffi::c_int = 1 as ::core::ffi::c_int;
pub const false_0: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
pub const DIR_SEPARATOR: ::core::ffi::c_int = '/' as i32;
pub const DIR_SEPARATOR_S: [::core::ffi::c_char; 2] =
    unsafe { ::core::mem::transmute::<[u8; 2], [::core::ffi::c_char; 2]>(*b"/\0") };
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_MakeDirectory(mut path: *mut ::core::ffi::c_char) {
    mkdir(path, 0o755 as mode_t);
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_FileExists(mut filename: *mut ::core::ffi::c_char) -> boolean {
    let mut fstream: *mut FILE = ::core::ptr::null_mut::<FILE>();
    fstream = fopen(filename, b"r\0" as *const u8 as *const ::core::ffi::c_char);
    if !fstream.is_null() {
        fclose(fstream);
        return true_0 as boolean;
    } else {
        return (*b83_errno() == EISDIR) as ::core::ffi::c_int as boolean;
    };
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_FileLength(mut handle: *mut FILE) -> ::core::ffi::c_long {
    let mut savedpos: ::core::ffi::c_long = 0;
    let mut length: ::core::ffi::c_long = 0;
    savedpos = ftell(handle);
    fseek(handle, 0 as ::core::ffi::c_long, SEEK_END);
    length = ftell(handle);
    fseek(handle, savedpos, SEEK_SET);
    return length;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_WriteFile(
    mut name: *mut ::core::ffi::c_char,
    mut source: *mut ::core::ffi::c_void,
    mut length: ::core::ffi::c_int,
) -> boolean {
    let mut handle: *mut FILE = ::core::ptr::null_mut::<FILE>();
    let mut count: ::core::ffi::c_int = 0;
    handle = fopen(name, b"wb\0" as *const u8 as *const ::core::ffi::c_char);
    if handle.is_null() {
        return false_0 as boolean;
    }
    count = fwrite(source, 1 as size_t, length as size_t, handle) as ::core::ffi::c_int;
    fclose(handle);
    if count < length {
        return false_0 as boolean;
    }
    return true_0 as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_ReadFile(
    mut name: *mut ::core::ffi::c_char,
    mut buffer: *mut *mut byte,
) -> ::core::ffi::c_int {
    let mut handle: *mut FILE = ::core::ptr::null_mut::<FILE>();
    let mut count: ::core::ffi::c_int = 0;
    let mut length: ::core::ffi::c_int = 0;
    let mut buf: *mut byte = ::core::ptr::null_mut::<byte>();
    handle = fopen(name, b"rb\0" as *const u8 as *const ::core::ffi::c_char);
    if handle.is_null() {
        I_Error(
            b"Couldn't read file %s\0" as *const u8 as *const ::core::ffi::c_char
                as *mut ::core::ffi::c_char,
            name,
        );
    }
    length = M_FileLength(handle) as ::core::ffi::c_int;
    buf = Z_Malloc(length, PU_STATIC as ::core::ffi::c_int, NULL) as *mut byte;
    count = fread(
        buf as *mut ::core::ffi::c_void,
        1 as size_t,
        length as size_t,
        handle,
    ) as ::core::ffi::c_int;
    fclose(handle);
    if count < length {
        I_Error(
            b"Couldn't read file %s\0" as *const u8 as *const ::core::ffi::c_char
                as *mut ::core::ffi::c_char,
            name,
        );
    }
    *buffer = buf;
    return length;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_TempFile(mut s: *mut ::core::ffi::c_char) -> *mut ::core::ffi::c_char {
    let mut tempdir: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    tempdir = b"/tmp\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char;
    return M_StringJoin(tempdir, DIR_SEPARATOR_S.as_ptr(), s, NULL);
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StrToInt(
    mut str: *const ::core::ffi::c_char,
    mut result: *mut ::core::ffi::c_int,
) -> boolean {
    return (sscanf(
        str,
        b" 0x%x\0" as *const u8 as *const ::core::ffi::c_char,
        result,
    ) == 1 as ::core::ffi::c_int
        || sscanf(
            str,
            b" 0X%x\0" as *const u8 as *const ::core::ffi::c_char,
            result,
        ) == 1 as ::core::ffi::c_int
        || sscanf(
            str,
            b" 0%o\0" as *const u8 as *const ::core::ffi::c_char,
            result,
        ) == 1 as ::core::ffi::c_int
        || sscanf(
            str,
            b" %d\0" as *const u8 as *const ::core::ffi::c_char,
            result,
        ) == 1 as ::core::ffi::c_int) as ::core::ffi::c_int as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_ExtractFileBase(
    mut path: *mut ::core::ffi::c_char,
    mut dest: *mut ::core::ffi::c_char,
) {
    let mut src: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    let mut filename: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    let mut length: ::core::ffi::c_int = 0;
    src = path
        .offset(strlen(path) as isize)
        .offset(-(1 as ::core::ffi::c_int as isize));
    while src != path
        && *src.offset(-(1 as ::core::ffi::c_int as isize)) as ::core::ffi::c_int != DIR_SEPARATOR
    {
        src = src.offset(-1);
    }
    filename = src;
    length = 0 as ::core::ffi::c_int;
    memset(
        dest as *mut ::core::ffi::c_void,
        0 as ::core::ffi::c_int,
        8 as size_t,
    );
    while *src as ::core::ffi::c_int != '\0' as i32 && *src as ::core::ffi::c_int != '.' as i32 {
        if length >= 8 as ::core::ffi::c_int {
            printf(
                b"Warning: Truncated '%s' lump name to '%.8s'.\n\0" as *const u8
                    as *const ::core::ffi::c_char,
                filename,
                dest,
            );
            break;
        } else {
            let fresh0 = src;
            src = src.offset(1);
            let fresh1 = length;
            length = length + 1;
            *dest.offset(fresh1 as isize) =
                toupper(*fresh0 as ::core::ffi::c_int) as ::core::ffi::c_char;
        }
    }
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_ForceUppercase(mut text: *mut ::core::ffi::c_char) {
    let mut p: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    p = text;
    while *p as ::core::ffi::c_int != '\0' as i32 {
        *p = toupper(*p as ::core::ffi::c_int) as ::core::ffi::c_char;
        p = p.offset(1);
    }
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StrCaseStr(
    mut haystack: *mut ::core::ffi::c_char,
    mut needle: *mut ::core::ffi::c_char,
) -> *mut ::core::ffi::c_char {
    let mut haystack_len: ::core::ffi::c_uint = 0;
    let mut needle_len: ::core::ffi::c_uint = 0;
    let mut len: ::core::ffi::c_uint = 0;
    let mut i: ::core::ffi::c_uint = 0;
    haystack_len = strlen(haystack) as ::core::ffi::c_uint;
    needle_len = strlen(needle) as ::core::ffi::c_uint;
    if haystack_len < needle_len {
        return ::core::ptr::null_mut::<::core::ffi::c_char>();
    }
    len = haystack_len.wrapping_sub(needle_len);
    i = 0 as ::core::ffi::c_uint;
    while i <= len {
        if strncasecmp(haystack.offset(i as isize), needle, needle_len as size_t) == 0 {
            return haystack.offset(i as isize);
        }
        i = i.wrapping_add(1);
    }
    return ::core::ptr::null_mut::<::core::ffi::c_char>();
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringDuplicate(
    mut orig: *const ::core::ffi::c_char,
) -> *mut ::core::ffi::c_char {
    let mut result: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    result = strdup(orig);
    if result.is_null() {
        I_Error(
            b"Failed to duplicate string (length %i)\n\0" as *const u8 as *const ::core::ffi::c_char
                as *mut ::core::ffi::c_char,
            strlen(orig),
        );
    }
    return result;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringReplace(
    mut haystack: *const ::core::ffi::c_char,
    mut needle: *const ::core::ffi::c_char,
    mut replacement: *const ::core::ffi::c_char,
) -> *mut ::core::ffi::c_char {
    let mut result: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    let mut dst: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    let mut p: *const ::core::ffi::c_char = ::core::ptr::null::<::core::ffi::c_char>();
    let mut needle_len: size_t = strlen(needle);
    let mut result_len: size_t = 0;
    let mut dst_len: size_t = 0;
    result_len = strlen(haystack).wrapping_add(1 as size_t);
    p = haystack;
    loop {
        p = strstr(p, needle);
        if p.is_null() {
            break;
        }
        p = p.offset(needle_len as isize);
        result_len = result_len.wrapping_add(strlen(replacement).wrapping_sub(needle_len));
    }
    result = malloc(result_len) as *mut ::core::ffi::c_char;
    if result.is_null() {
        I_Error(
            b"M_StringReplace: Failed to allocate new string\0" as *const u8
                as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        );
        return ::core::ptr::null_mut::<::core::ffi::c_char>();
    }
    dst = result;
    dst_len = result_len;
    p = haystack;
    while *p as ::core::ffi::c_int != '\0' as i32 {
        if strncmp(p, needle, needle_len) == 0 {
            M_StringCopy(dst, replacement, dst_len);
            p = p.offset(needle_len as isize);
            dst = dst.offset(strlen(replacement) as isize);
            dst_len = dst_len.wrapping_sub(strlen(replacement));
        } else {
            *dst = *p;
            dst = dst.offset(1);
            dst_len = dst_len.wrapping_sub(1);
            p = p.offset(1);
        }
    }
    *dst = '\0' as i32 as ::core::ffi::c_char;
    return result;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringCopy(
    mut dest: *mut ::core::ffi::c_char,
    mut src: *const ::core::ffi::c_char,
    mut dest_size: size_t,
) -> boolean {
    let mut len: size_t = 0;
    if dest_size >= 1 as size_t {
        *dest.offset(dest_size.wrapping_sub(1 as size_t) as isize) =
            '\0' as i32 as ::core::ffi::c_char;
        strncpy(dest, src, dest_size.wrapping_sub(1 as size_t));
    } else {
        return false_0 as boolean;
    }
    len = strlen(dest);
    return (*src.offset(len as isize) as ::core::ffi::c_int == '\0' as i32) as ::core::ffi::c_int
        as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringConcat(
    mut dest: *mut ::core::ffi::c_char,
    mut src: *const ::core::ffi::c_char,
    mut dest_size: size_t,
) -> boolean {
    let mut offset: size_t = 0;
    offset = strlen(dest);
    if offset > dest_size {
        offset = dest_size;
    }
    return M_StringCopy(
        dest.offset(offset as isize),
        src,
        dest_size.wrapping_sub(offset),
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringStartsWith(
    mut s: *const ::core::ffi::c_char,
    mut prefix: *const ::core::ffi::c_char,
) -> boolean {
    return (strlen(s) > strlen(prefix)
        && strncmp(s, prefix, strlen(prefix)) == 0 as ::core::ffi::c_int)
        as ::core::ffi::c_int as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringEndsWith(
    mut s: *const ::core::ffi::c_char,
    mut suffix: *const ::core::ffi::c_char,
) -> boolean {
    return (strlen(s) >= strlen(suffix)
        && strcmp(
            s.offset(strlen(s) as isize)
                .offset(-(strlen(suffix) as isize)),
            suffix,
        ) == 0 as ::core::ffi::c_int) as ::core::ffi::c_int as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_StringJoin(
    mut s: *const ::core::ffi::c_char,
    mut args: ...
) -> *mut ::core::ffi::c_char {
    let mut result: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    let mut v: *const ::core::ffi::c_char = ::core::ptr::null::<::core::ffi::c_char>();
    let mut args_0: ::core::ffi::VaList;
    let mut result_len: size_t = 0;
    result_len = strlen(s).wrapping_add(1 as size_t);
    args_0 = args.clone();
    loop {
        v = args_0.next_arg::<*const ::core::ffi::c_char>();
        if v.is_null() {
            break;
        }
        result_len = result_len.wrapping_add(strlen(v));
    }
    result = malloc(result_len) as *mut ::core::ffi::c_char;
    if result.is_null() {
        I_Error(
            b"M_StringJoin: Failed to allocate new string.\0" as *const u8
                as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        );
        return ::core::ptr::null_mut::<::core::ffi::c_char>();
    }
    M_StringCopy(result, s, result_len);
    args_0 = args.clone();
    loop {
        v = args_0.next_arg::<*const ::core::ffi::c_char>();
        if v.is_null() {
            break;
        }
        M_StringConcat(result, v, result_len);
    }
    return result;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_vsnprintf(
    mut buf: *mut ::core::ffi::c_char,
    mut buf_len: size_t,
    mut s: *const ::core::ffi::c_char,
    mut args: ::core::ffi::VaList,
) -> ::core::ffi::c_int {
    let mut result: ::core::ffi::c_int = 0;
    if buf_len < 1 as size_t {
        return 0 as ::core::ffi::c_int;
    }
    result = vsnprintf(buf, buf_len, s, args);
    if result < 0 as ::core::ffi::c_int || result as size_t >= buf_len {
        *buf.offset(buf_len.wrapping_sub(1 as size_t) as isize) =
            '\0' as i32 as ::core::ffi::c_char;
        result = buf_len.wrapping_sub(1 as size_t) as ::core::ffi::c_int;
    }
    return result;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_snprintf(
    mut buf: *mut ::core::ffi::c_char,
    mut buf_len: size_t,
    mut s: *const ::core::ffi::c_char,
    mut args: ...
) -> ::core::ffi::c_int {
    let mut args_0: ::core::ffi::VaList;
    let mut result: ::core::ffi::c_int = 0;
    args_0 = args.clone();
    result = M_vsnprintf(buf, buf_len, s, args_0);
    return result;
}
