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
    fn play_sound(wav_file: *const c_char, device_name: *const c_char, low_latency: bool);
    fn device_to_device(input: *const c_char, output: *const c_char, low_latency: bool, channel_name: *const c_char);
    fn app_to_device(input: *const c_char, output: *const c_char, low_latency: bool, channel_name: *const c_char);
    fn insert_volume(key: *const c_char, value: f32);
    fn reset_volume();
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
    let c_device = match files::get_output() {
        Some(output) => Some(CString::new(output).unwrap()),
        None => None,
    };

    let device: *const c_char = c_device
        .as_ref()
        .map_or(std::ptr::null(), |s| s.as_ptr());
    
    let file = CString::new(file_path).unwrap();

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

fn manage_device(input_device_name: Option<&str>, output_device_name: Option<&str>, low_latency: bool, channel_name: String) {
    let input_cstr = input_device_name.map(|device| CString::new(device).unwrap());
    let output_cstr = output_device_name.map(|device| CString::new(device).unwrap());
    let name_cstr = CString::new(channel_name).unwrap();

    let input: *const i8 = input_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());
    let output: *const i8 = output_cstr.as_ref().map_or(std::ptr::null(), |cstr| cstr.as_ptr());
    let name: *const i8 = name_cstr.as_ptr();

    unsafe {device_to_device(input, output, low_latency, name)};
}

fn manage_app(app_name: &str, output_device_name: Option<&str>, low_latency: bool, channel_name: String) {
    let output_cstr = output_device_name.map(|device| CString::new(device).unwrap());
    let input_cstr = CString::new(app_name).unwrap();
    let name_cstr = CString::new(channel_name).unwrap();

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

pub(crate) fn start() {
    if unsafe {stop_audio.load(Ordering::SeqCst) == false} {
        println!("Audio threads already running");
        println!("Please restart audio threads instead.");
        return;
    }

    unsafe {
        stop_audio.store(false, Ordering::SeqCst);
    }

    if let Some(channels) = files::get_channels() {
        for channel in channels {
            thread::Builder::new()
                .name(channel.name.clone())
                .spawn(move || {
                    unsafe {
                        let channel_cstr = CString::new(channel.name.clone()).unwrap();

                        let channel_name: *const i8 = channel_cstr.as_ptr();

                        insert_volume(channel_name, channel.volume);
                    }

                    if channel.deviceorapp {
                        manage_device(Some(&channel.device), files::get_output().as_deref(), channel.lowlatency, channel.name);
                    } else {
                        manage_app(&channel.device, files::get_output().as_deref(), channel.lowlatency, channel.name);
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