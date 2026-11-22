extern "C" {
    fn abs(_: ::core::ffi::c_int) -> ::core::ffi::c_int;
}
pub type int64_t = i64;
pub type fixed_t = ::core::ffi::c_int;
pub const INT_MAX: ::core::ffi::c_int = 2147483647 as ::core::ffi::c_int;
pub const INT_MIN: ::core::ffi::c_int = -INT_MAX - 1 as ::core::ffi::c_int;
pub const FRACBITS: ::core::ffi::c_int = 16 as ::core::ffi::c_int;
#[no_mangle]
pub unsafe extern "C" fn FixedMul(mut a: fixed_t, mut b: fixed_t) -> fixed_t {
    return (a as int64_t * b as int64_t >> FRACBITS) as fixed_t;
}
#[no_mangle]
pub unsafe extern "C" fn FixedDiv(mut a: fixed_t, mut b: fixed_t) -> fixed_t {
    if abs(a as ::core::ffi::c_int) >> 14 as ::core::ffi::c_int >= abs(b as ::core::ffi::c_int) {
        return if a ^ b < 0 as ::core::ffi::c_int {
            INT_MIN
        } else {
            INT_MAX
        };
    } else {
        let mut result: int64_t = 0;
        result = ((a as int64_t) << 16 as ::core::ffi::c_int) / b as int64_t;
        return result as fixed_t;
    };
}
