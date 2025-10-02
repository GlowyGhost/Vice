use std::{
    ffi::{c_char, CStr, CString},
    sync::{atomic::{AtomicBool, Ordering}},
    thread
};

use crate::files::{self};

#[link(name = "audio")]
unsafe extern "C" {
    static stop_audio: AtomicBool;

    fn get_outputs(len: *mut usize) -> *const *const c_char;
    fn get_inputs(len: *mut usize) -> *const *const c_char;
    fn get_apps(len: *mut usize) -> *const *const c_char;
    fn play_sound(wav_file: *const c_char, device_name: *const c_char);
    fn device_to_device(input: *const c_char, output: *const c_char);
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

pub(crate) fn play_sfx(file_path: &str) {
    let c_device = match files::get_output() {
        Some(output) => Some(CString::new(output).unwrap()),
        None => None, // nullptr => default device
    };

    let device: *const c_char = c_device
        .as_ref()
        .map_or(std::ptr::null(), |s| s.as_ptr());

    unsafe {play_sound(CString::new(file_path).unwrap().as_ptr(), device);}
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

fn manage_device(input_device_name: Option<&str>, output_device_name: Option<&str>) {
    let input_cstr = input_device_name.map(|device| CString::new(device).unwrap());
    let output_cstr = output_device_name.map(|device| CString::new(device).unwrap());

    let input: *const i8 = input_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());
    let output: *const i8 = output_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());

    unsafe {device_to_device(input, output)};
}

fn manage_app(_app_name: &str, _output_device_name: Option<&str>) {
    return;
}

pub(crate) fn start() {
    if let Some(channels) = files::get_channels() {
        for channel in channels {
            thread::Builder::new()
                .name(channel.name.clone())
                .spawn(move || {
                    if channel.deviceorapp {
                        manage_device(Some(&channel.device), files::get_output().as_deref());
                    } else {
                        manage_app(&channel.device, files::get_output().as_deref());
                    }
                })
                .expect(&format!("Failed to spawn channel thread"));
        }

        println!("Created audio threads");
        return;
    }

    println!("No threads to create");
}

pub(crate) fn restart() {
    println!("Restarting audio threads");

    unsafe {stop_audio.store(true, Ordering::SeqCst);}

    start();
}