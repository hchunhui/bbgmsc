'use strict';

let mem8;

function get_string(ptr)
{
    let len;
    for (len = 0; mem8[ptr + len]; len++);
    return new TextDecoder("utf-8").decode(mem8.slice(ptr, ptr + len));
}

function copy_string(str, malloc)
{
    const buf = new TextEncoder().encode(str);
    const ptr = malloc(buf.length + 1);
    for (let i = 0; i < buf.length; i++)
        mem8[ptr + i] = buf[i];
    mem8[ptr + buf.length] = 0;
    return ptr;
}

function __abort(strptr)
{
    throw new Error('wasm abort: ' + get_string(strptr));
}

function exit(status)
{
    throw new Error('wasm exit with ' + status);
}

function __get_mticks()
{
    return Date.now();
}

function dolog(s)
{
    const o = document.getElementById('logarea');
    const len = o.value.length;
    if (len > 40960)
        o.value = o.value.substring(len - 40960, len) + s;
    else
        o.value += s;
    o.scrollTop = o.scrollHeight;
}

function __console_print(ptr)
{
    dolog(get_string(ptr));
}

const filestore = {}
const filestore_list = []
function __filestore_fetch(pathptr)
{
    const path = get_string(pathptr);
    filestore_list.push(path);
}

function __open_get_size(pathptr)
{
    const path = get_string(pathptr);
    if (path in filestore) {
        return filestore[path].length;
    }
    return -1;
}

const fdtable = {}
let next_fd = 3;
function __open(pathptr)
{
    const path = get_string(pathptr);
    if (path in filestore) {
        const fd = next_fd;
        next_fd++;
        fdtable[fd] = filestore[path];
        return fd;
    }
    return -1;
}

function __read(fd, bufptr, off, len)
{
    if (fd in fdtable) {
        const src = fdtable[fd];
        if (off >= src.length || off + len > src.length)
            return -1;
        for (let i = 0; i < len; i++)
            mem8[bufptr + i] = src[off + i];
        return 0;
    }
    return -1;
}

function __write(fd, bufptr, off, len)
{
    if (fd in fdtable) {
        const dst = fdtable[fd];
        if (off >= dst.length || off + len > dst.length)
            return -1;
        for (let i = 0; i < len; i++)
            dst[off + i] = mem8[bufptr + i];
        return 0;
    }
    return -1;
}

function __close(fd)
{
    delete fdtable[fd];
}

function drawfb(fbptr, width, height)
{
    const screen = document.getElementById('screen');
    const ctx = screen.getContext('2d');

    screen.width = width;
    screen.height = height;

    const data = ctx.createImageData(screen.width, screen.height);

    const len = screen.width * screen.height;
    for (let i = 0; i < len; i++) {
        const rgb565 = mem8[fbptr + 2 * i] | (mem8[fbptr + 2 * i + 1] << 8);
        data.data[4 * i + 0] = (rgb565 & 0x1f) << 3;
        data.data[4 * i + 1] = ((rgb565 >> 5) & 0x3f) << 2;
        data.data[4 * i + 2] = ((rgb565 >> 11) & 0x1f) << 3;
        data.data[4 * i + 3] = 255;
    }
    ctx.putImageData(data, 0, 0);
}

const codemap = {
    "KeyA": 4, "KeyB": 5, "KeyC": 6, "KeyD": 7, "KeyE": 8, "KeyF": 9, "KeyG": 10,
    "KeyH": 11, "KeyI": 12, "KeyJ": 13, "KeyK": 14, "KeyL": 15, "KeyM": 16,
    "KeyN": 17, "KeyO": 18, "KeyP": 19, "KeyQ": 20, "KeyR": 21, "KeyS": 22,
    "KeyT": 23, "KeyU": 24, "KeyV": 25, "KeyW": 26, "KeyX": 27, "KeyY": 28, "KeyZ": 29,
    "Digit1": 30, "Digit2": 31, "Digit3": 32, "Digit4": 33, "Digit5": 34,
    "Digit6": 35, "Digit7": 36, "Digit8": 37, "Digit9": 38, "Digit0": 39,
    "Enter": 40,
    "Escape": 41,
    "Backspace": 42,
    "Tab": 43,
    "Space": 44,
    "Minus": 45,
    "Equal": 46,
    "BracketLeft": 47,
    "BracketRight": 48,
    "Backslash": 49,
    "Semicolon": 51,
    "Quote": 52,
    "Backquote": 53,
    "Comma": 54,
    "Period": 55,
    "Slash": 56,
    "Capslock": 57,
    "F1": 58, "F2": 59, "F3": 60, "F4": 61, "F5": 62, "F6": 63,
    "F7": 64, "F8": 65, "F9": 66, "F10": 67, "F11": 68, "F12": 69,
    "PrintScreen": 70,
    "ScrollLock": 71,
    "Pause": 72,
    "Insert": 73,
    "Home": 74,
    "PageUp": 75,
    "Delete": 76,
    "End": 77,
    "PageDown": 78,
    "ArrowRight": 79,
    "ArrowLeft": 80,
    "ArrowDown": 81,
    "ArrowUp": 82,
    "NumLock": 83,
    "NumpadDivide": 84,
    "NumpadMultiply": 85,
    "NumpadSubtract": 86,
    "NumpadAdd": 87,
    "NumpadEnter": 88,
    "Numpad1": 89, "Numpad2": 90, "Numpad3": 91,
    "Numpad4": 92, "Numpad5": 93, "Numpad6": 94,
    "Numpad7": 95, "Numpad8": 96, "Numpad9": 97,
    "Numpad0": 98,
    "NumpadDecimal": 99,
    "ControlLeft": 224,
    "ShiftLeft": 225,
    "AltLeft": 226,
    "ControlRight": 228,
    "ShiftRight": 229,
    "AltRight": 230
};

