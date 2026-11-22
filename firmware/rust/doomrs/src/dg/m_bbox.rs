pub type fixed_t = ::core::ffi::c_int;
pub type C2RustUnnamed = ::core::ffi::c_uint;
pub const BOXRIGHT: C2RustUnnamed = 3;
pub const BOXLEFT: C2RustUnnamed = 2;
pub const BOXBOTTOM: C2RustUnnamed = 1;
pub const BOXTOP: C2RustUnnamed = 0;
pub const INT_MAX: ::core::ffi::c_int = 2147483647 as ::core::ffi::c_int;
pub const INT_MIN: ::core::ffi::c_int = -INT_MAX - 1 as ::core::ffi::c_int;
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_ClearBox(mut box_0: *mut fixed_t) {
    let ref mut fresh0 = *box_0.offset(BOXRIGHT as ::core::ffi::c_int as isize);
    *fresh0 = INT_MIN as fixed_t;
    *box_0.offset(BOXTOP as ::core::ffi::c_int as isize) = *fresh0;
    let ref mut fresh1 = *box_0.offset(BOXLEFT as ::core::ffi::c_int as isize);
    *fresh1 = INT_MAX as fixed_t;
    *box_0.offset(BOXBOTTOM as ::core::ffi::c_int as isize) = *fresh1;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_AddToBox(mut box_0: *mut fixed_t, mut x: fixed_t, mut y: fixed_t) {
    if x < *box_0.offset(BOXLEFT as ::core::ffi::c_int as isize) {
        *box_0.offset(BOXLEFT as ::core::ffi::c_int as isize) = x;
    } else if x > *box_0.offset(BOXRIGHT as ::core::ffi::c_int as isize) {
        *box_0.offset(BOXRIGHT as ::core::ffi::c_int as isize) = x;
    }
    if y < *box_0.offset(BOXBOTTOM as ::core::ffi::c_int as isize) {
        *box_0.offset(BOXBOTTOM as ::core::ffi::c_int as isize) = y;
    } else if y > *box_0.offset(BOXTOP as ::core::ffi::c_int as isize) {
        *box_0.offset(BOXTOP as ::core::ffi::c_int as isize) = y;
    }
}
