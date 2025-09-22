use cpal::{traits::{DeviceTrait, HostTrait}, Host};
use rodio::{Decoder, Device as RodioDevice, OutputStreamBuilder};
use once_cell::sync::Lazy;
use std::{
    sync::Mutex,
    fs::File,
    io::BufReader
};

use crate::files;

static HOST: Lazy<Mutex<Host>> = Lazy::new(|| Mutex::new(cpal::default_host()));
static STREAM: Lazy<Mutex<Option<rodio::OutputStream>>> = Lazy::new(|| Mutex::new(None));

pub(crate) fn outputs() -> Vec<String> {
    let host = HOST.lock().unwrap();
    let devices = host.output_devices().expect("Failed to get output devices");

    let mut parsed: Vec<String> = vec![];
    for device in devices {
        parsed.push(device.name().unwrap_or("<Unknown>".to_string()));
    }

    parsed
}

pub(crate) fn inputs() -> Vec<String> {
    let host = HOST.lock().unwrap();
    let devices = host.input_devices().expect("Failed to get output devices");

    let mut parsed: Vec<String> = vec![];
    for device in devices {
        parsed.push(device.name().unwrap_or("<Unknown>".to_string()));
    }

    parsed
}

pub(crate) fn init_stream(device_name: Option<String>) -> std::result::Result<(), Box<dyn std::error::Error>> {
    let host = HOST.lock().unwrap();

    let cpal_device = if let Some(name) = device_name {
        host.output_devices()?
            .find(|d| d.name().map(|n| n == name).unwrap_or(false))
            .unwrap_or_else(|| host.default_output_device().expect("No default output device found"))
    } else {
        host.default_output_device().expect("No default output device found")
    };

    println!("Using device: {}", cpal_device.name()?);

    let rodio_device: RodioDevice = cpal_device.into();
    let stream = OutputStreamBuilder::from_device(rodio_device)?.open_stream_or_fallback()?;

    let mut guard = STREAM.lock().unwrap();
    *guard = Some(stream);

    Ok(())
}

pub(crate) fn play_sfx(file_path: &str) -> std::result::Result<(), Box<dyn std::error::Error>> {
    let guard = STREAM.lock().unwrap();
    let stream = guard.as_ref().expect("Stream not initialized");

    let mixer = stream.mixer();

    let file = File::open(file_path)?;
    let source = Decoder::try_from(BufReader::new(file))?;

    mixer.add(source);

    Ok(())
}

pub(crate) fn get_apps() -> Vec<String> {
    #[cfg(target_os = "linux")]
    fn running_apps() -> Vec<String> {
        use std::collections::HashSet;
        use std::fs;
        use std::path::Path;
        use sysinfo::{ProcessExt, System, SystemExt};
        use x11rb::connection::Connection;
        use x11rb::protocol::xproto::*;
        use x11rb::rust_connection::RustConnection;

        let (conn, screen_num) = RustConnection::connect(None).expect("Failed to connect to X server");
        let screen = &conn.setup().roots[screen_num];
        let root = screen.root;

        // Get the list of top-level windows
        let tree = conn.query_tree(root).unwrap().reply().unwrap();
        let mut pids = HashSet::new();

        for window in tree.children {
            if let Ok(reply) = conn.get_property(
                false,
                window,
                conn.intern_atom(false, b"_NET_WM_PID").unwrap().reply().unwrap().atom,
                AtomEnum::CARDINAL,
                0,
                1,
            ) {
                if let Ok(prop) = reply.reply() {
                    if prop.value_len == 1 {
                        let pid = u32::from_ne_bytes(prop.value[0..4].try_into().unwrap());
                        pids.insert(pid);
                    }
                }
            }
        }

        let mut sys = System::new_all();
        sys.refresh_all();

        let mut apps = Vec::new();
        for pid in pids {
            if let Some(process) = sys.process(pid as sysinfo::Pid) {
                apps.push(process.name().to_string());
            } else {
                let path = format!("/proc/{}/comm", pid);
                if Path::new(&path).exists() {
                    if let Ok(name) = fs::read_to_string(path) {
                        apps.push(name.trim().to_string());
                    }
                }
            }
        }

        apps
    }

    #[cfg(target_os = "windows")]
    fn running_apps() -> Vec<String> {
        use std::collections::HashSet;
        use sysinfo::{ProcessExt, System, SystemExt};
        use windows::core::BOOL;
        use windows::Win32::Foundation::{HWND, LPARAM};
        use windows::Win32::UI::WindowsAndMessaging::{EnumWindows, GetWindowThreadProcessId, IsWindowVisible};
        use sysinfo::PidExt as _;

        let mut sys = System::new_all();
        sys.refresh_all();

        let mut pids: HashSet<u32> = HashSet::new();

        unsafe extern "system" fn enum_window_proc(hwnd: HWND, lparam: LPARAM) -> BOOL {
            let pids = &mut *(lparam.0 as *mut HashSet<u32>);
            if IsWindowVisible(hwnd).as_bool() {
                let mut pid = 0;
                GetWindowThreadProcessId(hwnd, Some(&mut pid));
                pids.insert(pid);
            }
            BOOL(1)
        }

        unsafe {
            let _ = EnumWindows(Some(enum_window_proc), LPARAM(&mut pids as *mut _ as isize));
        }

        let mut apps = Vec::new();
        for pid in pids {
            if let Some(process) = sys.process(sysinfo::Pid::from_u32(pid)) {
                let name = process.name().strip_suffix(".exe").unwrap_or(process.name());
                apps.push(name.to_string());
            }
        }

        apps
    }
    
    running_apps()
}

pub(crate) fn start() {
    let _ = init_stream(files::get_output());

    loop {
        println!("Doing audio...");
        std::thread::sleep(std::time::Duration::from_secs(10));
    }
}