function request_pointer_lock()
{
    const screen = document.getElementById('screen');
    screen.tabIndex = 1;
    screen.focus();
    screen.requestPointerLock();
}

function request_fullscreen()
{
    const screen = document.getElementById('screen');
    screen.tabIndex = 1;
    screen.focus();
    screen.requestFullscreen();
    screen.style.cursor = 'none';
}

function register_kbdmouse(ptr)
{
    const screen = document.getElementById('screen');
    function kbdhandler(ev, keypress) {
        ev.preventDefault();
        const code = ev.code;
        if (code in codemap) {
            const i = codemap[code];
            mem8[ptr + i] = keypress;
        }
    }

    screen.addEventListener('keydown', (event) => kbdhandler(event, 1));
    screen.addEventListener('keyup', (event) => kbdhandler(event, 0));

    screen.addEventListener('fullscreenchange', (event) => {
        if (!document.fullscreenElement) {
            screen.style.cursor = 'default';
        } else {
            screen.requestPointerLock();
        }
    });
}

const imports = {
    env: {
        __abort,
        exit,
        __get_mticks,
        __console_print,
        __filestore_fetch,
        __open_get_size,
        __open,
        __read,
        __write,
        __close,
        sin: Math.sin,
        cos: Math.cos,
        pow: Math.pow,
        log10: Math.log10,
        log2: Math.log2,
        tan: Math.tan,
        atan2: Math.atan2,
        round: Math.round,
    }
};

const fetchopt = { cache: 'no-store' };

function loads(files, i, cont) {
    if (i == files.length)
        cont();
    else {
        dolog('fetch ' + files[i] + ' ...\n');
        fetch(files[i], fetchopt)
            .then(response => response.arrayBuffer())
            .then(bytes => {
                filestore[files[i]] = new Uint8Array(bytes);
                loads(files, i + 1, cont);
            });
    }
}

function start()
{
    document.getElementById('startkey').disabled = true;
    fetch('smolnes.wasm', fetchopt)
        .then(response => response.arrayBuffer())
        .then(bytes => WebAssembly.compile(bytes))
        .then(module => new WebAssembly.Instance(module, imports))
        .then(instance => {
            instance.exports.memory.grow(100); // 64K * 100
            mem8 = new Uint8Array(instance.exports.memory.buffer);
            const biosfile = 'bbk_bios10.nes';
            const diskfile = document.getElementById('diskname').value;
            dolog('disk file ' + diskfile + '\n');
            loads([biosfile, diskfile], 0, () => {
                const biosptr = copy_string(biosfile, instance.exports.malloc);
                const diskptr = copy_string(diskfile, instance.exports.malloc);
                instance.exports.set_rom(biosptr);
                const key_state_ptr = instance.exports.get_key_state();
                const fb_ptr = instance.exports.get_frame_buffer();
                instance.exports.init(diskptr);
                register_kbdmouse(key_state_ptr);

                const screen = document.getElementById('screen');
                screen.focus();

                // web audio
                const audctx = new window.AudioContext;
                const audlen = instance.exports.wasm_getaudiolen();
                const mf64 = new Float64Array(instance.exports.memory.buffer);

                const n = 1;
                const dummybuf = audctx.createBuffer(1, audlen * n, 44100);
                const dummysrc = audctx.createBufferSource();

                const audcb = audctx.createScriptProcessor(audlen * n, 1, 1);
                audcb.addEventListener(
                    "audioprocess",
                    (ev) => {
                        const out = ev.outputBuffer;
                        for (let j = 0; j < n; j++) {
                            const ap = instance.exports.wasm_getaudio() / 8;
                            for (let ch = 0; ch < 1; ch++) {
                                const buf = out.getChannelData(ch);
                                const off = audlen * ch;
                                for (let i = 0; i < audlen; i++) {
                                    buf[audlen * j + i] = mf64[ap + off + i];
                                }
                            }
                        }
                    });
                audcb.connect(audctx.destination);

                dummysrc.buffer = dummybuf;
                dummysrc.loop = true;
                dummysrc.connect(audcb);
                dummysrc.start();

                function main_loop() {
                    setTimeout(main_loop, 20);
                    instance.exports.loop_many();
                    drawfb(fb_ptr, 256, 240);
                }
                main_loop();
            });
        });
}
