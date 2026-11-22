extern "C" {
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
}
pub type size_t = usize;
pub type boolean = ::core::ffi::c_uint;
pub type GameMission_t = ::core::ffi::c_uint;
pub const none: GameMission_t = 9;
pub const strife: GameMission_t = 8;
pub const hexen: GameMission_t = 7;
pub const heretic: GameMission_t = 6;
pub const pack_hacx: GameMission_t = 5;
pub const pack_chex: GameMission_t = 4;
pub const pack_plut: GameMission_t = 3;
pub const pack_tnt: GameMission_t = 2;
pub const doom2: GameMission_t = 1;
pub const doom: GameMission_t = 0;
pub type GameMode_t = ::core::ffi::c_uint;
pub const indetermined: GameMode_t = 4;
pub const retail: GameMode_t = 3;
pub const commercial: GameMode_t = 2;
pub const registered: GameMode_t = 1;
pub const shareware: GameMode_t = 0;
pub type GameVersion_t = ::core::ffi::c_uint;
pub const exe_strife_1_31: GameVersion_t = 13;
pub const exe_strife_1_2: GameVersion_t = 12;
pub const exe_hexen_1_1: GameVersion_t = 11;
pub const exe_heretic_1_3: GameVersion_t = 10;
pub const exe_chex: GameVersion_t = 9;
pub const exe_final2: GameVersion_t = 8;
pub const exe_final: GameVersion_t = 7;
pub const exe_ultimate: GameVersion_t = 6;
pub const exe_hacx: GameVersion_t = 5;
pub const exe_doom_1_9: GameVersion_t = 4;
pub const exe_doom_1_8: GameVersion_t = 3;
pub const exe_doom_1_7: GameVersion_t = 2;
pub const exe_doom_1_666: GameVersion_t = 1;
pub const exe_doom_1_2: GameVersion_t = 0;
static mut gamemode__b82i: GameMode_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut gamemode: GameMode_t = unsafe { ::core::mem::zeroed() };
static mut gamemission__b82i: GameMission_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut gamemission: GameMission_t = unsafe { ::core::mem::zeroed() };
static mut gameversion__b82i: GameVersion_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut gameversion: GameVersion_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut gamedescription: *mut ::core::ffi::c_char =
    ::core::ptr::null::<::core::ffi::c_char>() as *mut ::core::ffi::c_char;
#[no_mangle]
pub static mut modifiedgame: boolean = 0;
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_doomstat() {
    memcpy(
        &raw mut gamemode as *mut ::core::ffi::c_void,
        &raw const gamemode__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<GameMode_t>() as size_t,
    );
    memcpy(
        &raw mut gamemission as *mut ::core::ffi::c_void,
        &raw const gamemission__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<GameMission_t>() as size_t,
    );
    memcpy(
        &raw mut gameversion as *mut ::core::ffi::c_void,
        &raw const gameversion__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<GameVersion_t>() as size_t,
    );
}

/// B83 (tools/fixdata.py): initial values, flash -> PSRAM .bss
#[optimize(size)]
pub unsafe fn b83i() {
    gamemode__b82i = indetermined;
    gamemode = shareware;
    gamemission__b82i = doom;
    gamemission = doom;
    gameversion__b82i = exe_final2;
    gameversion = exe_doom_1_2;
}