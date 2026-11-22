extern "C" {
    pub type b83_FILE;
    pub type _MEMFILE;
    pub type midi_file_s;
    pub type midi_track_iter_s;
    fn b83_stderr() -> *mut FILE;
    fn printf(_: *const ::core::ffi::c_char, ...) -> ::core::ffi::c_int;
    fn fprintf(_: *mut FILE, _: *const ::core::ffi::c_char, ...) -> ::core::ffi::c_int;
    fn malloc(_: size_t) -> *mut ::core::ffi::c_void;
    fn free(_: *mut ::core::ffi::c_void);
    fn getenv(_: *const ::core::ffi::c_char) -> *mut ::core::ffi::c_char;
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn memcmp(
        _: *const ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> ::core::ffi::c_int;
    fn strlen(_: *const ::core::ffi::c_char) -> size_t;
    fn strstr(
        _: *const ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
    ) -> *mut ::core::ffi::c_char;
    fn mem_fopen_read(buf: *mut ::core::ffi::c_void, buflen: size_t) -> *mut MEMFILE;
    fn mem_fopen_write() -> *mut MEMFILE;
    fn mem_get_buf(stream: *mut MEMFILE, buf: *mut *mut ::core::ffi::c_void, buflen: *mut size_t);
    fn mem_fclose(stream: *mut MEMFILE);
    fn mus2mid(musinput: *mut MEMFILE, midioutput: *mut MEMFILE) -> boolean;
    static mut snd_samplerate: ::core::ffi::c_int;
    fn M_StringConcat(
        dest: *mut ::core::ffi::c_char,
        src: *const ::core::ffi::c_char,
        dest_size: size_t,
    ) -> boolean;
    fn M_snprintf(
        buf: *mut ::core::ffi::c_char,
        buf_len: size_t,
        s: *const ::core::ffi::c_char,
        ...
    ) -> ::core::ffi::c_int;
    fn W_CacheLumpName(
        name: *mut ::core::ffi::c_char,
        tag: ::core::ffi::c_int,
    ) -> *mut ::core::ffi::c_void;
    fn W_ReleaseLumpName(name: *mut ::core::ffi::c_char);
    fn OPL_Init(port_base: ::core::ffi::c_uint) -> opl_init_result_t;
    fn OPL_Shutdown();
    fn OPL_SetSampleRate(rate: ::core::ffi::c_uint);
    fn OPL_WriteRegister(reg: ::core::ffi::c_int, value: ::core::ffi::c_int);
    fn OPL_InitRegisters(opl3: ::core::ffi::c_int);
    fn OPL_SetCallback(us: uint64_t, callback: opl_callback_t, data: *mut ::core::ffi::c_void);
    fn OPL_AdjustCallbacks(factor: ::core::ffi::c_float);
    fn OPL_ClearCallbacks();
    fn OPL_Lock();
    fn OPL_Unlock();
    fn OPL_SetPaused(paused: ::core::ffi::c_int);
    fn MIDI_LoadFileMem(data: *mut byte, len: size_t) -> *mut midi_file_t;
    fn MIDI_FreeFile(file: *mut midi_file_t);
    fn MIDI_GetFileTimeDivision(file: *mut midi_file_t) -> ::core::ffi::c_uint;
    fn MIDI_NumTracks(file: *mut midi_file_t) -> ::core::ffi::c_uint;
    fn MIDI_IterateTrack(
        file: *mut midi_file_t,
        track_num: ::core::ffi::c_uint,
    ) -> *mut midi_track_iter_t;
    fn MIDI_FreeIterator(iter: *mut midi_track_iter_t);
    fn MIDI_GetDeltaTime(iter: *mut midi_track_iter_t) -> ::core::ffi::c_uint;
    fn MIDI_GetNextEvent(
        iter: *mut midi_track_iter_t,
        event: *mut *mut midi_event_t,
    ) -> ::core::ffi::c_int;
    fn MIDI_RestartIterator(iter: *mut midi_track_iter_t);
}
pub type size_t = usize;
pub type FILE = b83_FILE;
pub type MEMFILE = _MEMFILE;
pub type uint64_t = u64;
pub type uint8_t = u8;
pub type boolean = ::core::ffi::c_uint;
pub type byte = uint8_t;
pub type snddevice_t = ::core::ffi::c_uint;
pub const SNDDEVICE_CD: snddevice_t = 10;
pub const SNDDEVICE_AWE32: snddevice_t = 9;
pub const SNDDEVICE_GENMIDI: snddevice_t = 8;
pub const SNDDEVICE_SOUNDCANVAS: snddevice_t = 7;
pub const SNDDEVICE_WAVEBLASTER: snddevice_t = 6;
pub const SNDDEVICE_GUS: snddevice_t = 5;
pub const SNDDEVICE_PAS: snddevice_t = 4;
pub const SNDDEVICE_SB: snddevice_t = 3;
pub const SNDDEVICE_ADLIB: snddevice_t = 2;
pub const SNDDEVICE_PCSPEAKER: snddevice_t = 1;
pub const SNDDEVICE_NONE: snddevice_t = 0;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct music_module_t {
    pub sound_devices: *mut snddevice_t,
    pub num_sound_devices: ::core::ffi::c_int,
    pub Init: Option<unsafe extern "C" fn() -> boolean>,
    pub Shutdown: Option<unsafe extern "C" fn() -> ()>,
    pub SetMusicVolume: Option<unsafe extern "C" fn(::core::ffi::c_int) -> ()>,
    pub PauseMusic: Option<unsafe extern "C" fn() -> ()>,
    pub ResumeMusic: Option<unsafe extern "C" fn() -> ()>,
    pub RegisterSong: Option<
        unsafe extern "C" fn(
            *mut ::core::ffi::c_void,
            ::core::ffi::c_int,
        ) -> *mut ::core::ffi::c_void,
    >,
    pub UnRegisterSong: Option<unsafe extern "C" fn(*mut ::core::ffi::c_void) -> ()>,
    pub PlaySong: Option<unsafe extern "C" fn(*mut ::core::ffi::c_void, boolean) -> ()>,
    pub StopSong: Option<unsafe extern "C" fn() -> ()>,
    pub MusicIsPlaying: Option<unsafe extern "C" fn() -> boolean>,
    pub Poll: Option<unsafe extern "C" fn() -> ()>,
}
pub type opl_driver_ver_t = ::core::ffi::c_uint;
pub const opl_doom_1_9: opl_driver_ver_t = 2;
pub const opl_doom2_1_666: opl_driver_ver_t = 1;
pub const opl_doom1_1_666: opl_driver_ver_t = 0;
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
pub type opl_callback_t = Option<unsafe extern "C" fn(*mut ::core::ffi::c_void) -> ()>;
pub type opl_init_result_t = ::core::ffi::c_uint;
pub const OPL_INIT_OPL3: opl_init_result_t = 2;
pub const OPL_INIT_OPL2: opl_init_result_t = 1;
pub const OPL_INIT_NONE: opl_init_result_t = 0;
pub type midi_file_t = midi_file_s;
pub type midi_track_iter_t = midi_track_iter_s;
pub type midi_event_type_t = ::core::ffi::c_uint;
pub const MIDI_EVENT_META: midi_event_type_t = 255;
pub const MIDI_EVENT_SYSEX_SPLIT: midi_event_type_t = 247;
pub const MIDI_EVENT_SYSEX: midi_event_type_t = 240;
pub const MIDI_EVENT_PITCH_BEND: midi_event_type_t = 224;
pub const MIDI_EVENT_CHAN_AFTERTOUCH: midi_event_type_t = 208;
pub const MIDI_EVENT_PROGRAM_CHANGE: midi_event_type_t = 192;
pub const MIDI_EVENT_CONTROLLER: midi_event_type_t = 176;
pub const MIDI_EVENT_AFTERTOUCH: midi_event_type_t = 160;
pub const MIDI_EVENT_NOTE_ON: midi_event_type_t = 144;
pub const MIDI_EVENT_NOTE_OFF: midi_event_type_t = 128;
pub type C2RustUnnamed_0 = ::core::ffi::c_uint;
pub const MIDI_CONTROLLER_ALL_NOTES_OFF: C2RustUnnamed_0 = 123;
pub const MIDI_CONTROLLER_PAN: C2RustUnnamed_0 = 10;
pub const MIDI_CONTROLLER_MAIN_VOLUME: C2RustUnnamed_0 = 7;
pub const MIDI_CONTROLLER_DATA_ENTRY: C2RustUnnamed_0 = 5;
pub const MIDI_CONTROLLER_PORTAMENTO: C2RustUnnamed_0 = 4;
pub const MIDI_CONTROLLER_FOOT_CONTROL: C2RustUnnamed_0 = 3;
pub const MIDI_CONTROLLER_BREATH_CONTROL: C2RustUnnamed_0 = 2;
pub const MIDI_CONTROLLER_MODULATION: C2RustUnnamed_0 = 1;
pub const MIDI_CONTROLLER_BANK_SELECT: C2RustUnnamed_0 = 0;
pub type C2RustUnnamed_1 = ::core::ffi::c_uint;
pub const MIDI_META_SEQUENCER_SPECIFIC: C2RustUnnamed_1 = 127;
pub const MIDI_META_KEY_SIGNATURE: C2RustUnnamed_1 = 89;
pub const MIDI_META_TIME_SIGNATURE: C2RustUnnamed_1 = 88;
pub const MIDI_META_SMPTE_OFFSET: C2RustUnnamed_1 = 84;
pub const MIDI_META_SET_TEMPO: C2RustUnnamed_1 = 81;
pub const MIDI_META_END_OF_TRACK: C2RustUnnamed_1 = 47;
pub const MIDI_META_CHANNEL_PREFIX: C2RustUnnamed_1 = 32;
pub const MIDI_META_CUE_POINT: C2RustUnnamed_1 = 7;
pub const MIDI_META_MARKER: C2RustUnnamed_1 = 6;
pub const MIDI_META_LYRICS: C2RustUnnamed_1 = 5;
pub const MIDI_META_INSTR_NAME: C2RustUnnamed_1 = 4;
pub const MIDI_META_TRACK_NAME: C2RustUnnamed_1 = 3;
pub const MIDI_META_COPYRIGHT: C2RustUnnamed_1 = 2;
pub const MIDI_META_TEXT: C2RustUnnamed_1 = 1;
pub const MIDI_META_SEQUENCE_NUMBER: C2RustUnnamed_1 = 0;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_meta_event_data_t {
    pub type_0: ::core::ffi::c_uint,
    pub length: ::core::ffi::c_uint,
    pub data: *mut byte,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_sysex_event_data_t {
    pub length: ::core::ffi::c_uint,
    pub data: *mut byte,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_channel_event_data_t {
    pub channel: ::core::ffi::c_uint,
    pub param1: ::core::ffi::c_uint,
    pub param2: ::core::ffi::c_uint,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_event_t {
    pub delta_time: ::core::ffi::c_uint,
    pub event_type: midi_event_type_t,
    pub data: C2RustUnnamed_2,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub union C2RustUnnamed_2 {
    pub channel: midi_channel_event_data_t,
    pub meta: midi_meta_event_data_t,
    pub sysex: midi_sysex_event_data_t,
}
#[derive(Copy, Clone)]
#[repr(C, packed)]
pub struct genmidi_op_t {
    pub tremolo: byte,
    pub attack: byte,
    pub sustain: byte,
    pub waveform: byte,
    pub scale: byte,
    pub level: byte,
}
#[derive(Copy, Clone)]
#[repr(C, packed)]
pub struct genmidi_voice_t {
    pub modulator: genmidi_op_t,
    pub feedback: byte,
    pub carrier: genmidi_op_t,
    pub unused: byte,
    pub base_note_offset: ::core::ffi::c_short,
}
#[derive(Copy, Clone)]
#[repr(C, packed)]
pub struct genmidi_instr_t {
    pub flags: ::core::ffi::c_ushort,
    pub fine_tuning: byte,
    pub fixed_note: byte,
    pub voices: [genmidi_voice_t; 2],
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct opl_channel_data_t {
    pub instrument: *mut genmidi_instr_t,
    pub volume: ::core::ffi::c_int,
    pub volume_base: ::core::ffi::c_int,
    pub pan: ::core::ffi::c_int,
    pub bend: ::core::ffi::c_int,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct opl_track_data_t {
    pub iter: *mut midi_track_iter_t,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct opl_voice_s {
    pub index: ::core::ffi::c_int,
    pub op1: ::core::ffi::c_int,
    pub op2: ::core::ffi::c_int,
    pub array: ::core::ffi::c_int,
    pub current_instr: *mut genmidi_instr_t,
    pub current_instr_voice: ::core::ffi::c_uint,
    pub channel: *mut opl_channel_data_t,
    pub key: ::core::ffi::c_uint,
    pub note: ::core::ffi::c_uint,
    pub freq: ::core::ffi::c_uint,
    pub note_volume: ::core::ffi::c_uint,
    pub car_volume: ::core::ffi::c_uint,
    pub mod_volume: ::core::ffi::c_uint,
    pub reg_pan: ::core::ffi::c_uint,
    pub priority: ::core::ffi::c_uint,
}
pub type opl_voice_t = opl_voice_s;
pub const NULL: *mut ::core::ffi::c_void = ::core::ptr::null_mut::<::core::ffi::c_void>();
pub const true_0: ::core::ffi::c_int = 1 as ::core::ffi::c_int;
pub const false_0: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
pub const OPL_NUM_VOICES: ::core::ffi::c_int = 9 as ::core::ffi::c_int;
pub const OPL_REGS_TREMOLO: ::core::ffi::c_int = 0x20 as ::core::ffi::c_int;
pub const OPL_REGS_LEVEL: ::core::ffi::c_int = 0x40 as ::core::ffi::c_int;
pub const OPL_REGS_ATTACK: ::core::ffi::c_int = 0x60 as ::core::ffi::c_int;
pub const OPL_REGS_SUSTAIN: ::core::ffi::c_int = 0x80 as ::core::ffi::c_int;
pub const OPL_REGS_WAVEFORM: ::core::ffi::c_int = 0xe0 as ::core::ffi::c_int;
pub const OPL_REGS_FREQ_1: ::core::ffi::c_int = 0xa0 as ::core::ffi::c_int;
pub const OPL_REGS_FREQ_2: ::core::ffi::c_int = 0xb0 as ::core::ffi::c_int;
pub const OPL_REGS_FEEDBACK: ::core::ffi::c_int = 0xc0 as ::core::ffi::c_int;
pub const MIDI_CHANNELS_PER_TRACK: ::core::ffi::c_int = 16 as ::core::ffi::c_int;
pub const MAXMIDLENGTH: ::core::ffi::c_int = 96 as ::core::ffi::c_int * 1024 as ::core::ffi::c_int;
pub const GENMIDI_NUM_INSTRS: ::core::ffi::c_int = 128 as ::core::ffi::c_int;
pub const GENMIDI_NUM_PERCUSSION: ::core::ffi::c_int = 47 as ::core::ffi::c_int;
pub const GENMIDI_HEADER: [::core::ffi::c_char; 9] =
    unsafe { ::core::mem::transmute::<[u8; 9], [::core::ffi::c_char; 9]>(*b"#OPL_II#\0") };
pub const GENMIDI_FLAG_FIXED: ::core::ffi::c_int = 0x1 as ::core::ffi::c_int;
pub const GENMIDI_FLAG_2VOICE: ::core::ffi::c_int = 0x4 as ::core::ffi::c_int;
pub const PERCUSSION_LOG_LEN: ::core::ffi::c_int = 16 as ::core::ffi::c_int;
static mut voice_operators: [[::core::ffi::c_int; 9]; 2] = unsafe { ::core::mem::zeroed() };
static mut frequency_curve: [::core::ffi::c_ushort; 668] = unsafe { ::core::mem::zeroed() };
static mut volume_mapping_table: [::core::ffi::c_uint; 128] = unsafe { ::core::mem::zeroed() };
static mut opl_drv_ver__b82i: opl_driver_ver_t = unsafe { ::core::mem::zeroed() };
static mut opl_drv_ver: opl_driver_ver_t = unsafe { ::core::mem::zeroed() };
static mut music_initialized__b82i: boolean = false_0 as boolean;
static mut music_initialized: boolean = 0;
static mut start_music_volume: ::core::ffi::c_int = 0;
static mut current_music_volume: ::core::ffi::c_int = 0;
static mut main_instrs: *mut genmidi_instr_t =
    ::core::ptr::null::<genmidi_instr_t>() as *mut genmidi_instr_t;
static mut percussion_instrs: *mut genmidi_instr_t =
    ::core::ptr::null::<genmidi_instr_t>() as *mut genmidi_instr_t;
static mut main_instr_names: *mut [::core::ffi::c_char; 32] =
    ::core::ptr::null::<[::core::ffi::c_char; 32]>() as *mut [::core::ffi::c_char; 32];
static mut percussion_names: *mut [::core::ffi::c_char; 32] =
    ::core::ptr::null::<[::core::ffi::c_char; 32]>() as *mut [::core::ffi::c_char; 32];
static mut voices: [opl_voice_t; 18] = unsafe { ::core::mem::zeroed() };
static mut voice_free_list: [*mut opl_voice_t; 18] =
    [::core::ptr::null::<opl_voice_t>() as *mut opl_voice_t; 18];
static mut voice_alloced_list: [*mut opl_voice_t; 18] =
    [::core::ptr::null::<opl_voice_t>() as *mut opl_voice_t; 18];
static mut voice_free_num: ::core::ffi::c_int = 0;
static mut voice_alloced_num: ::core::ffi::c_int = 0;
static mut opl_opl3mode: ::core::ffi::c_int = 0;
static mut num_opl_voices: ::core::ffi::c_int = 0;
static mut channels: [opl_channel_data_t; 16] = unsafe { ::core::mem::zeroed() };
static mut tracks: *mut opl_track_data_t =
    ::core::ptr::null::<opl_track_data_t>() as *mut opl_track_data_t;
static mut num_tracks__b82i: ::core::ffi::c_uint = 0 as ::core::ffi::c_uint;
static mut num_tracks: ::core::ffi::c_uint = 0;
static mut running_tracks__b82i: ::core::ffi::c_uint = 0 as ::core::ffi::c_uint;
static mut running_tracks: ::core::ffi::c_uint = 0;
static mut song_looping: boolean = 0;
static mut ticks_per_beat: ::core::ffi::c_uint = 0;
static mut us_per_beat: ::core::ffi::c_uint = 0;
static mut last_perc: [uint8_t; 16] = [0; 16];
static mut last_perc_count: ::core::ffi::c_uint = 0;
static mut snd_dmxoption__b82i: *mut ::core::ffi::c_char =
    b"\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char;
#[no_mangle]
pub static mut snd_dmxoption: *mut ::core::ffi::c_char =
    ::core::ptr::null::<::core::ffi::c_char>() as *mut ::core::ffi::c_char;
static mut opl_io_port__b82i: ::core::ffi::c_int = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut opl_io_port: ::core::ffi::c_int = 0;
static mut opl_stereo_correct__b82i: boolean = false_0 as boolean;
static mut opl_stereo_correct: boolean = 0;
#[optimize(size)]
unsafe extern "C" fn LoadInstrumentTable() -> boolean {
    let mut lump: *mut byte = ::core::ptr::null_mut::<byte>();
    lump = W_CacheLumpName(
        b"genmidi\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        PU_STATIC as ::core::ffi::c_int,
    ) as *mut byte;
    main_instrs = lump.offset(strlen(GENMIDI_HEADER.as_ptr()) as isize) as *mut genmidi_instr_t;
    percussion_instrs = main_instrs.offset(GENMIDI_NUM_INSTRS as isize);
    main_instr_names =
        percussion_instrs.offset(GENMIDI_NUM_PERCUSSION as isize) as *mut [::core::ffi::c_char; 32];
    percussion_names = main_instr_names.offset(GENMIDI_NUM_INSTRS as isize);
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn GetFreeVoice() -> *mut opl_voice_t {
    let mut result: *mut opl_voice_t = ::core::ptr::null_mut::<opl_voice_t>();
    let mut i: ::core::ffi::c_int = 0;
    if voice_free_num == 0 as ::core::ffi::c_int {
        return ::core::ptr::null_mut::<opl_voice_t>();
    }
    result = voice_free_list[0 as ::core::ffi::c_int as usize];
    voice_free_num -= 1;
    i = 0 as ::core::ffi::c_int;
    while i < voice_free_num {
        voice_free_list[i as usize] = voice_free_list[(i + 1 as ::core::ffi::c_int) as usize];
        i += 1;
    }
    let fresh0 = voice_alloced_num;
    voice_alloced_num = voice_alloced_num + 1;
    voice_alloced_list[fresh0 as usize] = result;
    return result;
}
#[optimize(size)]
unsafe extern "C" fn ReleaseVoice(mut index: ::core::ffi::c_int) {
    let mut voice: *mut opl_voice_t = ::core::ptr::null_mut::<opl_voice_t>();
    let mut double_voice: boolean = 0;
    let mut i: ::core::ffi::c_int = 0;
    if index >= voice_alloced_num {
        voice_alloced_num = 0 as ::core::ffi::c_int;
        voice_free_num = 0 as ::core::ffi::c_int;
        return;
    }
    voice = voice_alloced_list[index as usize];
    VoiceKeyOff(voice);
    (*voice).channel = ::core::ptr::null_mut::<opl_channel_data_t>();
    (*voice).note = 0 as ::core::ffi::c_uint;
    double_voice =
        ((*voice).current_instr_voice != 0 as ::core::ffi::c_uint) as ::core::ffi::c_int as boolean;
    voice_alloced_num -= 1;
    i = index;
    while i < voice_alloced_num {
        voice_alloced_list[i as usize] = voice_alloced_list[(i + 1 as ::core::ffi::c_int) as usize];
        i += 1;
    }
    let fresh1 = voice_free_num;
    voice_free_num = voice_free_num + 1;
    voice_free_list[fresh1 as usize] = voice;
    if double_voice != 0
        && (opl_drv_ver as ::core::ffi::c_uint)
            < opl_doom_1_9 as ::core::ffi::c_int as ::core::ffi::c_uint
    {
        ReleaseVoice(index);
    }
}
#[optimize(size)]
unsafe extern "C" fn LoadOperatorData(
    mut operator: ::core::ffi::c_int,
    mut data: *mut genmidi_op_t,
    mut max_level: boolean,
    mut volume: *mut ::core::ffi::c_uint,
) {
    let mut level: ::core::ffi::c_int = 0;
    level = (*data).scale as ::core::ffi::c_int;
    if max_level != 0 {
        level |= 0x3f as ::core::ffi::c_int;
    } else {
        level |= (*data).level as ::core::ffi::c_int;
    }
    *volume = level as ::core::ffi::c_uint;
    OPL_WriteRegister(OPL_REGS_LEVEL + operator, level);
    OPL_WriteRegister(
        OPL_REGS_TREMOLO + operator,
        (*data).tremolo as ::core::ffi::c_int,
    );
    OPL_WriteRegister(
        OPL_REGS_ATTACK + operator,
        (*data).attack as ::core::ffi::c_int,
    );
    OPL_WriteRegister(
        OPL_REGS_SUSTAIN + operator,
        (*data).sustain as ::core::ffi::c_int,
    );
    OPL_WriteRegister(
        OPL_REGS_WAVEFORM + operator,
        (*data).waveform as ::core::ffi::c_int,
    );
}
#[optimize(size)]
unsafe extern "C" fn SetVoiceInstrument(
    mut voice: *mut opl_voice_t,
    mut instr: *mut genmidi_instr_t,
    mut instr_voice: ::core::ffi::c_uint,
) {
    let mut data: *mut genmidi_voice_t = ::core::ptr::null_mut::<genmidi_voice_t>();
    let mut modulating: ::core::ffi::c_uint = 0;
    if (*voice).current_instr == instr && (*voice).current_instr_voice == instr_voice {
        return;
    }
    (*voice).current_instr = instr;
    (*voice).current_instr_voice = instr_voice;
    data = (&raw mut (*instr).voices as *mut genmidi_voice_t).offset(instr_voice as isize)
        as *mut genmidi_voice_t;
    modulating = ((*data).feedback as ::core::ffi::c_int & 0x1 as ::core::ffi::c_int
        == 0 as ::core::ffi::c_int) as ::core::ffi::c_int as ::core::ffi::c_uint;
    LoadOperatorData(
        (*voice).op2 | (*voice).array,
        &raw mut (*data).carrier,
        true_0 as boolean,
        &raw mut (*voice).car_volume,
    );
    LoadOperatorData(
        (*voice).op1 | (*voice).array,
        &raw mut (*data).modulator,
        (modulating == 0) as ::core::ffi::c_int as boolean,
        &raw mut (*voice).mod_volume,
    );
    OPL_WriteRegister(
        OPL_REGS_FEEDBACK + (*voice).index | (*voice).array,
        ((*data).feedback as ::core::ffi::c_uint | (*voice).reg_pan) as ::core::ffi::c_int,
    );
    (*voice).priority = (0xf as ::core::ffi::c_int
        - ((*data).carrier.attack as ::core::ffi::c_int >> 4 as ::core::ffi::c_int)
        + 0xf as ::core::ffi::c_int
        - ((*data).carrier.sustain as ::core::ffi::c_int & 0xf as ::core::ffi::c_int))
        as ::core::ffi::c_uint;
}
#[optimize(size)]
unsafe extern "C" fn SetVoiceVolume(mut voice: *mut opl_voice_t, mut volume: ::core::ffi::c_uint) {
    let mut opl_voice: *mut genmidi_voice_t = ::core::ptr::null_mut::<genmidi_voice_t>();
    let mut midi_volume: ::core::ffi::c_uint = 0;
    let mut full_volume: ::core::ffi::c_uint = 0;
    let mut car_volume: ::core::ffi::c_uint = 0;
    let mut mod_volume: ::core::ffi::c_uint = 0;
    (*voice).note_volume = volume;
    opl_voice = (&raw mut (*(*voice).current_instr).voices as *mut genmidi_voice_t)
        .offset((*voice).current_instr_voice as isize) as *mut genmidi_voice_t;
    midi_volume = (2 as ::core::ffi::c_uint).wrapping_mul(
        volume_mapping_table[(*(*voice).channel).volume as usize]
            .wrapping_add(1 as ::core::ffi::c_uint),
    );
    full_volume = volume_mapping_table[(*voice).note_volume as usize].wrapping_mul(midi_volume)
        >> 9 as ::core::ffi::c_int;
    car_volume = (0x3f as ::core::ffi::c_uint).wrapping_sub(full_volume);
    if car_volume != (*voice).car_volume & 0x3f as ::core::ffi::c_uint {
        (*voice).car_volume = car_volume | (*voice).car_volume & 0xc0 as ::core::ffi::c_uint;
        OPL_WriteRegister(
            OPL_REGS_LEVEL + (*voice).op2 | (*voice).array,
            (*voice).car_volume as ::core::ffi::c_int,
        );
        if (*opl_voice).feedback as ::core::ffi::c_int & 0x1 as ::core::ffi::c_int
            != 0 as ::core::ffi::c_int
            && (*opl_voice).modulator.level as ::core::ffi::c_int != 0x3f as ::core::ffi::c_int
        {
            mod_volume = (*opl_voice).modulator.level as ::core::ffi::c_uint;
            if mod_volume < car_volume {
                mod_volume = car_volume;
            }
            mod_volume |= (*voice).mod_volume & 0xc0 as ::core::ffi::c_uint;
            if mod_volume != (*voice).mod_volume {
                (*voice).mod_volume = mod_volume;
                OPL_WriteRegister(
                    OPL_REGS_LEVEL + (*voice).op1 | (*voice).array,
                    (mod_volume
                        | ((*opl_voice).modulator.scale as ::core::ffi::c_int
                            & 0xc0 as ::core::ffi::c_int)
                            as ::core::ffi::c_uint) as ::core::ffi::c_int,
                );
            }
        }
    }
}
#[optimize(size)]
unsafe extern "C" fn SetVoicePan(mut voice: *mut opl_voice_t, mut pan: ::core::ffi::c_uint) {
    let mut opl_voice: *mut genmidi_voice_t = ::core::ptr::null_mut::<genmidi_voice_t>();
    (*voice).reg_pan = pan;
    opl_voice = (&raw mut (*(*voice).current_instr).voices as *mut genmidi_voice_t)
        .offset((*voice).current_instr_voice as isize) as *mut genmidi_voice_t;
    OPL_WriteRegister(
        OPL_REGS_FEEDBACK + (*voice).index | (*voice).array,
        ((*opl_voice).feedback as ::core::ffi::c_uint | pan) as ::core::ffi::c_int,
    );
}
#[optimize(size)]
unsafe extern "C" fn InitVoices() {
    let mut i: ::core::ffi::c_int = 0;
    voice_free_num = num_opl_voices;
    voice_alloced_num = 0 as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_int;
    while i < num_opl_voices {
        voices[i as usize].index = i % OPL_NUM_VOICES;
        voices[i as usize].op1 =
            voice_operators[0 as ::core::ffi::c_int as usize][(i % OPL_NUM_VOICES) as usize];
        voices[i as usize].op2 =
            voice_operators[1 as ::core::ffi::c_int as usize][(i % OPL_NUM_VOICES) as usize];
        voices[i as usize].array = i / OPL_NUM_VOICES << 8 as ::core::ffi::c_int;
        voices[i as usize].current_instr = ::core::ptr::null_mut::<genmidi_instr_t>();
        voice_free_list[i as usize] =
            (&raw mut voices as *mut opl_voice_t).offset(i as isize) as *mut opl_voice_t;
        i += 1;
    }
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_SetMusicVolume(mut volume: ::core::ffi::c_int) {
    let mut i: ::core::ffi::c_uint = 0;
    if current_music_volume == volume {
        return;
    }
    current_music_volume = volume;
    i = 0 as ::core::ffi::c_uint;
    while i < MIDI_CHANNELS_PER_TRACK as ::core::ffi::c_uint {
        if i == 15 as ::core::ffi::c_uint {
            SetChannelVolume(
                (&raw mut channels as *mut opl_channel_data_t).offset(i as isize)
                    as *mut opl_channel_data_t,
                volume as ::core::ffi::c_uint,
                false_0 as boolean,
            );
        } else {
            SetChannelVolume(
                (&raw mut channels as *mut opl_channel_data_t).offset(i as isize)
                    as *mut opl_channel_data_t,
                channels[i as usize].volume_base as ::core::ffi::c_uint,
                false_0 as boolean,
            );
        }
        i = i.wrapping_add(1);
    }
}
#[optimize(size)]
unsafe extern "C" fn VoiceKeyOff(mut voice: *mut opl_voice_t) {
    OPL_WriteRegister(
        OPL_REGS_FREQ_2 + (*voice).index | (*voice).array,
        ((*voice).freq >> 8 as ::core::ffi::c_int) as ::core::ffi::c_int,
    );
}
#[optimize(size)]
unsafe extern "C" fn TrackChannelForEvent(
    mut track: *mut opl_track_data_t,
    mut event: *mut midi_event_t,
) -> *mut opl_channel_data_t {
    let mut channel_num: ::core::ffi::c_uint = (*event).data.channel.channel;
    if channel_num == 9 as ::core::ffi::c_uint {
        channel_num = 15 as ::core::ffi::c_uint;
    } else if channel_num == 15 as ::core::ffi::c_uint {
        channel_num = 9 as ::core::ffi::c_uint;
    }
    return (&raw mut channels as *mut opl_channel_data_t).offset(channel_num as isize)
        as *mut opl_channel_data_t;
}
#[optimize(size)]
unsafe extern "C" fn KeyOffEvent(mut track: *mut opl_track_data_t, mut event: *mut midi_event_t) {
    let mut channel: *mut opl_channel_data_t = ::core::ptr::null_mut::<opl_channel_data_t>();
    let mut i: ::core::ffi::c_int = 0;
    let mut key: ::core::ffi::c_uint = 0;
    channel = TrackChannelForEvent(track, event);
    key = (*event).data.channel.param1;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num {
        if (*voice_alloced_list[i as usize]).channel == channel
            && (*voice_alloced_list[i as usize]).key == key
        {
            ReleaseVoice(i);
            i -= 1;
        }
        i += 1;
    }
}
#[optimize(size)]
unsafe extern "C" fn ReplaceExistingVoice() {
    let mut i: ::core::ffi::c_int = 0;
    let mut result: ::core::ffi::c_int = 0;
    result = 0 as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num {
        if (*voice_alloced_list[i as usize]).current_instr_voice != 0 as ::core::ffi::c_uint
            || (*voice_alloced_list[i as usize]).channel
                >= (*voice_alloced_list[result as usize]).channel
        {
            result = i;
        }
        i += 1;
    }
    ReleaseVoice(result);
}
#[optimize(size)]
unsafe extern "C" fn ReplaceExistingVoiceDoom1() {
    let mut i: ::core::ffi::c_int = 0;
    let mut result: ::core::ffi::c_int = 0;
    result = 0 as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num {
        if (*voice_alloced_list[i as usize]).channel
            > (*voice_alloced_list[result as usize]).channel
        {
            result = i;
        }
        i += 1;
    }
    ReleaseVoice(result);
}
#[optimize(size)]
unsafe extern "C" fn ReplaceExistingVoiceDoom2(mut channel: *mut opl_channel_data_t) {
    let mut i: ::core::ffi::c_int = 0;
    let mut result: ::core::ffi::c_int = 0;
    let mut priority: ::core::ffi::c_int = 0;
    result = 0 as ::core::ffi::c_int;
    priority = 0x8000 as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num - 3 as ::core::ffi::c_int {
        if (*voice_alloced_list[i as usize]).priority < priority as ::core::ffi::c_uint
            && (*voice_alloced_list[i as usize]).channel >= channel
        {
            priority = (*voice_alloced_list[i as usize]).priority as ::core::ffi::c_int;
            result = i;
        }
        i += 1;
    }
    ReleaseVoice(result);
}
#[optimize(size)]
unsafe extern "C" fn FrequencyForVoice(mut voice: *mut opl_voice_t) -> ::core::ffi::c_uint {
    let mut gm_voice: *mut genmidi_voice_t = ::core::ptr::null_mut::<genmidi_voice_t>();
    let mut freq_index: ::core::ffi::c_int = 0;
    let mut octave: ::core::ffi::c_uint = 0;
    let mut sub_index: ::core::ffi::c_uint = 0;
    let mut note: ::core::ffi::c_int = 0;
    note = (*voice).note as ::core::ffi::c_int;
    gm_voice = (&raw mut (*(*voice).current_instr).voices as *mut genmidi_voice_t)
        .offset((*voice).current_instr_voice as isize) as *mut genmidi_voice_t;
    if (*(*voice).current_instr).flags as ::core::ffi::c_short as ::core::ffi::c_int
        & GENMIDI_FLAG_FIXED
        == 0 as ::core::ffi::c_int
    {
        note += (*gm_voice).base_note_offset as ::core::ffi::c_int;
    }
    while note < 0 as ::core::ffi::c_int {
        note += 12 as ::core::ffi::c_int;
    }
    while note > 95 as ::core::ffi::c_int {
        note -= 12 as ::core::ffi::c_int;
    }
    freq_index =
        64 as ::core::ffi::c_int + 32 as ::core::ffi::c_int * note + (*(*voice).channel).bend;
    if (*voice).current_instr_voice != 0 as ::core::ffi::c_uint {
        freq_index += (*(*voice).current_instr).fine_tuning as ::core::ffi::c_int
            / 2 as ::core::ffi::c_int
            - 64 as ::core::ffi::c_int;
    }
    if freq_index < 0 as ::core::ffi::c_int {
        freq_index = 0 as ::core::ffi::c_int;
    }
    if freq_index < 284 as ::core::ffi::c_int {
        return frequency_curve[freq_index as usize] as ::core::ffi::c_uint;
    }
    sub_index = ((freq_index - 284 as ::core::ffi::c_int)
        % (12 as ::core::ffi::c_int * 32 as ::core::ffi::c_int))
        as ::core::ffi::c_uint;
    octave = ((freq_index - 284 as ::core::ffi::c_int)
        / (12 as ::core::ffi::c_int * 32 as ::core::ffi::c_int))
        as ::core::ffi::c_uint;
    if octave >= 7 as ::core::ffi::c_uint {
        octave = 7 as ::core::ffi::c_uint;
    }
    return frequency_curve[sub_index.wrapping_add(284 as ::core::ffi::c_uint) as usize]
        as ::core::ffi::c_uint
        | octave << 10 as ::core::ffi::c_int;
}
#[optimize(size)]
unsafe extern "C" fn UpdateVoiceFrequency(mut voice: *mut opl_voice_t) {
    let mut freq: ::core::ffi::c_uint = 0;
    freq = FrequencyForVoice(voice);
    if (*voice).freq != freq {
        OPL_WriteRegister(
            OPL_REGS_FREQ_1 + (*voice).index | (*voice).array,
            (freq & 0xff as ::core::ffi::c_uint) as ::core::ffi::c_int,
        );
        OPL_WriteRegister(
            OPL_REGS_FREQ_2 + (*voice).index | (*voice).array,
            (freq >> 8 as ::core::ffi::c_int | 0x20 as ::core::ffi::c_uint) as ::core::ffi::c_int,
        );
        (*voice).freq = freq;
    }
}
#[optimize(size)]
unsafe extern "C" fn VoiceKeyOn(
    mut channel: *mut opl_channel_data_t,
    mut instrument: *mut genmidi_instr_t,
    mut instrument_voice: ::core::ffi::c_uint,
    mut note: ::core::ffi::c_uint,
    mut key: ::core::ffi::c_uint,
    mut volume: ::core::ffi::c_uint,
) {
    let mut voice: *mut opl_voice_t = ::core::ptr::null_mut::<opl_voice_t>();
    if opl_opl3mode == 0
        && opl_drv_ver as ::core::ffi::c_uint
            == opl_doom1_1_666 as ::core::ffi::c_int as ::core::ffi::c_uint
    {
        instrument_voice = 0 as ::core::ffi::c_uint;
    }
    voice = GetFreeVoice();
    if voice.is_null() {
        return;
    }
    (*voice).channel = channel;
    (*voice).key = key;
    if (*instrument).flags as ::core::ffi::c_short as ::core::ffi::c_int & GENMIDI_FLAG_FIXED
        != 0 as ::core::ffi::c_int
    {
        (*voice).note = (*instrument).fixed_note as ::core::ffi::c_uint;
    } else {
        (*voice).note = note;
    }
    (*voice).reg_pan = (*channel).pan as ::core::ffi::c_uint;
    SetVoiceInstrument(voice, instrument, instrument_voice);
    SetVoiceVolume(voice, volume);
    (*voice).freq = 0 as ::core::ffi::c_uint;
    UpdateVoiceFrequency(voice);
}
#[optimize(size)]
unsafe extern "C" fn KeyOnEvent(mut track: *mut opl_track_data_t, mut event: *mut midi_event_t) {
    let mut instrument: *mut genmidi_instr_t = ::core::ptr::null_mut::<genmidi_instr_t>();
    let mut channel: *mut opl_channel_data_t = ::core::ptr::null_mut::<opl_channel_data_t>();
    let mut note: ::core::ffi::c_uint = 0;
    let mut key: ::core::ffi::c_uint = 0;
    let mut volume: ::core::ffi::c_uint = 0;
    let mut voicenum: ::core::ffi::c_uint = 0;
    let mut double_voice: boolean = 0;
    note = (*event).data.channel.param1;
    key = (*event).data.channel.param1;
    volume = (*event).data.channel.param2;
    if volume <= 0 as ::core::ffi::c_uint {
        KeyOffEvent(track, event);
        return;
    }
    channel = TrackChannelForEvent(track, event);
    if (*event).data.channel.channel == 9 as ::core::ffi::c_uint {
        if key < 35 as ::core::ffi::c_uint || key > 81 as ::core::ffi::c_uint {
            return;
        }
        instrument = percussion_instrs.offset(key.wrapping_sub(35 as ::core::ffi::c_uint) as isize)
            as *mut genmidi_instr_t;
        last_perc[last_perc_count as usize] = key as uint8_t;
        last_perc_count = last_perc_count
            .wrapping_add(1 as ::core::ffi::c_uint)
            .wrapping_rem(PERCUSSION_LOG_LEN as ::core::ffi::c_uint);
        note = 60 as ::core::ffi::c_uint;
    } else {
        instrument = (*channel).instrument;
    }
    double_voice = ((*instrument).flags as ::core::ffi::c_short as ::core::ffi::c_int
        & GENMIDI_FLAG_2VOICE
        != 0 as ::core::ffi::c_int) as ::core::ffi::c_int as boolean;
    match opl_drv_ver as ::core::ffi::c_uint {
        0 => {
            voicenum = double_voice.wrapping_add(1 as boolean) as ::core::ffi::c_uint;
            if opl_opl3mode == 0 {
                voicenum = 1 as ::core::ffi::c_uint;
            }
            while voice_alloced_num as ::core::ffi::c_uint
                > (num_opl_voices as ::core::ffi::c_uint).wrapping_sub(voicenum)
            {
                ReplaceExistingVoiceDoom1();
            }
            if double_voice != 0 {
                VoiceKeyOn(
                    channel,
                    instrument,
                    1 as ::core::ffi::c_uint,
                    note,
                    key,
                    volume,
                );
            }
            VoiceKeyOn(
                channel,
                instrument,
                0 as ::core::ffi::c_uint,
                note,
                key,
                volume,
            );
        }
        1 => {
            if voice_alloced_num == num_opl_voices {
                ReplaceExistingVoiceDoom2(channel);
            }
            if voice_alloced_num == num_opl_voices - 1 as ::core::ffi::c_int && double_voice != 0 {
                ReplaceExistingVoiceDoom2(channel);
            }
            if double_voice != 0 {
                VoiceKeyOn(
                    channel,
                    instrument,
                    1 as ::core::ffi::c_uint,
                    note,
                    key,
                    volume,
                );
            }
            VoiceKeyOn(
                channel,
                instrument,
                0 as ::core::ffi::c_uint,
                note,
                key,
                volume,
            );
        }
        2 | _ => {
            if voice_free_num == 0 as ::core::ffi::c_int {
                ReplaceExistingVoice();
            }
            VoiceKeyOn(
                channel,
                instrument,
                0 as ::core::ffi::c_uint,
                note,
                key,
                volume,
            );
            if double_voice != 0 {
                VoiceKeyOn(
                    channel,
                    instrument,
                    1 as ::core::ffi::c_uint,
                    note,
                    key,
                    volume,
                );
            }
        }
    };
}
#[optimize(size)]
unsafe extern "C" fn ProgramChangeEvent(
    mut track: *mut opl_track_data_t,
    mut event: *mut midi_event_t,
) {
    let mut channel: *mut opl_channel_data_t = ::core::ptr::null_mut::<opl_channel_data_t>();
    let mut instrument: ::core::ffi::c_int = 0;
    channel = TrackChannelForEvent(track, event);
    instrument = (*event).data.channel.param1 as ::core::ffi::c_int;
    (*channel).instrument = main_instrs.offset(instrument as isize) as *mut genmidi_instr_t;
}
#[optimize(size)]
unsafe extern "C" fn SetChannelVolume(
    mut channel: *mut opl_channel_data_t,
    mut volume: ::core::ffi::c_uint,
    mut clip_start: boolean,
) {
    let mut i: ::core::ffi::c_uint = 0;
    (*channel).volume_base = volume as ::core::ffi::c_int;
    if volume > current_music_volume as ::core::ffi::c_uint {
        volume = current_music_volume as ::core::ffi::c_uint;
    }
    if clip_start != 0 && volume > start_music_volume as ::core::ffi::c_uint {
        volume = start_music_volume as ::core::ffi::c_uint;
    }
    (*channel).volume = volume as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_uint;
    while i < num_opl_voices as ::core::ffi::c_uint {
        if voices[i as usize].channel == channel {
            SetVoiceVolume(
                (&raw mut voices as *mut opl_voice_t).offset(i as isize) as *mut opl_voice_t,
                voices[i as usize].note_volume,
            );
        }
        i = i.wrapping_add(1);
    }
}
#[optimize(size)]
unsafe extern "C" fn SetChannelPan(
    mut channel: *mut opl_channel_data_t,
    mut pan: ::core::ffi::c_uint,
) {
    let mut reg_pan: ::core::ffi::c_uint = 0;
    let mut i: ::core::ffi::c_uint = 0;
    if opl_stereo_correct != 0 {
        pan = (144 as ::core::ffi::c_uint).wrapping_sub(pan);
    }
    if opl_opl3mode != 0 {
        if pan >= 96 as ::core::ffi::c_uint {
            reg_pan = 0x10 as ::core::ffi::c_uint;
        } else if pan <= 48 as ::core::ffi::c_uint {
            reg_pan = 0x20 as ::core::ffi::c_uint;
        } else {
            reg_pan = 0x30 as ::core::ffi::c_uint;
        }
        if (*channel).pan as ::core::ffi::c_uint != reg_pan {
            (*channel).pan = reg_pan as ::core::ffi::c_int;
            i = 0 as ::core::ffi::c_uint;
            while i < num_opl_voices as ::core::ffi::c_uint {
                if voices[i as usize].channel == channel {
                    SetVoicePan(
                        (&raw mut voices as *mut opl_voice_t).offset(i as isize)
                            as *mut opl_voice_t,
                        reg_pan,
                    );
                }
                i = i.wrapping_add(1);
            }
        }
    }
}
#[optimize(size)]
unsafe extern "C" fn AllNotesOff(
    mut channel: *mut opl_channel_data_t,
    mut param: ::core::ffi::c_uint,
) {
    let mut i: ::core::ffi::c_int = 0;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num {
        if (*voice_alloced_list[i as usize]).channel == channel {
            ReleaseVoice(i);
            i -= 1;
        }
        i += 1;
    }
}
#[optimize(size)]
unsafe extern "C" fn ControllerEvent(
    mut track: *mut opl_track_data_t,
    mut event: *mut midi_event_t,
) {
    let mut channel: *mut opl_channel_data_t = ::core::ptr::null_mut::<opl_channel_data_t>();
    let mut controller: ::core::ffi::c_uint = 0;
    let mut param: ::core::ffi::c_uint = 0;
    channel = TrackChannelForEvent(track, event);
    controller = (*event).data.channel.param1;
    param = (*event).data.channel.param2;
    match controller {
        7 => {
            SetChannelVolume(channel, param, true_0 as boolean);
        }
        10 => {
            SetChannelPan(channel, param);
        }
        123 => {
            AllNotesOff(channel, param);
        }
        _ => {}
    };
}
#[optimize(size)]
unsafe extern "C" fn PitchBendEvent(
    mut track: *mut opl_track_data_t,
    mut event: *mut midi_event_t,
) {
    let mut channel: *mut opl_channel_data_t = ::core::ptr::null_mut::<opl_channel_data_t>();
    let mut i: ::core::ffi::c_int = 0;
    let mut voice_updated_list: [*mut opl_voice_t; 18] =
        [::core::ptr::null_mut::<opl_voice_t>(); 18];
    let mut voice_updated_num: ::core::ffi::c_uint = 0 as ::core::ffi::c_uint;
    let mut voice_not_updated_list: [*mut opl_voice_t; 18] =
        [::core::ptr::null_mut::<opl_voice_t>(); 18];
    let mut voice_not_updated_num: ::core::ffi::c_uint = 0 as ::core::ffi::c_uint;
    channel = TrackChannelForEvent(track, event);
    (*channel).bend = (*event)
        .data
        .channel
        .param2
        .wrapping_sub(64 as ::core::ffi::c_uint) as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num {
        if (*voice_alloced_list[i as usize]).channel == channel {
            UpdateVoiceFrequency(voice_alloced_list[i as usize]);
            let fresh2 = voice_updated_num;
            voice_updated_num = voice_updated_num.wrapping_add(1);
            voice_updated_list[fresh2 as usize] = voice_alloced_list[i as usize];
        } else {
            let fresh3 = voice_not_updated_num;
            voice_not_updated_num = voice_not_updated_num.wrapping_add(1);
            voice_not_updated_list[fresh3 as usize] = voice_alloced_list[i as usize];
        }
        i += 1;
    }
    i = 0 as ::core::ffi::c_int;
    while (i as ::core::ffi::c_uint) < voice_not_updated_num {
        voice_alloced_list[i as usize] = voice_not_updated_list[i as usize];
        i += 1;
    }
    i = 0 as ::core::ffi::c_int;
    while (i as ::core::ffi::c_uint) < voice_updated_num {
        voice_alloced_list
            [(i as ::core::ffi::c_uint).wrapping_add(voice_not_updated_num) as usize] =
            voice_updated_list[i as usize];
        i += 1;
    }
}
#[optimize(size)]
unsafe extern "C" fn MetaSetTempo(mut tempo: ::core::ffi::c_uint) {
    OPL_AdjustCallbacks(us_per_beat as ::core::ffi::c_float / tempo as ::core::ffi::c_float);
    us_per_beat = tempo;
}
#[optimize(size)]
unsafe extern "C" fn MetaEvent(mut track: *mut opl_track_data_t, mut event: *mut midi_event_t) {
    let mut data: *mut byte = (*event).data.meta.data;
    let mut data_len: ::core::ffi::c_uint = (*event).data.meta.length;
    match (*event).data.meta.type_0 {
        81 => {
            if data_len == 3 as ::core::ffi::c_uint {
                MetaSetTempo(
                    ((*data.offset(0 as ::core::ffi::c_int as isize) as ::core::ffi::c_int)
                        << 16 as ::core::ffi::c_int
                        | (*data.offset(1 as ::core::ffi::c_int as isize) as ::core::ffi::c_int)
                            << 8 as ::core::ffi::c_int
                        | *data.offset(2 as ::core::ffi::c_int as isize) as ::core::ffi::c_int)
                        as ::core::ffi::c_uint,
                );
            }
        }
        0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 127 | 47 | _ => {}
    };
}
#[optimize(size)]
unsafe extern "C" fn ProcessEvent(mut track: *mut opl_track_data_t, mut event: *mut midi_event_t) {
    match (*event).event_type as ::core::ffi::c_uint {
        128 => {
            KeyOffEvent(track, event);
        }
        144 => {
            KeyOnEvent(track, event);
        }
        176 => {
            ControllerEvent(track, event);
        }
        192 => {
            ProgramChangeEvent(track, event);
        }
        224 => {
            PitchBendEvent(track, event);
        }
        255 => {
            MetaEvent(track, event);
        }
        240 | 247 | _ => {}
    };
}
#[optimize(size)]
unsafe extern "C" fn RestartSong(mut unused: *mut ::core::ffi::c_void) {
    let mut i: ::core::ffi::c_uint = 0;
    running_tracks = num_tracks;
    start_music_volume = current_music_volume;
    i = 0 as ::core::ffi::c_uint;
    while i < num_tracks {
        MIDI_RestartIterator((*tracks.offset(i as isize)).iter);
        ScheduleTrack(tracks.offset(i as isize) as *mut opl_track_data_t);
        i = i.wrapping_add(1);
    }
    i = 0 as ::core::ffi::c_uint;
    while i < MIDI_CHANNELS_PER_TRACK as ::core::ffi::c_uint {
        InitChannel(
            (&raw mut channels as *mut opl_channel_data_t).offset(i as isize)
                as *mut opl_channel_data_t,
        );
        i = i.wrapping_add(1);
    }
}
#[optimize(size)]
unsafe extern "C" fn TrackTimerCallback(mut arg: *mut ::core::ffi::c_void) {
    let mut track: *mut opl_track_data_t = arg as *mut opl_track_data_t;
    let mut event: *mut midi_event_t = ::core::ptr::null_mut::<midi_event_t>();
    if MIDI_GetNextEvent((*track).iter, &raw mut event) == 0 {
        return;
    }
    ProcessEvent(track, event);
    if (*event).event_type as ::core::ffi::c_uint
        == MIDI_EVENT_META as ::core::ffi::c_int as ::core::ffi::c_uint
        && (*event).data.meta.type_0
            == MIDI_META_END_OF_TRACK as ::core::ffi::c_int as ::core::ffi::c_uint
    {
        running_tracks = running_tracks.wrapping_sub(1);
        if running_tracks <= 0 as ::core::ffi::c_uint && song_looping != 0 {
            OPL_SetCallback(
                5000 as uint64_t,
                Some(RestartSong as unsafe extern "C" fn(*mut ::core::ffi::c_void) -> ()),
                NULL,
            );
        }
        return;
    }
    ScheduleTrack(track);
}
#[optimize(size)]
unsafe extern "C" fn ScheduleTrack(mut track: *mut opl_track_data_t) {
    let mut nticks: ::core::ffi::c_uint = 0;
    let mut us: uint64_t = 0;
    nticks = MIDI_GetDeltaTime((*track).iter);
    us = (nticks as uint64_t)
        .wrapping_mul(us_per_beat as uint64_t)
        .wrapping_div(ticks_per_beat as uint64_t);
    OPL_SetCallback(
        us,
        Some(TrackTimerCallback as unsafe extern "C" fn(*mut ::core::ffi::c_void) -> ()),
        track as *mut ::core::ffi::c_void,
    );
}
#[optimize(size)]
unsafe extern "C" fn InitChannel(mut channel: *mut opl_channel_data_t) {
    (*channel).instrument =
        main_instrs.offset(0 as ::core::ffi::c_int as isize) as *mut genmidi_instr_t;
    (*channel).volume = current_music_volume;
    (*channel).volume_base = 100 as ::core::ffi::c_int;
    if (*channel).volume > (*channel).volume_base {
        (*channel).volume = (*channel).volume_base;
    }
    (*channel).pan = 0x30 as ::core::ffi::c_int;
    (*channel).bend = 0 as ::core::ffi::c_int;
}
#[optimize(size)]
unsafe extern "C" fn StartTrack(mut file: *mut midi_file_t, mut track_num: ::core::ffi::c_uint) {
    let mut track: *mut opl_track_data_t = ::core::ptr::null_mut::<opl_track_data_t>();
    track = tracks.offset(track_num as isize) as *mut opl_track_data_t;
    (*track).iter = MIDI_IterateTrack(file, track_num);
    ScheduleTrack(track);
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_PlaySong(mut handle: *mut ::core::ffi::c_void, mut looping: boolean) {
    let mut file: *mut midi_file_t = ::core::ptr::null_mut::<midi_file_t>();
    let mut i: ::core::ffi::c_uint = 0;
    if music_initialized == 0 || handle.is_null() {
        return;
    }
    file = handle as *mut midi_file_t;
    tracks = malloc(
        (MIDI_NumTracks(file) as size_t)
            .wrapping_mul(::core::mem::size_of::<opl_track_data_t>() as size_t),
    ) as *mut opl_track_data_t;
    num_tracks = MIDI_NumTracks(file);
    running_tracks = num_tracks;
    song_looping = looping;
    ticks_per_beat = MIDI_GetFileTimeDivision(file);
    us_per_beat = (500 as ::core::ffi::c_int * 1000 as ::core::ffi::c_int) as ::core::ffi::c_uint;
    start_music_volume = current_music_volume;
    i = 0 as ::core::ffi::c_uint;
    while i < num_tracks {
        StartTrack(file, i);
        i = i.wrapping_add(1);
    }
    i = 0 as ::core::ffi::c_uint;
    while i < MIDI_CHANNELS_PER_TRACK as ::core::ffi::c_uint {
        InitChannel(
            (&raw mut channels as *mut opl_channel_data_t).offset(i as isize)
                as *mut opl_channel_data_t,
        );
        i = i.wrapping_add(1);
    }
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_PauseSong() {
    let mut i: ::core::ffi::c_uint = 0;
    if music_initialized == 0 {
        return;
    }
    OPL_SetPaused(1 as ::core::ffi::c_int);
    i = 0 as ::core::ffi::c_uint;
    while i < num_opl_voices as ::core::ffi::c_uint {
        if !voices[i as usize].channel.is_null()
            && voices[i as usize].current_instr < percussion_instrs
        {
            VoiceKeyOff(
                (&raw mut voices as *mut opl_voice_t).offset(i as isize) as *mut opl_voice_t
            );
        }
        i = i.wrapping_add(1);
    }
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_ResumeSong() {
    if music_initialized == 0 {
        return;
    }
    OPL_SetPaused(0 as ::core::ffi::c_int);
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_StopSong() {
    let mut i: ::core::ffi::c_uint = 0;
    if music_initialized == 0 {
        return;
    }
    OPL_Lock();
    OPL_ClearCallbacks();
    i = 0 as ::core::ffi::c_uint;
    while i < MIDI_CHANNELS_PER_TRACK as ::core::ffi::c_uint {
        AllNotesOff(
            (&raw mut channels as *mut opl_channel_data_t).offset(i as isize)
                as *mut opl_channel_data_t,
            0 as ::core::ffi::c_uint,
        );
        i = i.wrapping_add(1);
    }
    i = 0 as ::core::ffi::c_uint;
    while i < num_tracks {
        MIDI_FreeIterator((*tracks.offset(i as isize)).iter);
        i = i.wrapping_add(1);
    }
    free(tracks as *mut ::core::ffi::c_void);
    tracks = ::core::ptr::null_mut::<opl_track_data_t>();
    num_tracks = 0 as ::core::ffi::c_uint;
    OPL_Unlock();
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_UnRegisterSong(mut handle: *mut ::core::ffi::c_void) {
    if music_initialized == 0 {
        return;
    }
    if !handle.is_null() {
        MIDI_FreeFile(handle as *mut midi_file_t);
    }
}
#[optimize(size)]
unsafe extern "C" fn IsMid(mut mem: *mut byte, mut len: ::core::ffi::c_int) -> boolean {
    return (len > 4 as ::core::ffi::c_int
        && memcmp(
            mem as *const ::core::ffi::c_void,
            b"MThd\0" as *const u8 as *const ::core::ffi::c_char as *const ::core::ffi::c_void,
            4 as size_t,
        ) == 0) as ::core::ffi::c_int as boolean;
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_RegisterSong(
    mut data: *mut ::core::ffi::c_void,
    mut len: ::core::ffi::c_int,
) -> *mut ::core::ffi::c_void {
    let mut result: *mut midi_file_t = ::core::ptr::null_mut::<midi_file_t>();
    if music_initialized == 0 {
        return NULL;
    }
    if IsMid(data as *mut byte, len) != 0 && len < MAXMIDLENGTH {
        result = MIDI_LoadFileMem(data as *mut byte, len as size_t);
    } else {
        let mut instream: *mut MEMFILE = mem_fopen_read(data, len as size_t);
        let mut outstream: *mut MEMFILE = mem_fopen_write();
        if mus2mid(instream, outstream) == 0 as boolean {
            let mut outbuf: *mut ::core::ffi::c_void =
                ::core::ptr::null_mut::<::core::ffi::c_void>();
            let mut outbuf_len: size_t = 0;
            mem_get_buf(outstream, &raw mut outbuf, &raw mut outbuf_len);
            result = MIDI_LoadFileMem(outbuf as *mut byte, outbuf_len);
        }
        mem_fclose(instream);
        mem_fclose(outstream);
    }
    if result.is_null() {
        fprintf(
            b83_stderr(),
            b"I_OPL_RegisterSong: Failed to load MID.\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
    }
    return result as *mut ::core::ffi::c_void;
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_MusicIsPlaying() -> boolean {
    if music_initialized == 0 {
        return false_0 as boolean;
    }
    return (num_tracks > 0 as ::core::ffi::c_uint) as ::core::ffi::c_int as boolean;
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_ShutdownMusic() {
    if music_initialized != 0 {
        I_OPL_StopSong();
        OPL_Shutdown();
        W_ReleaseLumpName(
            b"genmidi\0" as *const u8 as *const ::core::ffi::c_char as *mut ::core::ffi::c_char,
        );
        music_initialized = false_0 as boolean;
    }
}
#[optimize(size)]
unsafe extern "C" fn I_OPL_InitMusic() -> boolean {
    let mut dmxoption: *mut ::core::ffi::c_char = ::core::ptr::null_mut::<::core::ffi::c_char>();
    let mut chip_type: opl_init_result_t = OPL_INIT_NONE;
    OPL_SetSampleRate(snd_samplerate as ::core::ffi::c_uint);
    chip_type = OPL_Init(opl_io_port as ::core::ffi::c_uint);
    if chip_type as ::core::ffi::c_uint
        == OPL_INIT_NONE as ::core::ffi::c_int as ::core::ffi::c_uint
    {
        printf(
            b"Dude.  The Adlib isn't responding.\n\0" as *const u8 as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    dmxoption = getenv(b"DMXOPTION\0" as *const u8 as *const ::core::ffi::c_char);
    if dmxoption.is_null() {
        dmxoption = (if !snd_dmxoption.is_null() {
            snd_dmxoption as *const ::core::ffi::c_char
        } else {
            b"\0" as *const u8 as *const ::core::ffi::c_char
        }) as *mut ::core::ffi::c_char;
    }
    if chip_type as ::core::ffi::c_uint
        == OPL_INIT_OPL3 as ::core::ffi::c_int as ::core::ffi::c_uint
        && !strstr(
            dmxoption,
            b"-opl3\0" as *const u8 as *const ::core::ffi::c_char,
        )
        .is_null()
    {
        opl_opl3mode = 1 as ::core::ffi::c_int;
        num_opl_voices = OPL_NUM_VOICES * 2 as ::core::ffi::c_int;
    } else {
        opl_opl3mode = 0 as ::core::ffi::c_int;
        num_opl_voices = OPL_NUM_VOICES;
    }
    opl_stereo_correct = (strstr(
        dmxoption,
        b"-reverse\0" as *const u8 as *const ::core::ffi::c_char,
    ) != NULL as *mut ::core::ffi::c_char) as ::core::ffi::c_int
        as boolean;
    OPL_InitRegisters(opl_opl3mode);
    if LoadInstrumentTable() == 0 {
        OPL_Shutdown();
        return false_0 as boolean;
    }
    InitVoices();
    tracks = ::core::ptr::null_mut::<opl_track_data_t>();
    num_tracks = 0 as ::core::ffi::c_uint;
    music_initialized = true_0 as boolean;
    return true_0 as boolean;
}
static mut music_opl_devices__b82i: [snddevice_t; 2] = unsafe { ::core::mem::zeroed() };
static mut music_opl_devices: [snddevice_t; 2] = unsafe { ::core::mem::zeroed() };
static mut DG_music_module__b82i: music_module_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
pub static mut DG_music_module: music_module_t = unsafe { ::core::mem::zeroed() };
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn I_SetOPLDriverVer(mut ver: opl_driver_ver_t) {
    opl_drv_ver = ver;
}
#[optimize(size)]
unsafe extern "C" fn NumActiveChannels() -> ::core::ffi::c_int {
    let mut i: ::core::ffi::c_int = 0;
    i = MIDI_CHANNELS_PER_TRACK - 1 as ::core::ffi::c_int;
    while i >= 0 as ::core::ffi::c_int {
        if channels[i as usize].instrument
            != main_instrs.offset(0 as ::core::ffi::c_int as isize) as *mut genmidi_instr_t
        {
            return i + 1 as ::core::ffi::c_int;
        }
        i -= 1;
    }
    return 0 as ::core::ffi::c_int;
}
#[optimize(size)]
unsafe extern "C" fn ChannelInUse(mut channel: *mut opl_channel_data_t) -> ::core::ffi::c_int {
    let mut i: ::core::ffi::c_int = 0;
    i = 0 as ::core::ffi::c_int;
    while i < voice_alloced_num {
        if (*voice_alloced_list[i as usize]).channel == channel {
            return 1 as ::core::ffi::c_int;
        }
        i += 1;
    }
    return 0 as ::core::ffi::c_int;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn I_OPL_DevMessages(
    mut result: *mut ::core::ffi::c_char,
    mut result_len: size_t,
) {
    let mut tmp: [::core::ffi::c_char; 80] = [0; 80];
    let mut instr_num: ::core::ffi::c_int = 0;
    let mut lines: ::core::ffi::c_int = 0;
    let mut i: ::core::ffi::c_int = 0;
    if num_tracks == 0 as ::core::ffi::c_uint {
        M_snprintf(
            result,
            result_len,
            b"No OPL track!\0" as *const u8 as *const ::core::ffi::c_char,
        );
        return;
    }
    M_snprintf(
        result,
        result_len,
        b"Tracks:\n\0" as *const u8 as *const ::core::ffi::c_char,
    );
    lines = 1 as ::core::ffi::c_int;
    i = 0 as ::core::ffi::c_int;
    while i < NumActiveChannels() {
        if !channels[i as usize].instrument.is_null() {
            instr_num = channels[i as usize].instrument.offset_from(main_instrs)
                as ::core::ffi::c_long as ::core::ffi::c_int;
            M_snprintf(
                &raw mut tmp as *mut ::core::ffi::c_char,
                ::core::mem::size_of::<[::core::ffi::c_char; 80]>() as size_t,
                b"chan %i: %c i#%i (%s)\n\0" as *const u8 as *const ::core::ffi::c_char,
                i,
                if ChannelInUse(
                    (&raw mut channels as *mut opl_channel_data_t).offset(i as isize)
                        as *mut opl_channel_data_t,
                ) != 0
                {
                    '\'' as i32
                } else {
                    ' ' as i32
                },
                instr_num + 1 as ::core::ffi::c_int,
                &raw mut *main_instr_names.offset(instr_num as isize) as *mut ::core::ffi::c_char,
            );
            M_StringConcat(result, &raw mut tmp as *mut ::core::ffi::c_char, result_len);
            lines += 1;
        }
        i += 1;
    }
    M_snprintf(
        &raw mut tmp as *mut ::core::ffi::c_char,
        ::core::mem::size_of::<[::core::ffi::c_char; 80]>() as size_t,
        b"\nLast percussion:\n\0" as *const u8 as *const ::core::ffi::c_char,
    );
    M_StringConcat(result, &raw mut tmp as *mut ::core::ffi::c_char, result_len);
    lines += 2 as ::core::ffi::c_int;
    i = last_perc_count
        .wrapping_add(PERCUSSION_LOG_LEN as ::core::ffi::c_uint)
        .wrapping_sub(1 as ::core::ffi::c_uint)
        .wrapping_rem(PERCUSSION_LOG_LEN as ::core::ffi::c_uint) as ::core::ffi::c_int;
    while !(last_perc[i as usize] as ::core::ffi::c_int == 0 as ::core::ffi::c_int) {
        M_snprintf(
            &raw mut tmp as *mut ::core::ffi::c_char,
            ::core::mem::size_of::<[::core::ffi::c_char; 80]>() as size_t,
            b"%cp#%i (%s)\n\0" as *const u8 as *const ::core::ffi::c_char,
            if i == 0 as ::core::ffi::c_int {
                '\'' as i32
            } else {
                ' ' as i32
            },
            last_perc[i as usize] as ::core::ffi::c_int,
            &raw mut *percussion_names.offset(
                (*(&raw mut last_perc as *mut uint8_t).offset(i as isize) as ::core::ffi::c_int
                    - 35 as ::core::ffi::c_int) as isize,
            ) as *mut ::core::ffi::c_char,
        );
        M_StringConcat(result, &raw mut tmp as *mut ::core::ffi::c_char, result_len);
        lines += 1;
        i = (i + PERCUSSION_LOG_LEN - 1 as ::core::ffi::c_int) % PERCUSSION_LOG_LEN;
        if !(lines < 25 as ::core::ffi::c_int && i as ::core::ffi::c_uint != last_perc_count) {
            break;
        }
    }
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_i_oplmusic() {
    memcpy(
        &raw mut opl_drv_ver as *mut ::core::ffi::c_void,
        &raw const opl_drv_ver__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<opl_driver_ver_t>() as size_t,
    );
    memcpy(
        &raw mut music_initialized as *mut ::core::ffi::c_void,
        &raw const music_initialized__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<boolean>() as size_t,
    );
    memcpy(
        &raw mut num_tracks as *mut ::core::ffi::c_void,
        &raw const num_tracks__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_uint>() as size_t,
    );
    memcpy(
        &raw mut running_tracks as *mut ::core::ffi::c_void,
        &raw const running_tracks__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_uint>() as size_t,
    );
    memcpy(
        &raw mut snd_dmxoption as *mut ::core::ffi::c_void,
        &raw const snd_dmxoption__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<*mut ::core::ffi::c_char>() as size_t,
    );
    memcpy(
        &raw mut opl_io_port as *mut ::core::ffi::c_void,
        &raw const opl_io_port__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_int>() as size_t,
    );
    memcpy(
        &raw mut opl_stereo_correct as *mut ::core::ffi::c_void,
        &raw const opl_stereo_correct__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<boolean>() as size_t,
    );
    memcpy(
        &raw mut music_opl_devices as *mut ::core::ffi::c_void,
        &raw const music_opl_devices__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<[snddevice_t; 2]>() as size_t,
    );
    memcpy(
        &raw mut DG_music_module as *mut ::core::ffi::c_void,
        &raw const DG_music_module__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<music_module_t>() as size_t,
    );
}
#[optimize(size)]
unsafe extern "C" fn run_static_initializers() {
    DG_music_module__b82i = music_module_t {
        sound_devices: &raw mut music_opl_devices as *mut snddevice_t,
        num_sound_devices: (::core::mem::size_of::<[snddevice_t; 2]>() as usize)
            .wrapping_div(::core::mem::size_of::<snddevice_t>() as usize)
            as ::core::ffi::c_int,
        Init: Some(I_OPL_InitMusic as unsafe extern "C" fn() -> boolean),
        Shutdown: Some(I_OPL_ShutdownMusic as unsafe extern "C" fn() -> ()),
        SetMusicVolume: Some(
            I_OPL_SetMusicVolume as unsafe extern "C" fn(::core::ffi::c_int) -> (),
        ),
        PauseMusic: Some(I_OPL_PauseSong as unsafe extern "C" fn() -> ()),
        ResumeMusic: Some(I_OPL_ResumeSong as unsafe extern "C" fn() -> ()),
        RegisterSong: Some(
            I_OPL_RegisterSong
                as unsafe extern "C" fn(
                    *mut ::core::ffi::c_void,
                    ::core::ffi::c_int,
                ) -> *mut ::core::ffi::c_void,
        ),
        UnRegisterSong: Some(
            I_OPL_UnRegisterSong as unsafe extern "C" fn(*mut ::core::ffi::c_void) -> (),
        ),
        PlaySong: Some(
            I_OPL_PlaySong as unsafe extern "C" fn(*mut ::core::ffi::c_void, boolean) -> (),
        ),
        StopSong: Some(I_OPL_StopSong as unsafe extern "C" fn() -> ()),
        MusicIsPlaying: Some(I_OPL_MusicIsPlaying as unsafe extern "C" fn() -> boolean),
        Poll: None,
    };
}

/// B83 (tools/fixdata.py): initial values, flash -> PSRAM .bss
#[optimize(size)]
pub unsafe fn b83i() {
    voice_operators = [
    [
        0 as ::core::ffi::c_int,
        0x1 as ::core::ffi::c_int,
        0x2 as ::core::ffi::c_int,
        0x8 as ::core::ffi::c_int,
        0x9 as ::core::ffi::c_int,
        0xa as ::core::ffi::c_int,
        0x10 as ::core::ffi::c_int,
        0x11 as ::core::ffi::c_int,
        0x12 as ::core::ffi::c_int,
    ],
    [
        0x3 as ::core::ffi::c_int,
        0x4 as ::core::ffi::c_int,
        0x5 as ::core::ffi::c_int,
        0xb as ::core::ffi::c_int,
        0xc as ::core::ffi::c_int,
        0xd as ::core::ffi::c_int,
        0x13 as ::core::ffi::c_int,
        0x14 as ::core::ffi::c_int,
        0x15 as ::core::ffi::c_int,
    ],
];
    frequency_curve = [
    0x133 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x133 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x134 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x134 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x135 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x136 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x136 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x137 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x137 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x138 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x138 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x139 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x139 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x13f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x140 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x140 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x141 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x142 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x142 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x143 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x143 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x144 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x144 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x145 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x146 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x146 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x147 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x147 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x148 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x149 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x149 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x14f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x150 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x150 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x151 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x152 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x152 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x153 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x153 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x154 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x155 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x155 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x156 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x157 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x157 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x158 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x158 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x159 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x15f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x160 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x161 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x161 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x162 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x162 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x163 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x164 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x164 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x165 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x166 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x166 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x167 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x168 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x168 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x169 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x16f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x170 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x170 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x171 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x172 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x172 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x173 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x174 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x174 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x175 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x176 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x176 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x177 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x178 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x178 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x179 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x17f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x180 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x181 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x181 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x182 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x183 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x183 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x184 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x185 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x185 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x186 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x187 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x188 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x188 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x189 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x18f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x190 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x191 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x192 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x192 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x193 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x194 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x194 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x195 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x196 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x197 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x197 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x198 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x199 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x19f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1a9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1aa as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ab as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ac as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ad as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ad as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ae as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1af as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1b9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ba as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1bb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1bc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1bc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1bd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1be as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1bf as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1c9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ca as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1cb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1cc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1cd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ce as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ce as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1cf as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1d9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1da as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1db as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1dc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1dd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1de as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1de as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1df as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1e9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ea as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1eb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ec as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ed as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ed as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ee as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ef as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1f9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1fa as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1fb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1fc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1fd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1fe as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x1ff as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x200 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x201 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x201 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x202 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x203 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x204 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x205 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x206 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x207 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x208 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x209 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x20a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x20b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x20c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x20d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x20e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x20f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x210 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x210 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x211 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x212 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x213 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x214 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x215 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x216 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x217 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x218 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x219 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x21a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x21b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x21c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x21d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x21e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x21f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x220 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x221 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x222 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x223 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x224 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x225 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x226 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x227 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x228 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x229 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x22a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x22b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x22c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x22d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x22e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x22f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x230 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x231 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x232 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x233 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x234 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x235 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x236 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x237 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x238 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x239 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x23a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x23b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x23c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x23d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x23e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x23f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x240 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x241 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x242 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x244 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x245 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x246 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x247 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x248 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x249 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x24a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x24b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x24c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x24d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x24e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x24f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x250 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x251 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x252 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x253 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x254 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x256 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x257 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x258 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x259 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x25a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x25b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x25c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x25d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x25e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x25f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x260 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x262 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x263 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x264 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x265 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x266 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x267 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x268 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x269 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x26a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x26c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x26d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x26e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x26f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x270 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x271 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x272 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x273 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x275 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x276 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x277 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x278 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x279 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x27a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x27b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x27d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x27e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x27f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x280 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x281 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x282 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x284 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x285 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x286 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x287 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x288 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x289 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x28b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x28c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x28d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x28e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x28f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x290 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x292 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x293 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x294 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x295 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x296 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x298 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x299 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x29a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x29b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x29c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x29e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x29f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2a9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2aa as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ab as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ac as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ae as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2af as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2b9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ba as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2bb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2bd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2be as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2bf as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2c9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2cb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2cc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2cd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ce as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2d9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2da as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2dc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2dd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2de as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2e9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ea as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ec as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ed as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ee as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2f9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2fb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2fc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2fd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x2ff as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x300 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x302 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x303 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x304 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x306 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x307 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x309 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x30a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x30b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x30d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x30e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x310 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x311 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x312 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x314 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x315 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x317 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x318 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x31a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x31b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x31c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x31e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x31f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x321 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x322 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x324 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x325 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x327 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x328 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x329 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x32b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x32c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x32e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x32f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x331 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x332 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x334 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x335 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x337 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x338 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x33a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x33b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x33d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x33e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x340 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x341 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x343 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x344 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x346 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x347 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x349 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x34a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x34c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x34d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x34f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x350 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x352 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x353 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x355 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x357 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x358 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x35a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x35b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x35d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x35e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x360 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x361 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x363 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x365 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x366 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x368 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x369 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x36b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x36c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x36e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x370 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x371 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x373 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x374 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x376 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x378 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x379 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x37b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x37c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x37e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x380 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x381 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x383 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x384 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x386 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x388 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x389 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x38b as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x38d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x38e as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x390 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x392 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x393 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x395 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x397 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x398 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x39a as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x39c as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x39d as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x39f as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3a1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3a2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3a4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3a6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3a7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3a9 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ab as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ac as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ae as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3b0 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3b1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3b3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3b5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3b7 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3b8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ba as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3bc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3bd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3bf as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3c1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3c3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3c4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3c6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3c8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ca as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3cb as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3cd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3cf as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3d1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3d2 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3d4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3d6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3d8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3da as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3db as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3dd as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3df as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3e1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3e3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3e4 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3e6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3e8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ea as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ec as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ed as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3ef as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3f1 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3f3 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3f5 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3f6 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3f8 as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3fa as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3fc as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x3fe as ::core::ffi::c_int as ::core::ffi::c_ushort,
    0x36c as ::core::ffi::c_int as ::core::ffi::c_ushort,
];
    volume_mapping_table = [
    0 as ::core::ffi::c_int as ::core::ffi::c_uint,
    1 as ::core::ffi::c_int as ::core::ffi::c_uint,
    3 as ::core::ffi::c_int as ::core::ffi::c_uint,
    5 as ::core::ffi::c_int as ::core::ffi::c_uint,
    6 as ::core::ffi::c_int as ::core::ffi::c_uint,
    8 as ::core::ffi::c_int as ::core::ffi::c_uint,
    10 as ::core::ffi::c_int as ::core::ffi::c_uint,
    11 as ::core::ffi::c_int as ::core::ffi::c_uint,
    13 as ::core::ffi::c_int as ::core::ffi::c_uint,
    14 as ::core::ffi::c_int as ::core::ffi::c_uint,
    16 as ::core::ffi::c_int as ::core::ffi::c_uint,
    17 as ::core::ffi::c_int as ::core::ffi::c_uint,
    19 as ::core::ffi::c_int as ::core::ffi::c_uint,
    20 as ::core::ffi::c_int as ::core::ffi::c_uint,
    22 as ::core::ffi::c_int as ::core::ffi::c_uint,
    23 as ::core::ffi::c_int as ::core::ffi::c_uint,
    25 as ::core::ffi::c_int as ::core::ffi::c_uint,
    26 as ::core::ffi::c_int as ::core::ffi::c_uint,
    27 as ::core::ffi::c_int as ::core::ffi::c_uint,
    29 as ::core::ffi::c_int as ::core::ffi::c_uint,
    30 as ::core::ffi::c_int as ::core::ffi::c_uint,
    32 as ::core::ffi::c_int as ::core::ffi::c_uint,
    33 as ::core::ffi::c_int as ::core::ffi::c_uint,
    34 as ::core::ffi::c_int as ::core::ffi::c_uint,
    36 as ::core::ffi::c_int as ::core::ffi::c_uint,
    37 as ::core::ffi::c_int as ::core::ffi::c_uint,
    39 as ::core::ffi::c_int as ::core::ffi::c_uint,
    41 as ::core::ffi::c_int as ::core::ffi::c_uint,
    43 as ::core::ffi::c_int as ::core::ffi::c_uint,
    45 as ::core::ffi::c_int as ::core::ffi::c_uint,
    47 as ::core::ffi::c_int as ::core::ffi::c_uint,
    49 as ::core::ffi::c_int as ::core::ffi::c_uint,
    50 as ::core::ffi::c_int as ::core::ffi::c_uint,
    52 as ::core::ffi::c_int as ::core::ffi::c_uint,
    54 as ::core::ffi::c_int as ::core::ffi::c_uint,
    55 as ::core::ffi::c_int as ::core::ffi::c_uint,
    57 as ::core::ffi::c_int as ::core::ffi::c_uint,
    59 as ::core::ffi::c_int as ::core::ffi::c_uint,
    60 as ::core::ffi::c_int as ::core::ffi::c_uint,
    61 as ::core::ffi::c_int as ::core::ffi::c_uint,
    63 as ::core::ffi::c_int as ::core::ffi::c_uint,
    64 as ::core::ffi::c_int as ::core::ffi::c_uint,
    66 as ::core::ffi::c_int as ::core::ffi::c_uint,
    67 as ::core::ffi::c_int as ::core::ffi::c_uint,
    68 as ::core::ffi::c_int as ::core::ffi::c_uint,
    69 as ::core::ffi::c_int as ::core::ffi::c_uint,
    71 as ::core::ffi::c_int as ::core::ffi::c_uint,
    72 as ::core::ffi::c_int as ::core::ffi::c_uint,
    73 as ::core::ffi::c_int as ::core::ffi::c_uint,
    74 as ::core::ffi::c_int as ::core::ffi::c_uint,
    75 as ::core::ffi::c_int as ::core::ffi::c_uint,
    76 as ::core::ffi::c_int as ::core::ffi::c_uint,
    77 as ::core::ffi::c_int as ::core::ffi::c_uint,
    79 as ::core::ffi::c_int as ::core::ffi::c_uint,
    80 as ::core::ffi::c_int as ::core::ffi::c_uint,
    81 as ::core::ffi::c_int as ::core::ffi::c_uint,
    82 as ::core::ffi::c_int as ::core::ffi::c_uint,
    83 as ::core::ffi::c_int as ::core::ffi::c_uint,
    84 as ::core::ffi::c_int as ::core::ffi::c_uint,
    84 as ::core::ffi::c_int as ::core::ffi::c_uint,
    85 as ::core::ffi::c_int as ::core::ffi::c_uint,
    86 as ::core::ffi::c_int as ::core::ffi::c_uint,
    87 as ::core::ffi::c_int as ::core::ffi::c_uint,
    88 as ::core::ffi::c_int as ::core::ffi::c_uint,
    89 as ::core::ffi::c_int as ::core::ffi::c_uint,
    90 as ::core::ffi::c_int as ::core::ffi::c_uint,
    91 as ::core::ffi::c_int as ::core::ffi::c_uint,
    92 as ::core::ffi::c_int as ::core::ffi::c_uint,
    92 as ::core::ffi::c_int as ::core::ffi::c_uint,
    93 as ::core::ffi::c_int as ::core::ffi::c_uint,
    94 as ::core::ffi::c_int as ::core::ffi::c_uint,
    95 as ::core::ffi::c_int as ::core::ffi::c_uint,
    96 as ::core::ffi::c_int as ::core::ffi::c_uint,
    96 as ::core::ffi::c_int as ::core::ffi::c_uint,
    97 as ::core::ffi::c_int as ::core::ffi::c_uint,
    98 as ::core::ffi::c_int as ::core::ffi::c_uint,
    99 as ::core::ffi::c_int as ::core::ffi::c_uint,
    99 as ::core::ffi::c_int as ::core::ffi::c_uint,
    100 as ::core::ffi::c_int as ::core::ffi::c_uint,
    101 as ::core::ffi::c_int as ::core::ffi::c_uint,
    101 as ::core::ffi::c_int as ::core::ffi::c_uint,
    102 as ::core::ffi::c_int as ::core::ffi::c_uint,
    103 as ::core::ffi::c_int as ::core::ffi::c_uint,
    103 as ::core::ffi::c_int as ::core::ffi::c_uint,
    104 as ::core::ffi::c_int as ::core::ffi::c_uint,
    105 as ::core::ffi::c_int as ::core::ffi::c_uint,
    105 as ::core::ffi::c_int as ::core::ffi::c_uint,
    106 as ::core::ffi::c_int as ::core::ffi::c_uint,
    107 as ::core::ffi::c_int as ::core::ffi::c_uint,
    107 as ::core::ffi::c_int as ::core::ffi::c_uint,
    108 as ::core::ffi::c_int as ::core::ffi::c_uint,
    109 as ::core::ffi::c_int as ::core::ffi::c_uint,
    109 as ::core::ffi::c_int as ::core::ffi::c_uint,
    110 as ::core::ffi::c_int as ::core::ffi::c_uint,
    110 as ::core::ffi::c_int as ::core::ffi::c_uint,
    111 as ::core::ffi::c_int as ::core::ffi::c_uint,
    112 as ::core::ffi::c_int as ::core::ffi::c_uint,
    112 as ::core::ffi::c_int as ::core::ffi::c_uint,
    113 as ::core::ffi::c_int as ::core::ffi::c_uint,
    113 as ::core::ffi::c_int as ::core::ffi::c_uint,
    114 as ::core::ffi::c_int as ::core::ffi::c_uint,
    114 as ::core::ffi::c_int as ::core::ffi::c_uint,
    115 as ::core::ffi::c_int as ::core::ffi::c_uint,
    115 as ::core::ffi::c_int as ::core::ffi::c_uint,
    116 as ::core::ffi::c_int as ::core::ffi::c_uint,
    117 as ::core::ffi::c_int as ::core::ffi::c_uint,
    117 as ::core::ffi::c_int as ::core::ffi::c_uint,
    118 as ::core::ffi::c_int as ::core::ffi::c_uint,
    118 as ::core::ffi::c_int as ::core::ffi::c_uint,
    119 as ::core::ffi::c_int as ::core::ffi::c_uint,
    119 as ::core::ffi::c_int as ::core::ffi::c_uint,
    120 as ::core::ffi::c_int as ::core::ffi::c_uint,
    120 as ::core::ffi::c_int as ::core::ffi::c_uint,
    121 as ::core::ffi::c_int as ::core::ffi::c_uint,
    121 as ::core::ffi::c_int as ::core::ffi::c_uint,
    122 as ::core::ffi::c_int as ::core::ffi::c_uint,
    122 as ::core::ffi::c_int as ::core::ffi::c_uint,
    123 as ::core::ffi::c_int as ::core::ffi::c_uint,
    123 as ::core::ffi::c_int as ::core::ffi::c_uint,
    123 as ::core::ffi::c_int as ::core::ffi::c_uint,
    124 as ::core::ffi::c_int as ::core::ffi::c_uint,
    124 as ::core::ffi::c_int as ::core::ffi::c_uint,
    125 as ::core::ffi::c_int as ::core::ffi::c_uint,
    125 as ::core::ffi::c_int as ::core::ffi::c_uint,
    126 as ::core::ffi::c_int as ::core::ffi::c_uint,
    126 as ::core::ffi::c_int as ::core::ffi::c_uint,
    127 as ::core::ffi::c_int as ::core::ffi::c_uint,
    127 as ::core::ffi::c_int as ::core::ffi::c_uint,
];
    opl_drv_ver__b82i = opl_doom_1_9;
    opl_drv_ver = opl_doom1_1_666;
    voices = [opl_voice_s {
    index: 0,
    op1: 0,
    op2: 0,
    array: 0,
    current_instr: ::core::ptr::null::<genmidi_instr_t>() as *mut genmidi_instr_t,
    current_instr_voice: 0,
    channel: ::core::ptr::null::<opl_channel_data_t>() as *mut opl_channel_data_t,
    key: 0,
    note: 0,
    freq: 0,
    note_volume: 0,
    car_volume: 0,
    mod_volume: 0,
    reg_pan: 0,
    priority: 0,
}; 18];
    channels = [opl_channel_data_t {
    instrument: ::core::ptr::null::<genmidi_instr_t>() as *mut genmidi_instr_t,
    volume: 0,
    volume_base: 0,
    pan: 0,
    bend: 0,
}; 16];
    opl_io_port__b82i = 0x388 as ::core::ffi::c_int;
    music_opl_devices__b82i = [SNDDEVICE_ADLIB, SNDDEVICE_SB];
    music_opl_devices = [SNDDEVICE_NONE; 2];
    DG_music_module__b82i = music_module_t {
    sound_devices: ::core::ptr::null::<snddevice_t>() as *mut snddevice_t,
    num_sound_devices: 0,
    Init: None,
    Shutdown: None,
    SetMusicVolume: None,
    PauseMusic: None,
    ResumeMusic: None,
    RegisterSong: None,
    UnRegisterSong: None,
    PlaySong: None,
    StopSong: None,
    MusicIsPlaying: None,
    Poll: None,
};
    DG_music_module = music_module_t {
    sound_devices: ::core::ptr::null::<snddevice_t>() as *mut snddevice_t,
    num_sound_devices: 0,
    Init: None,
    Shutdown: None,
    SetMusicVolume: None,
    PauseMusic: None,
    ResumeMusic: None,
    RegisterSong: None,
    UnRegisterSong: None,
    PlaySong: None,
    StopSong: None,
    MusicIsPlaying: None,
    Poll: None,
};
    run_static_initializers();
}