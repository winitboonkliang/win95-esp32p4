extern "C" {
    pub type b83_FILE;
    fn b83_stderr() -> *mut FILE;
    fn fprintf(_: *mut FILE, _: *const ::core::ffi::c_char, ...) -> ::core::ffi::c_int;
    fn fopen(_: *const ::core::ffi::c_char, _: *const ::core::ffi::c_char) -> *mut FILE;
    fn fclose(_: *mut FILE) -> ::core::ffi::c_int;
    fn fread(_: *mut ::core::ffi::c_void, _: size_t, _: size_t, _: *mut FILE) -> size_t;
    fn fseek(_: *mut FILE, _: ::core::ffi::c_long, _: ::core::ffi::c_int) -> ::core::ffi::c_int;
    fn fgetc(_: *mut FILE) -> ::core::ffi::c_int;
    fn fmemopen(_: *mut ::core::ffi::c_void, _: size_t, _: *const ::core::ffi::c_char)
        -> *mut FILE;
    fn malloc(_: size_t) -> *mut ::core::ffi::c_void;
    fn realloc(_: *mut ::core::ffi::c_void, _: size_t) -> *mut ::core::ffi::c_void;
    fn free(_: *mut ::core::ffi::c_void);
    fn memset(
        _: *mut ::core::ffi::c_void,
        _: ::core::ffi::c_int,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn memcmp(
        _: *const ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> ::core::ffi::c_int;
    fn b83_assert_fail(
        _: *const ::core::ffi::c_char,
        _: *const ::core::ffi::c_char,
        _: ::core::ffi::c_int,
    );
}
pub type size_t = usize;
pub type FILE = b83_FILE;
pub type uint16_t = u16;
pub type uint8_t = u8;
pub type boolean = ::core::ffi::c_uint;
pub type byte = uint8_t;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_file_s {
    pub header: midi_header_t,
    pub tracks: *mut midi_track_t,
    pub num_tracks: ::core::ffi::c_uint,
    pub buffer: *mut byte,
    pub buffer_size: ::core::ffi::c_uint,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_track_t {
    pub data_len: ::core::ffi::c_uint,
    pub events: *mut midi_event_t,
    pub num_events: ::core::ffi::c_int,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_event_t {
    pub delta_time: ::core::ffi::c_uint,
    pub event_type: midi_event_type_t,
    pub data: C2RustUnnamed,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub union C2RustUnnamed {
    pub channel: midi_channel_event_data_t,
    pub meta: midi_meta_event_data_t,
    pub sysex: midi_sysex_event_data_t,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_sysex_event_data_t {
    pub length: ::core::ffi::c_uint,
    pub data: *mut byte,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_meta_event_data_t {
    pub type_0: ::core::ffi::c_uint,
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
#[derive(Copy, Clone)]
#[repr(C, packed)]
pub struct midi_header_t {
    pub chunk_header: chunk_header_t,
    pub format_type: ::core::ffi::c_ushort,
    pub num_tracks: ::core::ffi::c_ushort,
    pub time_division: ::core::ffi::c_ushort,
}
#[derive(Copy, Clone)]
#[repr(C, packed)]
pub struct chunk_header_t {
    pub chunk_id: [byte; 4],
    pub chunk_size: ::core::ffi::c_uint,
}
pub type midi_file_t = midi_file_s;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct midi_track_iter_s {
    pub track: *mut midi_track_t,
    pub position: ::core::ffi::c_uint,
}
pub type midi_track_iter_t = midi_track_iter_s;
pub type C2RustUnnamed_0 = ::core::ffi::c_uint;
pub const MIDI_META_SEQUENCER_SPECIFIC: C2RustUnnamed_0 = 127;
pub const MIDI_META_KEY_SIGNATURE: C2RustUnnamed_0 = 89;
pub const MIDI_META_TIME_SIGNATURE: C2RustUnnamed_0 = 88;
pub const MIDI_META_SMPTE_OFFSET: C2RustUnnamed_0 = 84;
pub const MIDI_META_SET_TEMPO: C2RustUnnamed_0 = 81;
pub const MIDI_META_END_OF_TRACK: C2RustUnnamed_0 = 47;
pub const MIDI_META_CHANNEL_PREFIX: C2RustUnnamed_0 = 32;
pub const MIDI_META_CUE_POINT: C2RustUnnamed_0 = 7;
pub const MIDI_META_MARKER: C2RustUnnamed_0 = 6;
pub const MIDI_META_LYRICS: C2RustUnnamed_0 = 5;
pub const MIDI_META_INSTR_NAME: C2RustUnnamed_0 = 4;
pub const MIDI_META_TRACK_NAME: C2RustUnnamed_0 = 3;
pub const MIDI_META_COPYRIGHT: C2RustUnnamed_0 = 2;
pub const MIDI_META_TEXT: C2RustUnnamed_0 = 1;
pub const MIDI_META_SEQUENCE_NUMBER: C2RustUnnamed_0 = 0;
pub const NULL: *mut ::core::ffi::c_void = ::core::ptr::null_mut::<::core::ffi::c_void>();
pub const EOF: ::core::ffi::c_int = -(1 as ::core::ffi::c_int);
pub const SEEK_CUR: ::core::ffi::c_int = 1 as ::core::ffi::c_int;
pub const true_0: ::core::ffi::c_int = 1 as ::core::ffi::c_int;
pub const false_0: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
pub const HEADER_CHUNK_ID: [::core::ffi::c_char; 5] =
    unsafe { ::core::mem::transmute::<[u8; 5], [::core::ffi::c_char; 5]>(*b"MThd\0") };
pub const TRACK_CHUNK_ID: [::core::ffi::c_char; 5] =
    unsafe { ::core::mem::transmute::<[u8; 5], [::core::ffi::c_char; 5]>(*b"MTrk\0") };
#[optimize(size)]
unsafe extern "C" fn CheckChunkHeader(
    mut chunk: *mut chunk_header_t,
    mut expected_id: *mut ::core::ffi::c_char,
) -> boolean {
    let mut result: boolean = 0;
    result = (memcmp(
        &raw mut (*chunk).chunk_id as *mut byte as *mut ::core::ffi::c_char
            as *const ::core::ffi::c_void,
        expected_id as *const ::core::ffi::c_void,
        4 as size_t,
    ) == 0 as ::core::ffi::c_int) as ::core::ffi::c_int as boolean;
    if result == 0 {
        fprintf(
            b83_stderr(),
            b"CheckChunkHeader: Expected '%s' chunk header, got '%c%c%c%c'\n\0" as *const u8
                as *const ::core::ffi::c_char,
            expected_id,
            (*chunk).chunk_id[0 as ::core::ffi::c_int as usize] as ::core::ffi::c_int,
            (*chunk).chunk_id[1 as ::core::ffi::c_int as usize] as ::core::ffi::c_int,
            (*chunk).chunk_id[2 as ::core::ffi::c_int as usize] as ::core::ffi::c_int,
            (*chunk).chunk_id[3 as ::core::ffi::c_int as usize] as ::core::ffi::c_int,
        );
    }
    return result;
}
#[optimize(size)]
unsafe extern "C" fn ReadByte(mut result: *mut byte, mut stream: *mut FILE) -> boolean {
    let mut c: ::core::ffi::c_int = 0;
    c = fgetc(stream);
    if c == EOF {
        fprintf(
            b83_stderr(),
            b"ReadByte: Unexpected end of file\n\0" as *const u8 as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    } else {
        *result = c as byte;
        return true_0 as boolean;
    };
}
#[optimize(size)]
unsafe extern "C" fn ReadVariableLength(
    mut result: *mut ::core::ffi::c_uint,
    mut stream: *mut FILE,
) -> boolean {
    let mut i: ::core::ffi::c_int = 0;
    let mut b: byte = 0 as byte;
    *result = 0 as ::core::ffi::c_uint;
    i = 0 as ::core::ffi::c_int;
    while i < 4 as ::core::ffi::c_int {
        if ReadByte(&raw mut b, stream) == 0 {
            fprintf(
                b83_stderr(),
                b"ReadVariableLength: Error while reading variable-length value\n\0" as *const u8
                    as *const ::core::ffi::c_char,
            );
            return false_0 as boolean;
        }
        *result <<= 7 as ::core::ffi::c_int;
        *result |= (b as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as ::core::ffi::c_uint;
        if b as ::core::ffi::c_int & 0x80 as ::core::ffi::c_int == 0 as ::core::ffi::c_int {
            return true_0 as boolean;
        }
        i += 1;
    }
    fprintf(
        b83_stderr(),
        b"ReadVariableLength: Variable-length value too long: maximum of four bytes\n\0"
            as *const u8 as *const ::core::ffi::c_char,
    );
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn ReadByteSequence(
    mut num_bytes: ::core::ffi::c_uint,
    mut stream: *mut FILE,
) -> *mut ::core::ffi::c_void {
    let mut i: ::core::ffi::c_uint = 0;
    let mut result: *mut byte = ::core::ptr::null_mut::<byte>();
    result = malloc(num_bytes.wrapping_add(1 as ::core::ffi::c_uint) as size_t) as *mut byte;
    if result.is_null() {
        fprintf(
            b83_stderr(),
            b"ReadByteSequence: Failed to allocate buffer\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return NULL;
    }
    i = 0 as ::core::ffi::c_uint;
    while i < num_bytes {
        if ReadByte(result.offset(i as isize) as *mut byte, stream) == 0 {
            fprintf(
                b83_stderr(),
                b"ReadByteSequence: Error while reading byte %u\n\0" as *const u8
                    as *const ::core::ffi::c_char,
                i,
            );
            free(result as *mut ::core::ffi::c_void);
            return NULL;
        }
        i = i.wrapping_add(1);
    }
    return result as *mut ::core::ffi::c_void;
}
#[optimize(size)]
unsafe extern "C" fn ReadChannelEvent(
    mut event: *mut midi_event_t,
    mut event_type: byte,
    mut two_param: boolean,
    mut stream: *mut FILE,
) -> boolean {
    let mut b: byte = 0 as byte;
    (*event).event_type =
        (event_type as ::core::ffi::c_int & 0xf0 as ::core::ffi::c_int) as midi_event_type_t;
    (*event).data.channel.channel =
        (event_type as ::core::ffi::c_int & 0xf as ::core::ffi::c_int) as ::core::ffi::c_uint;
    if ReadByte(&raw mut b, stream) == 0 {
        fprintf(
            b83_stderr(),
            b"ReadChannelEvent: Error while reading channel event parameters\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    (*event).data.channel.param1 = b as ::core::ffi::c_uint;
    if two_param != 0 {
        if ReadByte(&raw mut b, stream) == 0 {
            fprintf(
                b83_stderr(),
                b"ReadChannelEvent: Error while reading channel event parameters\n\0" as *const u8
                    as *const ::core::ffi::c_char,
            );
            return false_0 as boolean;
        }
        (*event).data.channel.param2 = b as ::core::ffi::c_uint;
    }
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn ReadSysExEvent(
    mut event: *mut midi_event_t,
    mut event_type: ::core::ffi::c_int,
    mut stream: *mut FILE,
) -> boolean {
    (*event).event_type = event_type as midi_event_type_t;
    if ReadVariableLength(&raw mut (*event).data.sysex.length, stream) == 0 {
        fprintf(
            b83_stderr(),
            b"ReadSysExEvent: Failed to read length of SysEx block\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    (*event).data.sysex.data = ReadByteSequence((*event).data.sysex.length, stream) as *mut byte;
    if (*event).data.sysex.data.is_null() {
        fprintf(
            b83_stderr(),
            b"ReadSysExEvent: Failed while reading SysEx event\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn ReadMetaEvent(mut event: *mut midi_event_t, mut stream: *mut FILE) -> boolean {
    let mut b: byte = 0 as byte;
    (*event).event_type = MIDI_EVENT_META;
    if ReadByte(&raw mut b, stream) == 0 {
        fprintf(
            b83_stderr(),
            b"ReadMetaEvent: Failed to read meta event type\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    (*event).data.meta.type_0 = b as ::core::ffi::c_uint;
    if ReadVariableLength(&raw mut (*event).data.meta.length, stream) == 0 {
        fprintf(
            b83_stderr(),
            b"ReadSysExEvent: Failed to read length of SysEx block\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    (*event).data.meta.data = ReadByteSequence((*event).data.meta.length, stream) as *mut byte;
    if (*event).data.meta.data.is_null() {
        fprintf(
            b83_stderr(),
            b"ReadSysExEvent: Failed while reading SysEx event\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn ReadEvent(
    mut event: *mut midi_event_t,
    mut last_event_type: *mut ::core::ffi::c_uint,
    mut stream: *mut FILE,
) -> boolean {
    let mut event_type: byte = 0 as byte;
    if ReadVariableLength(&raw mut (*event).delta_time, stream) == 0 {
        fprintf(
            b83_stderr(),
            b"ReadEvent: Failed to read event timestamp\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    if ReadByte(&raw mut event_type, stream) == 0 {
        fprintf(
            b83_stderr(),
            b"ReadEvent: Failed to read event type\n\0" as *const u8 as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    if event_type as ::core::ffi::c_int & 0x80 as ::core::ffi::c_int == 0 as ::core::ffi::c_int {
        event_type = *last_event_type as byte;
        if fseek(
            stream,
            -(1 as ::core::ffi::c_int) as ::core::ffi::c_long,
            SEEK_CUR,
        ) < 0 as ::core::ffi::c_int
        {
            fprintf(
                b83_stderr(),
                b"ReadEvent: Unable to seek in stream\n\0" as *const u8
                    as *const ::core::ffi::c_char,
            );
            return false_0 as boolean;
        }
    } else {
        *last_event_type = event_type as ::core::ffi::c_uint;
    }
    match event_type as ::core::ffi::c_int & 0xf0 as ::core::ffi::c_int {
        128 | 144 | 160 | 176 | 224 => {
            return ReadChannelEvent(event, event_type, true_0 as boolean, stream);
        }
        192 | 208 => {
            return ReadChannelEvent(event, event_type, false_0 as boolean, stream);
        }
        _ => {}
    }
    match event_type as ::core::ffi::c_int {
        240 | 247 => {
            return ReadSysExEvent(event, event_type as ::core::ffi::c_int, stream);
        }
        255 => return ReadMetaEvent(event, stream),
        _ => {}
    }
    fprintf(
        b83_stderr(),
        b"ReadEvent: Unknown MIDI event type: 0x%x\n\0" as *const u8 as *const ::core::ffi::c_char,
        event_type as ::core::ffi::c_int,
    );
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn FreeEvent(mut event: *mut midi_event_t) {
    match (*event).event_type as ::core::ffi::c_uint {
        240 | 247 => {
            free((*event).data.sysex.data as *mut ::core::ffi::c_void);
        }
        255 => {
            free((*event).data.meta.data as *mut ::core::ffi::c_void);
        }
        _ => {}
    };
}
#[optimize(size)]
unsafe extern "C" fn ReadTrackHeader(
    mut track: *mut midi_track_t,
    mut stream: *mut FILE,
) -> boolean {
    let mut records_read: size_t = 0;
    let mut chunk_header: chunk_header_t = chunk_header_t {
        chunk_id: [0; 4],
        chunk_size: 0,
    };
    records_read = fread(
        &raw mut chunk_header as *mut ::core::ffi::c_void,
        ::core::mem::size_of::<chunk_header_t>() as size_t,
        1 as size_t,
        stream,
    );
    if records_read < 1 as size_t {
        return false_0 as boolean;
    }
    if CheckChunkHeader(
        &raw mut chunk_header,
        TRACK_CHUNK_ID.as_ptr() as *mut ::core::ffi::c_char,
    ) == 0
    {
        return false_0 as boolean;
    }
    (*track).data_len = chunk_header.chunk_size.swap_bytes();
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn ReadTrack(mut track: *mut midi_track_t, mut stream: *mut FILE) -> boolean {
    let mut new_events: *mut midi_event_t = ::core::ptr::null_mut::<midi_event_t>();
    let mut event: *mut midi_event_t = ::core::ptr::null_mut::<midi_event_t>();
    let mut last_event_type: ::core::ffi::c_uint = 0;
    (*track).num_events = 0 as ::core::ffi::c_int;
    (*track).events = ::core::ptr::null_mut::<midi_event_t>();
    if ReadTrackHeader(track, stream) == 0 {
        return false_0 as boolean;
    }
    last_event_type = 0 as ::core::ffi::c_uint;
    loop {
        new_events = realloc(
            (*track).events as *mut ::core::ffi::c_void,
            (::core::mem::size_of::<midi_event_t>() as size_t)
                .wrapping_mul(((*track).num_events + 1 as ::core::ffi::c_int) as size_t),
        ) as *mut midi_event_t;
        if new_events.is_null() {
            return false_0 as boolean;
        }
        (*track).events = new_events;
        event = (*track).events.offset((*track).num_events as isize) as *mut midi_event_t;
        if ReadEvent(event, &raw mut last_event_type, stream) == 0 {
            return false_0 as boolean;
        }
        (*track).num_events += 1;
        if (*event).event_type as ::core::ffi::c_uint
            == MIDI_EVENT_META as ::core::ffi::c_int as ::core::ffi::c_uint
            && (*event).data.meta.type_0
                == MIDI_META_END_OF_TRACK as ::core::ffi::c_int as ::core::ffi::c_uint
        {
            break;
        }
    }
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn FreeTrack(mut track: *mut midi_track_t) {
    let mut i: ::core::ffi::c_uint = 0;
    i = 0 as ::core::ffi::c_uint;
    while i < (*track).num_events as ::core::ffi::c_uint {
        FreeEvent((*track).events.offset(i as isize) as *mut midi_event_t);
        i = i.wrapping_add(1);
    }
    free((*track).events as *mut ::core::ffi::c_void);
}
#[optimize(size)]
unsafe extern "C" fn ReadAllTracks(mut file: *mut midi_file_t, mut stream: *mut FILE) -> boolean {
    let mut i: ::core::ffi::c_uint = 0;
    (*file).tracks = malloc(
        (::core::mem::size_of::<midi_track_t>() as size_t)
            .wrapping_mul((*file).num_tracks as size_t),
    ) as *mut midi_track_t;
    if (*file).tracks.is_null() {
        return false_0 as boolean;
    }
    memset(
        (*file).tracks as *mut ::core::ffi::c_void,
        0 as ::core::ffi::c_int,
        (::core::mem::size_of::<midi_track_t>() as size_t)
            .wrapping_mul((*file).num_tracks as size_t),
    );
    i = 0 as ::core::ffi::c_uint;
    while i < (*file).num_tracks {
        if ReadTrack(
            (*file).tracks.offset(i as isize) as *mut midi_track_t,
            stream,
        ) == 0
        {
            return false_0 as boolean;
        }
        i = i.wrapping_add(1);
    }
    return true_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn ReadFileHeader(mut file: *mut midi_file_t, mut stream: *mut FILE) -> boolean {
    let mut records_read: size_t = 0;
    let mut format_type: ::core::ffi::c_uint = 0;
    records_read = fread(
        &raw mut (*file).header as *mut ::core::ffi::c_void,
        ::core::mem::size_of::<midi_header_t>() as size_t,
        1 as size_t,
        stream,
    );
    if records_read < 1 as size_t {
        return false_0 as boolean;
    }
    if CheckChunkHeader(
        &raw mut (*file).header.chunk_header,
        HEADER_CHUNK_ID.as_ptr() as *mut ::core::ffi::c_char,
    ) == 0
        || (*file).header.chunk_header.chunk_size.swap_bytes() != 6 as ::core::ffi::c_uint
    {
        fprintf(
            b83_stderr(),
            b"ReadFileHeader: Invalid MIDI chunk header! chunk_size=%i\n\0" as *const u8
                as *const ::core::ffi::c_char,
            (*file).header.chunk_header.chunk_size.swap_bytes(),
        );
        return false_0 as boolean;
    }
    format_type =
        ((*file).header.format_type as uint16_t).swap_bytes() as uint16_t as ::core::ffi::c_uint;
    (*file).num_tracks =
        ((*file).header.num_tracks as uint16_t).swap_bytes() as uint16_t as ::core::ffi::c_uint;
    if format_type != 0 as ::core::ffi::c_uint && format_type != 1 as ::core::ffi::c_uint
        || (*file).num_tracks < 1 as ::core::ffi::c_uint
    {
        fprintf(
            b83_stderr(),
            b"ReadFileHeader: Only type 0/1 MIDI files supported!\n\0" as *const u8
                as *const ::core::ffi::c_char,
        );
        return false_0 as boolean;
    }
    return true_0 as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_FreeFile(mut file: *mut midi_file_t) {
    let mut i: ::core::ffi::c_int = 0;
    if !(*file).tracks.is_null() {
        i = 0 as ::core::ffi::c_int;
        while (i as ::core::ffi::c_uint) < (*file).num_tracks {
            FreeTrack((*file).tracks.offset(i as isize) as *mut midi_track_t);
            i += 1;
        }
        free((*file).tracks as *mut ::core::ffi::c_void);
    }
    free(file as *mut ::core::ffi::c_void);
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_LoadFile(mut filename: *mut ::core::ffi::c_char) -> *mut midi_file_t {
    let mut file: *mut midi_file_t = ::core::ptr::null_mut::<midi_file_t>();
    let mut stream: *mut FILE = ::core::ptr::null_mut::<FILE>();
    file = malloc(::core::mem::size_of::<midi_file_t>() as size_t) as *mut midi_file_t;
    if file.is_null() {
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    (*file).tracks = ::core::ptr::null_mut::<midi_track_t>();
    (*file).num_tracks = 0 as ::core::ffi::c_uint;
    (*file).buffer = ::core::ptr::null_mut::<byte>();
    (*file).buffer_size = 0 as ::core::ffi::c_uint;
    stream = fopen(filename, b"rb\0" as *const u8 as *const ::core::ffi::c_char);
    if stream.is_null() {
        fprintf(
            b83_stderr(),
            b"MIDI_LoadFile: Failed to open '%s'\n\0" as *const u8 as *const ::core::ffi::c_char,
            filename,
        );
        MIDI_FreeFile(file);
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    if ReadFileHeader(file, stream) == 0 {
        fclose(stream);
        MIDI_FreeFile(file);
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    if ReadAllTracks(file, stream) == 0 {
        fclose(stream);
        MIDI_FreeFile(file);
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    fclose(stream);
    return file;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_LoadFileMem(
    mut data: *mut byte,
    mut len: size_t,
) -> *mut midi_file_t {
    let mut file: *mut midi_file_t = ::core::ptr::null_mut::<midi_file_t>();
    let mut stream: *mut FILE = ::core::ptr::null_mut::<FILE>();
    file = malloc(::core::mem::size_of::<midi_file_t>() as size_t) as *mut midi_file_t;
    if file.is_null() {
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    (*file).tracks = ::core::ptr::null_mut::<midi_track_t>();
    (*file).num_tracks = 0 as ::core::ffi::c_uint;
    (*file).buffer = ::core::ptr::null_mut::<byte>();
    (*file).buffer_size = 0 as ::core::ffi::c_uint;
    stream = fmemopen(
        data as *mut ::core::ffi::c_void,
        len,
        b"rb\0" as *const u8 as *const ::core::ffi::c_char,
    );
    if stream.is_null() {
        fprintf(
            b83_stderr(),
            b"MIDI_LoadFileMem: fmemopen failed\n\0" as *const u8 as *const ::core::ffi::c_char,
        );
        MIDI_FreeFile(file);
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    if ReadFileHeader(file, stream) == 0 || ReadAllTracks(file, stream) == 0 {
        fclose(stream);
        MIDI_FreeFile(file);
        return ::core::ptr::null_mut::<midi_file_t>();
    }
    fclose(stream);
    return file;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_NumTracks(mut file: *mut midi_file_t) -> ::core::ffi::c_uint {
    return (*file).num_tracks;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_IterateTrack(
    mut file: *mut midi_file_t,
    mut track: ::core::ffi::c_uint,
) -> *mut midi_track_iter_t {
    let mut iter: *mut midi_track_iter_t = ::core::ptr::null_mut::<midi_track_iter_t>();
    if track < (*file).num_tracks {
    } else {
        b83_assert_fail(
            b"track < file->num_tracks\0" as *const u8 as *const ::core::ffi::c_char,
            b"esp/components/doomp4/dg/midifile.c\0"
                as *const u8 as *const ::core::ffi::c_char,
            697 as ::core::ffi::c_int,
        );
    };
    iter = malloc(::core::mem::size_of::<midi_track_iter_t>() as size_t) as *mut midi_track_iter_t;
    (*iter).track = (*file).tracks.offset(track as isize) as *mut midi_track_t;
    (*iter).position = 0 as ::core::ffi::c_uint;
    return iter;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_FreeIterator(mut iter: *mut midi_track_iter_t) {
    free(iter as *mut ::core::ffi::c_void);
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_GetDeltaTime(
    mut iter: *mut midi_track_iter_t,
) -> ::core::ffi::c_uint {
    if (*iter).position < (*(*iter).track).num_events as ::core::ffi::c_uint {
        let mut next_event: *mut midi_event_t = ::core::ptr::null_mut::<midi_event_t>();
        next_event = (*(*iter).track).events.offset((*iter).position as isize) as *mut midi_event_t;
        return (*next_event).delta_time;
    } else {
        return 0 as ::core::ffi::c_uint;
    };
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_GetNextEvent(
    mut iter: *mut midi_track_iter_t,
    mut event: *mut *mut midi_event_t,
) -> ::core::ffi::c_int {
    if (*iter).position < (*(*iter).track).num_events as ::core::ffi::c_uint {
        *event = (*(*iter).track).events.offset((*iter).position as isize) as *mut midi_event_t;
        (*iter).position = (*iter).position.wrapping_add(1);
        return 1 as ::core::ffi::c_int;
    } else {
        return 0 as ::core::ffi::c_int;
    };
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_GetFileTimeDivision(
    mut file: *mut midi_file_t,
) -> ::core::ffi::c_uint {
    let mut result: ::core::ffi::c_short =
        ((*file).header.time_division as uint16_t).swap_bytes() as uint16_t as ::core::ffi::c_short;
    if (result as ::core::ffi::c_int) < 0 as ::core::ffi::c_int {
        return (-(result as ::core::ffi::c_int / 256 as ::core::ffi::c_int)
            * (result as ::core::ffi::c_int & 0xff as ::core::ffi::c_int))
            as ::core::ffi::c_uint;
    } else {
        return result as ::core::ffi::c_uint;
    };
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn MIDI_RestartIterator(mut iter: *mut midi_track_iter_t) {
    (*iter).position = 0 as ::core::ffi::c_uint;
}
