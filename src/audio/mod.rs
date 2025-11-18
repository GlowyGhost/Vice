use std::{
    ffi::{c_char, CStr, CString},
    sync::{atomic::{AtomicBool, Ordering}},
    thread
};

use crate::files::{self, Channel};

#[link(name = "audio")]
unsafe extern "C" {
    static stop_audio: AtomicBool;

    fn get_outputs(len: *mut usize) -> *const *const c_char;
    fn get_inputs(len: *mut usize) -> *const *const c_char;
    fn get_apps(len: *mut usize) -> *const *const c_char;
    fn play_sound(file: *const c_char, device_name: *const c_char, low_latency: bool);
    fn device_to_device(input: *const c_char, output: *const c_char, low_latency: bool, channel_name: *const c_char);
    fn app_to_device(input: *const c_char, output: *const c_char, low_latency: bool, channel_name: *const c_char);
    fn insert_volume(key: *const c_char, value: f32);
    fn reset_volume();
    fn get_volume(name: *const c_char, get: bool, device: bool) -> *const c_char;
    fn free_cstr(ptr: *const c_char);
}

pub(crate) fn outputs() -> Vec<String> {
    unsafe {
        let mut len: usize = 0;
        let devices: *const *const c_char = get_outputs(&mut len as *mut usize);

        let slice = std::slice::from_raw_parts(devices, len);

        slice.iter()
            .map(|&cstr_ptr| {
                CStr::from_ptr(cstr_ptr).to_string_lossy().into_owned()
            })
            .collect()
    }
}

pub(crate) fn inputs() -> Vec<String> {
    unsafe {
        let mut len: usize = 0;
        let devices: *const *const c_char = get_inputs(&mut len as *mut usize);

        let slice = std::slice::from_raw_parts(devices, len);

        slice.iter()
            .map(|&cstr_ptr| {
                CStr::from_ptr(cstr_ptr).to_string_lossy().into_owned()
            })
            .collect()
    }
}

pub(crate) fn play_sfx(file_path: &str, low_latency: bool) {
    let output: String = files::get_settings().output;

    let c_device: Option<CString> = match output.is_empty() {
        true => None,
        false => Some(CString::new(output).unwrap())
    };

    let device: *const c_char = c_device
        .as_ref()
        .map_or(std::ptr::null(), |s| s.as_ptr());
    
    let file: CString = CString::new(file_path).unwrap();

    unsafe {play_sound(file.as_ptr(), device, low_latency);}
}

pub(crate) fn apps() -> Vec<String> {
    unsafe {
        let mut len: usize = 0;
        let apps: *const *const c_char = get_apps(&mut len as *mut usize);

        let slice = std::slice::from_raw_parts(apps, len);

        slice.iter()
            .map(|&cstr_ptr| {
                CStr::from_ptr(cstr_ptr).to_string_lossy().into_owned()
            })
            .collect()
    }
}

fn manage_device(input_device_name: String, output_device_name: String, low_latency: bool, channel_name: String) {
    let input_cstr: Option<CString> = match input_device_name.is_empty() {
        true => None,
        false => Some(CString::new(input_device_name).unwrap())
    };
    let output_cstr: Option<CString> = match output_device_name.is_empty() {
        true => None,
        false => Some(CString::new(output_device_name).unwrap())
    };
    let name_cstr: CString = CString::new(channel_name).unwrap();

    let input: *const i8 = input_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());
    let output: *const i8 = output_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());
    let name: *const i8 = name_cstr.as_ptr();

    unsafe {device_to_device(input, output, low_latency, name)};
}

fn manage_app(app_name: String, output_device_name: String, low_latency: bool, channel_name: String) {
    let input_cstr: CString = CString::new(app_name).unwrap();
    let output_cstr: Option<CString> = match output_device_name.is_empty() {
        true => None,
        false => Some(CString::new(output_device_name).unwrap())
    };
    let name_cstr: CString = CString::new(channel_name).unwrap();

    let input: *const i8 = input_cstr.as_ptr();
    let output: *const i8 = output_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());
    let name: *const i8 = name_cstr.as_ptr();

    unsafe {app_to_device(input, output, low_latency, name)};
}

pub(crate) fn set_volume(channel_name: String, volume: f32) {
    let name_cstr = CString::new(channel_name).unwrap();

    let name: *const i8 = name_cstr.as_ptr();

    unsafe { insert_volume(name, volume); }
}

pub(crate) fn get_volume_parsed(name: String, get: bool, device: bool) -> String {
    let name_cstr = CString::new(name).unwrap();

    unsafe {
        let vol = get_volume(name_cstr.as_ptr(), get, device);
        let string_volume: String = CStr::from_ptr(vol).to_string_lossy().into_owned();
        free_cstr(vol);
        string_volume
    }
}

pub(crate) fn start() {
    if unsafe {stop_audio.load(Ordering::SeqCst) == false} {
        println!("Audio threads already running");
        println!("Please restart audio threads instead.");
        return;
    }

    unsafe {
        stop_audio.store(false, Ordering::SeqCst);
    }

    let channels: Vec<Channel> = files::get_channels();

    if channels.is_empty() {
        println!("No threads to create");
        return;
    }

    for channel in channels {
        let thread_name: String = channel.name.clone();
        if let Err(e) = thread::Builder::new()
            .name(thread_name.clone())
            .spawn(move || {
                unsafe {
                    let channel_cstr = CString::new(channel.name.clone()).unwrap();

                    let channel_name: *const i8 = channel_cstr.as_ptr();

                    insert_volume(channel_name, channel.volume);
                }

                if channel.deviceorapp {
                    manage_device(channel.device, files::get_settings().output, channel.lowlatency, channel.name);
                } else {
                    manage_app(channel.device, files::get_settings().output, channel.lowlatency, channel.name);
                }
            }) {
            eprintln!("Failed to spawn audio thread \"{}\": {}", thread_name, e);
        }
    }

    println!("Created audio threads");
}

pub(crate) fn restart() {
    thread::Builder::new()
        .name("audio_restart".to_string())
        .spawn(|| {
            println!("Restarting audio threads");

            unsafe {
                reset_volume();

                stop_audio.store(true, Ordering::SeqCst);
            }

            std::thread::sleep(std::time::Duration::from_millis(500));
            
            start();
        })
        .expect("Failed to spawn audio restart thread");
}