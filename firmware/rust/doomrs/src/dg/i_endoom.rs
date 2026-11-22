pub type uint8_t = u8;
pub type byte = uint8_t;
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn I_Endoom(mut endoom_data: *mut byte) {}
