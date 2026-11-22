extern "C" {
    pub type _MEMFILE;
    fn memcpy(
        _: *mut ::core::ffi::c_void,
        _: *const ::core::ffi::c_void,
        _: size_t,
    ) -> *mut ::core::ffi::c_void;
    fn mem_fread(
        buf: *mut ::core::ffi::c_void,
        size: size_t,
        nmemb: size_t,
        stream: *mut MEMFILE,
    ) -> size_t;
    fn mem_fwrite(
        ptr: *const ::core::ffi::c_void,
        size: size_t,
        nmemb: size_t,
        stream: *mut MEMFILE,
    ) -> size_t;
    fn mem_fseek(
        stream: *mut MEMFILE,
        offset: ::core::ffi::c_long,
        whence: mem_rel_t,
    ) -> ::core::ffi::c_int;
}
pub type size_t = usize;
pub type uint8_t = u8;
pub type boolean = ::core::ffi::c_uint;
pub type byte = uint8_t;
pub type MEMFILE = _MEMFILE;
pub type mem_rel_t = ::core::ffi::c_uint;
pub const MEM_SEEK_END: mem_rel_t = 2;
pub const MEM_SEEK_CUR: mem_rel_t = 1;
pub const MEM_SEEK_SET: mem_rel_t = 0;
pub const mus_scoreend: musevent = 96;
pub const midi_changecontroller: C2RustUnnamed = 176;
pub const midi_changepatch: C2RustUnnamed = 192;
pub const mus_changecontroller: musevent = 64;
pub const mus_systemevent: musevent = 48;
pub const midi_pitchwheel: C2RustUnnamed = 224;
pub const mus_pitchwheel: musevent = 32;
pub const midi_presskey: C2RustUnnamed = 144;
pub const mus_presskey: musevent = 16;
pub const midi_releasekey: C2RustUnnamed = 128;
pub const mus_releasekey: musevent = 0;
pub type musevent = ::core::ffi::c_uint;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct musheader {
    pub id: [byte; 4],
    pub scorelength: ::core::ffi::c_ushort,
    pub scorestart: ::core::ffi::c_ushort,
    pub primarychannels: ::core::ffi::c_ushort,
    pub secondarychannels: ::core::ffi::c_ushort,
    pub instrumentcount: ::core::ffi::c_ushort,
}
pub type C2RustUnnamed = ::core::ffi::c_uint;
pub const midi_aftertouchchannel: C2RustUnnamed = 208;
pub const midi_aftertouchkey: C2RustUnnamed = 160;
pub const true_0: ::core::ffi::c_int = 1 as ::core::ffi::c_int;
pub const false_0: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
pub const NUM_CHANNELS: ::core::ffi::c_int = 16 as ::core::ffi::c_int;
pub const MIDI_PERCUSSION_CHAN: ::core::ffi::c_int = 9 as ::core::ffi::c_int;
pub const MUS_PERCUSSION_CHAN: ::core::ffi::c_int = 15 as ::core::ffi::c_int;
static mut midiheader: [byte; 22] = unsafe { ::core::mem::zeroed() };
static mut channelvelocities__b82i: [byte; 16] = unsafe { ::core::mem::zeroed() };
static mut channelvelocities: [byte; 16] = [0; 16];
static mut queuedtime__b82i: ::core::ffi::c_uint = 0 as ::core::ffi::c_uint;
static mut queuedtime: ::core::ffi::c_uint = 0;
static mut tracksize: ::core::ffi::c_uint = 0;
static mut controller_map: [byte; 15] = unsafe { ::core::mem::zeroed() };
static mut channel_map: [::core::ffi::c_int; 16] = [0; 16];
#[optimize(size)]
unsafe extern "C" fn WriteTime(
    mut time: ::core::ffi::c_uint,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut buffer: ::core::ffi::c_uint = time & 0x7f as ::core::ffi::c_uint;
    let mut writeval: byte = 0;
    loop {
        time >>= 7 as ::core::ffi::c_int;
        if !(time != 0 as ::core::ffi::c_uint) {
            break;
        }
        buffer <<= 8 as ::core::ffi::c_int;
        buffer |= time & 0x7f as ::core::ffi::c_uint | 0x80 as ::core::ffi::c_uint;
    }
    loop {
        writeval = (buffer & 0xff as ::core::ffi::c_uint) as byte;
        if mem_fwrite(
            &raw mut writeval as *const ::core::ffi::c_void,
            1 as size_t,
            1 as size_t,
            midioutput,
        ) != 1 as size_t
        {
            return true_0 as boolean;
        }
        tracksize = tracksize.wrapping_add(1);
        if buffer & 0x80 as ::core::ffi::c_uint != 0 as ::core::ffi::c_uint {
            buffer >>= 8 as ::core::ffi::c_int;
        } else {
            queuedtime = 0 as ::core::ffi::c_uint;
            return false_0 as boolean;
        }
    }
}
#[optimize(size)]
unsafe extern "C" fn WriteEndTrack(mut midioutput: *mut MEMFILE) -> boolean {
    let mut endtrack: [byte; 3] = [
        0xff as ::core::ffi::c_int as byte,
        0x2f as ::core::ffi::c_int as byte,
        0 as ::core::ffi::c_int as byte,
    ];
    if WriteTime(queuedtime, midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fwrite(
        &raw mut endtrack as *mut byte as *const ::core::ffi::c_void,
        1 as size_t,
        3 as size_t,
        midioutput,
    ) != 3 as size_t
    {
        return true_0 as boolean;
    }
    tracksize = tracksize.wrapping_add(3 as ::core::ffi::c_uint);
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn WritePressKey(
    mut channel: byte,
    mut key: byte,
    mut velocity: byte,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut working: byte =
        (midi_presskey as ::core::ffi::c_int | channel as ::core::ffi::c_int) as byte;
    if WriteTime(queuedtime, midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (key as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (velocity as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    tracksize = tracksize.wrapping_add(3 as ::core::ffi::c_uint);
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn WriteReleaseKey(
    mut channel: byte,
    mut key: byte,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut working: byte =
        (midi_releasekey as ::core::ffi::c_int | channel as ::core::ffi::c_int) as byte;
    if WriteTime(queuedtime, midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (key as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = 0 as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    tracksize = tracksize.wrapping_add(3 as ::core::ffi::c_uint);
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn WritePitchWheel(
    mut channel: byte,
    mut wheel: ::core::ffi::c_short,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut working: byte =
        (midi_pitchwheel as ::core::ffi::c_int | channel as ::core::ffi::c_int) as byte;
    if WriteTime(queuedtime, midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (wheel as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (wheel as ::core::ffi::c_int >> 7 as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int)
        as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    tracksize = tracksize.wrapping_add(3 as ::core::ffi::c_uint);
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn WriteChangePatch(
    mut channel: byte,
    mut patch: byte,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut working: byte =
        (midi_changepatch as ::core::ffi::c_int | channel as ::core::ffi::c_int) as byte;
    if WriteTime(queuedtime, midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (patch as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    tracksize = tracksize.wrapping_add(2 as ::core::ffi::c_uint);
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn WriteChangeController_Valued(
    mut channel: byte,
    mut control: byte,
    mut value: byte,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut working: byte =
        (midi_changecontroller as ::core::ffi::c_int | channel as ::core::ffi::c_int) as byte;
    if WriteTime(queuedtime, midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = (control as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int) as byte;
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    working = value;
    if working as ::core::ffi::c_int & 0x80 as ::core::ffi::c_int != 0 {
        working = 0x7f as byte;
    }
    if mem_fwrite(
        &raw mut working as *const ::core::ffi::c_void,
        1 as size_t,
        1 as size_t,
        midioutput,
    ) != 1 as size_t
    {
        return true_0 as boolean;
    }
    tracksize = tracksize.wrapping_add(3 as ::core::ffi::c_uint);
    return false_0 as boolean;
}
#[optimize(size)]
unsafe extern "C" fn WriteChangeController_Valueless(
    mut channel: byte,
    mut control: byte,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    return WriteChangeController_Valued(channel, control, 0 as byte, midioutput);
}
#[optimize(size)]
unsafe extern "C" fn AllocateMIDIChannel() -> ::core::ffi::c_int {
    let mut result: ::core::ffi::c_int = 0;
    let mut max: ::core::ffi::c_int = 0;
    let mut i: ::core::ffi::c_int = 0;
    max = -(1 as ::core::ffi::c_int);
    i = 0 as ::core::ffi::c_int;
    while i < NUM_CHANNELS {
        if channel_map[i as usize] > max {
            max = channel_map[i as usize];
        }
        i += 1;
    }
    result = max + 1 as ::core::ffi::c_int;
    if result == MIDI_PERCUSSION_CHAN {
        result += 1;
    }
    return result;
}
#[optimize(size)]
unsafe extern "C" fn GetMIDIChannel(
    mut mus_channel: ::core::ffi::c_int,
    mut midioutput: *mut MEMFILE,
) -> ::core::ffi::c_int {
    if mus_channel == MUS_PERCUSSION_CHAN {
        return MIDI_PERCUSSION_CHAN;
    } else {
        if channel_map[mus_channel as usize] == -(1 as ::core::ffi::c_int) {
            channel_map[mus_channel as usize] = AllocateMIDIChannel();
            WriteChangeController_Valueless(
                channel_map[mus_channel as usize] as byte,
                0x7b as byte,
                midioutput,
            );
        }
        return channel_map[mus_channel as usize];
    };
}
#[optimize(size)]
unsafe extern "C" fn ReadMusHeader(mut file: *mut MEMFILE, mut header: *mut musheader) -> boolean {
    let mut result: boolean = 0;
    result = (mem_fread(
        &raw mut (*header).id as *mut ::core::ffi::c_void,
        ::core::mem::size_of::<byte>() as size_t,
        4 as size_t,
        file,
    ) == 4 as size_t
        && mem_fread(
            &raw mut (*header).scorelength as *mut ::core::ffi::c_void,
            ::core::mem::size_of::<::core::ffi::c_short>() as size_t,
            1 as size_t,
            file,
        ) == 1 as size_t
        && mem_fread(
            &raw mut (*header).scorestart as *mut ::core::ffi::c_void,
            ::core::mem::size_of::<::core::ffi::c_short>() as size_t,
            1 as size_t,
            file,
        ) == 1 as size_t
        && mem_fread(
            &raw mut (*header).primarychannels as *mut ::core::ffi::c_void,
            ::core::mem::size_of::<::core::ffi::c_short>() as size_t,
            1 as size_t,
            file,
        ) == 1 as size_t
        && mem_fread(
            &raw mut (*header).secondarychannels as *mut ::core::ffi::c_void,
            ::core::mem::size_of::<::core::ffi::c_short>() as size_t,
            1 as size_t,
            file,
        ) == 1 as size_t
        && mem_fread(
            &raw mut (*header).instrumentcount as *mut ::core::ffi::c_void,
            ::core::mem::size_of::<::core::ffi::c_short>() as size_t,
            1 as size_t,
            file,
        ) == 1 as size_t) as ::core::ffi::c_int as boolean;
    if result != 0 {
        (*header).scorelength =
            (*header).scorelength as ::core::ffi::c_short as ::core::ffi::c_ushort;
        (*header).scorestart =
            (*header).scorestart as ::core::ffi::c_short as ::core::ffi::c_ushort;
        (*header).primarychannels =
            (*header).primarychannels as ::core::ffi::c_short as ::core::ffi::c_ushort;
        (*header).secondarychannels =
            (*header).secondarychannels as ::core::ffi::c_short as ::core::ffi::c_ushort;
        (*header).instrumentcount =
            (*header).instrumentcount as ::core::ffi::c_short as ::core::ffi::c_ushort;
    }
    return result;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn mus2mid(
    mut musinput: *mut MEMFILE,
    mut midioutput: *mut MEMFILE,
) -> boolean {
    let mut musfileheader: musheader = musheader {
        id: [0; 4],
        scorelength: 0,
        scorestart: 0,
        primarychannels: 0,
        secondarychannels: 0,
        instrumentcount: 0,
    };
    let mut eventdescriptor: byte = 0;
    let mut channel: ::core::ffi::c_int = 0;
    let mut event: musevent = mus_releasekey;
    let mut key: byte = 0;
    let mut controllernumber: byte = 0;
    let mut controllervalue: byte = 0;
    let mut tracksizebuffer: [byte; 4] = [0; 4];
    let mut hitscoreend: ::core::ffi::c_int = 0 as ::core::ffi::c_int;
    let mut working: byte = 0;
    let mut timedelay: ::core::ffi::c_uint = 0;
    channel = 0 as ::core::ffi::c_int;
    while channel < NUM_CHANNELS {
        channel_map[channel as usize] = -(1 as ::core::ffi::c_int);
        channel += 1;
    }
    if ReadMusHeader(musinput, &raw mut musfileheader) == 0 {
        return true_0 as boolean;
    }
    if musfileheader.id[0 as ::core::ffi::c_int as usize] as ::core::ffi::c_int != 'M' as i32
        || musfileheader.id[1 as ::core::ffi::c_int as usize] as ::core::ffi::c_int != 'U' as i32
        || musfileheader.id[2 as ::core::ffi::c_int as usize] as ::core::ffi::c_int != 'S' as i32
        || musfileheader.id[3 as ::core::ffi::c_int as usize] as ::core::ffi::c_int
            != 0x1a as ::core::ffi::c_int
    {
        return true_0 as boolean;
    }
    if mem_fseek(
        musinput,
        musfileheader.scorestart as ::core::ffi::c_long,
        MEM_SEEK_SET,
    ) != 0 as ::core::ffi::c_int
    {
        return true_0 as boolean;
    }
    mem_fwrite(
        &raw const midiheader as *const byte as *const ::core::ffi::c_void,
        1 as size_t,
        ::core::mem::size_of::<[byte; 22]>() as size_t,
        midioutput,
    );
    tracksize = 0 as ::core::ffi::c_uint;
    while hitscoreend == 0 {
        while hitscoreend == 0 {
            if mem_fread(
                &raw mut eventdescriptor as *mut ::core::ffi::c_void,
                1 as size_t,
                1 as size_t,
                musinput,
            ) != 1 as size_t
            {
                return true_0 as boolean;
            }
            channel = GetMIDIChannel(
                eventdescriptor as ::core::ffi::c_int & 0xf as ::core::ffi::c_int,
                midioutput,
            );
            event =
                (eventdescriptor as ::core::ffi::c_int & 0x70 as ::core::ffi::c_int) as musevent;
            match event as ::core::ffi::c_uint {
                0 => {
                    if mem_fread(
                        &raw mut key as *mut ::core::ffi::c_void,
                        1 as size_t,
                        1 as size_t,
                        musinput,
                    ) != 1 as size_t
                    {
                        return true_0 as boolean;
                    }
                    if WriteReleaseKey(channel as byte, key, midioutput) != 0 {
                        return true_0 as boolean;
                    }
                }
                16 => {
                    if mem_fread(
                        &raw mut key as *mut ::core::ffi::c_void,
                        1 as size_t,
                        1 as size_t,
                        musinput,
                    ) != 1 as size_t
                    {
                        return true_0 as boolean;
                    }
                    if key as ::core::ffi::c_int & 0x80 as ::core::ffi::c_int != 0 {
                        if mem_fread(
                            (&raw mut channelvelocities as *mut byte).offset(channel as isize)
                                as *mut byte
                                as *mut ::core::ffi::c_void,
                            1 as size_t,
                            1 as size_t,
                            musinput,
                        ) != 1 as size_t
                        {
                            return true_0 as boolean;
                        }
                        channelvelocities[channel as usize] =
                            (channelvelocities[channel as usize] as ::core::ffi::c_int
                                & 0x7f as ::core::ffi::c_int) as byte;
                    }
                    if WritePressKey(
                        channel as byte,
                        key,
                        channelvelocities[channel as usize],
                        midioutput,
                    ) != 0
                    {
                        return true_0 as boolean;
                    }
                }
                32 => {
                    if !(mem_fread(
                        &raw mut key as *mut ::core::ffi::c_void,
                        1 as size_t,
                        1 as size_t,
                        musinput,
                    ) != 1 as size_t)
                    {
                        if WritePitchWheel(
                            channel as byte,
                            (key as ::core::ffi::c_int * 64 as ::core::ffi::c_int)
                                as ::core::ffi::c_short,
                            midioutput,
                        ) != 0
                        {
                            return true_0 as boolean;
                        }
                    }
                }
                48 => {
                    if mem_fread(
                        &raw mut controllernumber as *mut ::core::ffi::c_void,
                        1 as size_t,
                        1 as size_t,
                        musinput,
                    ) != 1 as size_t
                    {
                        return true_0 as boolean;
                    }
                    if (controllernumber as ::core::ffi::c_int) < 10 as ::core::ffi::c_int
                        || controllernumber as ::core::ffi::c_int > 14 as ::core::ffi::c_int
                    {
                        return true_0 as boolean;
                    }
                    if WriteChangeController_Valueless(
                        channel as byte,
                        controller_map[controllernumber as usize],
                        midioutput,
                    ) != 0
                    {
                        return true_0 as boolean;
                    }
                }
                64 => {
                    if mem_fread(
                        &raw mut controllernumber as *mut ::core::ffi::c_void,
                        1 as size_t,
                        1 as size_t,
                        musinput,
                    ) != 1 as size_t
                    {
                        return true_0 as boolean;
                    }
                    if mem_fread(
                        &raw mut controllervalue as *mut ::core::ffi::c_void,
                        1 as size_t,
                        1 as size_t,
                        musinput,
                    ) != 1 as size_t
                    {
                        return true_0 as boolean;
                    }
                    if controllernumber as ::core::ffi::c_int == 0 as ::core::ffi::c_int {
                        if WriteChangePatch(channel as byte, controllervalue, midioutput) != 0 {
                            return true_0 as boolean;
                        }
                    } else {
                        if (controllernumber as ::core::ffi::c_int) < 1 as ::core::ffi::c_int
                            || controllernumber as ::core::ffi::c_int > 9 as ::core::ffi::c_int
                        {
                            return true_0 as boolean;
                        }
                        if WriteChangeController_Valued(
                            channel as byte,
                            controller_map[controllernumber as usize],
                            controllervalue,
                            midioutput,
                        ) != 0
                        {
                            return true_0 as boolean;
                        }
                    }
                }
                96 => {
                    hitscoreend = 1 as ::core::ffi::c_int;
                }
                _ => return true_0 as boolean,
            }
            if eventdescriptor as ::core::ffi::c_int & 0x80 as ::core::ffi::c_int != 0 {
                break;
            }
        }
        if hitscoreend == 0 {
            timedelay = 0 as ::core::ffi::c_uint;
            loop {
                if mem_fread(
                    &raw mut working as *mut ::core::ffi::c_void,
                    1 as size_t,
                    1 as size_t,
                    musinput,
                ) != 1 as size_t
                {
                    return true_0 as boolean;
                }
                timedelay = timedelay
                    .wrapping_mul(128 as ::core::ffi::c_uint)
                    .wrapping_add(
                        (working as ::core::ffi::c_int & 0x7f as ::core::ffi::c_int)
                            as ::core::ffi::c_uint,
                    );
                if working as ::core::ffi::c_int & 0x80 as ::core::ffi::c_int
                    == 0 as ::core::ffi::c_int
                {
                    break;
                }
            }
            queuedtime = queuedtime.wrapping_add(timedelay);
        }
    }
    if WriteEndTrack(midioutput) != 0 {
        return true_0 as boolean;
    }
    if mem_fseek(midioutput, 18 as ::core::ffi::c_long, MEM_SEEK_SET) != 0 {
        return true_0 as boolean;
    }
    tracksizebuffer[0 as ::core::ffi::c_int as usize] =
        (tracksize >> 24 as ::core::ffi::c_int & 0xff as ::core::ffi::c_uint) as byte;
    tracksizebuffer[1 as ::core::ffi::c_int as usize] =
        (tracksize >> 16 as ::core::ffi::c_int & 0xff as ::core::ffi::c_uint) as byte;
    tracksizebuffer[2 as ::core::ffi::c_int as usize] =
        (tracksize >> 8 as ::core::ffi::c_int & 0xff as ::core::ffi::c_uint) as byte;
    tracksizebuffer[3 as ::core::ffi::c_int as usize] =
        (tracksize & 0xff as ::core::ffi::c_uint) as byte;
    if mem_fwrite(
        &raw mut tracksizebuffer as *mut byte as *const ::core::ffi::c_void,
        1 as size_t,
        4 as size_t,
        midioutput,
    ) != 4 as size_t
    {
        return true_0 as boolean;
    }
    return false_0 as boolean;
}
#[no_mangle]
#[optimize(size)]
pub unsafe extern "C" fn b82i_mus2mid() {
    memcpy(
        &raw mut channelvelocities as *mut ::core::ffi::c_void,
        &raw const channelvelocities__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<[byte; 16]>() as size_t,
    );
    memcpy(
        &raw mut queuedtime as *mut ::core::ffi::c_void,
        &raw const queuedtime__b82i as *const ::core::ffi::c_void,
        ::core::mem::size_of::<::core::ffi::c_uint>() as size_t,
    );
}

/// B83 (tools/fixdata.py): initial values, flash -> PSRAM .bss
#[optimize(size)]
pub unsafe fn b83i() {
    midiheader = [
    'M' as i32 as byte,
    'T' as i32 as byte,
    'h' as i32 as byte,
    'd' as i32 as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0x6 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0x1 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0x46 as ::core::ffi::c_int as byte,
    'M' as i32 as byte,
    'T' as i32 as byte,
    'r' as i32 as byte,
    'k' as i32 as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
    0 as ::core::ffi::c_int as byte,
];
    channelvelocities__b82i = [
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
    127 as ::core::ffi::c_int as byte,
];
    controller_map = [
    0 as ::core::ffi::c_int as byte,
    0x20 as ::core::ffi::c_int as byte,
    0x1 as ::core::ffi::c_int as byte,
    0x7 as ::core::ffi::c_int as byte,
    0xa as ::core::ffi::c_int as byte,
    0xb as ::core::ffi::c_int as byte,
    0x5b as ::core::ffi::c_int as byte,
    0x5d as ::core::ffi::c_int as byte,
    0x40 as ::core::ffi::c_int as byte,
    0x43 as ::core::ffi::c_int as byte,
    0x78 as ::core::ffi::c_int as byte,
    0x7b as ::core::ffi::c_int as byte,
    0x7e as ::core::ffi::c_int as byte,
    0x7f as ::core::ffi::c_int as byte,
    0x79 as ::core::ffi::c_int as byte,
];
}