extern "C" {
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn M_BindVariable(name: *mut ::core::ffi::c_char, variable: *mut ::core::ffi::c_void);
    fn M_snprintf(
        buf: *mut ::core::ffi::c_char,
        buf_len: size_t,
        s: *const ::core::ffi::c_char,
        ...
    ) -> ::core::ffi::c_int;
}
pub type size_t = usize;
pub const KEY_RIGHTARROW: ::core::ffi::c_int = 0xae as ::core::ffi::c_int;
pub const KEY_LEFTARROW: ::core::ffi::c_int = 0xac as ::core::ffi::c_int;
pub const KEY_UPARROW: ::core::ffi::c_int = 0xad as ::core::ffi::c_int;
pub const KEY_DOWNARROW: ::core::ffi::c_int = 0xaf as ::core::ffi::c_int;
pub const KEY_STRAFE_L: ::core::ffi::c_int = 0xa0 as ::core::ffi::c_int;
pub const KEY_STRAFE_R: ::core::ffi::c_int = 0xa1 as ::core::ffi::c_int;
pub const KEY_USE: ::core::ffi::c_int = 0xa2 as ::core::ffi::c_int;
pub const KEY_FIRE: ::core::ffi::c_int = 0xa3 as ::core::ffi::c_int;
pub const KEY_ESCAPE: ::core::ffi::c_int = 27 as ::core::ffi::c_int;
pub const KEY_ENTER: ::core::ffi::c_int = 13 as ::core::ffi::c_int;
pub const KEY_TAB: ::core::ffi::c_int = 9 as ::core::ffi::c_int;
pub const KEY_F1: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x3b as ::core::ffi::c_int;
pub const KEY_F2: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x3c as ::core::ffi::c_int;
pub const KEY_F3: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x3d as ::core::ffi::c_int;
pub const KEY_F4: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x3e as ::core::ffi::c_int;
pub const KEY_F5: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x3f as ::core::ffi::c_int;
pub const KEY_F6: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x40 as ::core::ffi::c_int;
pub const KEY_F7: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x41 as ::core::ffi::c_int;
pub const KEY_F8: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x42 as ::core::ffi::c_int;
pub const KEY_F9: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x43 as ::core::ffi::c_int;
pub const KEY_F10: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x44 as ::core::ffi::c_int;
pub const KEY_F11: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x57 as ::core::ffi::c_int;
pub const KEY_F12: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x58 as ::core::ffi::c_int;
pub const KEY_BACKSPACE: ::core::ffi::c_int = 0x7f as ::core::ffi::c_int;
pub const KEY_PAUSE: ::core::ffi::c_int = 0xff as ::core::ffi::c_int;
pub const KEY_EQUALS: ::core::ffi::c_int = 0x3d as ::core::ffi::c_int;
pub const KEY_MINUS: ::core::ffi::c_int = 0x2d as ::core::ffi::c_int;
pub const KEY_RSHIFT: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x36 as ::core::ffi::c_int;
pub const KEY_RALT: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x38 as ::core::ffi::c_int;
pub const KEY_HOME: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x47 as ::core::ffi::c_int;
pub const KEY_END: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x4f as ::core::ffi::c_int;
pub const KEY_PGUP: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x49 as ::core::ffi::c_int;
pub const KEY_PGDN: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x51 as ::core::ffi::c_int;
pub const KEY_INS: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x52 as ::core::ffi::c_int;
pub const KEY_DEL: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int + 0x53 as ::core::ffi::c_int;
static mut key_right__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_right: ::core::ffi::c_int = 0;
static mut key_left__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_left: ::core::ffi::c_int = 0;
static mut key_up__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_up: ::core::ffi::c_int = 0;
static mut key_down__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_down: ::core::ffi::c_int = 0;
static mut key_strafeleft__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_strafeleft: ::core::ffi::c_int = 0;
static mut key_straferight__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_straferight: ::core::ffi::c_int = 0;
static mut key_fire__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_fire: ::core::ffi::c_int = 0;
static mut key_use__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_use: ::core::ffi::c_int = 0;
static mut key_strafe__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_strafe: ::core::ffi::c_int = 0;
static mut key_speed__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_speed: ::core::ffi::c_int = 0;
static mut key_flyup__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_flyup: ::core::ffi::c_int = 0;
static mut key_flydown__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_flydown: ::core::ffi::c_int = 0;
static mut key_flycenter__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_flycenter: ::core::ffi::c_int = 0;
static mut key_lookup__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_lookup: ::core::ffi::c_int = 0;
static mut key_lookdown__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_lookdown: ::core::ffi::c_int = 0;
static mut key_lookcenter__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_lookcenter: ::core::ffi::c_int = 0;
static mut key_invleft__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invleft: ::core::ffi::c_int = 0;
static mut key_invright__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invright: ::core::ffi::c_int = 0;
static mut key_useartifact__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_useartifact: ::core::ffi::c_int = 0;
static mut key_jump__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_jump: ::core::ffi::c_int = 0;
static mut key_arti_all__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_all: ::core::ffi::c_int = 0;
static mut key_arti_health__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_health: ::core::ffi::c_int = 0;
static mut key_arti_poisonbag__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_poisonbag: ::core::ffi::c_int = 0;
static mut key_arti_blastradius__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_blastradius: ::core::ffi::c_int = 0;
static mut key_arti_teleport__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_teleport: ::core::ffi::c_int = 0;
static mut key_arti_teleportother__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_teleportother: ::core::ffi::c_int = 0;
static mut key_arti_egg__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_egg: ::core::ffi::c_int = 0;
static mut key_arti_invulnerability__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_arti_invulnerability: ::core::ffi::c_int = 0;
static mut key_usehealth__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_usehealth: ::core::ffi::c_int = 0;
static mut key_invquery__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invquery: ::core::ffi::c_int = 0;
static mut key_mission__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_mission: ::core::ffi::c_int = 0;
static mut key_invpop__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invpop: ::core::ffi::c_int = 0;
static mut key_invkey__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invkey: ::core::ffi::c_int = 0;
static mut key_invhome__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invhome: ::core::ffi::c_int = 0;
static mut key_invend__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invend: ::core::ffi::c_int = 0;
static mut key_invuse__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invuse: ::core::ffi::c_int = 0;
static mut key_invdrop__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_invdrop: ::core::ffi::c_int = 0;
static mut mousebfire__b82i: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
#[no_mangle]
pub static mut mousebfire: ::core::ffi::c_int = 0;
static mut mousebstrafe__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebstrafe: ::core::ffi::c_int = 0;
static mut mousebforward__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebforward: ::core::ffi::c_int = 0;
static mut mousebjump__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebjump: ::core::ffi::c_int = 0;
static mut mousebstrafeleft__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebstrafeleft: ::core::ffi::c_int = 0;
static mut mousebstraferight__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebstraferight: ::core::ffi::c_int = 0;
static mut mousebbackward__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebbackward: ::core::ffi::c_int = 0;
static mut mousebuse__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebuse: ::core::ffi::c_int = 0;
static mut mousebprevweapon__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebprevweapon: ::core::ffi::c_int = 0;
static mut mousebnextweapon__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut mousebnextweapon: ::core::ffi::c_int = 0;
static mut key_message_refresh__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_message_refresh: ::core::ffi::c_int = 0;
static mut key_pause__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_pause: ::core::ffi::c_int = 0;
static mut key_demo_quit__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_demo_quit: ::core::ffi::c_int = 0;
static mut key_spy__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_spy: ::core::ffi::c_int = 0;
static mut key_multi_msg__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_multi_msg: ::core::ffi::c_int = 0;
#[no_mangle]
pub static mut key_multi_msgplayer: [::core::ffi::c_int; 8] = [0; 8];
static mut key_weapon1__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon1: ::core::ffi::c_int = 0;
static mut key_weapon2__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon2: ::core::ffi::c_int = 0;
static mut key_weapon3__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon3: ::core::ffi::c_int = 0;
static mut key_weapon4__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon4: ::core::ffi::c_int = 0;
static mut key_weapon5__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon5: ::core::ffi::c_int = 0;
static mut key_weapon6__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon6: ::core::ffi::c_int = 0;
static mut key_weapon7__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon7: ::core::ffi::c_int = 0;
static mut key_weapon8__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_weapon8: ::core::ffi::c_int = 0;
static mut key_prevweapon__b82i: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
#[no_mangle]
pub static mut key_prevweapon: ::core::ffi::c_int = 0;
static mut key_nextweapon__b82i: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
#[no_mangle]
pub static mut key_nextweapon: ::core::ffi::c_int = 0;
static mut key_map_north__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_north: ::core::ffi::c_int = 0;
static mut key_map_south__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_south: ::core::ffi::c_int = 0;
static mut key_map_east__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_east: ::core::ffi::c_int = 0;
static mut key_map_west__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_west: ::core::ffi::c_int = 0;
static mut key_map_zoomin__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_zoomin: ::core::ffi::c_int = 0;
static mut key_map_zoomout__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_zoomout: ::core::ffi::c_int = 0;
static mut key_map_toggle__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_toggle: ::core::ffi::c_int = 0;
static mut key_map_maxzoom__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_maxzoom: ::core::ffi::c_int = 0;
static mut key_map_follow__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_follow: ::core::ffi::c_int = 0;
static mut key_map_grid__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_grid: ::core::ffi::c_int = 0;
static mut key_map_mark__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_mark: ::core::ffi::c_int = 0;
static mut key_map_clearmark__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_map_clearmark: ::core::ffi::c_int = 0;
static mut key_menu_activate__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_activate: ::core::ffi::c_int = 0;
static mut key_menu_up__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_up: ::core::ffi::c_int = 0;
static mut key_menu_down__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_down: ::core::ffi::c_int = 0;
static mut key_menu_left__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_left: ::core::ffi::c_int = 0;
static mut key_menu_right__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_right: ::core::ffi::c_int = 0;
static mut key_menu_back__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_back: ::core::ffi::c_int = 0;
static mut key_menu_forward__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_forward: ::core::ffi::c_int = 0;
static mut key_menu_confirm__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_confirm: ::core::ffi::c_int = 0;
static mut key_menu_abort__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_abort: ::core::ffi::c_int = 0;
static mut key_menu_help__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_help: ::core::ffi::c_int = 0;
static mut key_menu_save__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_save: ::core::ffi::c_int = 0;
static mut key_menu_load__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_load: ::core::ffi::c_int = 0;
static mut key_menu_volume__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_volume: ::core::ffi::c_int = 0;
static mut key_menu_detail__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_detail: ::core::ffi::c_int = 0;
static mut key_menu_qsave__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_qsave: ::core::ffi::c_int = 0;
static mut key_menu_endgame__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_endgame: ::core::ffi::c_int = 0;
static mut key_menu_messages__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_messages: ::core::ffi::c_int = 0;
static mut key_menu_qload__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_qload: ::core::ffi::c_int = 0;
static mut key_menu_quit__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_quit: ::core::ffi::c_int = 0;
static mut key_menu_gamma__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_gamma: ::core::ffi::c_int = 0;
static mut key_menu_incscreen__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_incscreen: ::core::ffi::c_int = 0;
static mut key_menu_decscreen__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut key_menu_decscreen: ::core::ffi::c_int = 0;
static mut key_menu_screenshot__b82i: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
#[no_mangle]
pub static mut key_menu_screenshot: ::core::ffi::c_int = 0;
static mut joybfire__b82i: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
#[no_mangle]
pub static mut joybfire: ::core::ffi::c_int = 0;
static mut joybstrafe__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybstrafe: ::core::ffi::c_int = 0;
static mut joybuse__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybuse: ::core::ffi::c_int = 0;
static mut joybspeed__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybspeed: ::core::ffi::c_int = 0;
static mut joybstrafeleft__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybstrafeleft: ::core::ffi::c_int = 0;
static mut joybstraferight__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybstraferight: ::core::ffi::c_int = 0;
static mut joybjump__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybjump: ::core::ffi::c_int = 0;
static mut joybprevweapon__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybprevweapon: ::core::ffi::c_int = 0;
static mut joybnextweapon__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybnextweapon: ::core::ffi::c_int = 0;
static mut joybmenu__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut joybmenu: ::core::ffi::c_int = 0;
static mut dclick_use__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut dclick_use: ::core::ffi::c_int = 0;
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindBaseControls() {
    M_BindVariable(
        b"key_right\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_right as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_left\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_left as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_up\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_up as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_down\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_down as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_strafeleft\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_strafeleft as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_straferight\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_straferight as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_fire\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_fire as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_use\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_use as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_strafe\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_strafe as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_speed\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_speed as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_fire\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebfire as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_strafe\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebstrafe as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_forward\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebforward as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_fire\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybfire as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_strafe\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybstrafe as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_use\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybuse as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_speed\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybspeed as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_menu_activate\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut joybmenu as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_strafeleft\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybstrafeleft as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_straferight\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut joybstraferight as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_strafeleft\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut mousebstrafeleft as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_straferight\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut mousebstraferight as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_use\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebuse as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_backward\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebbackward as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"dclick_use\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut dclick_use as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_pause\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_pause as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_message_refresh\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_message_refresh as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindHereticControls() {
    M_BindVariable(
        b"key_flyup\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_flyup as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_flydown\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_flydown as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_flycenter\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_flycenter as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_lookup\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_lookup as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_lookdown\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_lookdown as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_lookcenter\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_lookcenter as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invleft\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invleft as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invright\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invright as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_useartifact\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_useartifact as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindHexenControls() {
    M_BindVariable(
        b"key_jump\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_jump as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_jump\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebjump as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_jump\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybjump as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_all\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_arti_all as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_health\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_arti_health as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_poisonbag\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_arti_poisonbag as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_blastradius\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_arti_blastradius as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_teleport\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_arti_teleport as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_teleportother\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_arti_teleportother as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_egg\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_arti_egg as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_arti_invulnerability\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_arti_invulnerability as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindStrifeControls() {
    key_message_refresh = '/' as i32;
    key_jump = 'a' as i32;
    key_lookup = KEY_PGUP;
    key_lookdown = KEY_PGDN;
    key_invleft = KEY_INS;
    key_invright = KEY_DEL;
    M_BindVariable(
        b"key_jump\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_jump as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_lookUp\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_lookup as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_lookDown\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_lookdown as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invLeft\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invleft as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invRight\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invright as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_useHealth\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_usehealth as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invquery\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invquery as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_mission\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_mission as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invPop\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invpop as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invKey\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invkey as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invHome\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invhome as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invEnd\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invend as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invUse\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invuse as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_invDrop\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_invdrop as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_jump\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut mousebjump as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_jump\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybjump as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindWeaponControls() {
    M_BindVariable(
        b"key_weapon1\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon1 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon2\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon2 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon3\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon3 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon4\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon4 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon5\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon5 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon6\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon6 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon7\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon7 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_weapon8\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_weapon8 as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_prevweapon\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_prevweapon as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_nextweapon\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_nextweapon as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_prevweapon\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybprevweapon as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"joyb_nextweapon\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut joybnextweapon as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_prevweapon\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut mousebprevweapon as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"mouseb_nextweapon\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut mousebnextweapon as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindMapControls() {
    M_BindVariable(
        b"key_map_north\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_north as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_south\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_south as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_east\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_east as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_west\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_west as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_zoomin\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_zoomin as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_zoomout\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_zoomout as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_toggle\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_toggle as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_maxzoom\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_maxzoom as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_follow\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_follow as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_grid\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_grid as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_mark\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_map_mark as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_map_clearmark\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_map_clearmark as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindMenuControls() {
    M_BindVariable(
        b"key_menu_activate\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_activate as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_up\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_up as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_down\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_down as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_left\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_left as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_right\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_right as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_back\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_back as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_forward\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_forward as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_confirm\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_confirm as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_abort\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_abort as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_help\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_help as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_save\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_save as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_load\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_load as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_volume\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_volume as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_detail\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_detail as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_qsave\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_qsave as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_endgame\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_endgame as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_messages\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_messages as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_qload\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_qload as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_quit\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_quit as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_gamma\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_menu_gamma as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_incscreen\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_incscreen as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_decscreen\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_decscreen as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_menu_screenshot\0" as *const u8 as *const ::core::ffi::c_char
            as *mut ::core::ffi::c_char,
        &raw mut key_menu_screenshot as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_demo_quit\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_demo_quit as *mut ::core::ffi::c_void,
    );
    M_BindVariable(
        b"key_spy\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_spy as *mut ::core::ffi::c_void,
    );
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_BindChatControls(mut num_players: ::core::ffi::c_uint) {
    let mut name: [::core::ffi::c_char; 32] = [0; 32];
    let mut i: ::core::ffi::c_uint = 0;
    M_BindVariable(
        b"key_multi_msg\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        &raw mut key_multi_msg as *mut ::core::ffi::c_void,
    );
    i = 0 as ::core::ffi::c_uint;
    while i < num_players {
        M_snprintf(
            &raw mut name as *mut ::core::ffi::c_char,
            ::core::mem::size_of::<[::core::ffi::c_char; 32]>() as size_t,
            b"key_multi_msgplayer%i\0" as *const u8 as *const ::core::ffi::c_char,
            i.wrapping_add(1 as ::core::ffi::c_uint),
        );
        M_BindVariable(
            &raw mut name as *mut ::core::ffi::c_char,
            (&raw mut key_multi_msgplayer as *mut ::core::ffi::c_int).offset(i as isize)
                as *mut ::core::ffi::c_int as *mut ::core::ffi::c_void,
        );
        i = i.wrapping_add(1);
    }
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn M_ApplyPlatformDefaults() {}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_m_controls() {
    memcpy(
        &raw mut key_right as *mut ::core::ffi::c_void,
        &raw const key_right__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_left as *mut ::core::ffi::c_void,
        &raw const key_left__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_up as *mut ::core::ffi::c_void,
        &raw const key_up__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_down as *mut ::core::ffi::c_void,
        &raw const key_down__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_strafeleft as *mut ::core::ffi::c_void,
        &raw const key_strafeleft__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_straferight as *mut ::core::ffi::c_void,
        &raw const key_straferight__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_fire as *mut ::core::ffi::c_void,
        &raw const key_fire__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_use as *mut ::core::ffi::c_void,
        &raw const key_use__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_strafe as *mut ::core::ffi::c_void,
        &raw const key_strafe__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_speed as *mut ::core::ffi::c_void,
        &raw const key_speed__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_flyup as *mut ::core::ffi::c_void,
        &raw const key_flyup__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_flydown as *mut ::core::ffi::c_void,
        &raw const key_flydown__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_flycenter as *mut ::core::ffi::c_void,
        &raw const key_flycenter__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_lookup as *mut ::core::ffi::c_void,
        &raw const key_lookup__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_lookdown as *mut ::core::ffi::c_void,
        &raw const key_lookdown__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_lookcenter as *mut ::core::ffi::c_void,
        &raw const key_lookcenter__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invleft as *mut ::core::ffi::c_void,
        &raw const key_invleft__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invright as *mut ::core::ffi::c_void,
        &raw const key_invright__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_useartifact as *mut ::core::ffi::c_void,
        &raw const key_useartifact__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_jump as *mut ::core::ffi::c_void,
        &raw const key_jump__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_all as *mut ::core::ffi::c_void,
        &raw const key_arti_all__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_health as *mut ::core::ffi::c_void,
        &raw const key_arti_health__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_poisonbag as *mut ::core::ffi::c_void,
        &raw const key_arti_poisonbag__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_blastradius as *mut ::core::ffi::c_void,
        &raw const key_arti_blastradius__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_teleport as *mut ::core::ffi::c_void,
        &raw const key_arti_teleport__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_teleportother as *mut ::core::ffi::c_void,
        &raw const key_arti_teleportother__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_egg as *mut ::core::ffi::c_void,
        &raw const key_arti_egg__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_arti_invulnerability as *mut ::core::ffi::c_void,
        &raw const key_arti_invulnerability__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_usehealth as *mut ::core::ffi::c_void,
        &raw const key_usehealth__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invquery as *mut ::core::ffi::c_void,
        &raw const key_invquery__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_mission as *mut ::core::ffi::c_void,
        &raw const key_mission__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invpop as *mut ::core::ffi::c_void,
        &raw const key_invpop__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invkey as *mut ::core::ffi::c_void,
        &raw const key_invkey__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invhome as *mut ::core::ffi::c_void,
        &raw const key_invhome__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invend as *mut ::core::ffi::c_void,
        &raw const key_invend__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invuse as *mut ::core::ffi::c_void,
        &raw const key_invuse__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_invdrop as *mut ::core::ffi::c_void,
        &raw const key_invdrop__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebfire as *mut ::core::ffi::c_void,
        &raw const mousebfire__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebstrafe as *mut ::core::ffi::c_void,
        &raw const mousebstrafe__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebforward as *mut ::core::ffi::c_void,
        &raw const mousebforward__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebjump as *mut ::core::ffi::c_void,
        &raw const mousebjump__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebstrafeleft as *mut ::core::ffi::c_void,
        &raw const mousebstrafeleft__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebstraferight as *mut ::core::ffi::c_void,
        &raw const mousebstraferight__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebbackward as *mut ::core::ffi::c_void,
        &raw const mousebbackward__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebuse as *mut ::core::ffi::c_void,
        &raw const mousebuse__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebprevweapon as *mut ::core::ffi::c_void,
        &raw const mousebprevweapon__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut mousebnextweapon as *mut ::core::ffi::c_void,
        &raw const mousebnextweapon__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_message_refresh as *mut ::core::ffi::c_void,
        &raw const key_message_refresh__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_pause as *mut ::core::ffi::c_void,
        &raw const key_pause__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_demo_quit as *mut ::core::ffi::c_void,
        &raw const key_demo_quit__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_spy as *mut ::core::ffi::c_void,
        &raw const key_spy__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_multi_msg as *mut ::core::ffi::c_void,
        &raw const key_multi_msg__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon1 as *mut ::core::ffi::c_void,
        &raw const key_weapon1__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon2 as *mut ::core::ffi::c_void,
        &raw const key_weapon2__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon3 as *mut ::core::ffi::c_void,
        &raw const key_weapon3__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon4 as *mut ::core::ffi::c_void,
        &raw const key_weapon4__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon5 as *mut ::core::ffi::c_void,
        &raw const key_weapon5__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon6 as *mut ::core::ffi::c_void,
        &raw const key_weapon6__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon7 as *mut ::core::ffi::c_void,
        &raw const key_weapon7__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_weapon8 as *mut ::core::ffi::c_void,
        &raw const key_weapon8__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_prevweapon as *mut ::core::ffi::c_void,
        &raw const key_prevweapon__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_nextweapon as *mut ::core::ffi::c_void,
        &raw const key_nextweapon__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_north as *mut ::core::ffi::c_void,
        &raw const key_map_north__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_south as *mut ::core::ffi::c_void,
        &raw const key_map_south__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_east as *mut ::core::ffi::c_void,
        &raw const key_map_east__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_west as *mut ::core::ffi::c_void,
        &raw const key_map_west__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_zoomin as *mut ::core::ffi::c_void,
        &raw const key_map_zoomin__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_zoomout as *mut ::core::ffi::c_void,
        &raw const key_map_zoomout__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_toggle as *mut ::core::ffi::c_void,
        &raw const key_map_toggle__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_maxzoom as *mut ::core::ffi::c_void,
        &raw const key_map_maxzoom__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_follow as *mut ::core::ffi::c_void,
        &raw const key_map_follow__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_grid as *mut ::core::ffi::c_void,
        &raw const key_map_grid__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_mark as *mut ::core::ffi::c_void,
        &raw const key_map_mark__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_map_clearmark as *mut ::core::ffi::c_void,
        &raw const key_map_clearmark__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_activate as *mut ::core::ffi::c_void,
        &raw const key_menu_activate__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_up as *mut ::core::ffi::c_void,
        &raw const key_menu_up__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_down as *mut ::core::ffi::c_void,
        &raw const key_menu_down__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_left as *mut ::core::ffi::c_void,
        &raw const key_menu_left__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_right as *mut ::core::ffi::c_void,
        &raw const key_menu_right__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_back as *mut ::core::ffi::c_void,
        &raw const key_menu_back__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_forward as *mut ::core::ffi::c_void,
        &raw const key_menu_forward__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_confirm as *mut ::core::ffi::c_void,
        &raw const key_menu_confirm__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_abort as *mut ::core::ffi::c_void,
        &raw const key_menu_abort__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_help as *mut ::core::ffi::c_void,
        &raw const key_menu_help__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_save as *mut ::core::ffi::c_void,
        &raw const key_menu_save__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_load as *mut ::core::ffi::c_void,
        &raw const key_menu_load__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_volume as *mut ::core::ffi::c_void,
        &raw const key_menu_volume__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_detail as *mut ::core::ffi::c_void,
        &raw const key_menu_detail__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_qsave as *mut ::core::ffi::c_void,
        &raw const key_menu_qsave__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_endgame as *mut ::core::ffi::c_void,
        &raw const key_menu_endgame__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_messages as *mut ::core::ffi::c_void,
        &raw const key_menu_messages__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_qload as *mut ::core::ffi::c_void,
        &raw const key_menu_qload__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_quit as *mut ::core::ffi::c_void,
        &raw const key_menu_quit__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_gamma as *mut ::core::ffi::c_void,
        &raw const key_menu_gamma__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_incscreen as *mut ::core::ffi::c_void,
        &raw const key_menu_incscreen__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_decscreen as *mut ::core::ffi::c_void,
        &raw const key_menu_decscreen__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut key_menu_screenshot as *mut ::core::ffi::c_void,
        &raw const key_menu_screenshot__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybfire as *mut ::core::ffi::c_void,
        &raw const joybfire__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybstrafe as *mut ::core::ffi::c_void,
        &raw const joybstrafe__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybuse as *mut ::core::ffi::c_void,
        &raw const joybuse__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybspeed as *mut ::core::ffi::c_void,
        &raw const joybspeed__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybstrafeleft as *mut ::core::ffi::c_void,
        &raw const joybstrafeleft__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybstraferight as *mut ::core::ffi::c_void,
        &raw const joybstraferight__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybjump as *mut ::core::ffi::c_void,
        &raw const joybjump__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybprevweapon as *mut ::core::ffi::c_void,
        &raw const joybprevweapon__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybnextweapon as *mut ::core::ffi::c_void,
        &raw const joybnextweapon__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut joybmenu as *mut ::core::ffi::c_void,
        &raw const joybmenu__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut dclick_use as *mut ::core::ffi::c_void,
        &raw const dclick_use__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
}

/// B83 (tools/fixdata.py): initial values, flash -> PSRAM .bss
#[optimize(size)]
pub unsafe fn b83i() {
    key_right__b82i = KEY_RIGHTARROW;
    key_left__b82i = KEY_LEFTARROW;
    key_up__b82i = KEY_UPARROW;
    key_down__b82i = KEY_DOWNARROW;
    key_strafeleft__b82i = KEY_STRAFE_L;
    key_straferight__b82i = KEY_STRAFE_R;
    key_fire__b82i = KEY_FIRE;
    key_use__b82i = KEY_USE;
    key_strafe__b82i = KEY_RALT;
    key_speed__b82i = KEY_RSHIFT;
    key_flyup__b82i = KEY_PGUP;
    key_flydown__b82i = KEY_INS;
    key_flycenter__b82i = KEY_HOME;
    key_lookup__b82i = KEY_PGDN;
    key_lookdown__b82i = KEY_DEL;
    key_lookcenter__b82i = KEY_END;
    key_invleft__b82i = '[' as i32;
    key_invright__b82i = ']' as i32;
    key_useartifact__b82i = KEY_ENTER;
    key_jump__b82i = '/' as i32;
    key_arti_all__b82i = KEY_BACKSPACE;
    key_arti_health__b82i = '\\' as i32;
    key_arti_poisonbag__b82i = '0' as i32;
    key_arti_blastradius__b82i = '9' as i32;
    key_arti_teleport__b82i = '8' as i32;
    key_arti_teleportother__b82i = '7' as i32;
    key_arti_egg__b82i = '6' as i32;
    key_arti_invulnerability__b82i = '5' as i32;
    key_usehealth__b82i = 'h' as i32;
    key_invquery__b82i = 'q' as i32;
    key_mission__b82i = 'w' as i32;
    key_invpop__b82i = 'z' as i32;
    key_invkey__b82i = 'k' as i32;
    key_invhome__b82i = KEY_HOME;
    key_invend__b82i = KEY_END;
    key_invuse__b82i = KEY_ENTER;
    key_invdrop__b82i = KEY_BACKSPACE;
    mousebstrafe__b82i = 1 as ::core::ffi::c_int;
    mousebforward__b82i = 2 as ::core::ffi::c_int;
    mousebjump__b82i = -(1 as ::core::ffi::c_int);
    mousebstrafeleft__b82i = -(1 as ::core::ffi::c_int);
    mousebstraferight__b82i = -(1 as ::core::ffi::c_int);
    mousebbackward__b82i = -(1 as ::core::ffi::c_int);
    mousebuse__b82i = -(1 as ::core::ffi::c_int);
    mousebprevweapon__b82i = -(1 as ::core::ffi::c_int);
    mousebnextweapon__b82i = -(1 as ::core::ffi::c_int);
    key_message_refresh__b82i = KEY_ENTER;
    key_pause__b82i = KEY_PAUSE;
    key_demo_quit__b82i = 'q' as i32;
    key_spy__b82i = KEY_F12;
    key_multi_msg__b82i = 't' as i32;
    key_weapon1__b82i = '1' as i32;
    key_weapon2__b82i = '2' as i32;
    key_weapon3__b82i = '3' as i32;
    key_weapon4__b82i = '4' as i32;
    key_weapon5__b82i = '5' as i32;
    key_weapon6__b82i = '6' as i32;
    key_weapon7__b82i = '7' as i32;
    key_weapon8__b82i = '8' as i32;
    key_map_north__b82i = KEY_UPARROW;
    key_map_south__b82i = KEY_DOWNARROW;
    key_map_east__b82i = KEY_RIGHTARROW;
    key_map_west__b82i = KEY_LEFTARROW;
    key_map_zoomin__b82i = '=' as i32;
    key_map_zoomout__b82i = '-' as i32;
    key_map_toggle__b82i = KEY_TAB;
    key_map_maxzoom__b82i = '0' as i32;
    key_map_follow__b82i = 'f' as i32;
    key_map_grid__b82i = 'g' as i32;
    key_map_mark__b82i = 'm' as i32;
    key_map_clearmark__b82i = 'c' as i32;
    key_menu_activate__b82i = KEY_ESCAPE;
    key_menu_up__b82i = KEY_UPARROW;
    key_menu_down__b82i = KEY_DOWNARROW;
    key_menu_left__b82i = KEY_LEFTARROW;
    key_menu_right__b82i = KEY_RIGHTARROW;
    key_menu_back__b82i = KEY_BACKSPACE;
    key_menu_forward__b82i = KEY_ENTER;
    key_menu_confirm__b82i = 'y' as i32;
    key_menu_abort__b82i = 'n' as i32;
    key_menu_help__b82i = KEY_F1;
    key_menu_save__b82i = KEY_F2;
    key_menu_load__b82i = KEY_F3;
    key_menu_volume__b82i = KEY_F4;
    key_menu_detail__b82i = KEY_F5;
    key_menu_qsave__b82i = KEY_F6;
    key_menu_endgame__b82i = KEY_F7;
    key_menu_messages__b82i = KEY_F8;
    key_menu_qload__b82i = KEY_F9;
    key_menu_quit__b82i = KEY_F10;
    key_menu_gamma__b82i = KEY_F11;
    key_menu_incscreen__b82i = KEY_EQUALS;
    key_menu_decscreen__b82i = KEY_MINUS;
    joybstrafe__b82i = 1 as ::core::ffi::c_int;
    joybuse__b82i = 3 as ::core::ffi::c_int;
    joybspeed__b82i = 2 as ::core::ffi::c_int;
    joybstrafeleft__b82i = -(1 as ::core::ffi::c_int);
    joybstraferight__b82i = -(1 as ::core::ffi::c_int);
    joybjump__b82i = -(1 as ::core::ffi::c_int);
    joybprevweapon__b82i = -(1 as ::core::ffi::c_int);
    joybnextweapon__b82i = -(1 as ::core::ffi::c_int);
    joybmenu__b82i = -(1 as ::core::ffi::c_int);
    dclick_use__b82i = 1 as ::core::ffi::c_int;
}